# WT-1 for the M-Vave FM-1: User Manual

WT-1 is a replacement firmware for the M-Vave FM-1 keyboard. It turns the FM-1 into a Serum-inspired wavetable synthesizer with a TR-909 / TR-808 drum machine, a step sequencer, an arpeggiator and an effects section. This manual describes the FM-1 build with drums only (the one installed from `out/wt1.fwsc`). The drumless **WT-2** build for the FM-1 (new button layout, drawn LFO shapes, chorus) has its own manual, [WT-2-FM1-manual.md](WT-2-FM1-manual.md), and its own editor, `out/wt2-editor.html`. It was written from the firmware source, so every control, page and range below is what the code does. Anything that could not be confirmed from the source is listed under "Notes to verify" at the end.

Contents

1. Overview and what is different from stock
2. Installing and recovering
3. Front-panel map
4. The screen and page navigation
5. Every page, by group
6. Playing
7. Sound design
8. The step sequencer
9. Drums
10. Saving and loading patches (the SAVE page)
11. Global, MIDI, clock and setup pages
12. The browser editor and USB patch transfer
13. Troubleshooting
14. Appendices (parameter reference, keys, scales, drum map, factory patches)
15. Notes to verify

---

## 1. Overview and what is different from stock

### 1.1 What WT-1 is

- **Two wavetable oscillators (A and B)**, each with up to 5 unison voices, a wavetable position, table-to-table morph, a warp mode (bend, sync, mirror), and FM and phase distortion between the two oscillators.
- **A sub oscillator** (sine, triangle, square, pulse) and a **noise oscillator** (white, pink, brown, geiger, plus a looping sample layer).
- **A multimode filter** (LP12, LP24, HP12, BP12, notch, vowel), **three envelopes** (amp, ENV 2, ENV 3), **two LFOs** (with tempo sync, restart and polarity options), **four macros** and an **8-slot modulation matrix**.
- **Effects**: per-source distortion and sends, master distortion and compressor, tempo-synced delay (digital or tape), reverb, a 3-band EQ, and a bass-drum sidechain duck.
- **8 voices** polyphonic, or mono / legato with glide.
- **A step sequencer** (16 patterns of up to 64 steps, with accent, slide, long notes, chance, swing, direction, transpose, bar looping, random generator) and an **arpeggiator**.
- **A drum machine**: the TR-909 and TR-808 engines (11 tracks each), 8 drum patterns of up to 64 steps, a mixer with mute and solo.
- **Patch storage**: 11 factory patches and user slots in flash, plus saving of the sequencer patterns, the drum patterns and kit sounds, and the MIDI / clock / scale setup.
- **A browser editor** (`out/wt1-editor.html`) that runs the same synth engine in your browser and can send, read and back up patches over USB MIDI SysEx.

The firmware engine is the same code as the Mac and browser builds, so what you hear in the editor is what the device plays.

### 1.2 What is different from the stock FM-1 firmware

WT-1 replaces the stock firmware completely. It is not an add-on. After installing it:

- The FM-1 no longer behaves as M-Vave's stock instrument. All buttons, dials and the screen belong to WT-1 as described in this manual.
- The four dials K1 to K4 always edit the four values shown on the screen. Pages are reached with the page buttons, with SELECT, or with the hold-a-button-and-press-a-key shortcut.
- The keys play the synth, except on the STEPS and DRUMS pages where they become step buttons and pads (section 8 and 9).
- Key velocity: the FM-1 keys have no velocity. They always play at a fixed velocity of 100. Velocity from USB MIDI (and from the sequencer) is used.
- The FM-1 has no pitch-bend or mod wheel. Bend and mod wheel (CC 1) only arrive over MIDI.
- Stock firmware can be put back (section 2.5).

### 1.3 Limits worth knowing

- Three factory wavetables only on the device: BASIC SHP, FOLD-SINE, ODD-EVEN. The editor can import your own wavetables and samples for auditioning in the browser, but the device protocol transfers only patch settings, not wavetables or samples (section 12).
- Three built-in noise sample loops: CRACKLE, AIR, RUMBLE.
- The CPU is finite. Many unison voices on both oscillators plus drums and effects can run it out. A CPU governor is built in (section 11.7).

---

## 2. Installing and recovering

### 2.1 Files

| File | What it is |
|---|---|
| `out/wt1.fwsc` | The installable firmware package for the FM-1 (package identity `FM-1_900`). |
| `out/wt1-editor.html` | The browser editor, one self-contained file. |
| `open-editor.command` | Double-click on a Mac: serves `out/` on `http://127.0.0.1:8787/` and opens the editor. |
| `tools/fm1_install.py` | Command-line installer over USB MIDI (needs Python with `mido` and `python-rtmidi`). |
| `tools/rename_pkg.py` | Gives a `.fwsc` package another identity string (model name and version). |
| `tools/fm1_rescue.py`, `tools/fm1_rescue.sh` | Puts M-Vave's stock FM-1 V15 firmware back on a unit stuck in a crash loop. |

The editor and the firmware must be built from the same source: the editor refuses to connect if its parameter count differs from the firmware's (section 12.6). The files in `out/` were built together.

### 2.2 Installing with M-UPGRADE

1. Connect the FM-1 to the computer with a USB-C data cable and switch it on.
2. Open M-VAVE's M-UPGRADE updater.
3. Choose `out/wt1.fwsc` as the firmware file and start the update.
4. Do not unplug or switch off the FM-1 while it writes. When it finishes the FM-1 restarts and shows "BUILDING WAVETABLES..." for a moment, then the first page.

M-UPGRADE reads the package identity and compares it with the keyboard's. If M-UPGRADE refuses `wt1.fwsc` because of its identity (`FM-1_900`), use `tools/rename_pkg.py` to give the package an identity the updater accepts:

```
python3 tools/rename_pkg.py IN.fwsc OUT.fwsc "IDENTITY-STRING"
```

The tool changes only the identity marker bytes (outside every checksum) and verifies the package afterwards. See the notes at the end about which identity to use.

### 2.3 Installing from the command line

```
python3 tools/fm1_install.py out/wt1.fwsc
python3 tools/fm1_install.py --info          # shows what is connected
```

Options: `--port NAME` (part of the MIDI port name), `--yes` (do not ask), `--force` (install a package without the update-loader marker). The installer asks `Install? [y/N]`, writes the package in two steps (the running firmware starts the update loader; the loader reads and writes the package), restarts the FM-1 and checks the installed identity. If an earlier install was cut off and the FM-1 is still in update mode, running the install again finishes the write. Exit codes: 0 done, 1 cancelled, 2 bad arguments or package, 3 FM-1 not found, 4 connection lost, 5 timeout, 6 wrong model or identity mismatch afterwards.

### 2.4 Power-on and update-mode shortcuts

| Action | What happens |
|---|---|
| Hold **OCT-** and **OCT+** while switching on | HARDWARE CALIBRATION: the screen asks you to press each button and turn each knob in turn, so the firmware learns which physical control carries which label. It stores the result in flash. If you do nothing for 30 s it gives up and keeps the old table. You normally never need this. |
| Hold **OCT-** and **OCT+** for 2 s while running | A countdown "UPDATE MODE IN n" appears. Keep holding for 5 s in total to enter the chip's update mode (the screen is for the update tool). |
| Two failed boots in a row | The firmware starts in safe mode. |
| Four failed boots in a row | The chip's own update mode (UBOOT) is entered. Install a firmware from the computer. |
| After a crash | The first screen shows "RESTARTED AFTER A CRASH" and the crash address for a moment. |

### 2.5 Going back to stock

Install M-VAVE's own FM-1 firmware with M-UPGRADE the same way. If the FM-1 is stuck in a crash loop and no tool can reach it, `tools/fm1_rescue.sh` (Mac) or `tools/fm1_rescue.py` writes the stock FM-1 V15 package through the chip's update mode. It accepts only the exact V15 file (SHA-256 `db1642b2...7edb8a`), reads and saves the whole flash first, and writes only the 4 KiB sectors between 0x4000 and 0x93000 that differ. It needs `sudo` on the Mac. Run it with the package and no `--write` first to check and back up, then with `--write`.

---

## 3. Front-panel map

![panel](panel.png)

The screen is 240 x 240 pixels. Each of the 14 buttons carries a printed label, and the knobs are the FM-1's own.

### 3.1 Encoders (knobs)

| Control | What it does in WT-1 |
|---|---|
| **K1, K2, K3, K4** | Edit the four values shown on the current page (left to right, top to bottom). On STEPS with a step held they become pitch, velocity, gate and length. |
| **SELECT** | Steps through every page in order (72 pages). Clockwise = next. |
| **ALGORITHM** | Loads the next / previous patch (factory first, then saved user patches). With **SEL** held (or latched) it chooses the modulation source instead. |
| **PRESETS** | Changes the wavetable of the oscillator whose page you are on (OSC B pages change B; every other page changes A). A box names the table for a moment. |
| **MASTER** | The output level (an analog-style volume curve). Not the same as the MASTER parameter on the LEVELS page (section 11.7). |

### 3.2 Buttons

Page buttons: press once for the button's first page, press again for its next page, and so on around. The button of the page you are on is lit.

| Button | Pages (press again for the next) | Also |
|---|---|---|
| **HOME** | PERFORM, MIXER, DRUMS, DRUM KIT, DRUM ACCENT, DRUM LENGTH, DRUM REMOVE BARS, DRUM PATTERN, DRUM GENERATE, SOUND 1, SOUND 2, SOUND 3, SOUND 4 | |
| **EDIT** | OSC A, A WARP, A TABLE, A TUNE, FILTER, FILTER 2, OSC B, B WARP, B TABLE, B TUNE, SUB, SUB 2, NOISE, NOISE 2 | |
| **ENV** | AMP ENV, AMP ENV 2, ENV 2, ENV 3, ENV 2 GOES TO, ENV 3 GOES TO | On STEPS with a step held: toggles accent on that step. |
| **LFO** | LFO 1, LFO 1 GOES TO, LFO 2, LFO 2 GOES TO, MOD 1 to MOD 8 | On STEPS with a step held: toggles slide on that step. |
| **FX** | MASTER FX, SYNTH FX, SIDECHAIN, DRUM FX, DELAY, DELAY 2, TAPE, REVERB, REVERB 2, EQ, EQ 2 | |
| **SEQ** | STEPS, PATTERN, GENERATE, PATTERN LENGTH, REMOVE BARS, PATTERN 2, PLAY SETUP | |
| **ARP** | ARP, ARP 2 | Lit while the arpeggiator is on. On STEPS with a step held: with **ARP** held, K3 sets the step's chance. |
| **GLOBAL (GLO)** | VOICE, SCALE, MIDI, SYNC, PERFORMANCE, LEVELS | |
| **SAVE** | SAVE (page). On the SAVE page, press SAVE again to arm and again to store. | On STEPS / DRUMS with a step held: clears that step. |
| **SEL** | Modulation assign mode (section 7.9). Not a page button. | |
| **PLAY** | Starts / stops the synth sequencer and the drums together. Double-tap = panic. | Lit while anything runs. |
| **REC** | Turns REC on / off. | Lit while REC is on. |
| **OCT -, OCT +** | Octave down / up (limit 3 octaves each way). On STEPS and DRUMS they page the step grid instead. | With a step held: shift that step's note down / up one octave. |

Press-and-hold shortcut: hold a page button and the first white keys light. Each lit key jumps straight to a page of that button (section 4.3).

### 3.3 Keys

The FM-1 has 27 keys, F3 to G5 (MIDI notes 53 to 79 at octave 0). Their role depends on the page:

| Page | White keys (16) | Black keys (11) |
|---|---|---|
| Any synth page | Play notes | Play notes |
| STEPS | The 16 steps of the shown bar | Select pattern 1 to 11 |
| DRUMS, and the other drum pages (DRUM ..., SOUND ..., MIXER) | On DRUMS: the 16 steps of the shown bar. On the other drum pages: play notes (nothing else) | The 11 drum pads (select and hit a track) |
| Any page, with a page button held | Shortcut keys 1 to 7 (HOME: 1 to 9) | (play notes) |

---

## 4. The screen and page navigation

### 4.1 What is on the screen

Top line: the page title (large), the loaded patch name (small, grey, right), the page number out of 72 (small, dim), the scale and root if scale mode is on (amber), `CPU` with the percentage, and `LIMIT` (the CPU governor has reduced unison) or `SAVE` (CPU MODE is SAVE) in amber.

Dial pages show four big dials in a 2 x 2 grid. A dial shows:

- An **amber arc** from the minimum to the value. For a value that can go negative the arc starts at 12 o'clock.
- Inside the dial, the value as text (a percent, a number, or a name such as `LP24`).
- Under the dial, its label.
- A **blue ring** outside the amber arc, if a modulation source is assigned: it shows how far modulation can move the value. A bright blue dot follows the live modulated value.

