#!/usr/bin/env bash

set -Eeuo pipefail

repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
qt_root=${1:-$HOME/Qt/6.11.2/gcc_64}
qt_install_root=$(cd -- "$qt_root/../.." && pwd)
build_dir=${2:-$repo_root/src/desktop-app/build}
output_root=${3:-$repo_root/dist/WhaleMaid-for-Codex-Ubuntu-x64-v1.3.3}
sdk_root=${WHALE_LIVE2D_SDK_ROOT:-$repo_root/../SDK/CubismSdkForNative-5-r.5}

if [[ $(basename "$output_root") != WhaleMaid-for-Codex-Ubuntu-x64-v1.3.3 ]]; then
    printf '拒绝清理异常输出目录：%s\n' "$output_root" >&2
    exit 2
fi
for required in \
    "$build_dir/WhaleMaidPet" \
    "$qt_root/plugins/platforms/libqxcb.so" \
    "$qt_root/plugins/xcbglintegrations/libqxcb-glx-integration.so"; do
    [[ -e "$required" ]] || { printf '缺少发布输入：%s\n' "$required" >&2; exit 3; }
done

rm -rf "$output_root"
mkdir -p "$output_root/app/lib" \
    "$output_root/app/plugins/platforms" \
    "$output_root/app/plugins/xcbglintegrations" \
    "$output_root/licenses/Qt" \
    "$output_root/licenses/Live2D"

cmake --install "$build_dir" --prefix "$output_root/app"
cp -L "$qt_root/plugins/platforms/libqxcb.so" \
    "$output_root/app/plugins/platforms/"
cp -L "$qt_root/plugins/xcbglintegrations/libqxcb-glx-integration.so" \
    "$output_root/app/plugins/xcbglintegrations/"

copy_runtime_dependencies() {
    local target=$1
    while read -r dependency; do
        [[ -f "$dependency" ]] || continue
        local name=${dependency##*/}
        local destination="$output_root/app/lib/$name"
        if [[ $(readlink -f "$dependency") == $(readlink -m "$destination") ]]; then
            continue
        fi
        case "$dependency" in
            "$qt_root"/*)
                cp -L "$dependency" "$destination"
                ;;
            *)
                case "$name" in
                    libxkbcommon-x11.so.*|libxcb-cursor.so.*|libxcb-icccm.so.*|\
                    libxcb-util.so.*|libxcb-image.so.*|libxcb-keysyms.so.*|\
                    libxcb-render-util.so.*|libxcb-xkb.so.*)
                        cp -L "$dependency" "$destination"
                        ;;
                esac
                ;;
        esac
    done < <(ldd "$target" | awk '/=> \// {print $3}')
}

copy_runtime_dependencies "$build_dir/WhaleMaidPet"
copy_runtime_dependencies "$qt_root/plugins/platforms/libqxcb.so"
copy_runtime_dependencies \
    "$qt_root/plugins/xcbglintegrations/libqxcb-glx-integration.so"

cat >"$output_root/app/qt.conf" <<'EOF'
[Paths]
Prefix=.
Libraries=lib
Plugins=plugins
EOF

cp -a "$repo_root/packaging/linux/release/." "$output_root/"
cp "$repo_root/LICENSE" "$repo_root/NOTICE.md" \
    "$repo_root/ASSETS-LICENSE.md" "$output_root/"
cp "$qt_install_root/Licenses/LICENSE" "$qt_install_root/Licenses/Copyright.txt" \
    "$output_root/licenses/Qt/"
cp "$sdk_root/LICENSE.md" \
    "$output_root/licenses/Live2D/Cubism-SDK-LICENSE.md"
cp "$sdk_root/NOTICE.md" \
    "$output_root/licenses/Live2D/Cubism-SDK-NOTICE.md"
cp "$sdk_root/Core/LICENSE.md" \
    "$output_root/licenses/Live2D/Cubism-Core-LICENSE.md"

chmod +x "$output_root/"*.sh "$output_root/scripts/"*.sh \
    "$output_root/app/WhaleMaidPet"

if ldd "$output_root/app/WhaleMaidPet" | grep -q 'not found'; then
    printf '发布程序仍有缺失动态库。\n' >&2
    ldd "$output_root/app/WhaleMaidPet" >&2
    exit 4
fi

printf 'Release 已生成：%s\n' "$output_root"
