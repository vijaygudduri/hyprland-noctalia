#!/usr/bin/env bash
# battery-notify.sh — event-driven charger + battery level notifications

# --- Single-instance lock (prevents duplicate notifications) ---
LOCK_FILE="/tmp/battery-notify.lock"
exec 200>"$LOCK_FILE"
if ! flock -n 200; then
    echo "Already running, exiting." >&2
    exit 1
fi

# --- Auto-detect battery and AC adapter ---
BAT=$(find /sys/class/power_supply -maxdepth 1 -name 'BAT*' | head -n1)
AC=$(find /sys/class/power_supply -maxdepth 1 \( -name 'AC*' -o -name 'ADP*' \) | head -n1)

if [[ -z "$BAT" || -z "$AC" ]]; then
    echo "Could not detect battery/AC in /sys/class/power_supply" >&2
    exit 1
fi

echo "Started: BAT=$BAT AC=$AC"

# --- Config ---
STATE_FILE="/tmp/battery-notify.state"
THRESHOLDS_LOW=(20 15 10 5)
THRESHOLDS_HIGH=(80 85 90 95 100)

# --- Load previous state ---
LAST_AC=""
LAST_CAPACITY=""
[[ -f "$STATE_FILE" ]] && source "$STATE_FILE"

check_state() {
    local CAPACITY ONLINE STATUS

    # Read sysfs files directly, no subshells
    read -r CAPACITY < "$BAT/capacity"
    read -r ONLINE   < "$AC/online"
    read -r STATUS   < "$BAT/status"

    # Charger connect/disconnect
    if [[ "$ONLINE" != "$LAST_AC" ]]; then
        if [[ "$ONLINE" == "1" ]]; then
            echo "Charger connected (battery ${CAPACITY}%)"
            notify-send -u normal -i battery "Charger Connected" "Battery at ${CAPACITY}%"
        else
            echo "Charger disconnected (battery ${CAPACITY}%)"
            notify-send -u normal -i battery "Charger Disconnected" "Battery at ${CAPACITY}%"
        fi
        LAST_AC="$ONLINE"
    fi

    # Low battery thresholds
    if [[ "$STATUS" == "Discharging" ]]; then
        for t in "${THRESHOLDS_LOW[@]}"; do
            if [[ "$CAPACITY" -eq "$t" && "$LAST_CAPACITY" != "$t" ]]; then
                urgency="normal"
                [[ "$t" -le 10 ]] && urgency="critical"
                echo "Low battery threshold hit: ${t}%"
                notify-send -u "$urgency" -i battery "Battery Low" "Battery at ${t}%, please connect the charger"
            fi
        done
    fi

    # High/charged thresholds
    if [[ "$STATUS" == "Charging" || "$STATUS" == "Full" ]]; then
        for t in "${THRESHOLDS_HIGH[@]}"; do
            if [[ "$CAPACITY" -eq "$t" && "$LAST_CAPACITY" != "$t" ]]; then
                echo "High battery threshold hit: ${t}%"
                notify-send -u normal -i battery "Battery Charged" "Battery at ${t}%, please disconnect the charger"
            fi
        done
    fi

    LAST_CAPACITY="$CAPACITY"
    printf 'LAST_AC=%q\nLAST_CAPACITY=%q\n' "$LAST_AC" "$LAST_CAPACITY" > "$STATE_FILE"
}

# Sync state immediately on startup
check_state

# Block on udev, only wake on real power_supply events
stdbuf -oL udevadm monitor --udev --subsystem-match=power_supply | while read -r line; do
    [[ "$line" == *"power_supply"* ]] || continue
    check_state
done