Oscillator pages (OSC A, A WARP, A TABLE, A TUNE, OSC B, B WARP, B TABLE, B TUNE) show that oscillator's own waveform at its current wavetable position (it moves with modulation) with four small dials below. The table name is shown, and for BASIC SHP the name of the shape at the current position (TRIANGLE, SAW, SQUARE, SINE, PULSE 37, PULSE 25, PULSE 17, PULSE 10, PULSE 5). OSC B shows `OFF` in red when the oscillator is off.

Special pages (STEPS, DRUMS, MIXER, SOUND 1 to 4, PERFORMANCE, SAVE, SYNC) are described in their sections.

Messages that appear over the screen for a moment: the table name when you turn PRESETS, the result of double / halve / remove-bars, "PANIC / ALL STOPPED".

### 4.2 Moving between pages

- Press a page button (first page; again for the next, wrapping around).
- Turn SELECT to step through all 72 pages.
- Hold a page button and press a white key (below).

### 4.3 Hold a page button and press a white key

Hold a page button and a list of its shortcut pages appears after a moment, with the first white keys lit. Press a lit white key to jump. Let go of the button and the keys play again. (Pressing the button first changes the page as usual; the shortcut then jumps from there.)

| Hold | Key 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 |
|---|---|---|---|---|---|---|---|---|---|
| **EDIT** | OSC A | OSC B | FILTER | SUB | NOISE | | | | |
| **FX** | MASTER FX | SYNTH FX | DRUM FX | SIDECHAIN | DELAY | REVERB | EQ | | |
| **ENV** | AMP ENV | ENV 2 | ENV 3 | ENV 2 GOES TO | | | | | |
| **LFO** | LFO 1 | LFO 1 GOES TO | LFO 2 | LFO 2 GOES TO | MOD 1 | | | | |
| **HOME** | PERFORM | MIXER | DRUMS | DRUM KIT | SOUND 1 | DRUM GENERATE | DRUM ACCENT | DRUM LENGTH | DRUM REMOVE BARS |
| **SEQ** | STEPS | PATTERN | PATTERN LENGTH | REMOVE BARS | GENERATE | PLAY SETUP | | | |
| **GLOBAL** | VOICE | LEVELS | SCALE | MIDI | SYNC | PERFORMANCE | | | |

Key 1 is the leftmost white key (F3), key 2 the next white key (G3), and so on. ARP and SAVE have no shortcut list.

### 4.4 Complete page list

| # | Page | Button | | # | Page | Button |
|---|---|---|---|---|---|---|
| 1 | PERFORM | HOME | | 37 | LFO 2 GOES TO | LFO |
| 2 | MIXER | HOME | | 38 | MOD 1 | LFO |
| 3 | DRUMS | HOME | | 39 | MOD 2 | LFO |
| 4 | DRUM KIT | HOME | | 40 | MOD 3 | LFO |
| 5 | DRUM ACCENT | HOME | | 41 | MOD 4 | LFO |
| 6 | DRUM LENGTH | HOME | | 42 | MOD 5 | LFO |
| 7 | DRUM REMOVE BARS | HOME | | 43 | MOD 6 | LFO |
| 8 | DRUM PATTERN | HOME | | 44 | MOD 7 | LFO |
| 9 | DRUM GENERATE | HOME | | 45 | MOD 8 | LFO |
| 10 | SOUND 1 | HOME | | 46 | STEPS | SEQ |
| 11 | SOUND 2 | HOME | | 47 | PATTERN | SEQ |
| 12 | SOUND 3 | HOME | | 48 | GENERATE | SEQ |
| 13 | SOUND 4 | HOME | | 49 | PATTERN LENGTH | SEQ |
| 14 | OSC A | EDIT | | 50 | REMOVE BARS | SEQ |
| 15 | A WARP | EDIT | | 51 | PATTERN 2 | SEQ |
| 16 | A TABLE | EDIT | | 52 | PLAY SETUP | SEQ |
| 17 | A TUNE | EDIT | | 53 | SAVE | SAVE |
| 18 | FILTER | EDIT | | 54 | MASTER FX | FX |
| 19 | FILTER 2 | EDIT | | 55 | SYNTH FX | FX |
| 20 | OSC B | EDIT | | 56 | SIDECHAIN | FX |
| 21 | B WARP | EDIT | | 57 | DRUM FX | FX |
| 22 | B TABLE | EDIT | | 58 | DELAY | FX |
| 23 | B TUNE | EDIT | | 59 | DELAY 2 | FX |
| 24 | SUB | EDIT | | 60 | TAPE | FX |
| 25 | SUB 2 | EDIT | | 61 | REVERB | FX |
| 26 | NOISE | EDIT | | 62 | REVERB 2 | FX |
| 27 | NOISE 2 | EDIT | | 63 | EQ | FX |
| 28 | AMP ENV | ENV | | 64 | EQ 2 | FX |
| 29 | AMP ENV 2 | ENV | | 65 | ARP | ARP |
| 30 | ENV 2 | ENV | | 66 | ARP 2 | ARP |
| 31 | ENV 3 | ENV | | 67 | VOICE | GLOBAL |
| 32 | ENV 2 GOES TO | ENV | | 68 | SCALE | GLOBAL |
| 33 | ENV 3 GOES TO | ENV | | 69 | MIDI | GLOBAL |
| 34 | LFO 1 | LFO | | 70 | SYNC | GLOBAL |
| 35 | LFO 1 GOES TO | LFO | | 71 | PERFORMANCE | GLOBAL |
| 36 | LFO 2 | LFO | | 72 | LEVELS | GLOBAL |

SEL has no pages (it is the modulation assign mode).

---

## 5. Every page, by group

Values are written as the screen shows them. "Percent" values are stored as 0 to 1000 and shown as 0 to 100%; where a range is stated in the stored unit it is given in brackets.

How a dial turns: a slow turn changes the value by one step per click; a fast turn (more than one click at a time) moves twice as fast. Wide-range values (a span of 200 or more) move by 1% of the span per click.

### 5.1 HOME group (performance and drums)

**PERFORM** (1): four free macros.

| Knob | Control | Range | Meaning |
|---|---|---|---|
| K1 | MACRO 1 | 0 to 100% | A free knob. Use it as a modulation source (MAC1) in the matrix, or as a performance control. |
| K2 | MACRO 2 | 0 to 100% | Source MAC2. |
| K3 | MACRO 3 | 0 to 100% | Source MAC3. |
| K4 | MACRO 4 | 0 to 100% | Source MAC4. |

**MIXER** (2): 13 faders: the synth (SY), the drums as a whole (DR), and the 11 drum tracks (track names depend on the kit). K1 = channel, K2 = level, K3 = pan (drum tracks only), K4 = normal / MUTE / SOLO. See section 9.8.

**DRUMS** (3): the drum step grid. See section 9.

**DRUM KIT** (4)

| Knob | Control | Range | Meaning |
|---|---|---|---|
| K1 | KIT | 909 / 808 | The drum engine. |
| K2 | LEVEL | 0 to 100% | Drum machine output level. |
| K3 | WITH PLAY | OFF / ON | ON: PLAY also starts and stops the drum sequencer. OFF: PLAY only runs the synth sequencer (the drum sequencer then plays only when started another way, for example from the external clock). |
| K4 | PATTERN | 1 to 8 | The drum pattern edited and played. While playing, the change happens at the end of the playing pattern. |

**DRUM ACCENT** (5)

| Knob | Control | Range | Meaning |
|---|---|---|---|
| K1 | LOUD ACCENT | -127..+127, then FIX 0..127 | The level of a loud-accented drum step: a relative amount added to the step's velocity (+50 / -50), or past +127 a fixed velocity (FIX n). Default FIX 127 (full level). |
| K2 | SOFT ACCENT | same | The same for a soft-accented step (default +25). The FM-1's front panel can only set the loud accent on a step (hold the step and press ENV); soft accents come from the SMK-37 build or from patterns it saved. |

**DRUM LENGTH** (6) and **DRUM REMOVE BARS** (7): see section 8.7 (they work the same as for the synth patterns, on the current drum pattern).

**DRUM PATTERN** (8)

| Knob | Control | Range | Meaning |
|---|---|---|---|
| K1 | LENGTH | 1 to 64 steps | Length of the current drum pattern. |
| K2 | RATE | 1/16, 1/16T, 1/32, 1/8T | Length of a step. |
| K3 | SWING | 50% to 75% | 50% is straight; higher delays every second step. |
| K4 | CLEAR | TURN > / DONE | Turn clockwise to empty the current drum pattern (all tracks, all steps, loop setting; length returns to 16, rate 1/16, swing 50%). |

**DRUM GENERATE** (9)

| Knob | Control | Range | Meaning |
|---|---|---|---|
| K1 | STYLE | FOUR FLOOR / BREAK / RANDOM | The backbone of the new pattern. |
| K2 | EXTRAS | 0 to 100% (default 40) | How many extra hits (ghost kicks and snares, toms, rim or clap, and hat variation) are added. |
| K3 | ROLL | TURN > / ROLLED | Turn clockwise: replaces the current drum pattern with a new random one. Each roll is different. There is no undo. |

**SOUND 1, 2, 3, 4** (10 to 13): every pot of the selected drum track. K1 chooses the track (BD to the last), K2 to K4 are three of its pots per page: SOUND 1 shows pots 1 to 3, SOUND 2 pots 4 to 6, SOUND 3 pots 7 to 9, SOUND 4 pots 10 to 12 (a track has at most 10). The pots are the 909 / 808 engine's own, see section 9.5. A dial turn moves 2 units on a 0 to 127 pot, 1 unit on a switch.

### 5.2 EDIT group (oscillators, filter, sub, noise)

**OSC A** (14)

| Knob | Control | Range | Meaning |
|---|---|---|---|
| K1 | WT POS | 0 to 100% | Position in the wavetable. It sweeps through the table's frames, blending smoothly between neighbours. It snaps to each stored frame as the knob passes it (so a clean saw or square is easy to land on); turn faster to pass through. |
| K2 | DETUNE | 0 to 100% | Spread of the unison voices. Squared curve: fine at the bottom, widest at the top (the outer voices reach about one semitone away at 100%). |
| K3 | UNISON | 1 to 5 | Voices per note in this oscillator. More unison = more CPU. |
| K4 | LEVEL | 0 to 100% | Output level. It can be 0 and still modulate the other oscillator with FM or PD. |

**A WARP** (15)

| Knob | Control | Range | Meaning |
|---|---|---|---|
| K1 | WARP | OFF, BEND, SYNC, MIRROR | Warp mode. |
| K2 | AMOUNT | -100% to +100% | How far the warp is pushed. BEND and MIRROR use the whole range; SYNC uses the positive half (0 = master pitch ratio 1, 100% = 8). |
| K3 | FM FROM B | 0 to 100% | Frequency modulation of oscillator A by oscillator B (linear, through-zero). |
| K4 | PD FROM B | 0 to 100% | Phase distortion of oscillator A driven by oscillator B. |

**A TABLE** (16)

| Knob | Control | Range | Meaning |
|---|---|---|---|
| K1 | TABLE | BASIC SHP, FOLD-SINE, ODD-EVEN | The wavetable A reads. |
| K2 | MORPH TO | the same three | A second wavetable to cross-fade towards (read at the same WT POS). |
| K3 | MORPH | 0 to 100% | How far the cross-fade towards MORPH TO has gone. 0 = only TABLE. |
| K4 | PHASE | 0 to 360 degrees | Start phase of oscillator A on every note. |

**A TUNE** (17)

| Knob | Control | Range | Meaning |
|---|---|---|---|
| K1 | OCTAVE | -3 to +3 | Octave shift. |
| K2 | SEMITONE | -12 to +12 | Semitone shift. |
| K3 | FINE | -100 to +100 cents | Fine tune. |
| K4 | PAN | -100% to +100% | Stereo position of the oscillator. |

**FILTER** (18)

| Knob | Control | Range | Meaning |
|---|---|---|---|
| K1 | TYPE | OFF, LP12, LP24, HP12, BP12, NOTCH, VOWEL | Filter type. The default patch has it OFF. VOWEL turns CUTOFF into a vowel position (section 7.7). |
| K2 | CUTOFF | 0 to 135 (shown with a % sign; 100% is the default) | Cutoff frequency (details below). |
| K3 | RESONANCE | 0 to 100% | Resonance. |
| K4 | DRIVE | 0 to 100% | Drive into the filter. |

