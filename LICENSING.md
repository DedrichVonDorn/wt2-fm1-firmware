# WT-2 licensing

WT-2 is free software under the GNU General Public License, version 3 only
(`GPL-3.0-only`, full text in `LICENSE`). It is a fork of X0X, which is a fork of Felucca, and it
keeps their licence. If you distribute WT-2, or firmware derived from it, you must give your
recipients its complete corresponding source under the same licence.

## Where the code comes from

| What | Origin | Licence |
| --- | --- | --- |
| Platform: `firmware/hal/`, `firmware/loader/` (the update loader), `firmware/src/{libc,lcd,gfx,usb,storage,ota}.c`, `tools/` (build, package, install) | [Felucca](https://github.com/hugelton/Felucca), Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments; changed for X0X and WT-2 | GPL-3.0-only |
| `firmware/src/app/{slots,panel}.c` | adapted from Felucca's `eng_sample.c` and `panel.c` via X0X | GPL-3.0-only |
| Effects chain (reverb, tape and ping-pong delay, distortion, glue compressor, stereo) and its tables and math: `core/wt_fx.*`, `core/fx_tables.h`, `core/fx_dsp.h`, `core/fastmath.h`; the step sequencer's design; the knob and button input code; the flash storage layout | X0X | GPL-3.0-only |
| The effects in X0X and WT-2 | a modified port of **9W9 by A Thousand Details - https://github.com/athousanddetails/schwung-9W9** (Copyright (C) 2026 A Thousand Details), itself after [ER-99](https://github.com/matthewcieplak/er-99) (Matthew Cieplak) | GPL-3.0 with section 7 additional terms (9W9's NOTICE) |
| SMK-37 Pro port code in `firmware/hal/smk_input.h`, `firmware/hal/fm1_flash_map.h`, `firmware/src/lcd.c` | Copyright (C) 2026 M. Geisler (genkigenki/makkuro) | GPL-3.0-only |
| 909 drum engine (`core/drums/drum909*`, `core/drums/dr_tables.*`), not linked into the WT-2 firmware | a modified port, via X0X, of **9W9 by A Thousand Details - https://github.com/athousanddetails/schwung-9W9**, itself after [ER-99](https://github.com/matthewcieplak/er-99) (Matthew Cieplak) | GPL-3.0 |
| 909 hi-hat, ride and crash samples (`core/drums/dr_samples.*`), not linked into the WT-2 firmware | ER-99 via 9W9 | GPL-3.0 |
| 808 drum engine (`core/drums/drum808*`), not linked into the WT-2 firmware | a modified port, via X0X, of **8W8 by A Thousand Details - https://github.com/athousanddetails/schwung-8W8** (Copyright (C) 2026 A Thousand Details); circuit models after the TR-808 service notes and Werner / Abel / Smith; rim shot after sc808 (Yoshinosuke Horiuchi / Sam Aaron) | GPL-3.0 |
| Everything else (the wavetable synth in `core/`, the WT-2 app and screens in `firmware/src/app/`, `host/`, `tests/`, `factory/`) | WT-1 / WT-2 by Dedrich | GPL-3.0-only |

The drum files stay in the tree because the shared synth sources still include their headers
(`core/wt_drums.h` from `core/wt_synth.c`, `core/wt_seq.c` and the app). The WT-2 build
(`VARIANT=wt2`) does not link the drum engines.

## 9W9 and 8W8 additional terms

9W9 and 8W8 are Copyright (C) 2026 A Thousand Details and licensed under GPL-3.0 with additional terms
under section 7 (see each project's NOTICE). As those terms require:

- Attribution: "9W9 by A Thousand Details - https://github.com/athousanddetails/schwung-9W9" and
  "8W8 by A Thousand Details - https://github.com/athousanddetails/schwung-8W8".
- Origin: the code in this tree is a **modified** version (ported to the FM-1 and changed via X0X and
  WT-2). It is not the original, and it is not made or endorsed by A Thousand Details.
- The copyright notices in the source files are kept.

## Felucca assets

Felucca's non-GPL assets (the icon atlas, the panel photo and the Hügelton drum pack)
are **not** part of this fork and are not used by it. Felucca's section 7 additional
permission concerns those assets only, so it is not needed here.

## Third-party material

| What | Licence | Where |
| --- | --- | --- |
| Barlow Semi Condensed (The Barlow Project Authors), the default UI face | SIL OFL 1.1 | `assets/fonts/BarlowSemiCondensed-*.ttf`, `assets/fonts/Barlow-OFL.txt` |
| Terminus (Dimitar Toshkov Zhekov) | SIL OFL 1.1 | `assets/fonts/ter-u*.bdf`, `assets/fonts/Terminus-LICENSE.txt` |
| JieLi AC79 SDK: `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` are read from your SDK checkout at build time and placed in the package; no SDK files are in this tree | Apache-2.0 | <https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK> |
| `official/FM-1.fwsc`: M-VAVE's factory FM-1 firmware V16, unchanged | **Not GPL.** It belongs to M-VAVE. See `official/README.md` | `official/` |

## Trademarks

"Felucca" and "Hügelton Instruments" are names of Hügelton Instruments. "M-VAVE" and
"FM-1" are trademarks of their respective owners. TR-808 and TR-909 are trademarks of
Roland Corporation, used only to describe what the parts emulate. Serum is a trademark of
its owner. WT-2 is independent firmware; it is not affiliated with, endorsed by or
supported by any of them.

