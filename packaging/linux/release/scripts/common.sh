#!/usr/bin/env bash

set -Eeuo pipefail

whalemaid_data_home() {
    printf '%s\n' "${XDG_DATA_HOME:-$HOME/.local/share}"
}

whalemaid_config_home() {
    printf '%s\n' "${XDG_CONFIG_HOME:-$HOME/.config}"
}

whalemaid_state_home() {
    printf '%s\n' "${XDG_STATE_HOME:-$HOME/.local/state}"
}

whalemaid_install_root() {
    printf '%s\n' "${WHALEMAID_INSTALL_ROOT:-$(whalemaid_data_home)/WhaleMaid}"
}

whalemaid_state_root() {
    printf '%s\n' "$(whalemaid_state_home)/WhaleMaid"
}

whalemaid_autostart_path() {
    printf '%s\n' "$(whalemaid_config_home)/autostart/whalemaid.desktop"
}

whalemaid_application_path() {
    printf '%s\n' "$(whalemaid_data_home)/applications/whalemaid.desktop"
}

whalemaid_uninstall_application_path() {
    printf '%s\n' "$(whalemaid_data_home)/applications/whalemaid-uninstall.desktop"
}

desktop_quote() {
    local value=$1
    value=${value//\\/\\\\}
    value=${value//\"/\\\"}
    value=${value//\`/\\\`}
    value=${value//\$/\\$}
    printf '"%s"' "$value"
}

write_start_desktop() {
    local destination=$1
    local executable=$2
    mkdir -p "$(dirname "$destination")"
    cat >"$destination" <<EOF
[Desktop Entry]
Type=Application
Version=1.0
Name=WhaleMaid
Comment=WhaleMaid-for-Codex Live2D desktop pet
Exec=$(desktop_quote "$executable") --manual-start
Icon=applications-graphics
Terminal=false
Categories=Utility;
StartupNotify=false
EOF
    chmod 0644 "$destination"
}

write_uninstall_desktop() {
    local destination=$1
    local script=$2
    mkdir -p "$(dirname "$destination")"
    cat >"$destination" <<EOF
[Desktop Entry]
Type=Application
Version=1.0
Name=卸载 WhaleMaid
Comment=Remove WhaleMaid from this user account
Exec=$(desktop_quote "$script")
Icon=edit-delete
Terminal=true
Categories=Utility;
StartupNotify=false
EOF
    chmod 0644 "$destination"
}

installed_pet_pids() {
    local expected
    expected=$(readlink -f "$(whalemaid_install_root)/app/WhaleMaidPet" 2>/dev/null || true)
    [[ -n "$expected" ]] || return 0
    local process
    for process in /proc/[0-9]*; do
        [[ -r "$process/exe" ]] || continue
        if [[ $(readlink -f "$process/exe" 2>/dev/null || true) == "$expected" ]]; then
            printf '%s\n' "${process##*/}"
        fi
    done
}

stop_installed_pet() {
    local executable="$(whalemaid_install_root)/app/WhaleMaidPet"
    if [[ -x "$executable" && -n ${DISPLAY:-} ]]; then
        "$executable" --codex-status shutdown >/dev/null 2>&1 || true
    fi

    local deadline=$((SECONDS + 5))
    local pids
    while (( SECONDS < deadline )); do
        pids=$(installed_pet_pids || true)
        [[ -z "$pids" ]] && return 0
        sleep 0.2
    done

    pids=$(installed_pet_pids || true)
    [[ -z "$pids" ]] || kill $pids
}

assert_safe_install_root() {
    local install_root
    install_root=$(readlink -m "$(whalemaid_install_root)")
    if [[ ${WHALEMAID_ALLOW_TEST_ROOT:-0} == 1 ]]; then
        return 0
    fi
    local data_home
    data_home=$(readlink -m "$(whalemaid_data_home)")
    if [[ $install_root != "$data_home/WhaleMaid" ]]; then
        printf '拒绝操作异常安装目录：%s\n' "$install_root" >&2
        return 1
    fi
}