CUTOFF is stored 0 to 1350 with 120 units per octave (10 per semitone) (the screen shows the value divided by 10 with a % sign, so 100% is 1000 and the top is 135%). 1000 is about 2.6 kHz, 1350 about 19.6 kHz (the editor's Hz readout). The filter is a zero-delay-feedback state-variable filter. There is one filter; a source can bypass it (per-source filter switches exist in the editor, section 12 and Appendix A).

**FILTER 2** (19)

| Knob | Control | Range | Meaning |
|---|---|---|---|
| K1 | KEY TRACK | 0 to 100% | How much the cutoff follows the key played (100% = one semitone of cutoff per semitone of pitch). |
| K2 | MIX | 0 to 100% | Dry / filtered mix. |

**OSC B** (20)

| Knob | Control | Range | Meaning |
|---|---|---|---|
| K1 | OSC B | OFF / ON | Oscillator B on or off (default off). |
| K2 | WT POS | 0 to 100% | As OSC A. |
| K3 | UNISON | 1 to 5 | As OSC A. |
| K4 | LEVEL | 0 to 100% | As OSC A. |

**B WARP** (21): as A WARP with FM FROM A and PD FROM A.

**B TABLE** (22): K1 TABLE, K2 MORPH TO, K3 MORPH as A TABLE; K4 **DETUNE** (B's unison spread). There is no PHASE knob on B.

**B TUNE** (23): as A TUNE.

**SUB** (24)

| Knob | Control | Range | Meaning |
|---|---|---|---|
| K1 | ON | OFF / ON | Sub oscillator on or off (default off). |
| K2 | WAVE | SINE, TRIANGLE, SQUARE, PULSE | Wave. |
| K3 | PULSE W | 0 to 100% | Pulse width for PULSE (0% = 50% duty, 100% = 4% duty). It can be modulated. |
| K4 | LEVEL | 0 to 100% | Level. |

**SUB 2** (25)

| Knob | Control | Range | Meaning |
|---|---|---|---|
| K1 | OCTAVE | -3 to +3 | Octave (default -1). |
| K2 | COARSE | -1200 to +1200 | Tuning in cents (shown as a plain number). |
| K3 | PAN | -100% to +100% | Stereo position. |

**NOISE** (26)

| Knob | Control | Range | Meaning |
|---|---|---|---|
| K1 | ON | OFF / ON | Noise section on or off (default off). |
| K2 | COLOUR | WHITE, PINK, BROWN, GEIGER, OFF | Noise colour. OFF = no generated noise (only the sample layer). |
| K3 | LEVEL | 0 to 100% | Level (default 20%). |
| K4 | STEREO | 0 to 100% | Stereo width (0 = mono, 100% = uncorrelated left and right). |

**NOISE 2** (27)

| Knob | Control | Range | Meaning |
|---|---|---|---|
| K1 | FILTER | -100% to +100% | Tilt: negative = low-pass, positive = high-pass, 0 = off. |
| K2 | PAN | -100% to +100% | Stereo position. |
| K3 | SAMPLE | NONE, CRACKLE, AIR, RUMBLE, (EMPTY) | The looping sample layered with the noise. (EMPTY) is a fourth slot with nothing in it. |
| K4 | SMP LEVEL | 0 to 100% | Level of the sample layer. |

### 5.3 ENV group

**AMP ENV** (28) and **ENV 2** (30), **ENV 3** (31)

| Knob | Control | Range | Meaning |
|---|---|---|---|
| K1 | ATTACK | 0 to 100% | Attack time. |
| K2 | DECAY | 0 to 100% | Decay time. |
| K3 | SUSTAIN | 0 to 100% | Sustain level. |
| K4 | RELEASE | 0 to 100% | Release time. |

Times are shown as percent of the control; the control maps to about 0.5 ms (0) up to about 10 s (100%) on an exponential curve (the editor shows real times). Defaults: AMP ENV decay 50%, sustain 100%, release 30%; ENV 2 and ENV 3 decay 50%, sustain 0, release 30%.

**AMP ENV 2** (29): K1 DELAY (time before the amp envelope starts), K2 HOLD (time held at full level before decaying). Only the amp envelope has these on the device screens. (ENV 2 and ENV 3 have delay and hold too, in the editor.)

**ENV 2 GOES TO** (32), **ENV 3 GOES TO** (33)

| Knob | Control | Range | Meaning |
|---|---|---|---|
| K1 | GOES TO | NONE or one of 20 destinations | Where the envelope sends its modulation. |
| K2 | AMOUNT | -100% to +100% | How much. Negative inverts it. |

The envelope uses the first matrix slot that has it as source (or the first free slot). Choosing a destination when the amount is 0 sets the amount to +50%. If all 8 matrix slots are in use, the knob does nothing. The destination list is in section 7.9. ENV 2 is source ENV2, ENV 3 is source ENV3.

### 5.4 LFO group

**LFO 1** (34) and **LFO 2** (36)

| Knob | Control | Range | Meaning |
|---|---|---|---|
| K1 | SHAPE | SINE, TRIANGLE, SAW UP, SQUARE, S&H, SMOOTH, SAW DOWN | LFO shape (S&H = sample and hold; SMOOTH = smoothed random). |
| K2 | RATE | 0 to 100% | Free rate, about 0.05 Hz to 40 Hz on an exponential curve. Ignored while SYNC is not FREE. |
| K3 | SYNC TO TEMPO | FREE, 4 BARS, 2 BARS, 1 BAR, 1/2., 1/2, 1/4., 1/4, 1/4T, 1/8., 1/8, 1/8T, 1/16., 1/16, 1/16T, 1/32 | FREE uses RATE; any other value runs the LFO in note values of the tempo (a dot means dotted, T triplet). It follows the PLAY SETUP tempo or the external clock, and its cycle restarts when the sequencer (or the drums) starts. |
| K4 | RESTART | FREE, EVERY NOTE, FIRST NOTE | FREE: free-running. EVERY NOTE: the LFO cycle restarts at each note. FIRST NOTE: restarts only when no note was sounding (the start of a phrase). |

**LFO 1 GOES TO** (35) and **LFO 2 GOES TO** (37)

| Knob | Control | Range | Meaning |
|---|---|---|---|
| K1 | GOES TO | NONE or one of 20 destinations | Where the LFO goes. First after NONE is A_WTPOS. |
| K2 | AMOUNT | -100% to +100% | How much. |
| K3 | POLARITY | UP + DOWN, UP ONLY, DOWN ONLY | UP + DOWN: the LFO swings both sides of zero. UP ONLY: from 0 up to the top. DOWN ONLY: from the bottom up to 0. Each at full strength. |

**MOD 1 to MOD 8** (38 to 45): the 8 modulation matrix slots, each with K1 SOURCE, K2 DEST, K3 AMOUNT (-100% to +100%). See section 7.9. The LFO and ENV "GOES TO" pages use these same slots.

### 5.5 SEQ group

**STEPS** (46): the 16-step grid (section 8).

**PATTERN** (47)

| Knob | Control | Range | Meaning |
|---|---|---|---|
| K1 | LENGTH | 1 to 64 steps | Pattern length. |
| K2 | RATE | 1/16, 1/16T, 1/32, 1/8T | Length of a step. |
| K3 | SWING | 50% to 75% | Delay of every second step. |
| K4 | DIRECTION | FORWARD, REVERSE, PING-PONG, RANDOM | Playback direction. |

**GENERATE** (48)

| Knob | Control | Range | Meaning |
|---|---|---|---|
| K1 | DENSITY | 0 to 100% (default 60) | Share of steps that get a note. |
| K2 | RANGE | 1 to 3 octaves (default 2) | Octaves the notes cover, upward from the scale's root near C2. |
| K3 | SPICE | 0 to 100% (default 30) | Accents, slides, long notes and (above 50%) chance. |
| K4 | ROLL | TURN > / ROLLED | Turn clockwise: a new random pattern in the current scale (minor pentatonic when scale mode is off) replaces the current pattern's notes. |

**PATTERN LENGTH** (49) and **REMOVE BARS** (50): section 8.7.

**PATTERN 2** (51)

| Knob | Control | Range | Meaning |
|---|---|---|---|
| K1 | PATTERN | 1 to 16 | The synth pattern edited and played. While playing, it changes when the playing pattern wraps. |
| K2 | TRANSPOSE | -24 to +24 semitones | Transposes the pattern (the note names on the step grid include it). |
| K3 | CLEAR | TURN > / DONE | Turn clockwise to empty the current pattern. |

**PLAY SETUP** (52)

| Knob | Control | Range | Meaning |
|---|---|---|---|
| K1 | TEMPO | 40 to 240 BPM (default 120) | Tempo for the sequencer, drums, arpeggiator and delay. The same value is on DELAY K4. |
| K2 | ACCENT | 0 to 100% (default 60) | How much an accented synth step lifts its velocity towards 127. |
| K3 | SLIDE TIME | 0 to 100% (default 30) | Glide time of a slide step. |

### 5.6 SAVE

**SAVE** (53): see section 10.

### 5.7 FX group

**MASTER FX** (54)

| Knob | Control | Range | Meaning |
|---|---|---|---|
| K1 | DISTORT | OFF, DIODE, CLIP, SAT, BFZ, PDIST, FOLD, CRUSH | Distortion on the whole mix. |
| K2 | DRIVE | 0 to 100% | Drive of that distortion. |
| K3 | COMPRESS | 0 to 100% | One-knob glue compressor on the whole mix. 0 = bypassed. |
| K4 | OUTPUT | 0 to 100% | Effects output level (default 100%). |

**SYNTH FX** (55): the synth's own distortion and sends.

| Knob | Control | Range | Meaning |
|---|---|---|---|
| K1 | DISTORT | OFF, DIODE, CLIP, SAT, BFZ, PDIST, FOLD, CRUSH | Distortion on the synth only. |
| K2 | DRIVE | 0 to 100% | Its drive. |
| K3 | DELAY SEND | 0 to 100% | How much of the synth goes to the delay. |
| K4 | REVERB SEND | 0 to 100% | How much goes to the reverb. |

**SIDECHAIN** (56)

| Knob | Control | Range | Meaning |
|---|---|---|---|
| K1 | DUCK | 0 to 100% (default 0 = off) | How far a bass drum pushes the synth down. The synth's reverb and delay sends duck with it. Any bass drum hit triggers it: the drum sequencer, the BD pad, or a muted bass drum. |
| K2 | RECOVER | 0 to 100% (default 45) | How long the synth takes to come back. |

**DRUM FX** (57): as SYNTH FX for the drums (DISTORT, DRIVE, DELAY SEND, REVERB SEND). The drums' sends scale the per-sound REV and DLY pots of each drum track (default 100%).

**DELAY** (58)

| Knob | Control | Range | Meaning |
|---|---|---|---|
| K1 | SYNTH SEND | 0 to 100% | The synth's send to the delay (same as SYNTH FX K3). |
| K2 | TIME | 1/32, 1/16T, 1/16, 1/8T, 1/16., 1/8, 1/4T, 1/8., 1/4, 1/2T, 1/4., 1/2, 1/2. | Delay time as a note value of the tempo (default 1/8.). |
| K3 | FEEDBACK | 0 to 100% | Number of repeats. |
| K4 | TEMPO | 40 to 240 BPM | The same tempo as PLAY SETUP. |

**DELAY 2** (59): K1 TONE, K2 RETURN (level back into the mix), K3 HI-PASS, K4 TYPE (DIGI or TAPE).

**TAPE** (60): K1 WEAR (tape wobble, flutter and saturation; only audible with TYPE = TAPE), K2 PING-PONG (OFF / ON: the delay bounces between left and right).

**REVERB** (61): K1 SYNTH SEND (same as SYNTH FX K4), K2 DECAY, K3 TONE, K4 RETURN. **REVERB 2** (62): K1 HI-PASS.

**EQ** (63): a 3-band EQ on the master.

| Knob | Control | Range | Meaning |
|---|---|---|---|
| K1 | LOW | -12.0 to +12.0 dB | Low shelf gain. |
| K2 | MID | -12.0 to +12.0 dB | Peak gain. |
| K3 | HIGH | -12.0 to +12.0 dB | High shelf gain. |
| K4 | EQ | OFF / ON | On by default. The EQ is also bypassed while all three gains are 0. |

**EQ 2** (64): K1 LOW FREQ (40 Hz to 400 Hz, default 120), K2 MID FREQ (200 Hz to 8 kHz, default 1 kHz), K3 MID Q (0.3 to 4.0, default 1.0; higher = narrower), K4 HIGH FREQ (2 kHz to 12 kHz, default 6 kHz).

Global or per source: compress globally on MASTER FX, but send, distort or leave dry per source (SYNTH FX, DRUM FX). For one global delay or reverb, set the same send on both sources.

### 5.8 ARP

**ARP** (65)

| Knob | Control | Range | Meaning |
|---|---|---|---|
| K1 | ARP | OFF / ON | Arpeggiator on or off. The ARP button lights while it is on. |
| K2 | MODE | UP, DOWN, UP-DOWN, DOWN-UP, RANDOM, PLAYED, CHORD | The order. PLAYED is the order you pressed the keys; the other modes sort by pitch. CHORD plays the held notes together at each step. |
| K3 | RATE | 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32 | Step rate of the tempo (default 1/16). |
| K4 | OCTAVES | 1 to 4 | How many octaves the arpeggio climbs. |

**ARP 2** (66): K1 GATE (5 to 100% of the step, default 60), K2 SWING (50 to 75%), K3 LATCH (OFF / ON: keep playing after you let go; the next chord replaces it).

### 5.9 GLOBAL group

**VOICE** (67)

| Knob | Control | Range | Meaning |
|---|---|---|---|
| K1 | MODE | POLY, MONO, LEGATO, POLY 2 to POLY 7 | Voice mode (section 6.2). |
| K2 | GLIDE | 0 to 100% | Portamento time (see section 6.3). |
| K3 | BEND RANGE | 0 to 24 semitones (default 2) | Pitch-bend range (bend comes over MIDI). |
| K4 | TUNE | -100 to +100 cents | Master tune. |

**SCALE** (68): K1 SCALE, K2 ROOT, K3 HOLD SEQ TO IT. See section 6.6.

**MIDI** (69), **SYNC** (70), **PERFORMANCE** (71), **LEVELS** (72): section 11.

---

## 6. Playing

### 6.1 Keys and octave

The 27 keys play F3 to G5 at octave 0. **OCT -** and **OCT +** shift the whole keyboard by an octave, up to three octaves either way (MIDI note = 53 + 12 x octave + key number, limited to 0 to 127). On the STEPS and DRUMS pages OCT -/+ page the step grid instead (and shift a held step), so change the octave from another page.

Key velocity is fixed at 100. How much velocity matters (for MIDI input and the sequencer) is set by LEVELS K1 VELOCITY: 0 = loudness ignores velocity, 100% = full velocity response; default 50%.

Notes always release when you let go of a key, on whatever page you are on.

### 6.2 Voice modes

- **POLY**: 8 voices. A new note takes a free voice; when all are busy it steals the oldest released voice, then the oldest.
- **POLY 2 to POLY 7**: polyphonic like POLY, but at most as many notes sound at once as the number says (POLY 2 allows 2 notes, POLY 7 allows 7): the oldest note is cut when you play one more. Use them to keep the CPU in time on heavy sounds (many unison voices, both oscillators).
- **MONO**: one voice. The most recent held note sounds; letting go returns to the previous held note (the note stack holds 16). Each new note re-triggers the envelopes.
- **LEGATO**: like MONO, but a note played while another is held does not retrigger the envelopes: the pitch moves (gliding if GLIDE is up).

### 6.3 Glide

GLIDE (VOICE K2) sets the portamento time. In the code, notes only glide in MONO and LEGATO mode; in POLY mode each note starts directly at its own pitch. A sequencer slide step uses its own SLIDE TIME (PLAY SETUP K3) instead.

### 6.4 Arpeggiator

Press **ARP**, turn K1 to ON, and hold a chord: the held notes play one at a time in time with the tempo (set on PLAY SETUP or DELAY). With the arpeggiator on, the keys (and USB MIDI notes) feed it; the step sequencer is not affected. Latch (ARP 2 K3) keeps the arpeggio going after you let go; playing a new chord replaces it. Up to 16 notes can be held. When ARP is switched off, the arpeggio stops.

### 6.5 Pitch bend and mod wheel

The FM-1 has neither. Over USB (or TRS) MIDI, pitch bend moves pitch by BEND RANGE and the mod wheel (CC 1) is the modulation source MOD (section 7.9).

### 6.6 Scale mode (GLOBAL, SCALE page)

| Knob | Control | Meaning |
|---|---|---|
| K1 | SCALE | OFF, MAJOR, MINOR, DORIAN, PHRYGIAN, LYDIAN, MIXOLYDIAN, LOCRIAN, HARM MINOR, MEL MINOR, MAJ PENT, MIN PENT, BLUES, WHOLE TONE, PHRYG DOM, IN (JAPAN) |
| K2 | ROOT | C, C#, D, D#, E, F, F#, G, G#, A, A#, B |
| K3 | HOLD SEQ TO IT | OFF / ON |

While a scale is on (the root and scale show in the header): the keys and USB MIDI play the nearest note of the scale (the note below when equidistant); a held step's K1 pitch steps through the scale's notes and a key pressed to set a step's note snaps to the scale; the random generator uses the scale. With HOLD SEQ TO IT on, the sequencer's playback is also snapped to the scale, so TRANSPOSE stays in key. Scale, root and the hold switch are kept when you change patches.

### 6.7 Panic

**Double-tap PLAY** (two presses within about half a second): the sequencers, the drums and the arpeggiator stop and every voice is cut at once, with an all-notes-off sent on the MIDI out channel. The screen says "PANIC / ALL STOPPED".

---

## 7. Sound design

### 7.1 Signal flow

For each of 8 voices: OSC A + OSC B + SUB + NOISE (and the noise sample layer) go through the filter (each source can be routed through the filter or around it), then the amp envelope and velocity. The voices are summed, mixed with the drums, then the effects: synth and drum sends feed the delay and reverb, then distortion, the compressor and EQ, then the output. Modulation (LFO 1, LFO 2, ENV 2, ENV 3, velocity, note, macros, random, mod wheel, accent) goes through the 8-slot matrix to the destinations in section 7.9.

### 7.2 Oscillators and wavetables

Each oscillator reads one wavetable at a position (WT POS) that sweeps through the table's frames. Three factory tables exist:

| Table | Contents |
|---|---|
| **BASIC SHP** | In Serum's order: triangle, saw, square, sine, then the pulse width narrowing (pulse 37, 25, 17, 10, 5%). Nine stored frames; WT POS 0 to 100% passes frame n at n/8 of the way (0%, 12.5%, 25%, ...). |
| **FOLD-SINE** | A sine through a wavefolder, increasingly folded. |
| **ODD-EVEN** | An additive sweep from odd harmonics to even harmonics. |

Choosing a table: turn **PRESETS** (changes A, or B on the OSC B pages), or use the A TABLE / B TABLE pages. Between the stored frames the table is smoothed (spectral morph), so WT POS is continuous. Everything is band-limited, so high notes do not alias badly.

**Table morph**: TABLE chooses the main table, MORPH TO a second one, MORPH cross-fades between them at the same WT POS. Modulate WT POS and MORPH from one source to glide from one table into the other.

**Tuning**: OCTAVE (-3 to +3), SEMITONE (-12 to +12), FINE (cents), PAN. The patch-wide TUNE (VOICE page) and the pitch bend add to this.

**Phase**: PHASE sets the start phase of A on every note (editor: both oscillators, plus a RAND phase amount).

### 7.3 Warp

WARP on the A WARP / B WARP page chooses the mode, AMOUNT how far:

- **OFF**: no warp.
- **BEND**: bends the phase of the wave (asymmetric).
- **SYNC**: hard sync to an internal master; the amount sets the ratio from 1 to 8 times.
- **MIRROR**: folds the wave.

### 7.4 FM and PD between oscillators

On the WARP pages: **FM FROM B** (on A) and **FM FROM A** (on B) apply linear, through-zero frequency modulation. **PD FROM B / A** applies phase distortion. Both directions can be used at once. The modulating oscillator does not need to be audible: set its LEVEL to 0 and it still modulates (OSC B must be ON). The factory patch `fmbell` uses this.

### 7.5 Unison

UNISON (1 to 5 voices) per oscillator spreads copies of the oscillator in pitch (DETUNE) and across the stereo field. With an odd number of 3 or more voices the centre voice is plain and the others are weighted by BLEND (editor). SPREAD (editor) sets the stereo width. The loudness is normalised. Unison costs CPU: the engine can run at most 80 oscillator "lanes" (8 voices x 2 oscillators x 5); see CPU MODE (section 11.7).

### 7.6 Sub and noise

SUB (section 5.2) adds a sine, triangle, square or pulse oscillator by default one octave down. SUB 2: OCTAVE, COARSE (in cents), PAN. Square and pulse are built from band-limited saws. (Editor only: phase, "contiguous" phase between notes, filter routing.)

NOISE adds generated white, pink, brown or geiger noise with a stereo width and a low / high tilt, plus an optional looping sample layer (CRACKLE, AIR, RUMBLE). (Editor only: sample pitch, fine, key tracking, start, random start, loop on / off, per-source filter routing.)

### 7.7 Filter

See page FILTER and FILTER 2 (section 5.2). The filter cutoff is modulated exponentially. A modulation amount of 100% on CUTOFF moves the cutoff by 600 units (five octaves). KEY TRACK makes the cutoff follow the key. DRIVE can overdrive the filter input. MIX blends dry and filtered signal.

**VOWEL** is a formant filter: two band-pass filters tuned to the first two formants of a vowel, so the sound "speaks". With TYPE on VOWEL the cutoff no longer sets a frequency, it moves through the vowels A, E, I, O, U (the screen shows `A>E 40%`: 40% of the way from A to E; 0 to 135 spans A to U), RESONANCE makes the vowel sharper, DRIVE and MIX work as usual, and KEY TRACK does nothing. Modulating the cutoff (an LFO or envelope) makes the vowel move.

### 7.8 Envelopes

Three envelopes per voice: AMP (controls loudness, always), ENV 2 and ENV 3 (modulation sources). Each has delay, attack, hold, decay, sustain and release. The device screens show attack, decay, sustain, release for all three, and delay and hold for the amp envelope on AMP ENV 2. They are modulation sources ENV2 and ENV3 in the matrix. ENV 2 and ENV 3 have their own "GOES TO" pages for quick routing.

### 7.9 Modulation: LFOs, envelopes and the matrix

#### Sources

| Name on screen | What it is |
|---|---|
| LFO1, LFO2 | The two LFOs (global, not per voice). |
| ENV2, ENV3 | The two modulation envelopes (per voice). |
| VEL | Note velocity. |
| NOTE | Key pitch (centred on middle C: below it negative, above it positive). |
| MAC1 to MAC4 | The four macros (PERFORM page). |
| RAND | A random value per note. |
| MOD | The MIDI mod wheel (CC 1). |
| ACCENT | On while an accented sequencer step plays. |

#### Destinations (20)

A_WTPOS, B_WTPOS, A_WARP, B_WARP, A_DETUNE, B_DETUNE, A_LEVEL, B_LEVEL, A_PAN, B_PAN, A_PITCH, B_PITCH (shown on dials as SEMITONE; an amount of 100% = 24 semitones), A_FM, B_FM, A_MORPH, B_MORPH, CUTOFF (100% = 600 units, five octaves), RESO, DRIVE, SUB_PW. For the others, 100% = the whole range of the parameter.

#### Three ways to assign

1. **LFO and ENV "GOES TO" pages** (the easy way): on LFO 1 GOES TO, LFO 2 GOES TO, ENV 2 GOES TO or ENV 3 GOES TO, K1 picks the destination and K2 the amount (plus or minus). Each uses the first matrix slot that has that source, or the first free one.
2. **The MOD 1 to MOD 8 pages** (list view): K1 source, K2 destination, K3 amount, for each of the 8 slots. The same slots as above.
3. **Hold-SEL assign (by touch)**:
   1. Tap **SEL** (it stays on and the SEL light stays lit) or hold it. The screen shows `MOD` and the current source.
   2. Turn **ALGORITHM** to pick the source (LFO1, LFO2, ENV2, ENV3, VEL, NOTE, MAC1 to MAC4, RAND, MOD, ACCENT; it wraps).
   3. Turn the knob of the parameter you want to modulate, on any page. Clockwise = positive, anticlockwise = negative. Each click is 1% (a fast turn 2%). The dial turns blue and shows that source's amount on that parameter. A parameter that cannot be modulated shows `--`.
   4. Tap **SEL** again to end the mode (if you held it instead, just let go).

The destinations that can be modulated by touch are: both oscillators' WT POS, WARP, DETUNE, LEVEL, PAN, SEMITONE (pitch), FM and MORPH; CUTOFF, RESONANCE and DRIVE of the filter; and the SUB PULSE W. A new assignment takes the first free matrix slot (one with nothing in it). If all eight are in use, nothing happens. The amount is limited to +/-100%.

On the MOD pages the knobs edit the slots directly, also in the assign mode.

### 7.10 Macros

MACRO 1 to MACRO 4 (PERFORM, K1 to K4) are plain 0 to 100% knobs that do nothing by themselves. Use them as sources MAC1 to MAC4 in the matrix, so one knob can move several destinations at once (for example MAC1 to A_WTPOS +80%, to CUTOFF +40%, to A_FM +30%). They are saved with the patch.

### 7.11 Effects

See section 5.7. The effects chain (as designed): per-source sends to the delay and reverb, then distortion, compressor, volume, and the EQ on the master. Everything is off by default: with sends at 0, no distortion and no compression the effects stage is skipped. The factory patches `space` (pad with reverb and tape ping-pong delay) and `crunch` (lead with clip distortion and compression) show them in use.

Distortion types: DIODE, CLIP, SAT, BFZ, PDIST, FOLD, CRUSH (also used per source, with its own drive). The drum tracks have their own DIST and DRIVE pots as well (section 9.5).

### 7.12 Voice and global sound controls

VOICE page: mode, glide, bend range, tune. LEVELS page: velocity depth, patch master, CPU mode, second core (section 11.7).

---

## 8. The step sequencer

### 8.1 Overview

16 patterns (1 to 16), each up to 64 steps (4 bars of 16). Each step holds: a note (up to 4 notes in the engine, one from the panel), velocity, gate (how much of its last step it sounds), accent, slide, a length in steps (1 to 16), and a chance (5 to 100%). Pattern settings: length, rate, swing, direction, transpose. Pattern 1 starts as a short bass line. The sequencer runs inside the audio code, so it stays in time.

### 8.2 The STEPS page

Press **SEQ** (first page is STEPS). The 16 white keys are 16 steps of the shown bar; the 11 black keys pick patterns 1 to 11 (patterns 12 to 16 are picked on PATTERN 2). A key is lit when its step has a note, the step that is playing dims, and the black key of the current pattern glows dim. The blue / yellow outline on the grid is the playhead; a held step has a white outline.

Each step card shows: the note name (including the pattern's transpose), a velocity bar, an amber accent bar on top, a slide line, a yellow bar along the bottom for a long note (and a mint bar under the steps it lasts over), and a blue dot when the chance is below 100%. Steps beyond the pattern length are blank; steps outside a looped bar are greyed.

Below the grid: pattern number, length, tempo. With REC on it shows the REC state; with a step held it shows that step's note, VEL, GATE and LEN. Press OCT - / OCT + (no step held) to page through steps 17 to 64 of a longer pattern.

### 8.3 Editing steps

- **Tap an empty step**: it switches on with the last note you played (default C3, velocity 100, gate 60%, chance 100%).
- **Tap a step that has a note**: it switches on / off (it is kept: a tap off then on returns the same note).
- **Hold a step and press any other key**: that key's pitch becomes the step's note (it sounds, so you can press several to find the one you want). OCT - / OCT + while holding shift the note an octave.
- **Hold a step and turn the knobs**:

| Knob | Does | Range |
|---|---|---|
| K1 | Pitch (a blip sounds). In scale mode each click is the next note of the scale. | 1 to 127 |
| K2 | Velocity (3 per click) | 1 to 127 |
| K3 | Gate: how much of the note's last step it sounds | 5 to 100% (5% per click) |
| K3 with **ARP** held | Chance the step plays | 5 to 100% (5% per click) |
| K4 | Length: how many steps the note lasts, clockwise longer | 1 to 16 |

- **Hold a step and press a button**: **ENV** toggles accent, **LFO** toggles slide, **SAVE** clears the step (note, flags, velocity, gate, chance back to defaults), **OCT -/+** shift the note by an octave. You can also hold ENV or LFO first and then press a step to toggle that flag.

### 8.4 Accent, slide, long notes and chance

- **Accent**: lifts the step's velocity towards 127 by the ACCENT amount (PLAY SETUP K2), and switches on the ACCENT modulation source for that note (use it to open the filter on accents, for example).
- **Slide**: the note is held until the next step; if the next step has a single note, the pitch glides into it over SLIDE TIME (PLAY SETUP K3) without a new attack. If the next step is a rest, the note ends there.
- **Long notes (LEN)**: a note with length n rings for n steps, then the gate of the last step, whatever the steps after it hold. Steps under a long note can have notes of their own; in POLY mode they play on top without cutting it off.
- **Chance**: below 100% the step plays only that percent of the time (otherwise it is a rest). The blue dot shows it.
- **Tie**: the engine supports tie steps (the notes before are held through the step), shown as a tie mark; there is no front-panel control that sets it (the editor has a TIE button). See Notes to verify.

### 8.5 Recording (REC)

- **REC while stopped** (step by step): each key you play writes the next step (starting at step 1), and how long you hold the key sets the note's length (measured in step lengths at the current tempo, 1 to 16; the next note goes after it). A chord played one key after another goes in successive steps. The screen says "KEYS WRITE STEP BY STEP" and "NEXT STEP n".
- **REC while playing** (live): the notes you play go onto the nearest step as you play (a note a little early goes to the next step); the steps that pass while you hold the key become its length. The screen says "LIVE: KEYS WRITE THE NEAREST STEP".
- During REC the white keys play notes (they are not the step keys); press REC again to stop. Written notes use velocity 100, gate 70%, and snap to the scale if scale mode is on.
- On the drum pages, REC with the drums playing makes the pads write hits (section 9.6).

### 8.6 Patterns and playback

- **Choosing a pattern**: black keys on STEPS (patterns 1 to 11), or PATTERN 2 K1 (1 to 16). When stopped it changes at once; while playing it changes when the playing pattern wraps.
- **PLAY** starts the sequencer from step 1 (and the drums if WITH PLAY is on) and stops it. A synced LFO restarts with the start.
- **Direction**: FORWARD, REVERSE, PING-PONG, RANDOM.
- **Rate**: 1/16, 1/16T, 1/32, 1/8T per step. **Swing** 50 to 75%.
- **Transpose** (PATTERN 2): -24 to +24 semitones, stored with the pattern.
- **Clear** (PATTERN 2 K3): empties the pattern (length 16, rate 1/16, swing 50, forward, no transpose).
- **GENERATE** (section 5.5): random patterns.

### 8.7 Pattern length, remove bars, loop a bar

A bar is 16 steps; a pattern is at most 4 bars (64 steps). The same pages exist for the drums under HOME (DRUM LENGTH, DRUM REMOVE BARS) and edit the current drum pattern.

**PATTERN LENGTH** (SEQ) / **DRUM LENGTH** (HOME)

| Knob | Control | What it does |
|---|---|---|
| K1 | DOUBLE | Turn clockwise: doubles the length (1 bar to 2, 2 to 4; shorter lengths double too) and copies the steps into the new half, so the pattern repeats across the longer length. At 64 steps it says "ALREADY AT THE MOST". |
| K2 | HALVE | Turn clockwise to cut the pattern in half and keep the first half; turn anticlockwise to keep the second half (it moves to the start). Each click acts. |
| K3 | LOOP BAR | OFF, or BAR 1 to BAR 4: plays just that bar over and over. Nothing is deleted: the other bars wait, greyed out on the grid, and OFF plays everything again. The grid shows "LOOPING BAR n". A bar that does not exist says "NO SUCH BAR". |

Doubling, halving or removing bars switches the loop off.

**REMOVE BARS** (SEQ) / **DRUM REMOVE BARS** (HOME)

| Knob | Control | Range | What it does |
|---|---|---|---|
| K1 | FROM BAR | BAR 1 to 4 | The first bar to remove. |
| K2 | HOW MANY | 1 to 3 | How many bars. |
| K3 | REMOVE | TURN > / REMOVED | Turn clockwise: those bars are taken out and what follows moves up. For example "from bar 1, 2 bars" of a 4-bar pattern leaves bars 3 and 4 as a 2-bar pattern. It will not remove everything; if there is nothing to remove it says "NOTHING REMOVED". |

There is no undo for any of these. SAVE first (section 10) if you want to keep the old version.

---

## 9. Drums

### 9.1 Overview

The drum machine is X0X's TR-909 and TR-808 engines, with a drum step sequencer that shares the synth's tempo. It has 8 patterns (1 to 8) of up to 64 steps, 11 tracks per kit, per-step velocity and accent, a per-kit set of sound pots for every track, per-track level and pan, mute and solo. Pattern 1 starts as a four-on-the-floor beat.

### 9.2 Kits and tracks

| Track | 909 kit | 808 kit |
|---|---|---|
| 1 | BD (bass drum) | BD |
| 2 | SD (snare) | SD |
| 3 | LT (low tom) | LT |
| 4 | MT (mid tom) | MT |
| 5 | HT (high tom) | HT |
| 6 | RS (rim shot) | RS (rim / clave) |
| 7 | CP (clap) | CP (clap / maracas) |
| 8 | CH (closed hat) | CB (cowbell) |
| 9 | OH (open hat) | CY (cymbal) |
| 10 | CR (crash) | OH (open hat) |
| 11 | RD (ride) | CH (closed hat) |

Switch kit on DRUM KIT K1. The 808 hats use the 909's choke (a closed hat cuts the open one).

### 9.3 The DRUMS page

Press **HOME** until DRUMS (page 3, or hold HOME and press white key 3). The grid is 11 tracks (rows) by 16 steps, with the track names at the left (a label flashes white when the track sounds, a solo track's label is green, a muted track's label is dim). Step colours: amber = hit, bright (light) = accented hit, with a white line showing velocity. Beats are shaded every 4 steps; steps beyond the pattern length are dark; steps outside a looped bar are dark. The playhead is a blue marker. At the bottom: the kit, the selected track (or ACCENT), the pattern number (PAT), LEN, and PG (the page of 16) when the pattern is longer than 16.

### 9.4 Editing the grid

- **Black keys are the pads** (on DRUMS and every other drum page: DRUM ..., SOUND ..., MIXER): each hits its track at velocity 100 and selects it. The 11 black keys are tracks 1 to 11, left to right.
- **White keys are the 16 steps** of the shown bar of the selected track. **OCT - / OCT +** page to steps 17 to 32, 33 to 48, 49 to 64 (up to the pattern length).
- **Tap an empty step**: it switches on at velocity 100. **Tap an on step**: it switches off.
- **Hold a step**: K2 sets its velocity (3 per click, 1 to 127), **ENV** toggles its accent (a new accented step if it was empty), **SAVE** clears it.
- **K1** (with no step held) selects the track.
- Accent: a loud accent sounds at the LOUD ACCENT level of the DRUM ACCENT page (default fixed 127 = full level). A normal step's velocity maps to at most 75% of full level.

### 9.5 Sound pages (SOUND 1 to 4)

Choose the track with K1; K2 to K4 are three pots per page (SOUND 1 pots 1 to 3, 2 pots 4 to 6, 3 pots 7 to 9, 4 pots 10 to 12). Values are 0 to 127 (shown as percent) or a choice. They are stored with the kit (section 10), per kit, per track.

909 pots (in the order of the pages):

| Track | Pots |
|---|---|
| BD | Tune, Attack, Decay, Level, P.Dpth, Pitch, Drive, Dist, Rev, Dly, Pan |
| SD | Tune, Tone, Snappy, Decay, Level, Drive, Dist, Rev, Dly, Pan |
| LT, MT, HT | Tune, Decay, Level, Attack, Drive, Dist, Rev, Dly, Pan |
| RS | Level, Tune, Drive, Dist, Rev, Dly, Pan |
| CP | Level, Tune, Tail, Drive, Dist, Rev, Dly, Pan |
| CH, OH | Decay, Level, Tune, Drive, Dist, Rev, Dly, Pan |
| CR, RD | Tune, Level, Decay, Drive, Dist, Rev, Dly, Pan |

808 pots:

| Track | Pots |
|---|---|
| BD | Level, Tone, Decay, Tune, Attack, Drive, Dist, Rev, Dly, Pan |
| SD | Level, Tone, Snappy, Tune, Decay, Drive, Dist, Rev, Dly, Pan |
| LT, MT, HT | Level, Tune, Decay, Sound (TOM / CONGA), Drive, Dist, Rev, Dly, Pan |
| RS | Level, Tune, Decay, Sound (RIM / CLAVE), Drive, Dist, Rev, Dly, Pan |
| CP | Level, Tune, Decay, Attack, Sound (CLAP / MARAC), Drive, Dist, Rev, Dly, Pan |
| CB, CH | Level, Tune, Decay, Drive, Dist, Rev, Dly, Pan |
| CY, OH | Level, Decay, Tune, Drive, Dist, Rev, Dly, Pan |

Dist (per track) is a choice of DIODE, CLIP, SAT, BFZ, PDIST, FOLD, CRUSH. Rev and Dly are the track's own sends (scaled by DRUM FX sends). Level and Pan feed the MIXER page.

### 9.6 Playing and recording drums

- **PLAY** starts the drums with the synth sequencer when DRUM KIT K3 WITH PLAY is ON. With it OFF PLAY only runs the synth sequencer.
- **REC** on a drum page: while the drums play, hit the pads and each hit is written onto the nearest step of the selected pattern at velocity 100.
- **Pattern select**: DRUM KIT K4 (1 to 8), changing at the end of the playing pattern.
- **Clear**: DRUM PATTERN K4. **Length, rate, swing**: DRUM PATTERN K1 to K3.
- **Double, halve, loop a bar, remove bars**: DRUM LENGTH and DRUM REMOVE BARS, as in section 8.7.

### 9.7 Generate

DRUM GENERATE replaces the current drum pattern (section 5.1).

- FOUR FLOOR: bass drum on every beat (accent on step 1), snare on 2 and 4 (accent on 4), closed hat every other step, open hat on steps 7 and 15 of each bar.
- BREAK: a breakbeat bass drum (steps 1 and 11) and snare on 2 and 4, with closed hats.
- RANDOM: scattered bass drum, snare and hats by density.
- EXTRAS adds ghost kicks and snares, toms, and rim or clap hits.

### 9.8 The mixer

Page MIXER (HOME, second page). One screen with 13 faders: synth (SY), drums (DR) and the 11 tracks. The selected one is outlined; the fader length shows its level; a muted track is dim; a track flashes when it plays; an M or S sits above a muted or soloed fader.

| Knob | Does |
|---|---|
| K1 | Selects the channel (or hit a drum pad with a black key). |
| K2 | Level. The synth and drums levels are 0 to 100% (2% a click). A track's level is that track's own Level pot, so it also shows on the SOUND pages and is saved with the kit. |
| K3 | Pan, drum tracks only (L100 to C to R100). |
| K4 | One click clockwise: MUTE; two clicks: SOLO. Anticlockwise returns to normal. |

Mute and solo work on the synth (SY), on the drum machine as a whole (DR) and on each track. A solo silences everything that is not soloed: solo the synth and the drums stop; solo a drum track and you hear only that track; solo DR and every drum track plays without the synth. A muted drum track does not sound, but the bass-drum sidechain still ducks for a muted kick. The mute / solo state is not changed by loading a patch.

---

## 10. Saving and loading patches (the SAVE page)

### 10.1 Loading

Turn **ALGORITHM**: the next (clockwise) or previous patch. The 11 factory patches come first (init, pad, bass, pluck, lead, sweep, fmbell, tmorph, subnoise, space, crunch), then your saved patches (empty slots are skipped), wrapping around. The patch name shows top right. The unit starts on `init` at power-on.

Loading a patch sets every synth parameter (oscillators, filter, envelopes, LFOs, macros, matrix, effects, arpeggiator, EQ, tempo, kit choice and level, and so on), but it does not touch: the synth sequencer patterns, the drum patterns and kit sounds, and the "setup" that outlives a patch change: mute / solo state, MIDI channels, USB audio, second core, scale, root and hold-sequence switch, clock source and start / stop switch.

### 10.2 The SAVE page

Press **SAVE** to open the page (page 53). Dials (small, in a row), plus the name being typed:

| Knob | Control | Range | Meaning |
|---|---|---|---|
| K1 | SLOT | U1 to U20 on the dial; U1 to U18 exist | The user slot to save into. |
| K2 | WHAT | PATCH, PATTERNS, DRUMS+SETUP, ALL, ERASE | What to store or do. |
| K3 | PLACE | 1 to 11 | Which of the 11 character places of the name you edit (underlined). |
| K4 | LETTER | space, A to Z, 0 to 9, - | The character at that place. |

The name starts from the loaded patch's name (letters upper-cased; characters not in the set become spaces). A space shows as `_`. Names are up to 11 characters; an empty name is stored as `PATCH`.

What the choices do:

| WHAT | Stores |
|---|---|
| PATCH | The current sound, under the typed name, into the chosen slot. |
| PATTERNS | All 16 synth sequencer patterns (not slot dependent). |
| DRUMS+SETUP | The 8 drum patterns, both kits' sounds (every pot of every track), and the setup (MIDI channels, USB audio, scale, root, hold-sequence switch, clock source and start / stop). Not slot dependent. |
| ALL | The patch (into the slot), the patterns, and the drums and setup. |
| ERASE | Empties the chosen slot. |

To do it: press **SAVE** once to arm (the screen says "PRESS SAVE AGAIN"; it disarms after 5 s or if you turn any knob), press **SAVE** again to store. The result shows for 3 s: SAVED, ERASED or FAILED. A saved patch becomes the loaded patch and appears after the factory ones when you turn ALGORITHM.

Everything stored in flash is loaded again at power-on: the saved patterns, the drum patterns, kit sounds and the setup. Patches are loaded when you choose them.

The bottom of the SAVE page shows what the flash held at power-on (PATCH STORE, PATTERNS, DRUMS: OK, BAD or NONE), which helps after a firmware update. The whole sound is saved; wavetables are not user data on the device, so nothing needs saving for them.

### 10.3 Saving from the editor

The browser editor can save patches into the same user slots over USB, read them back, erase and back them up (section 12.5).

---

## 11. Global, MIDI, clock and setup pages

### 11.1 MIDI (page 69)

| Knob | Control | Range | Meaning |
|---|---|---|---|
| K1 | IN CHANNEL | ALL, 1 to 16 (default ALL) | The channel the synth listens to. |
| K2 | DRUM CHANNEL | OFF, 1 to 16 (default 10) | The channel that plays the drums. A drum channel that is also the synth's channel goes to the drums. |
| K3 | OUT CHANNEL | OFF, 1 to 16 (default OFF) | The notes you play, and the sequencer and arpeggiator notes, are sent out over USB MIDI on this channel. Notes that came in are not echoed. A panic sends all notes off here. |
| K4 | USB AUDIO | OFF / ON (default ON) | Sends the stereo mix to the computer as USB audio. |

Received (USB MIDI, and the TRS jack): note on / off, pitch bend, mod wheel (CC 1), all notes off (CC 123). Drum notes on the drum channel (General MIDI drum notes) hit the tracks of the current kit; see Appendix D. These settings stay when you change patches; SAVE with WHAT = DRUMS+SETUP (or ALL) keeps them across power-off.

### 11.2 SYNC (page 70)

| Knob | Control | Range | Meaning |
|---|---|---|---|
| K1 | CLOCK FROM | INTERNAL, MIDI IN, USB | INTERNAL: its own tempo (PLAY SETUP). MIDI IN: the clock on the 3.5 mm TRS jack. USB: the clock on the USB-C port. A clock on the other port is ignored. |
| K2 | START / STOP | OFF / ON (default ON) | ON: Start, Continue and Stop from the clock start and stop the synth sequencer and the drums (Start begins at step 1 on the first tick, Continue picks up where it stopped). OFF: only the clock's tempo is used and PLAY / STOP stay yours. |

The page shows "OWN TEMPO (PLAY SETUP)" when internal, "WAITING FOR CLOCK ..." until ticks arrive, then "LOCKED" with the measured BPM. The drum and synth sequencers, the arpeggiator and the synced LFOs run on the clock, and the delay time follows its tempo. With a clock chosen, the sequencers move only when ticks arrive, so PLAY alone will not run them; set INTERNAL to play without a computer. In Ableton Live, switch on Sync for the FM-1's MIDI port (Settings, Link / Tempo / MIDI), choose USB here, and play. The setting is stored with SAVE, WHAT = DRUMS+SETUP.

### 11.3 SCALE (page 68)

Section 6.6.

### 11.4 VOICE (page 67)

Section 5.9.

### 11.5 Sidechain and drum FX

Section 5.7.

### 11.6 PERFORMANCE (page 71)

A read-only page showing where the audio time goes: bars for VOICES, DRUMS (WAIT), EFFECTS + EQ and CONTROL (as shares of the audio block's time, scaled to the CPU %), the CPU % and its PEAK, the number of voices sounding, the LANES in use out of 80, whether the 2ND CORE is ON, and the CPU MODE.

### 11.7 LEVELS (page 72)

| Knob | Control | Range | Meaning |
|---|---|---|---|
| K1 | VELOCITY | 0 to 100% (default 50%) | How much key velocity affects loudness. |
| K2 | MASTER | 0 to 100% (default 80%) | The patch's own output level (a squared curve), saved in the patch. Separate from the MASTER knob on the panel. |
| K3 | CPU MODE | FULL, AUTO, SAVE (default AUTO) | FULL: no limiter (may break up). AUTO: sheds unison lanes when the CPU runs low (nearly out of time: it drops a quarter of the lanes at once; above 80%: 2 lanes; it wins them back slowly below 60%; never fewer than 16 lanes) and shows LIMIT in the header. SAVE: always lean (cheaper linear table reads, at most 32 lanes) and shows SAVE in the header. |
| K4 | 2ND CORE | OFF / ON (default ON) | ON: the drums render on the FM-1's second processor core (more headroom). OFF: everything on one core. If the second core does not answer at power-on, everything runs on one core. |

---

## 12. The browser editor and USB patch transfer

### 12.1 Opening it

The editor is `out/wt1-editor.html`, one self-contained file. Use Chrome or Edge (they have Web MIDI with SysEx). Web MIDI needs the page to be served from `localhost` or `https`, not opened as a plain file: on a Mac double-click `open-editor.command`, which serves the `out/` folder at `http://127.0.0.1:8787/wt1-editor.html` and opens your browser (leave its window open; closing it stops the server). Click **Start audio** (a browser needs a click before it plays sound). The synth runs in your browser (WebAssembly in an AudioWorklet at 44.1 kHz), with the same code as the firmware. The page uses Producer Tools' dark colour scheme.

### 12.2 What is on the page

- **Header**: a Load patch... menu for the 11 factory patches, the MASTER slider, a level meter and voice count, the keyboard selector (choose **M-Vave FM-1**; the other entry is for a different keyboard), the patch name box, **Save patch**, **Save with sounds**, **Load patch file...**, and **Save folder...**.
- **Wavetables**: the three table slots (3D stack view with the oscillators' positions), Import WAV (Serum-style multi-frame files, any frame size, or a whole file as one cycle), From shapes (build a table from a list of shapes), Factory (BASIC SHAPES, FOLD-SINE, ODD-EVEN, SAW), Export WAV, a SPECTRAL / CROSSFADE morph switch, and a storage meter. Noise samples: four slots (load any WAV, mono-mixed and resampled to 44.1 kHz, up to 1.49 s, optional normalise and seamless loop).
- **Output**: scope and spectrum views, with **Pop out** to show them in a floating window that keeps drawing while you work in another window (**Bring back** returns it).
- **Panels for every parameter**: Voice, Macros, Device (USB), Sequencer, Drums (grid, mixer, track sounds), Oscillator A and B (every parameter, including those without a front-panel knob), Sub, Noise, Filter, effects, three envelopes with delay and hold, two LFOs, and the 8-slot modulation matrix. Double-click a slider to reset it.
- **Keyboard** at the bottom: click the on-screen keys, use the computer keys (the row `a w s e d f t g y h u j k o l p ; '` plays C upward chromatically; **z** / **x** change the octave), or choose a MIDI input. Bend and mod sliders, All notes off.

Editor-only values (no front-panel knob on the device screens): oscillator BLEND, SPREAD, RAND phase, B PHASE, filter-routing switches for A, B, SUB and NOISE, SUB PHASE and CONTINUOUS, the noise sample's pitch, fine, key track, start, random and loop, and the delay and hold of ENV 2 and ENV 3. They are part of the patch, so a patch sent to the device carries them.

### 12.3 Patch name and default save folder

- The **name box** sets the patch's name: it is the file name when you save, and the name on the device (the device uses capital letters, the characters A to Z, 0 to 9 and -, other characters become spaces, at most 11 characters).
- **Save folder...** (Chrome) lets you pick the folder patches are saved to. It remembers the folder (stored in your browser) and asks for permission again when needed. Patch files and device backups are written there; without a folder (or in a browser that cannot pick one) they download to your Downloads folder. **Load patch file...** opens in that folder.
- **Save patch** writes `name.json` (format `wt1-patch`: all parameters, plus the drum patterns and kit sounds). **Save with sounds** also embeds imported wavetables and samples.

### 12.4 Connecting to the device

1. Connect the FM-1 with a USB-C data cable and switch it on (WT-1 firmware running).
2. In the header choose **M-Vave FM-1** as the keyboard.
3. In the **Device (USB)** panel click **Connect**. Chrome asks for MIDI access with SysEx: allow it.
4. The status line says "Connected: <port name>" and the slot list fills in.

The editor finds the port by a name pattern (names containing felucca, fm-1, m-vave or wt-1; if none match it uses the first MIDI port).

### 12.5 What you can do

| Button | Does |
|---|---|
| **Save to slot** | Sends the editor's whole sound to the chosen slot (U1 to U18) under the patch name. |
| **Save + play** | Saves it and loads it on the device now. |
| **Play on device** | Loads it on the device without saving. |
| **Read from slot** | Brings a saved sound from the chosen slot into the editor (and its name). |
| **Read what is playing** | Brings the sound the device is playing now (as its knobs have it, saved or not) into the editor. |
| **Erase slot** | Empties the slot on the device (asks first). |
| **Back up all slots** | Writes every saved sound on the device to `wt1-device-backup-YYYYMMDD.json` (in your save folder). |
| **Restore backup...** | Writes a backup file's sounds back to the device (asks first; it replaces what is in those slots). |

These transfers carry sounds only (all patch parameters). They do not carry sequencer patterns, drum patterns, drum kit sounds, the setup, imported wavetables or imported noise samples. The device's three wavetables and three noise loops are fixed. A patch saved in the editor that uses an editor wavetable will play with the device's own table of the same slot number.

### 12.6 How the transfer works (SysEx)

For those writing their own tools: frames are `F0 7D 57 54 <cmd> ... F7` (7-bit data; a 16-bit value is three bytes). One request is answered at a time.

| Cmd | Name | Meaning |
|---|---|---|
| 01 | HELLO | Answer: protocol version (2), parameter count (206), slot count (18). |
| 02 | LIST | Answer: slot count, then 11 name characters per slot (0 = empty). |
| 03 | PUT | Stage a patch in chunks (offset, count, values). |
| 05 | COMMIT | Slot, mode (0 save, 1 play now, 2 both), 11-character name. |
| 06 | GET | Read a slot (127 = the sound playing now) in chunks. |
| 07 | ERASE | Empty a slot. |

Status codes: 0 ok, 1 empty slot, 2 incomplete patch, 3 out of order, 4 bad slot, 5 busy, 6 flash write failed. The editor stops if its parameter count differs from the firmware's ("the editor and the firmware are different versions"): rebuild or reinstall so both match.

---

## 13. Troubleshooting

| Problem | What to try |
|---|---|
| No sound | Turn up the panel MASTER knob. Check LEVELS K2 MASTER, the OSC A LEVEL (and that OSC A is on: A_ON is on in every factory patch). Check the MIXER: SY must not be muted, and nothing else should be soloed. Check the keys are not on a drum page where black keys are pads and white keys steps (on DRUMS). Check the amp envelope (sustain 0 with a short decay is silent while held). |
| Filter does nothing | The default patch has FILTER TYPE OFF. Choose LP12 or LP24. |
| Keys play steps or drum hits instead of notes | You are on STEPS or DRUMS. Go to another page (EDIT, HOME, or SELECT). |
| A note hangs | Let go of the key (notes release on any page). Otherwise double-tap PLAY for a panic. |
| PLAY starts nothing | If CLOCK FROM (GLOBAL, SYNC) is MIDI IN or USB, the sequencers wait for clock ticks: set INTERNAL. Check the pattern has notes on steps within its length. |
| Drums do not start with PLAY | DRUM KIT K3 WITH PLAY is OFF, or the drums are muted / another track is soloed on MIXER, or DR level is 0. |
| Drums are silent over MIDI | The drum channel (MIDI K2) is OFF, or the notes are not GM drum notes (Appendix D). |
| A synth note plays with the wrong pitch | Scale mode is on and snapping to the scale. Set SCALE to OFF. |
| WT POS jumps instead of moving smoothly | WT POS snaps to each stored frame as the knob passes it; turn faster to pass through, or modulate it. |
| Crackling or "LIMIT" in the header | The CPU is busy. Lower UNISON, turn OSC B or unneeded effects off, put CPU MODE on AUTO or SAVE, keep 2ND CORE ON, or check the PERFORMANCE page. |
| Everything is quiet after loading a patch | Patch master or the mixer level changed. Check LEVELS and the MIXER; the mute / solo state is kept across patches. |
| Saved patch missing | Check the SAVE result said SAVED (not FAILED). Use slots U1 to U18. The SAVE page's bottom lines show whether the flash stores read OK at power-on. |
| SAVE says FAILED | Flash write failed. Try again; if it persists, do not use that slot and reinstall the firmware. |
| Drum pattern or setup not remembered | They are saved only with WHAT = DRUMS+SETUP or ALL. Patches alone do not store them. |
| The editor will not connect | Use Chrome or Edge. Open the editor from localhost or https (use `open-editor.command`). Allow MIDI with SysEx when asked. Choose M-Vave FM-1 in the keyboard selector. Use a USB data cable. Close other apps using the MIDI port (a DAW, M-UPGRADE). "The device did not answer": the unit is off, or WT-1 is not running. |
| "The editor and the firmware are different versions" | The parameter counts differ. Install the `wt1.fwsc` and use the `wt1-editor.html` from the same build. |
| "The device has an older firmware" | Install the latest `out/wt1.fwsc`, then connect again. |
| The editor shows a patch with different sounds on the device | Editor wavetables and samples are not sent; the device uses its own three tables and noise loops. |
| The screen shows RESTARTED AFTER A CRASH | The firmware reset. Note the address; if it repeats at every start it enters safe mode and then update mode (section 2.4). Reinstall, or use the rescue tool (section 2.5). |
| Buttons or knobs report the wrong controls | Run HARDWARE CALIBRATION (hold OCT - and OCT + while switching on). |
| Install fails in M-UPGRADE | See section 2.2 (identity) or install with `tools/fm1_install.py`. If the FM-1 is stuck, hold OCT - and OCT + for 5 s to enter update mode, or use the rescue tool. |

---

## 14. Appendices

### Appendix A: parameter reference

Parameter names are those used in patches, the editor and the SysEx protocol (206 in all). Ranges are stored values. "Screen" shows where the front panel edits it ("editor" = no front-panel knob).

**Global and voice**

| Name | Range | Default | Screen |
|---|---|---|---|
| MASTER | 0 to 1000 | 800 | LEVELS K2 |
| TUNE | -100 to 100 cents | 0 | VOICE K4 |
| VOICEMODE | 0 POLY, 1 MONO, 2 LEGATO, 3 to 8 POLY 2 to POLY 7 | 0 | VOICE K1 |
| GLIDE | 0 to 1000 | 0 | VOICE K2 |
| BEND | 0 to 24 semitones | 2 | VOICE K3 |
| VELAMP | 0 to 1000 | 500 | LEVELS K1 |
| CPU_MODE | 0 FULL, 1 AUTO, 2 SAVE | 1 | LEVELS K3 |
| CORE2 | 0 to 1 | 1 | LEVELS K4 |

**Oscillators** (prefix A_ or B_; B_ON default 0, A_ON default 1)

| Name | Range | Default | Screen |
|---|---|---|---|
| ON | 0 to 1 | A 1, B 0 | OSC B K1 (A is always on in the UI) |
| TABLE | 0 to 2 | 0 | TABLE K1 / PRESETS |
| WTPOS | 0 to 1000 | 0 | OSC K1 (A) / K2 (B) |
| WARPMODE | 0 OFF, 1 BEND, 2 SYNC, 3 MIRROR | 0 | WARP K1 |
| WARP | -1000 to 1000 | 0 | WARP K2 |
| OCT | -3 to 3 | 0 | TUNE K1 |
| SEMI | -12 to 12 | 0 | TUNE K2 |
| FINE | -100 to 100 cents | 0 | TUNE K3 |
| PHASE | 0 to 360 | 0 | A TABLE K4 (A only; B editor) |
| RAND | 0 to 100 | 0 | editor |
| UNI | 1 to 5 | 1 | OSC K3 |
| DETUNE | 0 to 1000 | 250 | OSC A K2, B TABLE K4 |
| BLEND | 0 to 1000 | 750 | editor |
| SPREAD | 0 to 1000 | 500 | editor |
| LEVEL | 0 to 1000 | 700 | OSC K4 |
| PAN | -1000 to 1000 | 0 | TUNE K4 |
| FM | 0 to 1000 | 0 | WARP K3 |
| PD | 0 to 1000 | 0 | WARP K4 |
| TABLE2 | 0 to 2 | 0 | TABLE K2 |
| MORPH | 0 to 1000 | 0 | TABLE K3 |
| FLT | 0 to 1 (through the filter) | 1 | editor |

**Sub and noise**

| Name | Range | Default | Screen |
|---|---|---|---|
| SUB_ON | 0 to 1 | 0 | SUB K1 |
| SUB_WAVE | 0 SINE, 1 TRIANGLE, 2 SQUARE, 3 PULSE | 0 | SUB K2 |
| SUB_OCT | -3 to 3 | -1 | SUB 2 K1 |
| SUB_CRS | -1200 to 1200 cents | 0 | SUB 2 K2 |
| SUB_PW | 0 to 1000 | 500 | SUB K3 |
| SUB_PHASE | 0 to 360 | 0 | editor |
| SUB_CONT | 0 to 1 | 0 | editor |
| SUB_LEVEL | 0 to 1000 | 500 | SUB K4 |
| SUB_PAN | -1000 to 1000 | 0 | SUB 2 K3 |
| SUB_FLT | 0 to 1 | 1 | editor |
| NOI_ON | 0 to 1 | 0 | NOISE K1 |
| NOI_TYPE | 0 WHITE, 1 PINK, 2 BROWN, 3 GEIGER, 4 OFF | 0 | NOISE K2 |
| NOI_STEREO | 0 to 1000 | 0 | NOISE K4 |
| NOI_FILTER | -1000 to 1000 | 0 | NOISE 2 K1 |
| NOI_LEVEL | 0 to 1000 | 200 | NOISE K3 |
| NOI_PAN | -1000 to 1000 | 0 | NOISE 2 K2 |
| NOI_FLT | 0 to 1 | 1 | editor |
| NOI_SMP | 0 to 4 | 0 | NOISE 2 K3 |
| NOI_SMP_LEVEL | 0 to 1000 | 500 | NOISE 2 K4 |
| NOI_SMP_PITCH | -24 to 24 | 0 | editor |
| NOI_SMP_FINE | -100 to 100 | 0 | editor |
| NOI_SMP_KEY | 0 to 1 | 0 | editor |
| NOI_SMP_START | 0 to 1000 | 0 | editor |
| NOI_SMP_RAND | 0 to 1000 | 0 | editor |
| NOI_SMP_LOOP | 0 to 1 | 1 | editor |

**Filter and envelopes**

| Name | Range | Default | Screen |
|---|---|---|---|
| F_TYPE | 0 OFF, 1 LP12, 2 LP24, 3 HP12, 4 BP12, 5 NOTCH, 6 VOWEL | 0 | FILTER K1 |
| F_CUTOFF | 0 to 1350 (120 per octave) | 1000 | FILTER K2 |
| F_RESO | 0 to 1000 | 200 | FILTER K3 |
| F_DRIVE | 0 to 1000 | 0 | FILTER K4 |
| F_KEYTRK | 0 to 1000 | 0 | FILTER 2 K1 |
| F_MIX | 0 to 1000 | 1000 | FILTER 2 K2 |
| AMP_ / ENV2_ / ENV3_ DLY, ATK, HLD, DEC, SUS, REL | 0 to 1000 | DLY 0, ATK 0, HLD 0, DEC 500, SUS 1000 (amp) / 0 (ENV 2, 3), REL 300 | ENV pages (DLY and HLD: AMP ENV 2 only; ENV 2 and 3: editor) |

**LFOs, macros, matrix**

| Name | Range | Default | Screen |
|---|---|---|---|
| LFO1_SHAPE, LFO2_SHAPE | 0 to 6 | 0 | LFO K1 |
| LFO1_RATE, LFO2_RATE | 0 to 1000 | 400 | LFO K2 |
| LFO1_SYNC, LFO2_SYNC | 0 to 15 | 0 | LFO K3 |
| LFO1_KEY, LFO2_KEY | 0 FREE, 1 EVERY NOTE, 2 FIRST NOTE | 0 | LFO K4 |
| LFO1_POL, LFO2_POL | 0 UP + DOWN, 1 UP ONLY, 2 DOWN ONLY | 0 | LFO GOES TO K3 |
| MACRO1 to MACRO4 | 0 to 1000 | 0 | PERFORM |
| MX1_SRC to MX8_SRC | 0 to 13 | 0 | MOD pages K1 |
| MX1_DST to MX8_DST | 0 to 20 | 0 | MOD pages K2 |
| MX1_AMT to MX8_AMT | -1000 to 1000 | 0 | MOD pages K3 |

**Effects** (the delay and reverb values and sends are 0 to 127 internally, shown as percent)

| Name | Range | Default | Screen |
|---|---|---|---|
| FX_VOL | 0 to 127 | 127 | MASTER FX K4 |
| FX_DIST | 0 to 7 | 0 | MASTER FX K1 |
| FX_DRIVE | 0 to 127 | 41 | MASTER FX K2 |
| FX_COMP | 0 to 127 | 0 | MASTER FX K3 |
| RV_DECAY | 0 to 127 | 73 | REVERB K2 |
| RV_TONE | 0 to 127 | 57 | REVERB K3 |
| RV_HPF | 0 to 127 | 62 | REVERB 2 K1 |
| RV_LEVEL | 0 to 127 | 85 | REVERB K4 |
| DL_TIME | 0 to 12 | 7 | DELAY K2 |
| DL_FDBK | 0 to 127 | 52 | DELAY K3 |
| DL_TONE | 0 to 127 | 51 | DELAY 2 K1 |
| DL_LEVEL | 0 to 127 | 85 | DELAY 2 K2 |
| DL_HPF | 0 to 127 | 62 | DELAY 2 K3 |
| DL_TYPE | 0 DIGI, 1 TAPE | 0 | DELAY 2 K4 |
| DL_WEAR | 0 to 127 | 64 | TAPE K1 |
| DL_PING | 0 to 1 | 0 | TAPE K2 |
| RV_SEND, DL_SEND | 0 to 127 | 0 | SYNTH FX K4, K3 (also REVERB K1, DELAY K1) |
| SYN_DIST, SYN_DRIVE | 0 to 7, 0 to 127 | 0, 40 | SYNTH FX K1, K2 |
| DR_DIST, DR_DRIVE | 0 to 7, 0 to 127 | 0, 40 | DRUM FX K1, K2 |
| DR_RVSEND, DR_DLSEND | 0 to 127 | 127 | DRUM FX K4, K3 |
| SC_AMT, SC_REL | 0 to 1000 | 0, 450 | SIDECHAIN K1, K2 |
| EQ_ON | 0 to 1 | 1 | EQ K4 |
| EQ_LOW, EQ_MID, EQ_HIGH | -120 to 120 (0.1 dB) | 0 | EQ K1, K2, K3 |
| EQ_LOWF, EQ_MIDF, EQ_HIGHF | 40 to 400, 200 to 8000, 2000 to 12000 Hz | 120, 1000, 6000 | EQ 2 K1, K2, K4 |
| EQ_Q | 3 to 40 (tenths) | 10 | EQ 2 K3 |

**Sequencer, arp, drums, mixer, setup**

| Name | Range | Default | Screen |
|---|---|---|---|
| BPM | 40 to 240 | 120 | PLAY SETUP K1, DELAY K4 |
| ACCENT | 0 to 1000 | 600 | PLAY SETUP K2 |
| SLIDE | 0 to 1000 | 300 | PLAY SETUP K3 |
| ARP_ON | 0 to 1 | 0 | ARP K1 |
| ARP_MODE | 0 to 6 | 0 | ARP K2 |
| ARP_RATE | 0 to 5 | 3 | ARP K3 |
| ARP_OCT | 1 to 4 | 1 | ARP K4 |
| ARP_GATE | 5 to 100 | 60 | ARP 2 K1 |
| ARP_SWING | 50 to 75 | 50 | ARP 2 K2 |
| ARP_LATCH | 0 to 1 | 0 | ARP 2 K3 |
| DR_KIT | 0 909, 1 808 | 0 | DRUM KIT K1 |
| DR_LEVEL | 0 to 1000 | 800 | DRUM KIT K2, MIXER |
| DR_SEQ | 0 to 1 | 1 | DRUM KIT K3 |
| DR_ACCENT | -127 to 255 (128..255 = fixed 0..127) | 255 | DRUM ACCENT K1 |
| DR_SOFT | -127 to 255 (same coding) | 25 | DRUM ACCENT K2 |
| SYN_LEVEL | 0 to 1000 | 1000 | MIXER (SY) |
| SYN_MUTE, DR_MUTE | 0 normal, 1 mute, 2 solo | 0 | MIXER K4 |
| SCALE, ROOT, SCALE_SEQ | 0 to 15, 0 to 11, 0 to 1 | 0, 0, 0 | SCALE |
| MIDI_CH, MIDI_DRCH, MIDI_OUTCH | 0 to 16 | 0, 10, 0 | MIDI K1 to K3 |
| USB_AUDIO | 0 to 1 | 1 | MIDI K4 |
| CLK_SRC, CLK_SS | 0 to 2, 0 to 1 | 0, 1 | SYNC |

(FDR0 to FDR7 are used by another keyboard model's faders and have no function on the FM-1.)

### Appendix B: the keys

| Key | Note (octave 0) | Role on STEPS | Role on DRUMS |
|---|---|---|---|
| F3 (white 1) | 53 | Step 1 | Step 1 |
| F#3 (black 1) | 54 | Pattern 1 | Pad: track 1 |
| G3 (white 2) | 55 | Step 2 | Step 2 |
| G#3 (black 2) | 56 | Pattern 2 | Pad: track 2 |
| A3 (white 3) | 57 | Step 3 | Step 3 |
| A#3 (black 3) | 58 | Pattern 3 | Pad: track 3 |
| B3 (white 4) | 59 | Step 4 | Step 4 |
| C4 (white 5) | 60 | Step 5 | Step 5 |
| C#4 (black 4) | 61 | Pattern 4 | Pad: track 4 |
| D4 (white 6) | 62 | Step 6 | Step 6 |
| D#4 (black 5) | 63 | Pattern 5 | Pad: track 5 |
| E4 (white 7) | 64 | Step 7 | Step 7 |
| F4 (white 8) | 65 | Step 8 | Step 8 |
| F#4 (black 6) | 66 | Pattern 6 | Pad: track 6 |
| G4 (white 9) | 67 | Step 9 | Step 9 |
| G#4 (black 7) | 68 | Pattern 7 | Pad: track 7 |
| A4 (white 10) | 69 | Step 10 | Step 10 |
| A#4 (black 8) | 70 | Pattern 8 | Pad: track 8 |
| B4 (white 11) | 71 | Step 11 | Step 11 |
| C5 (white 12) | 72 | Step 12 | Step 12 |
| C#5 (black 9) | 73 | Pattern 9 | Pad: track 9 |
| D5 (white 13) | 74 | Step 13 | Step 13 |
| D#5 (black 10) | 75 | Pattern 10 | Pad: track 10 |
| E5 (white 14) | 76 | Step 14 | Step 14 |
| F5 (white 15) | 77 | Step 15 | Step 15 |
| F#5 (black 11) | 78 | Pattern 11 | Pad: track 11 |
| G5 (white 16) | 79 | Step 16 | Step 16 |

### Appendix C: factory patches

| Name | What it is |
|---|---|
| init | Plain saw, one voice, filter off. |
| pad | Two detuned unison oscillators (saw and an octave-down square), low-pass, slow attack, LFO on wavetable position and cutoff. |
| bass | Mono, glide, saw + sine sub, LP24 with envelope on cutoff. |
| pluck | Pulse wave unison, LP24, envelopes on cutoff and wavetable position. |
| lead | Mono, glide, sync warp, vibrato from LFO 1, mod wheel on warp. |
| sweep | Wavetable sweep from a saw-down LFO on WT POS. |
| fmbell | FM bell: B is a silent modulator an octave and a fifth up. |
| tmorph | Table morph from BASIC SHP to FOLD-SINE with one slow LFO. |
| subnoise | Pulse sub with swept width, pink noise and a looping sample layer. |
| space | The pad with reverb and a tape ping-pong delay. |
| crunch | The lead with clip distortion and compression, plus delay and reverb. |

### Appendix D: General MIDI drum notes (on the drum channel)

| Note | Track (909 kit) | Track (808 kit) |
|---|---|---|
| 35, 36 | BD | BD |
| 38, 40 | SD | SD |
| 41, 43 | LT | LT |
| 45, 47 | MT | MT |
| 48, 50 | HT | HT |
| 37 | RS | RS |
| 39 | CP | CP |
| 42, 44 | CH | CH |
| 46 | OH | OH |
| 49, 52, 55, 57 | CR | CY |
| 51, 53, 59 | RD | CY |
| 56 | (none) | CB |

The note's velocity sets the hit's velocity.

### Appendix E: LFO sync values

FREE, 4 BARS, 2 BARS, 1 BAR, 1/2 dotted, 1/2, 1/4 dotted, 1/4, 1/4 triplet, 1/8 dotted, 1/8, 1/8 triplet, 1/16 dotted, 1/16, 1/16 triplet, 1/32.

### Appendix F: Delay times

1/32, 1/16 triplet, 1/16, 1/8 triplet, 1/16 dotted, 1/8, 1/4 triplet, 1/8 dotted, 1/4, 1/2 triplet, 1/4 dotted, 1/2, 1/2 dotted.

---

## 15. Notes to verify

These are things the source did not settle, or places where the source and the older documents disagree. They were left as stated above or described cautiously.

1. **User slot count.** The SAVE page's SLOT dial runs U1 to U20 (maximum 20 in `wt_ui.c`), but the number of slots that fit the flash objects is computed from the parameter count and the 3840-byte store size: with 206 parameters that is 9 patches per object, 18 in all (the SysEx HELLO also reports 18, and the editor lists 18). U19 and U20 on the dial would index past the two patch objects. This manual says to use U1 to U18. It looks like a bug worth fixing or capping in the firmware.
2. **`docs/WT-1-controls.md` is partly out of date** (it is generated by `tools/make_manual.py`). It says there are 16 drum patterns and drum patterns up to 32 steps; the code has 8 drum patterns (`WT_NDPAT`) of up to 64 steps. It lacks the DRUM ACCENT page and the ninth HOME shortcut key. Its SAVE WHAT list (PATCH / PATTERNS / BOTH / ERASE) predates the current PATCH / PATTERNS / DRUMS+SETUP / ALL / ERASE. A few of its generic knob descriptions are copied from other pages (for example "K2 LEVEL" on DRUM KIT). This manual follows the code.
3. **Tie steps.** The engine and editor support tie steps, and the older header comment in `wt_ui.c` says ARP held toggles a tie. In the current code ARP held with a step held makes K3 the chance, and nothing on the panel sets the tie flag. The panel way to make a long note is K4 length.
4. **SEL tap does not open MOD pages.** An old comment in `wt_ui.c` says it does; the code excludes SEL from page opening. MOD 1 to 8 are reached with the LFO button (after LFO 2 GOES TO), SELECT, or hold-LFO + key 5.
5. **M-UPGRADE.** Nothing in the repository documents M-UPGRADE. The identity of `out/wt1.fwsc` is `FM-1_900`; `out/FM-1.fwsc` in the same folder has the identity `FM-1_016` and is not the stock V15 file (its SHA-256 differs from the stock one that `fm1_rescue.py` expects), and its contents were not checked. Whether M-UPGRADE accepts `FM-1_900` directly, or needs `rename_pkg.py` to another identity string, was not verified. The installer `tools/fm1_install.py` checks the model, not the version.
6. **The TRS jack.** The code puts bytes from the TRS MIDI input into the same queue as USB MIDI (cable 1), so notes and controllers from the jack should play the synth and drums according to the MIDI channel settings, and its clock is selectable on the SYNC page. Not tested on the unit. Whether MIDI OUT also appears on the TRS jack was not checked; the manual says USB.
7. **Drums and the second core** were not confirmed running on the real FM-1 at the time of the last project notes (2026-10-24). The latest hardware testing may have changed that.
8. **Boot time and what safe mode does.** The first screen shows "BUILDING WAVETABLES..."; how long this takes on the device was not measured here. The code enters safe mode after two failed boots but what safe mode disables was not read.
9. **Glide in POLY mode.** By code reading, GLIDE only acts in MONO and LEGATO; a poly note always starts at its own pitch.
10. **Filter cutoff display.** The CUTOFF dial shows its 0 to 1350 value divided by 10 with a % sign (100% at the default, up to 135%), which is how `wui_value` formats any parameter with a maximum of 1000 or more. The Hz figures come from the editor's readout formula, not from a measurement.
11. **SUB 2 COARSE** is in cents (the engine parameter is `SUB_CRS`), although the older documents call it semitones.
12. **Detune amount.** "About one semitone at 100%" is derived from the detune formula for the outer unison voices, not measured.
13. **Envelope and LFO time ranges** (0.5 ms to about 10 s; 0.05 Hz to about 40 Hz) are from the editor's readout formulas.
14. **PATCH loading keeps the drum kit choice from the patch.** `DR_KIT`, `DR_LEVEL`, `DR_SEQ` and `DR_ACCENT` are patch parameters (not in the keep-list), so loading a patch can switch the kit and the drum level.
15. **Editor keyboard selector** has a second entry for another keyboard model; this manual ignores it.
16. **Hardware facts** (240 x 240 screen, 27 keys F3 to G5, fixed key velocity 100) come from the firmware; the schematic in `docs/panel.png` shows the control layout but is "not to scale".
