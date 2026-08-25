#!/usr/bin/env bash

set -Eeuo pipefail
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
source "$script_dir/common.sh"

install_root=$(whalemaid_install_root)
state_root=$(whalemaid_state_root)
executable="$install_root/app/WhaleMaidPet"
if [[ ! -x "$executable" ]]; then
    printf '没有找到已安装的 WhaleMaid，请先运行“安装WhaleMaid.sh”。\n' >&2
    exit 2
fi

mkdir -p "$state_root/logs"
rm -f "$state_root/manual-exit.flag"
nohup "$executable" --manual-start >>"$state_root/logs/runtime.log" 2>&1 </dev/null &
sleep 1
if [[ -z $(installed_pet_pids || true) ]]; then
    printf '启动失败，请检查日志：%s\n' "$state_root/logs/runtime.log" >&2
    exit 3
fi
printf 'WhaleMaid 已启动。\n'
