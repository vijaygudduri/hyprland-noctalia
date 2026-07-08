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
APP_NAME="battery-notification"
THRESHOLDS_LOW=(20 15 10 5)
THRESHOLDS_HIGH=(80 85 90 95 100)

# --- Load previous state ---
LAST_AC=""
LAST_CAPACITY=""
[[ -f "$STATE_FILE" ]] && source "$STATE_FILE"

# Picks a battery icon name based on percentage and charge state.
# Rounds down to the nearest 10 (standard Freedesktop icon naming only ships
# in steps of 10, e.g. battery-020-symbolic, battery-full-charging-symbolic).
battery_icon() {
    local percentage="$1" charging="$2"
    local step=$(( percentage / 10 * 10 ))
    (( step > 100 )) && step=100

    if (( step >= 100 )); then
        if [[ "$charging" == "1" ]]; then
            echo "battery-full-charging-symbolic"
        else
            echo "battery-full-symbolic"
        fi
    else
        printf -v padded "%03d" "$step"
        if [[ "$charging" == "1" ]]; then
            echo "battery-${padded}-charging-symbolic"
        else
            echo "battery-${padded}-symbolic"
        fi
    fi
}

check_state() {
    local CAPACITY ONLINE STATUS ICON

    # Read sysfs files directly, no subshells
    read -r CAPACITY < "$BAT/capacity"
    read -r ONLINE   < "$AC/online"
    read -r STATUS   < "$BAT/status"

    # Charger connect/disconnect
    if [[ "$ONLINE" != "$LAST_AC" ]]; then
        ICON=$(battery_icon "$CAPACITY" "$ONLINE")
        if [[ "$ONLINE" == "1" ]]; then
            echo "Charger connected (battery ${CAPACITY}%)"
            notify-send -h int:transient:1 -a "$APP_NAME" -u normal -i "$ICON" "Charger Connected" "Battery at ${CAPACITY}%"
        else
            echo "Charger disconnected (battery ${CAPACITY}%)"
            notify-send -h int:transient:1 -a "$APP_NAME" -u normal -i "$ICON" "Charger Disconnected" "Battery at ${CAPACITY}%"
        fi
        LAST_AC="$ONLINE"
    fi

    # Low battery thresholds
    if [[ "$STATUS" == "Discharging" ]]; then
        for t in "${THRESHOLDS_LOW[@]}"; do
            if [[ "$CAPACITY" -eq "$t" && "$LAST_CAPACITY" != "$t" ]]; then
                urgency="normal"
                [[ "$t" -le 10 ]] && urgency="critical"
                ICON=$(battery_icon "$t" "0")
                echo "Low battery threshold hit: ${t}%"
                notify-send -h int:transient:1 -a "$APP_NAME" -u "$urgency" -i "$ICON" "Battery Low" "Battery at ${t}%, please connect the charger"
            fi
        done
    fi

    # High/charged thresholds
    if [[ "$STATUS" == "Charging" || "$STATUS" == "Full" ]]; then
        for t in "${THRESHOLDS_HIGH[@]}"; do
            if [[ "$CAPACITY" -eq "$t" && "$LAST_CAPACITY" != "$t" ]]; then
                ICON=$(battery_icon "$t" "1")
                echo "High battery threshold hit: ${t}%"
                notify-send -h int:transient:1 -a "$APP_NAME" -u normal -i "$ICON" "Battery Charged" "Battery at ${t}%, please disconnect the charger"
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