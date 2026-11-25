#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Builds the whole app (renderer + game + UI) for the desktop with software Vulkan and takes scripted screenshots.
#   wsl -d Ubuntu -- bash tools/host-app.sh [height]
set -euo pipefail
PROJ="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
W="${LANTERN_HOST_WORK:-$HOME/lantern-host}"
H="${1:-1080}"
HDR="$HOME/ps5deps/PS5_Mesa/include"
mkdir -p "$W/gfx" "$W/data" "$W/shots" "$PROJ/docs/screenshots"
bash "$PROJ/tools/gen-shaders.sh" "$W/gfx"

clang -g -std=gnu17 -O1 -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -Wno-unused-variable -I"$HDR" -I"$W/gfx" -I"$PROJ/ps5/gfx" -I"$PROJ/ps5/host" -I"$PROJ/ps5/game" \
    "$PROJ"/ps5/gfx/vkr.c "$PROJ"/ps5/game/lm_*.c "$PROJ/ps5/host/vk_host.c" "$PROJ/ps5/host/app_shots.c" \
    -l:libvulkan.so.1 -lz -lm -lpthread -o "$W/app_shots"
rm -rf "$W/shots" && mkdir -p "$W/shots"
# a sample table so the High Scores screenshots are not empty
rm -f "$W/data/scores.cfg" "$W/data/save.cfg" "$W/data/run.sav"
cat > "$W/data/scores.cfg" <<'EOT'
last=WILLY
WILLY|24800|17
LANTERN|18350|13
BARNABY|12100|10
DAISY|9400|8
MOLE|6150|6
SPARKY|3800|5
EOT
"$W/app_shots" "$W/data" "$PROJ/assets/fonts" "$PROJ/assets" "$W" "$H"
rm -f "$PROJ/docs/screenshots/"*.png
cp "$W/shots/"*.png "$PROJ/docs/screenshots/"
ls "$PROJ/docs/screenshots"
