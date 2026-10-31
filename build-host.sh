#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Build the Mac harness: build/wtrender. Same flags idea as X0X's host build: no fused multiply-add.
set -e
cd "$(dirname "$0")"
mkdir -p build
CC="${CC:-cc}"
$CC -O2 -std=gnu99 -Wall -Wextra -ffp-contract=off ${WT_DEFS:-} -o build/wtrender host/wtrender.c core/wt_patch.c core/wt_tables.c core/wt_build.c core/wt_fx.c core/wt_seq.c core/wt_drums.c core/wt_synth.c -lm
echo "built build/wtrender"
