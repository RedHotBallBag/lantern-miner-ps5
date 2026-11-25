#!/usr/bin/env bash
# Lantern Miner PS5 title build (PPSA58001). Run inside WSL:
#   wsl -d Ubuntu -- bash tools/build-title.sh            # LTO build   -> build/PPSA58001
#   LANTERN_LTO=0 LANTERN_OUT_SUFFIX=-safe bash tools/build-title.sh   # no LTO -> build/PPSA58001-safe
#
# Copyright (C) 2026 RedHotBallBag
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Uses the link recipe from PS5 Vulkan (tools/radv-link.sh, Copyright (C) 2026 Mihawk-99) in a private copy of
# that project (~/lantern-spike). The payload SDK is only read. See THIRD_PARTY_NOTICES.md.
set -euo pipefail

PROJ="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
root="${SPIKE_ROOT:-$HOME/lantern-spike}"
mesa="${PS5_MESA_FORK:-$HOME/rpcs3-PS5/deps/PS5_Mesa}"
archive="${RADV_ARCHIVE:-$root/.deps/native/radv-release/lib/libvulkan_radeon.ps5.a}"
sdk_root="${PS5_SDK:-$HOME/rpcs3-PS5/deps/sdk-rpcs3}"
native="$root/tooling/native"
tool="$root/build/host/ps5-native-tool"
work="$root/build/lantern"
title=PPSA58001
param="$root/sce_sys/param-lantern.json"

for f in "$archive" "$tool" "$sdk_root/bin/prospero-lld" "$PROJ/assets/art/title.jpg" "$PROJ/assets/music/theme.wav" \
    "$PROJ/assets/brand/icon0.png" "$PROJ/assets/brand/pic1.png" "$PROJ/assets/brand/pic.dds" "$PROJ/assets/brand/snd0.at9"; do
    [[ -e $f ]] || { echo "missing $f" >&2; exit 2; }
done

export PS5_CLANG="${PS5_CLANG:-$(command -v clang)}"
sed -e 's/99014/58001/g' -e 's/UP9000-PPSA58001_00-PS5VKRADV0000001/UP9000-PPSA58001_00-LANTERNMINER0001/' \
    -e 's/PS5 Vulkan RADV/Lantern Miner/' "$root/sce_sys/param-radv.json" > "$param"
grep -q '"titleId": "PPSA58001"' "$param" || { echo "param.json titleId not set" >&2; exit 2; }

rm -rf "$work"; mkdir -p "$work/obj" "$work/stubs" "$work/gen"
cc() { PS5_PAYLOAD_SDK="$sdk_root" sh "$root/tooling/prospero-clang18" "$@"; }

bash "$PROJ/tools/gen-shaders.sh" "$work/gen"

LTO_FLAG=()
[ "${LANTERN_LTO:-1}" = 1 ] && LTO_FLAG=(-flto)
echo "compiling: -O2 ${LTO_FLAG[*]:-(no LTO)}"
# -fno-math-errno: sqrt becomes an instruction (no libm import)
MYFLAGS=(-std=gnu17 -O2 -fno-math-errno -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -Wno-unused-variable "${LTO_FLAG[@]}"
         -ffunction-sections -fdata-sections
         -ffile-prefix-map="$PROJ"=. -fdebug-prefix-map="$PROJ"=. -fmacro-prefix-map="$PROJ"=. # no local paths in the shipped binary
         -I "$mesa/include" -I "$work/gen" -I "$PROJ/ps5/gfx" -I "$PROJ/ps5/game")
objs=()
for src in "$PROJ/ps5/gfx/vkr.c" "$PROJ"/ps5/game/lm_*.c "$PROJ/ps5/platform/main_ps5.c"; do
    o="$work/obj/$(basename "$src" .c).o"; objs+=("$o")
    cc "${MYFLAGS[@]}" -c "$src" -o "$o" &
    while [ "$(jobs -r | wc -l)" -ge "$(nproc)" ]; do wait -n; done
done
cc -std=c++20 -O2 -Wall -Wextra -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections -c "$native/app_crt.cpp" -o "$work/obj/app_crt.o" &
cc -std=c++20 -O2 -Wall -Wextra -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections -c "$native/app_cpp_runtime.cpp" -o "$work/obj/app_cpp_runtime.o" &
wait
echo "compiled"

module_sdk=0x02000009
companion_sdk=0x08050001
fself_magic=0x1D3D154F
stub() {
    local library=$1 source=$2
    cc -std=c11 -O2 -fPIC -c "$root/$source" -o "$work/obj/${library}_stub.o"
    "$sdk_root/bin/prospero-lld" --shared -soname "${library}.prx" -o "$work/stubs/${library}.so" "$work/obj/${library}_stub.o"
}
stub libSceAgc vendor/ps5/sdk/stubs/agc_canary_link_stub.c
stub libSceAgcDriver vendor/ps5/sdk/stubs/agc_driver_canary_link_stub.c

