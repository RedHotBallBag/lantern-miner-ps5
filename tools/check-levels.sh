#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Plays every hand-built cavern to prove it can be finished.
#   wsl -d Ubuntu -- bash tools/check-levels.sh [first-map last-map]
set -euo pipefail
PROJ="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
G="$PROJ/ps5/game"
bin="$(mktemp)"
trap 'rm -f "$bin"' EXIT
# chests only appear by luck, so the checker forces them all in
clang -O2 -std=gnu17 -DLM_ALL_CHESTS -I"$G" "$PROJ/tools/check_levels.c" "$G/lm_engine.c" "$G/lm_gen.c" "$G/lm_data.c" \
    "$G/lm_audio.c" -lm -lpthread -o "$bin"
"$bin" "$@"
