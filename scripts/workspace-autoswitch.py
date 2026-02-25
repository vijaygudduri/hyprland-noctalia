#!/usr/bin/env python3

import asyncio
import json
import os
import logging
import signal
import sys
import fcntl
from pathlib import Path

# ──────────────────────────────────────────────────────────────────────────────
# CONFIGURATION
# ──────────────────────────────────────────────────────────────────────────────
LOG_PATH    = Path.home() / ".cache/autoswitch.log"
PID_PATH    = Path.home() / ".cache/autoswitch.pid"
LOG_BYTES   = 20_000   # ~300-350 lines before rotation
LOG_BACKUPS = 1        # Keep one .log.1 backup
RETRY_DELAY = 2        # Seconds before reconnecting on socket error
# ──────────────────────────────────────────────────────────────────────────────


def setup_logger() -> logging.Logger:
    """Configure a rotating file logger."""
    from logging.handlers import RotatingFileHandler
    LOG_PATH.parent.mkdir(parents=True, exist_ok=True)

    logger = logging.getLogger("HyprAutoswitch")
    logger.setLevel(logging.INFO)

    handler = RotatingFileHandler(LOG_PATH, maxBytes=LOG_BYTES, backupCount=LOG_BACKUPS)
    handler.setFormatter(logging.Formatter("%(asctime)s - %(message)s", "%Y-%m-%d %H:%M:%S"))
    logger.addHandler(handler)
    return logger


class PidLock:
    """
    Kernel-level exclusive lock via flock.
    Automatically released if the process crashes (no stale PID issue).
    """
    def __init__(self, path: Path, logger: logging.Logger):
        self.path   = path
        self.logger = logger
        self._file  = None

    def acquire(self):
        try:
            self._file = open(self.path, "a+")
            fcntl.flock(self._file, fcntl.LOCK_EX | fcntl.LOCK_NB)
            self._file.seek(0)
            self._file.truncate()
            self._file.write(str(os.getpid()))
            self._file.flush()
        except (IOError, BlockingIOError):
            stale_pid = self._read_pid()
            self.logger.info(f"STARTUP ERROR: Already running (PID {stale_pid}). Exiting.")
            sys.exit(0)

    def release(self):
        if self._file:
            try:
                self._file.close()
            except OSError:
                pass
        if self.path.exists():
            self.path.unlink(missing_ok=True)

    def _read_pid(self) -> str:
        try:
            self._file.seek(0)
            return self._file.read().strip() or "unknown"
        except Exception:
            return "unknown"