# shellcheck source=/dev/null
source "$root/tools/radv-link.sh"
radv_link_recipe "$root" "$sdk_root" "$archive" || exit 2
# A title publishes no exports; weak references nothing defines resolve to 0 instead of becoming imports no module exports.
printf '{
    local: *;
};
' > "$work/title-local.map"
radv_link_flags+=(--version-script "$work/title-local.map")
"$sdk_root/bin/prospero-lld" --error-limit=0 -z nodynamic-undefined-weak "${radv_linker_script[@]}" --eh-frame-hdr "${radv_link_flags[@]}" \
    --version-script "$native/app-symbols.map" --exclude-libs=ALL -e _start -o "$work/llvm-pie.elf" \
    "$work/obj/app_crt.o" "$work/obj/app_cpp_runtime.o" "${objs[@]}" \
    "$work/stubs/libSceAgc.so" "$work/stubs/libSceAgcDriver.so" \
    "${radv_link_inputs[@]}" --as-needed "$sdk_root"/target/lib/*.so

# Refuse imports no loaded module exports (they would be null at run time), here and not on the console.
null_imports=$(comm -23 <("$sdk_root/bin/llvm-nm" -D --undefined-only "$work/llvm-pie.elf" |
        awk '$1 == "U" { sub(/@.*/, "", $2); print $2 }' | sort -u) \
    <(for library in "$sdk_root"/target/lib/*.so "$work/stubs/libSceAgc.so" "$work/stubs/libSceAgcDriver.so"; do
        case ${library##*/} in libkernel_sys.so | libScePosixForWebKit.so) continue ;; esac
        "$sdk_root/bin/llvm-nm" -D --defined-only "$library" 2>/dev/null | awk '{ print $NF }'
    done | sort -u))
if [[ -n $null_imports ]]; then
    echo "imports no module a title loads exports (null at run time): ${null_imports//$'\n'/ }" >&2
    exit 1
fi
echo "link ok, no null imports"
"$tool" link --in "$work/llvm-pie.elf" --out "$work/eboot.elf" \
    --stub-dir "$sdk_root/target/lib" --stub "$work/stubs/libSceAgc.so" \
    --stub "$work/stubs/libSceAgcDriver.so" --module-sdk "$module_sdk" \
    --companion-sdk "$companion_sdk" --file-name eboot.elf

app="$root/dist/$title"
rm -rf -- "$app"
mkdir -p "$app/sce_sys" "$app/sce_module" "$app/assets/fonts" "$app/assets/art" "$app/assets/music"
"$tool" self --sign --in "$work/eboot.elf" --out "$app/eboot.bin" --magic "$fself_magic"
cp "$param" "$app/sce_sys/param.json"
cp "$PROJ/LICENSE" "$PROJ/THIRD_PARTY_NOTICES.md" "$app/"
cp "$PROJ"/assets/fonts/*.ttf "$PROJ"/assets/fonts/LICENSES.txt "$app/assets/fonts/"
cp "$PROJ"/assets/art/*.jpg "$PROJ"/assets/art/*.png "$app/assets/art/"
cp "$PROJ/assets/music/theme.wav" "$app/assets/music/"
# Home screen presentation, made by tools/make_presentation.py (run it after changing the version or the theme):
# pic0.dds = background while the icon is highlighted, pic1.dds/.png = launch background, snd0.at9 = highlight music.
cp "$PROJ/assets/brand/icon0.png" "$PROJ/assets/brand/pic1.png" "$PROJ/assets/brand/snd0.at9" "$app/sce_sys/"
cp "$PROJ/assets/brand/pic.dds" "$app/sce_sys/pic0.dds"
cp "$PROJ/assets/brand/pic.dds" "$app/sce_sys/pic1.dds"
[[ -f $root/runtime/libc.prx ]] || bash "$root/tools/rebuild-libc.sh"
(cd "$root/runtime" && sha256sum --check --strict --quiet libc.prx.sha256)
cp "$root/runtime/libc.prx" "$app/sce_module/libc.prx"
"$tool" self --inspect --file "$app/sce_module/libc.prx" > /dev/null
"$tool" self --inspect --file "$app/eboot.bin" > /dev/null

out_name="$title${LANTERN_OUT_SUFFIX:-}"
rm -rf "$PROJ/build/$out_name"; mkdir -p "$PROJ/build"
cp -r "$app" "$PROJ/build/$out_name"
ls -l "$PROJ/build/$out_name" "$PROJ/build/$out_name/sce_sys"
echo "BUILD_OK  eboot $(stat -c %s "$app/eboot.bin") bytes -> build/$out_name"
