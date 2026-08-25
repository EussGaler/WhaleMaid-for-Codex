#!/usr/bin/env bash

set -Eeuo pipefail
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
package_root=$(cd -- "$script_dir/.." && pwd)
source "$script_dir/common.sh"

install_root=$(whalemaid_install_root)
state_root=$(whalemaid_state_root)
autostart_path=$(whalemaid_autostart_path)
application_path=$(whalemaid_application_path)
uninstall_application_path=$(whalemaid_uninstall_application_path)
source_app="$package_root/app"

assert_safe_install_root
for required in \
    "$source_app/WhaleMaidPet" \
    "$source_app/qt.conf" \
    "$source_app/plugins/platforms/libqxcb.so" \
    "$source_app/Resources/WhaleMaid/WhaleMaid-runtime-v1.model3.json"; do
    if [[ ! -e "$required" ]]; then
        printf '安装包不完整，缺少：%s\n' "$required" >&2
        exit 2
    fi
done

mkdir -p "$state_root/logs"
log_path="$state_root/logs/installer.log"
exec > >(tee -a "$log_path") 2>&1

printf '[1/4] 正在停止旧版本……\n'
first_install=0
[[ -d "$install_root/app" ]] || first_install=1
autostart_enabled=0
[[ -f "$autostart_path" ]] && autostart_enabled=1
stop_installed_pet

printf '[2/4] 正在安装程序、Qt 与 Live2D 资源……\n'
parent=$(dirname "$install_root")
staging="$parent/.WhaleMaid.install.$$"
rm -rf "$staging"
mkdir -p "$staging"
cp -a "$source_app" "$staging/app"
cp -a "$package_root/scripts" "$staging/scripts"
chmod +x "$staging/app/WhaleMaidPet" "$staging/scripts/"*.sh
rm -rf "$install_root"
mv "$staging" "$install_root"
rm -f "$state_root/manual-exit.flag"

printf '[3/4] 正在创建启动与卸载快捷方式……\n'
write_start_desktop "$application_path" "$install_root/app/WhaleMaidPet"
write_uninstall_desktop \
    "$uninstall_application_path" \
    "$install_root/scripts/uninstall-whalemaid.sh"
if (( first_install || autostart_enabled )); then
    write_start_desktop "$autostart_path" "$install_root/app/WhaleMaidPet"
fi

printf '[4/4] 正在启动 WhaleMaid……\n'
if [[ ${WHALEMAID_NO_LAUNCH:-0} != 1 ]]; then
    nohup "$install_root/app/WhaleMaidPet" --manual-start \
        >>"$state_root/logs/runtime.log" 2>&1 </dev/null &
fi

printf '\n安装完成。\n安装目录：%s\n日志：%s\n' "$install_root" "$log_path"
if (( first_install )); then
    printf '首次安装已默认启用开机启动；可在 WhaleMaid“关于”窗口中关闭。\n'
elif (( autostart_enabled )); then
    printf '已保留原有设置：开机启动已启用。\n'
else
    printf '已保留原有设置：开机启动已关闭。\n'
fi