class HyprAutoswitch:
    def __init__(self):
        self.logger     = setup_logger()
        self.pid_lock   = PidLock(PID_PATH, self.logger)
        self.pid_lock.acquire()

        self.stop_event  = asyncio.Event()
        self._switching  = False  # Debounce: ignore events while a switch is in progress

        # Events that indicate a window may have left the current workspace
        self._watched_events = ("closewindow>>", "movewindow>>")

    # ── Logging ───────────────────────────────────────────────────────────────

    def log(self, message: str):
        try:
            self.logger.info(message)
        except Exception as e:
            print(f"Logging error: {e}", file=sys.stderr)

    # ── Hyprland IPC ──────────────────────────────────────────────────────────

    async def _hyprctl(self, *args) -> bytes | None:
        """Run a hyprctl command and return raw stdout, or None on error."""
        try:
            proc = await asyncio.create_subprocess_exec(
                "hyprctl", *args,
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE,
            )
            stdout, stderr = await proc.communicate()
            if proc.returncode != 0:
                self.log(f"hyprctl error ({' '.join(args)}): {stderr.decode().strip()}")
                return None
            return stdout
        except Exception as e:
            self.log(f"hyprctl exec error: {e}")
            return None

    async def get_hypr_json(self, command: str) -> dict | list | None:
        """Fetch and parse JSON from hyprctl."""
        raw = await self._hyprctl(command, "-j")
        if raw is None:
            return None
        try:
            return json.loads(raw)
        except json.JSONDecodeError as e:
            self.log(f"JSON parse error ({command}): {e}")
            return None

    async def dispatch(self, *args) -> bool:
        """Send a dispatch command to Hyprland. Returns True on success."""
        raw = await self._hyprctl("dispatch", *args)
        return raw is not None

    # ── Workspace logic ───────────────────────────────────────────────────────

    async def switch_if_needed(self, current_ws: int):
        """
        If the active workspace is empty, jump to the nearest occupied one.
        Uses a single 'clients' call to avoid a TOCTOU race between fetching
        state and dispatching the switch.
        """
        # Debounce: if a switch is already in progress, skip this event
        if self._switching:
            return
        self._switching = True

        try:
            clients = await self.get_hypr_json("clients")
            # Treat None (error) and [] (no windows at all) the same way
            if not clients:
                return

            # Workspaces that still have at least one window, sorted ascending
            occupied: list[int] = sorted({
                c["workspace"]["id"]
                for c in clients
                if c.get("workspace", {}).get("id") is not None
            })

            # Nothing to do if the current workspace still has windows
            if current_ws in occupied:
                return

            # Nothing to switch to
            if not occupied:
                return

            # Prefer the next workspace above, fall back to the next one below
            above = next((ws for ws in occupied if ws > current_ws), None)
            below = next((ws for ws in reversed(occupied) if ws < current_ws), None)
            target = above if above is not None else below

            if target is None or target == current_ws:
                return

            ok = await self.dispatch("workspace", str(target))
            if ok:
                self.log(f"Switched {current_ws} -> {target} (workspace empty)")
        finally:
            self._switching = False

    # ── Event loop ────────────────────────────────────────────────────────────

    async def _get_socket_path(self) -> Path | None:
        sig = os.getenv("HYPRLAND_INSTANCE_SIGNATURE")
        if not sig:
            self.log("CRITICAL: HYPRLAND_INSTANCE_SIGNATURE not set. Are you running Hyprland?")
            return None
        xdg_runtime = os.getenv("XDG_RUNTIME_DIR", f"/run/user/{os.getuid()}")
        return Path(xdg_runtime) / "hypr" / sig / ".socket2.sock"

    async def _handle_event(self, raw: str):
        """Route a raw socket event to the appropriate handler."""
        if not any(raw.startswith(ev) for ev in self._watched_events):
            return
        ws_data = await self.get_hypr_json("activeworkspace")
        if ws_data and "id" in ws_data:
            await self.switch_if_needed(ws_data["id"])

    async def _read_events(self, reader: asyncio.StreamReader):
        """
        Inner read loop. Reads lines from the socket until EOF or stop_event.
        Returns True if we should reconnect, False if we should exit.
        """
        stop_task = asyncio.create_task(self.stop_event.wait())
        try:
            while not self.stop_event.is_set():
                line_task = asyncio.create_task(reader.readline())

                done, _ = await asyncio.wait(
                    [line_task, stop_task],
                    return_when=asyncio.FIRST_COMPLETED,
                )

                if stop_task in done:
                    line_task.cancel()
                    return False  # Clean shutdown

                # line_task completed
                line = line_task.result()
                if not line:
                    return True  # EOF — reconnect

                await self._handle_event(line.decode().strip())
        finally:
            stop_task.cancel()

        return False

    async def listen(self):
        """Connect to Hyprland's event socket and maintain the connection."""
        socket_path = await self._get_socket_path()
        if socket_path is None:
            return

        loop = asyncio.get_running_loop()
        for sig in (signal.SIGINT, signal.SIGTERM):
            loop.add_signal_handler(sig, self.stop_event.set)

        while not self.stop_event.is_set():
            if not socket_path.exists():
                await asyncio.sleep(1)
                continue

            try:
                reader, writer = await asyncio.open_unix_connection(str(socket_path))
                self.log("Connected to Hyprland socket.")

                reconnect = await self._read_events(reader)

                writer.close()
                await writer.wait_closed()

                if not reconnect or self.stop_event.is_set():
                    break

                self.log("Socket closed — reconnecting...")

            except (ConnectionRefusedError, FileNotFoundError):
                await asyncio.sleep(RETRY_DELAY)
            except Exception as e:
                self.log(f"Socket error: {e}")
                await asyncio.sleep(RETRY_DELAY)

        self._cleanup()

    # ── Shutdown ──────────────────────────────────────────────────────────────

    def _cleanup(self):
        self.pid_lock.release()
        self.log("Autoswitch stopped cleanly.")


# ── Entry point ───────────────────────────────────────────────────────────────

if __name__ == "__main__":
    app = HyprAutoswitch()
    try:
        asyncio.run(app.listen())
    except Exception as e:
        app.log(f"Fatal crash: {e}")
        app._cleanup()
        sys.exit(1)
        
