# WT-2 for the M-Vave FM-1

WT-2 is wavetable synth firmware for the M-Vave FM-1 keyboard, made by Dedrich. It replaces the
FM-1's own firmware with a full wavetable synth that has 206 parameters, and it installs and
updates through Felucca's update loader, so you put it on with M-VAVE's own M-UPGRADE app.

Home: [ThatSoundsHot.com](https://thatsoundshot.com)

## What you get

**The synth**

- Two wavetable oscillators with up to 5 unison voices each, wavetable position, table to table morph, warp (bend, sync, mirror, plus ASYM, QUANT and PWM), FM and phase distortion between A and B.
- A sub oscillator, and a noise oscillator with a sample layer.
- A multimode filter plus a VOWEL (formant) filter that sings A E I O U.
- Three envelopes, two LFOs with tempo sync, four macros and an 8 slot modulation matrix with lots of destinations (envelope times, LFO rates, chorus mix, delay and reverb sends, noise and sub level, master).
- An LFO shape editor right on the keyboard: draw 8 shapes of your own with up to 16 points each and a curve on every segment.
- POLY 2 to POLY 7 voice modes, unison on the top note only, and chord headroom so big chords don't clip.

**Effects**

- Synth distortion, CHORUS and HYPER (a stereo widener), a one knob compressor, delay (digital or tape), reverb and a 3 band EQ.

**Play and sequence**

- A step sequencer with 16 patterns of up to 64 steps: accent, slide, long notes, chance, swing, direction, transpose, bar looping and a pattern generator.
- An arpeggiator, scale mode, MIDI and clock sync.
- One button per place: SEL is oscillator A, EDIT is oscillator B, HOME is the filter, sub and noise, then LFO, ENV, ARP, FX, SEQ, GLOBAL and SAVE.
- 11 factory patches (including reece1) plus your own patch slots, and patch transfer over USB.

**Good to know**

- WT-2 has no drum machine. The drums from WT-1 were taken out to make room for the synth.
- There are three wavetables on the device (BASIC SHP, FOLD-SINE, ODD-EVEN) and three noise sample loops.
- Your 8 drawn LFO shapes are shared by every patch, not stored inside a patch, and they aren't sent over USB.

The full manual is in [docs/WT-2-FM1-manual.md](docs/WT-2-FM1-manual.md).

## Install

1. Get the WT-2 package, `wt2-fm1-922.fwsc`, from this repo's Releases page once it's published (or build it yourself, see below).
2. Open M-VAVE's M-UPGRADE app, choose the package and install it, just like a normal FM-1 update.
3. At power on the screen says **WT-2**, and over USB the MIDI port is called WT-2.

M-UPGRADE only installs a package whose identity is different from the one already on the keyboard.
To install a newer WT-2 build over an older one, give it a new number first:

```
python3 tools/rename_pkg.py wt2-fm1-922.fwsc wt2-fm1-923.fwsc "FM-1_923"
```

## Go back to the factory firmware

- Install M-VAVE's own FM-1 firmware with M-UPGRADE the same way. M-VAVE's factory V16 firmware is in [official/](official/) for exactly this.
- If the FM-1 gets stuck, hold **OCT-** and **OCT+** for 5 seconds while it's running to enter the chip's update mode, then install again.
- If it's stuck in a crash loop and no tool can reach it, `tools/fm1_rescue.sh` (Mac) or `tools/fm1_rescue.py` writes M-VAVE's factory V15 package through the chip's update mode. It only accepts the exact V15 file, backs up the whole flash first, and needs `sudo` on the Mac. Run it without `--write` first to check and back up, then with `--write`.

## Compatibility

- Keyboard: M-Vave FM-1.
- Package identity: `FM-1_922`.
- Testing on a real FM-1 is in progress.

## Saved projects

WT-2 can't open projects saved by other firmwares, and saving in WT-2 may overwrite them. Back up anything you want to keep before installing.

## Editor and manual

- Browser editor: https://fm1.thatsoundshot.com (coming soon). It isn't part of this repo.
- Manual: [docs/WT-2-FM1-manual.md](docs/WT-2-FM1-manual.md). The [FM-1 manual](docs/WT-1-FM1-manual.md) from WT-1 covers the shared parts it points to.

## Build it yourself

You need Docker (or OrbStack on a Mac), the JieLi toolchain and three files from the JieLi AC79 SDK (see `tools/get_toolchain.sh` and `build-fw.sh`). Then:

```
VARIANT=wt2 ./build-fw.sh
```

That makes `build/wt2.fwsc` with the identity `FM-1_922`. Host tests run with `sh tests/run_tests.sh`.

## License and credits

WT-2 is free software under the GPL-3.0 (`GPL-3.0-only`, see [LICENSE](LICENSE)). It stands on the work of:

- [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita (@kurogedelic), Hügelton Instruments: the platform code and the update loader.
- X0X by Charles Vestal: the effects chain, the sequencer design, and the input and storage code.
- [9W9](https://github.com/athousanddetails/schwung-9W9) by athousanddetails, after [ER-99](https://github.com/matthewcieplak/er-99) by Matthew Cieplak: the effects.
- The SMK-37 Pro port by M. Geisler.
- Fonts: Barlow Semi Condensed and Terminus (SIL OFL 1.1).

All the details are in [LICENSING.md](LICENSING.md). The factory firmware in `official/` belongs to M-VAVE and isn't covered by the GPL.

## No warranty

WT-2 comes with no warranty of any kind. Installing custom firmware is at your own risk.
