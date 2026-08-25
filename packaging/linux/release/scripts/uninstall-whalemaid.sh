#!/usr/bin/env bash

set -Eeuo pipefail
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
source "$script_dir/common.sh"

assert_safe_install_root
if [[ ${1:-} != --yes ]]; then
    read -r -p '确认卸载 WhaleMaid？[y/N] ' answer
    [[ $answer == y || $answer == Y ]] || exit 0
fi

install_root=$(whalemaid_install_root)
state_root=$(whalemaid_state_root)
printf '正在停止 WhaleMaid……\n'
stop_installed_pet
rm -f \
    "$(whalemaid_autostart_path)" \
    "$(whalemaid_application_path)" \
    "$(whalemaid_uninstall_application_path)"
rm -rf "$install_root" "$state_root"
printf 'WhaleMaid 已卸载。\n'
