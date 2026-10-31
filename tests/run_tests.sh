#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Build and run the WT-1 core tests, then print the aliasing comparison of the candidate configurations.
set -e
cd "$(dirname "$0")/.."
mkdir -p build
SRC="tests/wt_test.c core/wt_patch.c core/wt_tables.c core/wt_build.c core/wt_fx.c core/wt_seq.c core/wt_drums.c core/wt_synth.c"
cc -O2 -std=gnu99 -Wall -Wextra -ffp-contract=off -o build/wt_test $SRC -lm
./build/wt_test --golden tests/golden.txt "$@"
# the device code path (const generated data, integer table builder) must render exactly like the normal build
./build-host.sh >/dev/null
mkdir -p build/gen
cc -O2 -std=gnu99 -ffp-contract=off -Icore -o build/gen/gen_wt_data tools/gen_wt_data.c core/wt_tables.c core/wt_build.c -lm
./build/gen/gen_wt_data build/gen/wt_data.c build/gen/wt_data.h 2>/dev/null
cc -O2 -std=gnu99 -fno-builtin -ffp-contract=off -DWT_DEVICE=1 -Icore -Ibuild/gen -o build/device_core_test tests/device_core_test.c core/wt_synth.c core/wt_build.c core/wt_fx.c core/wt_seq.c core/wt_drums.c core/wt_patch.c build/gen/wt_data.c 2>/dev/null
for P in pad space; do
  printf "patch $P\nnote 52 90 5\nnote 59 90 5\nnote 64 90 5\nlen 1\n" > build/dev_ref.wts
  ./build/wtrender build/dev_ref.wts build/dev_ref_$P.wav >/dev/null
  echo "scene $P:"; ./build/device_core_test build/dev_ref_$P.wav $P
done
printf "patch pluck\nplay\nlen 1\n" > build/dev_ref.wts          # the sequencer: its bass line through the pluck patch
./build/wtrender build/dev_ref.wts build/dev_ref_seq.wav >/dev/null
echo "scene seq:"; ./build/device_core_test build/dev_ref_seq.wav pluck seq
# the drums: every track of both kits, the drum sequencer's timing, one core = two cores, under the memory and undefined-behaviour checkers
cc -g -O1 -std=gnu99 -fsanitize=address,undefined -fno-sanitize-recover=undefined -ffp-contract=off -Icore -o build/drum_test tests/drum_test.c core/wt_patch.c core/wt_tables.c core/wt_build.c core/wt_fx.c core/wt_seq.c core/wt_drums.c core/wt_synth.c -lm
./build/drum_test
# the drawn LFO shapes (WT2): the breakpoints' tables and the LFO reading them
cc -g -O1 -std=gnu99 -fsanitize=address,undefined -fno-sanitize-recover=undefined -ffp-contract=off -DWT_LFOSHAPES=1 -Icore -o build/lfo_test tests/lfo_test.c core/wt_patch.c core/wt_tables.c core/wt_build.c core/wt_fx.c core/wt_seq.c core/wt_drums.c core/wt_synth.c -lm
./build/lfo_test
# WT-2 chord headroom, the gain rider, unison shedding, and a host CPU/RAM printout
cc -O2 -std=gnu99 -Wall -Wextra -ffp-contract=off -DWT_NODRUMS=1 -Icore -o build/chord_test tests/chord_test.c core/wt_patch.c core/wt_tables.c core/wt_build.c core/wt_fx.c core/wt_seq.c core/wt_synth.c -lm
./build/chord_test
# the device's screens and controls on the Mac, under the memory and undefined-behaviour checkers (needs build/gen/felucca_font.h: run ./build-fw.sh once)
if [ -f build/gen/felucca_font.h ]; then
  mkdir -p build/ui
  cc -g -O1 -std=gnu99 -fsanitize=address,undefined -fno-sanitize-recover=undefined -ffp-contract=off -D'__attribute__(x)=' -Icore -Ifirmware/src -Ifirmware/src/app -Ibuild/gen \
     -o build/ui_host tests/ui_host.c core/wt_patch.c core/wt_tables.c core/wt_build.c core/wt_fx.c core/wt_seq.c core/wt_drums.c core/wt_synth.c -lm
  ./build/ui_host build/ui
  mkdir -p build/ui_p37
  cc -g -O1 -std=gnu99 -fsanitize=address,undefined -fno-sanitize-recover=undefined -ffp-contract=off -DWT_P37=1 -D'__attribute__(x)=' -Icore -Ifirmware/src -Ifirmware/src/app -Ibuild/gen \
     -o build/ui_host_p37 tests/ui_host.c core/wt_patch.c core/wt_tables.c core/wt_build.c core/wt_fx.c core/wt_seq.c core/wt_drums.c core/wt_synth.c -lm
  ./build/ui_host_p37 build/ui_p37
  mkdir -p build/ui_w2
  cc -g -O1 -std=gnu99 -fsanitize=address,undefined -fno-sanitize-recover=undefined -ffp-contract=off -DWT_NODRUMS=1 -D'__attribute__(x)=' -Icore -Ifirmware/src -Ifirmware/src/app -Ibuild/gen \
     -o build/ui_host_w2 tests/ui_host.c core/wt_patch.c core/wt_tables.c core/wt_build.c core/wt_fx.c core/wt_seq.c core/wt_synth.c -lm
  ./build/ui_host_w2 build/ui_w2
fi
./build-host.sh >/dev/null
if [ -d web ] && command -v emcc >/dev/null 2>&1; then ./web/build.sh >/dev/null && node web/test_wasm.mjs && ./web/build-wt2.sh >/dev/null && node web/test_wasm2.mjs; fi
if [ -n "$WT_MATRIX" ]; then
  echo; echo "== configuration matrix (aliased energy vs total, worst case C3..C8; lower is better) =="
  for N in 256 512; do
    cc -O2 -std=gnu99 -ffp-contract=off -DWT_N=$N -o build/wt_test_$N $SRC -lm
    for I in 1 3; do for B in 2 4 8; do for T in 0 60; do
      ./build/wt_test_$N --interp $I --bwdiv $B --taper $T --matrix | sed -n '1p;3p' | tr '\n' ' '; echo
    done; done; done
  done
fi
