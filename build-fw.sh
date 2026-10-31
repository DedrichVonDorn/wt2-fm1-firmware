#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Build the WT-1 firmware for the M-VAVE FM-1: build/wt1.fwsc (see tools/build.py; needs Docker, the JieLi toolchain
# and the three AC79 SDK files: tools/get_toolchain.sh, and the notes in SPEC.md section 11).
#   ./build-fw.sh [--release X.Y]
set -e
cd "$(dirname "$0")"
export JIELI_TOOLCHAIN="${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}"
export AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"
export PATH="/opt/homebrew/bin:$PATH"
python3 -c 'import PIL' 2>/dev/null || { echo "build-fw.sh: python3 has no Pillow (pip3 install Pillow; brew install libraqm)"; exit 1; }
[ -x "$JIELI_TOOLCHAIN/pi32v2/bin/clang" ] || { echo "build-fw.sh: no toolchain in $JIELI_TOOLCHAIN (run tools/get_toolchain.sh)"; exit 1; }
[ -f "$AC79_SDK/cpu/wl82/tools/uboot.boot" ] || { echo "build-fw.sh: no AC79 SDK files in $AC79_SDK"; exit 1; }
docker info >/dev/null 2>&1 || { echo "build-fw.sh: Docker is not running (open OrbStack)"; exit 1; }
export BOARD="${BOARD:-fm1}"
exec python3 tools/build.py "$@"
