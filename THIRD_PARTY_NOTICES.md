# Licences and credits

## Lantern Miner

The code is Copyright (C) 2026 RedHotBallBag and licensed under the **GNU General Public Licence, version 3 or later**
(see `LICENSE`). It comes with no warranty.

The artwork (`assets/art`, `assets/brand`, `sce_sys/icon0.png`), the title music (`assets/music`, `assets/brand/snd0.at9`), the screenshots
and the Lantern Miner name are Copyright (C) 2026 RedHotBallBag and are not covered by the GPL. You're welcome to pass
them on unchanged as part of the game. The buy me a coffee code and logo are only there to point at my page.

## Code adapted into this project

| What | From | Licence |
| --- | --- | --- |
| Display and Vulkan set-up (`ps5/platform/main_ps5.c`, `ps5/gfx/vkr.c`) | PS5 Vulkan, Copyright (C) 2026 Mihawk-99 | GPL-3.0-or-later |
| Audio output approach | [PS5SX2](https://github.com/Swordpdf/PS5SX2) | GPL-3.0 |
| Controller handling approach | ROM Vault, from ps5-native-app-boilerplate by BlackBearReloaded | GPL-3.0-or-later |
| BC7 texture encoder in `tools/make_presentation.py` | My own FurrySXDumper | GPL-3.0-or-later here |
| `ps5/gfx/third_party/stb_image.h`, `stb_truetype.h` | [Sean Barrett](https://github.com/nothings/stb) | MIT or public domain |

## Built into the released game

| What | From | Licence |
| --- | --- | --- |
| Start-up code, C++ allocation runtime and `sce_module/libc.prx` | ps5-native-app-boilerplate, Copyright (C) 2026 BlackBearReloaded (and Mihawk-99) | GPL-3.0-or-later |
| Link recipe and graphics import stubs | PS5 Vulkan, Copyright (C) 2026 Mihawk-99 | GPL-3.0-or-later |
| Payload SDK and platform layer | [ps5-payload-dev/sdk](https://github.com/ps5-payload-dev/sdk) and Mihawk-99's fork | GPL-3.0-or-later |
| RADV Vulkan driver | [Mesa](https://mesa3d.org), PS5 fork by Mihawk-99 | MIT and the other licences listed in Mesa |
| libc++, libc++abi, libunwind and compiler built-ins | [LLVM](https://llvm.org) | Apache-2.0 with LLVM exception |

## Fonts

Fraunces, DM Sans and JetBrains Mono, under the SIL Open Font Licence 1.1. See `assets/fonts/LICENSES.txt`.

## Source code

The complete source for a released build is this repository at the matching release tag, plus these exact versions of the
projects it builds with:

| Project | Revision |
| --- | --- |
| [PS5_Vulkan](https://github.com/lavavex/PS5_Vulkan) | `5b5e4fc2d80fdb67a4f61ba9e6a8a424026bb8a5` |
| [PS5_Mesa](https://github.com/lavavex/PS5_Mesa) (the RADV driver) | `7b59ef27c1b09b9671bc4153c41940c3155c3af2` |
| [PS5_PayloadSDK](https://github.com/lavavex/PS5_PayloadSDK) | `b5efad528e0ac1b8289f39d72a0e890ff68ff0a6` (driver built against `b83202be73e930050e00de0f1b4d9ec46c0391bf`) |
| [PS5_LLVM](https://github.com/lavavex/PS5_LLVM) | `be5b58f51c93c67947b7e18cd45c2f9913f2c311` |

`tools/build-title.sh` is the script that builds the release. If any of those links stop working, open an issue here and
I'll sort you out a copy.

## Not affiliated

Lantern Miner is a fan homage to Manic Miner and Jet Set Willy by Matthew Smith. It isn't affiliated with or endorsed by
their authors or rights holders, or by Sony.
