#!/usr/bin/env bash
# Compiles ps5/gfx/shaders/*.{vert,frag} to SPIR-V, validates them, and writes one C header with the arrays.
#   gen-shaders.sh <out-dir>      (writes <out-dir>/spv_shaders.h)
set -euo pipefail
PROJ="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
out="${1:?usage: gen-shaders.sh <out-dir>}"
mkdir -p "$out"
tmp="$(mktemp -d)"
: > "$out/spv_shaders.h.tmp"
for s in ui.vert ui.frag; do
    name="spv_${s/./_}"
    glslangValidator -V --target-env vulkan1.0 --vn "$name" "$PROJ/ps5/gfx/shaders/$s" -o "$tmp/$s.h" > /dev/null
    glslangValidator -V --target-env vulkan1.0 "$PROJ/ps5/gfx/shaders/$s" -o "$tmp/$s.spv" > /dev/null
    spirv-val --target-env vulkan1.0 "$tmp/$s.spv"
    sed 's/^const uint32_t/static const uint32_t/' "$tmp/$s.h" >> "$out/spv_shaders.h.tmp"
    echo "static const unsigned long ${name}_size = sizeof(${name});" >> "$out/spv_shaders.h.tmp"
done
mv "$out/spv_shaders.h.tmp" "$out/spv_shaders.h"
rm -rf "$tmp"
echo "shaders ok: $out/spv_shaders.h"
