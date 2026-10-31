# WT-2 for the M-Vave FM-1: User Manual

WT-2 is the drumless build of the WT-1 firmware for the M-Vave FM-1 keyboard. It keeps the whole synth (two wavetable oscillators, sub, noise, filter, three envelopes, two LFOs, matrix, effects, step sequencer, arpeggiator) and uses the room the drums freed for what a Serum-style synth needs: **buttons that take you straight to oscillator A, oscillator B, the filter, the LFOs, the envelopes and the mods; an LFO shape editor where you draw your own shapes with points and curves; a vowel (formant) filter; and a chorus.** It was written from the firmware source (`firmware/src/app/wt_ui.c`, `core/`, `tests/ui_host.c`, and the editor source), so every page and range below is what the code does. What could not be confirmed on a real keyboard is listed in "Notes to verify" at the end.

This manual covers WT-2 only. The sound-design chapters of the [FM-1 manual](WT-1-FM1-manual.md) (oscillators, FM, sequencer, MIDI and clock setup) still apply; warp, unison, the matrix and the output level are different here, and this manual describes those differences.

Contents

1. What WT-2 is and how it differs from WT-1
2. Installing, recovering, and the WT-2 editor
3. Front panel: knobs, buttons, keys
4. Moving between pages
5. Every page
6. The LFO shape editor
7. Sound design with WT-2 (vowel filter, chorus, hyper, chords, voice modes)
8. Saving and loading
9. The WT-2 editor (browser)
10. Troubleshooting
11. Appendices
12. Notes to verify

---

## 1. What WT-2 is and how it differs from WT-1

### 1.1 What is the same

- Two wavetable oscillators with up to 5 unison voices each, wavetable position, table-to-table morph, warp (bend, sync, mirror), FM and phase distortion between A and B; a sub oscillator; a noise oscillator with a sample layer.
- A multimode filter, three envelopes, two LFOs with tempo sync, four macros, an 8-slot modulation matrix.
- Synth distortion, delay (digital or tape), reverb, 3-band EQ.
- The step sequencer (16 patterns of up to 64 steps, accent, slide, long notes, chance, swing, direction, transpose, bar looping, generator), the arpeggiator, scale mode, MIDI, clock sync, 11 factory patches and user patch slots, USB patch transfer.

### 1.2 What is gone

- **The drums**: no drum machine, no DRUMS, MIXER, DRUM KIT or SOUND pages, no drum FX.
- **Sidechain**: it only ducked the synth under the bass drum.
- **MASTER FX** (master distortion and the master FX output level): the page is removed. Master distortion stays off. The one-knob compressor is on the FX **COMPRESS** page (section 5.7). Use SYNTH FX for distortion and the LEVELS page for the output level.
- **SEL as a modulation mode**: SEL is now the OSC A button (section 3.2). Assign modulation with the "GOES TO" pages and the MOD pages (section 5.5).

### 1.3 What is new

| New | Where |
|---|---|
| One button per place: SEL = oscillator A, EDIT = oscillator B, HOME = filter, sub and noise, LFO (double tap = MOD), ENV, ARP, FX, SEQ, GLOBAL, SAVE | section 3.2 |
| **LFO shape editor**: 8 shapes you draw (up to 16 points each, with a curve on every segment), played by LFO 1 and LFO 2 | section 6 |
| **VOWEL filter** (formant filter A E I O U) | section 7.1 |
| **CHORUS**, and **HYPER** (a cheap stereo widener on the same buffer) | section 7.2 |
| **COMPRESS** (the existing one-knob compressor, given its own page) | section 5.7 |
| Warp modes **ASYM**, **QUANT** and **PWM** | section 5.2 |
| **UNISON** page: unison on the top note only | section 5.9 |
| More matrix destinations (envelope times, LFO rates, chorus mix, delay and reverb sends, noise and sub level, master) | section 5.5 |
| Chord headroom: voices sum about 1/sqrt(how many are sounding), with a gain-riding limiter | section 7.5 |
| **POLY 2 to POLY 7** voice modes | section 7.4 |
| The **WT-2 editor**: draw the same shapes in the browser | section 9 |

### 1.4 Limits worth knowing

- Three factory wavetables on the device (BASIC SHP, FOLD-SINE, ODD-EVEN) and three noise sample loops. The editor can audition your own wavetables and samples, but the device protocol sends only patch settings.
- The 8 drawn shapes are **global**: one bank shared by every patch, not stored inside a patch. A patch that uses a drawn shape (LFO SHAPE 8 to 15) plays whatever shapes the keyboard holds.
- Drawn shapes are **not sent over USB** by the editor: draw them on the keyboard, or draw them in the editor to design and audition them (section 9).
- The 96 KB of ordinary RAM had about 7.5 KB free before the drum engines were taken out of this build. Dropping them, and moving the reverb lines and two scratch buffers into the pool the drums used to occupy, leaves on the order of 30 KB free. Drawn shapes still live in the setup store, not in the patch: the patch is still exactly 206 parameters. A new firmware link has not re-measured the linker map (the device toolchain was not available for that build); the figure is the previous map plus the sizes of the buffers that moved.

---

## 2. Installing, recovering, and the WT-2 editor

### 2.1 Files

| File | What it is |
|---|---|
| `wt2f.fwsc` | The WT-2 firmware package for the FM-1 (package identity `FM-1_922`). The screen and the USB MIDI port say WT-2. |
| WT-2 editor | The browser editor, at https://fm1.thatsoundshot.com (coming soon). It is not part of this repository. |
| `VARIANT=wt2 ./build-fw.sh` | Builds the firmware into `build/wt2.fwsc`, identity `FM-1_922` (needs Docker or OrbStack). |
| `tools/rename_pkg.py` | Gives a package another identity so M-UPGRADE accepts it. |

### 2.2 Installing with M-UPGRADE

M-UPGRADE only installs a package whose identity differs from the one in the keyboard. If you have just installed a build and want to install another, give the new package a new number:

```
python3 tools/rename_pkg.py build/wt2.fwsc wt2f.fwsc "FM-1_923"
```

Then open M-UPGRADE, choose the package and install, exactly as for WT-1 (FM-1 manual section 2.2). At power-on the screen says **WT-2** (the crash screen and safe mode use the same name). Over USB the MIDI port is WT-2, and the updater's identity request is answered `FM-1_922`, which is also the identity written into `wt2f.fwsc`. Power-on shortcuts, the crash-recovery path and going back to stock firmware are the same as WT-1 (FM-1 manual sections 2.4 and 2.5). Saved patches stay in the same format; the drum patterns of a WT-1 save are ignored.

---

## 3. Front panel: knobs, buttons, keys

### 3.1 Knobs

| Control | What it does |
|---|---|
| **K1 to K4** | Edit the four values shown on the page, left to right, top to bottom. On the shape editor pages they pick the shape, the point and edit it (section 6). |
| **SELECT** | Steps through every page in order (63 pages). |
| **ALGORITHM** | Loads the next / previous patch (factory first, then your saved patches). It no longer picks a modulation source: SEL is not a mode in WT-2. |
| **PRESETS** | Changes the wavetable of the oscillator whose page you are on (OSC B pages change B, the others change A). |
| **MASTER** | The analog-style output volume. |

### 3.2 Buttons

Page buttons: press once for the button's first page, press again for the next page, and so on around. The button of the page you are on is lit.

| Button | First press | Press again for | Notes |
|---|---|---|---|
| **SEL** | OSC A | A WARP, A TABLE, A TUNE | SEL is "oscillator A". It is not a mode. |
| **EDIT** | OSC B | B WARP, B TABLE, B TUNE | |
| **HOME** | FILTER | FILTER 2, SUB, SUB 2, NOISE, NOISE 2 | The sound's tone section. |
| **ENV** | AMP ENV | AMP ENV 2, ENV 2, ENV 3, ENV 2 GOES TO, ENV 3 GOES TO | On STEPS with a step held: toggles accent on that step. |
| **LFO** | LFO 1 | LFO 1 GOES TO, LFO 2, LFO 2 GOES TO, SHAPE DRAW, SHAPE CURVE | **Double tap** (two presses within about a third of a second): the MOD pages (MOD 1). On a MOD page a single press steps MOD 1 to MOD 8 and round again; a double tap there returns to LFO 1. The LFO pages and the MOD pages are two separate rings. On STEPS with a step held: toggles slide on that step. |
| **FX** | SYNTH FX | CHORUS, COMPRESS, DELAY, DELAY 2, TAPE, REVERB, REVERB 2, EQ, EQ 2 | |
| **SEQ** | STEPS | PATTERN, GENERATE, PATTERN LENGTH, REMOVE BARS, PATTERN 2, PLAY SETUP | |
| **ARP** | ARP | ARP 2 | **ARP is the arpeggiator**, and it is lit while the arpeggiator runs. On STEPS with a step held: with ARP held, K3 sets the step's chance. |
| **GLOBAL (GLO)** | PERFORM | VOICE, UNISON, SCALE, MIDI, SYNC, PERFORMANCE, LEVELS, ABOUT | PERFORM is the four macros. UNISON is "top note only". ABOUT is read-only. |
| **SAVE** | SAVE | (press again to arm, again to store) | On STEPS with a step held: clears that step. |
| **PLAY** | Starts / stops the sequencer. Double-tap = panic. | | |
| **REC** | Turns recording on / off. | | |
| **OCT -, OCT +** | Octave down / up. On STEPS they page the step grid. | | |

Hold a page button and the first white keys light: each lit key jumps straight to a page of that button (section 4.2).

### 3.3 Keys

27 keys, F3 to G5 (MIDI 53 to 79 at octave 0). They play notes on every page, with these exceptions:

| Page | White keys | Black keys |
|---|---|---|
| STEPS | The 16 steps of the shown bar | Select pattern 1 to 11 |
| SHAPE DRAW, SHAPE CURVE | Play notes (so you hear the shape while you edit) | **The nine edit keys** (section 6.2) |
| Any page, with a page button held | Shortcut keys 1 to 9 | Play notes |

Key velocity on the FM-1 is fixed at 100.

---

## 4. Moving between pages

### 4.1 Ways to move

- Press a page button (first page; again for the next, wrapping).
- Turn SELECT through all 63 pages.
- Hold a page button and press a white key (below).
- Double tap LFO for the MOD pages.

### 4.2 Hold a page button and press a white key

Hold a page button: a list of its shortcut pages appears after a moment and the first white keys light. Press a lit white key to jump; let go and the keys play again.

| Hold | Key 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 |
|---|---|---|---|---|---|---|---|---|---|
| **SEL** | OSC A | A WARP | A TABLE | A TUNE | | | | | |
| **EDIT** | OSC B | B WARP | B TABLE | B TUNE | | | | | |
| **HOME** | FILTER | FILTER 2 | SUB | SUB 2 | NOISE | NOISE 2 | | | |
| **ENV** | AMP ENV | ENV 2 | ENV 3 | ENV 2 GOES TO | | | | | |
| **LFO** | LFO 1 | LFO 1 GOES TO | LFO 2 | LFO 2 GOES TO | SHAPE DRAW | SHAPE CURVE | MOD 1 | MOD 2 | MOD 3 |
| **FX** | SYNTH FX | CHORUS | COMPRESS | DELAY | REVERB | EQ | | | |
| **SEQ** | STEPS | PATTERN | PATTERN LENGTH | REMOVE BARS | GENERATE | PLAY SETUP | | | |
| **ARP** | ARP | ARP 2 | | | | | | | |
| **GLOBAL** | PERFORM | VOICE | UNISON | LEVELS | SCALE | MIDI | SYNC | PERFORMANCE | ABOUT |

Key 1 is the leftmost white key (F3), key 2 the next (G3), and so on. SAVE has no shortcut list.

---

## 5. Every page

Values are written as the screen shows them. Percent values are stored 0 to 1000 and shown 0 to 100%. A slow turn of a dial changes the value by one step per click, a fast turn moves twice as fast, and wide ranges move by 1% of the span per click.

### 5.1 The page table

Each row is a page, the button that reaches it and what K1 to K4 control. "(special page)" pages draw their own screens and are described below the table.

| Page | Button | K1 | K2 | K3 | K4 |
|---|---|---|---|---|---|
| PERFORM | GLOBAL | MACRO 1 | MACRO 2 | MACRO 3 | MACRO 4 |
| OSC A | SEL | WT POS | DETUNE | UNISON | LEVEL |
| A WARP | SEL | WARP | AMOUNT | FM FROM B | PD FROM B |
| A TABLE | SEL | TABLE | MORPH TO | MORPH | PHASE |
| A TUNE | SEL | OCTAVE | SEMITONE | FINE | PAN |
| FILTER | HOME | TYPE | CUTOFF | RESONANCE | DRIVE |
| FILTER 2 | HOME | KEY TRACK | MIX |  |  |
| OSC B | EDIT | OSC B | WT POS | UNISON | LEVEL |
| B WARP | EDIT | WARP | AMOUNT | FM FROM A | PD FROM A |
| B TABLE | EDIT | TABLE | MORPH TO | MORPH | DETUNE |
| B TUNE | EDIT | OCTAVE | SEMITONE | FINE | PAN |
| SUB | HOME | ON | WAVE | PULSE W | LEVEL |
| SUB 2 | HOME | OCTAVE | COARSE | PAN |  |
| NOISE | HOME | ON | COLOUR | LEVEL | STEREO |
| NOISE 2 | HOME | FILTER | PAN | SAMPLE | SMP LEVEL |
| AMP ENV | ENV | ATTACK | DECAY | SUSTAIN | RELEASE |
| AMP ENV 2 | ENV | DELAY | HOLD |  |  |
| ENV 2 | ENV | ATTACK | DECAY | SUSTAIN | RELEASE |
| ENV 3 | ENV | ATTACK | DECAY | SUSTAIN | RELEASE |
| ENV 2 GOES TO | ENV | GOES TO | AMOUNT |  |  |
| ENV 3 GOES TO | ENV | GOES TO | AMOUNT |  |  |
| LFO 1 | LFO | SHAPE | RATE | SYNC TO TEMPO | RESTART |
| LFO 1 GOES TO | LFO | GOES TO | AMOUNT | POLARITY |  |
| LFO 2 | LFO | SHAPE | RATE | SYNC TO TEMPO | RESTART |
| LFO 2 GOES TO | LFO | GOES TO | AMOUNT | POLARITY |  |
| SHAPE DRAW | LFO | SHAPE | POINT | HEIGHT | TIME |
| SHAPE CURVE | LFO | SHAPE | POINT | CURVE |  |
| MOD 1 | LFO (double tap) | SOURCE | DEST | AMOUNT |  |
| MOD 2 | LFO (double tap) | SOURCE | DEST | AMOUNT |  |
| MOD 3 | LFO (double tap) | SOURCE | DEST | AMOUNT |  |
| MOD 4 | LFO (double tap) | SOURCE | DEST | AMOUNT |  |
| MOD 5 | LFO (double tap) | SOURCE | DEST | AMOUNT |  |
| MOD 6 | LFO (double tap) | SOURCE | DEST | AMOUNT |  |
| MOD 7 | LFO (double tap) | SOURCE | DEST | AMOUNT |  |
| MOD 8 | LFO (double tap) | SOURCE | DEST | AMOUNT |  |
| STEPS | SEQ | (special page) |  |  |  |
| PATTERN | SEQ | LENGTH | RATE | SWING | DIRECTION |
| GENERATE | SEQ | DENSITY | RANGE | SPICE | ROLL |
| PATTERN LENGTH | SEQ | DOUBLE | HALVE | LOOP BAR |  |
| REMOVE BARS | SEQ | FROM BAR | HOW MANY | REMOVE |  |
| PATTERN 2 | SEQ | PATTERN | TRANSPOSE | CLEAR |  |
| PLAY SETUP | SEQ | TEMPO | ACCENT | SLIDE TIME |  |
| SAVE | SAVE | SLOT | WHAT | PLACE | LETTER |
| SYNTH FX | FX | DISTORT | DRIVE | DELAY SEND | REVERB SEND |
| CHORUS | FX | MIX | RATE | DEPTH | HYPER |
| COMPRESS | FX | AMOUNT |  |  |  |
| DELAY | FX | SYNTH SEND | TIME | FEEDBACK | TEMPO |
| DELAY 2 | FX | TONE | RETURN | HI-PASS | TYPE |
| TAPE | FX | WEAR | PING-PONG |  |  |
| REVERB | FX | SYNTH SEND | DECAY | TONE | RETURN |
| REVERB 2 | FX | HI-PASS |  |  |  |
| EQ | FX | LOW | MID | HIGH | EQ |
| EQ 2 | FX | LOW FREQ | MID FREQ | MID Q | HIGH FREQ |
| ARP | ARP | ARP | MODE | RATE | OCTAVES |
| ARP 2 | ARP | GATE | SWING | LATCH |  |
| VOICE | GLOBAL | MODE | GLIDE | BEND RANGE | TUNE |
| UNISON | GLOBAL | TOP NOTE ONLY |  |  |  |
| SCALE | GLOBAL | SCALE | ROOT | HOLD SEQ TO IT |  |
| MIDI | GLOBAL | IN CHANNEL | OUT CHANNEL | USB AUDIO | |
| SYNC | GLOBAL | CLOCK FROM | START / STOP |  |  |
| PERFORMANCE | GLOBAL | (special page) |  |  |  |
| LEVELS | GLOBAL | VELOCITY | MASTER | CPU MODE | 2ND CORE |
| ABOUT | GLOBAL | (read only) |  |  |  |

### 5.2 Oscillators (SEL = A, EDIT = B)

Oscillator pages show that oscillator's waveform at its current wavetable position (it moves with modulation) above four small dials.

- **WT POS**: position in the wavetable, morphing smoothly between frames. **DETUNE**: unison spread. **UNISON**: voices per note (1 to 5; more is more CPU). **LEVEL**: output level (can be 0 and still drive FM / PD of the other oscillator).
- **WARP**: OFF, BEND, SYNC, MIRROR, **ASYM** (the bend sits on the first half of the cycle), **QUANT** (the phase snaps to 2..16 steps) and **PWM** (a window that narrows, then stretches to fill the cycle). **AMOUNT** how far. **FM FROM B / A** and **PD FROM B / A**: frequency / phase modulation by the other oscillator.
- **TABLE**, **MORPH TO**, **MORPH**: the table, a second table, how far the cross-fade has gone. **PHASE** (A): start phase.
- **OCTAVE, SEMITONE, FINE, PAN**.
- **OSC B** (K1 of OSC B) switches B on or off; it shows OFF in red when off.

### 5.3 Filter, sub, noise (HOME)

- **FILTER**: TYPE (OFF, LP12, LP24, HP12, BP12, NOTCH, **VOWEL**), CUTOFF, RESONANCE, DRIVE. The default patch has the filter OFF: choose a type to hear it. **FILTER 2**: KEY TRACK, MIX (dry / filtered).
- **SUB**: ON, WAVE (SINE, TRIANGLE, SQUARE, PULSE), PULSE W, LEVEL. **SUB 2**: OCTAVE, COARSE, PAN.
- **NOISE**: ON, COLOUR (WHITE, PINK, BROWN, GEIGER, OFF), LEVEL, STEREO. **NOISE 2**: FILTER, PAN, SAMPLE (CRACKLE, AIR, RUMBLE), SMP LEVEL.

With TYPE on VOWEL the CUTOFF dial shows a vowel position such as `A>E 40%` (section 7.1).

### 5.4 Envelopes (ENV)

AMP ENV (loudness), ENV 2 and ENV 3 (modulation sources): attack, decay, sustain, release; AMP ENV 2 has DELAY and HOLD. **ENV 2 GOES TO / ENV 3 GOES TO**: K1 picks the destination, K2 the amount (plus or minus).

### 5.5 LFOs and modulation (LFO)

- **LFO 1 / LFO 2**: SHAPE, RATE, SYNC TO TEMPO, RESTART. SHAPE has 15 values: the six classic shapes (SINE, TRIANGLE, SAW UP, SQUARE, S&H, SMOOTH), SAW DOWN, then the **8 drawn shapes** (SHARK FIN, FALL, STAIRS, PULSE 25, SKEW TRI, TWIN HUMP, BOUNCE, BLANK are the factory drawings; your edits keep the slot but the name stays).
- **LFO 1 GOES TO / LFO 2 GOES TO**: GOES TO (destination), AMOUNT (plus or minus), POLARITY (UP AND DOWN, UP ONLY, DOWN ONLY).
- **MOD 1 to MOD 8** (double tap LFO): the 8 matrix slots as a list: SOURCE, DEST, AMOUNT. The GOES TO pages use the same slots (each takes the first slot that already has its source, or the first free one).

Sources: LFO1, LFO2, ENV2, ENV3, VEL, NOTE, MAC1 to MAC4, RAND, MOD (the MIDI mod wheel), ACCENT. Destinations (31, the original 20 and then these, so older patches keep their destination numbers): A_WTPOS, B_WTPOS, A_WARP, B_WARP, A_DETUNE, B_DETUNE, A_LEVEL, B_LEVEL, A_PAN, B_PAN, A_PITCH, B_PITCH, A_FM, B_FM, A_MORPH, B_MORPH, CUTOFF, RESO, DRIVE, SUB_PW, then **SUB_LVL, NOI_LVL, AMP_ATK, AMP_DEC, AMP_REL, LFO1_RATE, LFO2_RATE, CH_MIX, DL_SEND, RV_SEND, MASTER**.

**Using a drawn shape**: set an LFO's SHAPE to a drawn shape (or press LFO 1 / LFO 2 on the shape editor, section 6.2), then on LFO 1 GOES TO choose CUTOFF (or any destination) and set AMOUNT. With the vowel filter, CUTOFF is the vowel, so a drawn shape can sweep through A E I O U. The same LFO can now move the amp envelope's attack, decay or release, an LFO rate (when that LFO is free-running, not tempo-synced), the chorus mix, the delay or reverb send, the sub or noise level, or the master level.

**What a drawn shape still cannot reach**: chorus rate and depth, delay time and the reverb's own knobs, the filter type, the arp and the sequencer. LFO rate modulation uses the previous block's value (about 16 samples behind). Sends and the master level use the newest note's modulation, and they catch up a fraction of a second later than the note itself. This routing is not yet verified on a real FM-1.

### 5.6 Sequencer (SEQ)

STEPS (16 steps per bar page, pattern chooser on the black keys), PATTERN (length, rate, swing, direction), GENERATE (density, range, spice, roll), PATTERN LENGTH (double, halve, loop bar), REMOVE BARS, PATTERN 2 (pattern, transpose, clear), PLAY SETUP (tempo, accent, slide time). Editing steps, accent, slide, long notes, chance and recording work exactly as in the FM-1 manual, chapter 8. There is no drum sequencer.

### 5.7 Effects (FX)

- **SYNTH FX**: DISTORT (OFF, DIODE, CLIP, SAT, BFZ, PDIST, FOLD, CRUSH), DRIVE, DELAY SEND, REVERB SEND. On a chord the drive eases off as more notes sound, so a stack does not distort as hard as one note.
- **CHORUS**: MIX, RATE (0.1 to 4.0 Hz), DEPTH, **HYPER** (section 7.2).
- **COMPRESS**: AMOUNT, the one-knob glue compressor that was already in the effects (0 is bypass). It had no page after MASTER FX was removed. Patches that already stored a non-zero FX_COMP were already compressing.
- **DELAY** (SYNTH SEND, TIME, FEEDBACK, TEMPO), **DELAY 2** (TONE, RETURN, HI-PASS, TYPE digital / tape), **TAPE** (WEAR, PING-PONG), **REVERB** (SYNTH SEND, DECAY, TONE, RETURN), **REVERB 2** (HI-PASS), **EQ** (LOW, MID, HIGH gains, EQ on), **EQ 2** (LOW FREQ, MID FREQ, MID Q, HIGH FREQ).

Signal order: voices, a level trim of about 1/sqrt(sounding notes), 6 dB of headroom, chorus and HYPER, the synth level, synth distortion, then delay / reverb / compressor and the EQ, then the 6 dB back and the gain rider (section 7.5).

### 5.8 ARP

**ARP** page: ARP on / off, MODE (UP, DOWN, UP-DOWN, DOWN-UP, RANDOM, PLAYED, CHORD), RATE, OCTAVES. **ARP 2**: GATE, SWING, LATCH. The ARP button is lit while it is on. Hold a chord and the notes play one at a time with the tempo.

### 5.9 GLOBAL

- **PERFORM**: MACRO 1 to MACRO 4, free knobs you can aim with the matrix (sources MAC1 to MAC4).
- **VOICE**: MODE (POLY, MONO, LEGATO, POLY 2 to POLY 7), GLIDE, BEND RANGE, TUNE.
- **UNISON**: TOP NOTE ONLY (OFF / ON). Off, every note gets the unison you set, until the CPU governor has to take some away. On, only the highest note held keeps the full unison; the others play one voice each. This reuses the parameter WT-1 called the drum kit, so a WT-1 patch saved with the 808 kit (DR_KIT = 1) opens here with TOP NOTE ONLY on.
- **SCALE**: SCALE (OFF and 15 scales), ROOT, HOLD SEQ TO IT.
- **MIDI**: IN CHANNEL (ALL, or one channel 1 to 16), OUT CHANNEL, USB AUDIO. There is no drum channel. Notes on every channel the synth listens on play the synth, including channel 10.
- **SYNC**: CLOCK FROM (INTERNAL, MIDI IN, USB), START / STOP.
- **PERFORMANCE**: a read-only page of where the audio time goes (bars for VOICES, 2ND CORE, EFFECTS + EQ and CONTROL). 2ND CORE is the time this core waits while the other core renders voices. The CPU % and its peak, the voices sounding, whether the 2ND CORE is on and the CPU MODE are along the bottom.
- **LEVELS**: VELOCITY, MASTER, CPU MODE (FULL, AUTO, SAVE), 2ND CORE.
- **ABOUT** (the last GLOBAL page): read only. It shows "WT-2 by Dedrich", "Web Editor at thatsoundshot.com", the firmware version, and the package identity. A normal build says version `dev` and identity `FM-1_922`. A `--release` build shows that release's version string and its `FM-1_9…` identity. The knobs do nothing here.

### 5.10 SAVE

Dials: SLOT (U1 to U20 on the dial; U1 to U18 exist), WHAT (PATCH, PATTERNS, **SETUP+SHAPES**, ALL, ERASE), PLACE and LETTER (the 11-character name).

| WHAT | Stores |
|---|---|
| PATCH | The current sound under the typed name, into the slot. |
| PATTERNS | All 16 synth sequencer patterns. |
| **SETUP+SHAPES** | The setup (MIDI channels, USB audio, scale, root, hold-sequence, clock source and start / stop) **and the 8 drawn LFO shapes**. |
| ALL | Patch, patterns, setup and shapes. |
| ERASE | Empties the slot. |

Press **SAVE** to arm ("PRESS SAVE AGAIN"; it disarms after 5 s or when you turn a knob), press it again to store. The result shows SAVED, ERASED or FAILED. Saved patterns, setup and shapes are loaded at power-on.

---

## 6. The LFO shape editor

WT-2 has 8 drawn LFO shapes. Each shape is a line through up to **16 points** (at least 2). Every point has a **time** (how far along the cycle), a **height** (-127 to +127, the bottom to the top of the LFO) and the **curve** of the segment that arrives at it. You draw a shark fin by putting a point at the bottom, one near the end at the top and one at the end at the bottom, then bending the climb. LFO 1 or LFO 2 plays the shape at its own RATE (free or synced), like any other LFO shape.

### 6.1 The two pages (LFO button: press until SHAPE DRAW or SHAPE CURVE, or hold LFO and press white key 5 or 6)

| Page | K1 | K2 | K3 | K4 |
|---|---|---|---|---|
| **SHAPE DRAW** | SHAPE (USER 1 to 8) | POINT (which point) | HEIGHT of the point | TIME of the point |
| **SHAPE CURVE** | SHAPE | POINT | CURVE of the segment that arrives at the point | |

The screen draws the shape and its points (the selected point is highlighted; the segment you are bending is amber), the numbers, and a reminder of the keys. An `L1` or `L2` tag shows when LFO 1 or LFO 2 is using the shape.

- The first point sits at time 0 and the last at the end: change their height, not their time. K4 moves the points in between, never past their neighbours.
- **Curve belongs to the segment that arrives at the point**, so point 1 has none: on SHAPE CURVE pick **point 2 or more** with K2 (pressing a curve edit while point 1 is selected moves the selection to point 2).
- A positive curve gets the segment to its end height early (a plucked, fast-then-slow line); a negative curve holds back and rises late. A curve of 0 is a straight line. The range is -100 to +100; K3 moves it by 8 per click.

### 6.2 The nine edit keys

On both pages the black keys are edit keys, counted **from the left** (F#3 is the first):

| Key | Does |
|---|---|
| 1 ADD | Adds a point after the selected one, half way to the next. |
| 2 DEL | Deletes the selected point (not the first or last). |
| 3 RESET | Puts the factory drawing back in this slot. |
| 4 FLIP | Turns the shape upside down. |
| 5 FLAT | Sets every curve to 0 (all straight lines). |
| 6 BEND+ | Bends the segment that arrives at the selected point by +25. |
| 7 BEND- | The same by -25. |
| 8 LFO 1 | Makes **LFO 1** play this shape (LFO 1 SHAPE = the shape; a toast confirms). |
| 9 LFO 2 | The same for **LFO 2**. |

The white keys play, so you hear the change while you press a note. A change takes effect at once.

### 6.3 The factory shapes

| # | Name | What it is |
|---|---|---|
| 1 | SHARK FIN | A curved climb, then a drop |
| 2 | FALL | A plucked decay (high to low, curved) |
| 3 | STAIRS | Four steps up |
| 4 | PULSE 25 | High for a quarter, then low |
| 5 | SKEW TRI | A triangle that rises fast and falls slowly |
| 6 | TWIN HUMP | Two humps, the second smaller |
| 7 | BOUNCE | A high start that falls and bounces back twice, curved |
| 8 | BLANK | A flat line at 0 (to start from) |

### 6.4 Where the shapes are kept

The 8 shapes are one bank for the whole keyboard. They are kept in RAM while it is on, and stored in flash with **SAVE, WHAT = SETUP+SHAPES (or ALL)**. They are not part of a patch: if you change shape 1, every patch that uses it changes with it. Power off without saving and you get the last saved bank (or the factory shapes if you never saved).

---

## 7. Sound design with WT-2

### 7.1 The vowel filter

FILTER TYPE = **VOWEL**: two band-pass filters tuned to the first two formants of a vowel (for A they sit near 770 Hz and 2310 Hz), mixed to a single voice. CUTOFF becomes the vowel: it moves from A through E, I and O to U across its range (0 to 135%), and the screen says `A>E 40%` for 40% of the way from A to E. RESONANCE sharpens the vowel, DRIVE and MIX work as usual, KEY TRACK has no effect.

Recipes:

- A talking pad: both oscillators on a saw table, TYPE VOWEL, RESONANCE 40 to 60%, LFO 1 (a drawn shape or TRIANGLE, slow) GOES TO CUTOFF at 40%: the vowel drifts.
- Sweep a vowel with an envelope: ENV 2 GOES TO CUTOFF.
- Use MIX below 100% if the vowel is too extreme.

### 7.2 Chorus

FX, CHORUS page. **MIX** (0 = off, no CPU cost), **RATE** (0.1 to 4.0 Hz, how fast the delay is swept), **DEPTH** (how far it is swept). It is a short delay (about 6 ms) swept by a triangle, with the right side a quarter cycle behind the left, mixed in with the dry signal. Try MIX 40 to 50%, RATE 0.5 Hz, DEPTH 50% on pads and leads. It sits before the synth distortion, the delay and the reverb.

**HYPER** (K4, 0 off, 1 to 7) is a cheaper width than stacking unison. It cross-mixes two short taps of the chorus delay (about 4 ms and 7 ms, drifting a little). At 0 it does nothing and, if the chorus mix is also 0, it does not touch the buffer. It reuses the parameter WT-1 called the drum distortion, so a WT-1 patch with drum distortion above 0 opens here with HYPER on and will sound wider.

### 7.3 Drawn LFO shapes in practice

- **Rhythmic cutoff**: draw STAIRS or a pulse on LFO 1, set RATE to a tempo sync (SYNC TO TEMPO 1/8), LFO 1 GOES TO CUTOFF: a stepped, repeating filter.
- **A plucked sweep**: FALL on LFO 1, RESTART "AT EVERY NOTE", RATE slow: each note's filter falls the same way.
- **A shark fin on wavetable position**: LFO 1 SHARK FIN, destination A_WTPOS: a slow climb and a fast reset.
- A shape's own polarity: POLARITY on the GOES TO page chooses UP AND DOWN (the full shape), UP ONLY or DOWN ONLY.

### 7.4 Voice modes

- **POLY**: 8 voices. **MONO**, **LEGATO**: as in the FM-1 manual.
- **POLY 2 to POLY 7**: polyphonic like POLY, but at most as many notes sound at once as the number (POLY 2 allows 2 notes, POLY 7 allows 7): the oldest is cut when you play one more. Use them to keep the CPU in time on heavy sounds (both oscillators with 5 unison voices and a 4-note chord).

### 7.5 Chords, headroom and the limiter

A chord used to add the voices at full level and then run into the waveshaper and the limiter, so four notes could grit and pump where one note was clean. WT-2 does four things about that. A single note is left at full level. Existing patches can sound a little different where this gain changes; a quiet single note moves by about one bit.

1. The voice sum is scaled by about 1/sqrt(how many notes are sounding). Two notes sit near 0.7, four notes near half. The change is smoothed over the block so a new note does not click.
2. Each voice is turned down 6 dB before the chorus, the distortion and the effects, and turned back up after the EQ. The chorus delay and the effect delay were the places a hot chord clipped inside an integer buffer. The quieter write clips less. The turn-down and turn-up drop the bottom bit, and the delay's own rounding is about 6 dB more obvious after the makeup.
3. Filter drive is per voice, and both it and the synth-distortion drive ease off as the chord grows. With more than one note, each voice is also softly clipped before it joins the sum.
4. The old fixed limiter (a curve that started working at about -4 dB) is a gain rider. It leaves anything under about -2 dB alone. When a peak crosses that, the gain comes down in a few samples and climbs back over about 50 ms. The last step is still a hard clip at full scale, so a transient cannot wrap.

### 7.6 The CPU

The PERFORMANCE page shows the CPU. CPU MODE on LEVELS: FULL (no shedding), AUTO (sheds unison when the CPU runs out; the header shows LIMIT), SAVE (always lean). When it sheds, the oldest notes keep their unison and a new note loses unison voices first. A note is not dropped to save CPU (POLY 2 to POLY 7 still limit how many notes exist). The chorus, HYPER and the vowel filter cost a little; a heavy patch plus both is the thing to watch (note 4).

---

## 8. Saving and loading

Turn **ALGORITHM** to load patches (11 factory patches, then your saved ones). Loading sets every synth parameter; it does not touch the sequencer patterns, the setup, or the drawn shapes.

Save a sound on the SAVE page with WHAT = PATCH, press SAVE twice. To keep drawn shapes, save once with SETUP+SHAPES (or ALL). A patch that uses drawn shapes does not carry them: save the shapes alongside.

---

## 9. The WT-2 editor (browser)

The WT-2 editor is a separate editor for WT-2, not the WT-1 editor. It will be at https://fm1.thatsoundshot.com (coming soon); it is not part of this repository. Use Chrome or Edge (Web MIDI with SysEx). Click **Start audio**. The synth is the firmware's own core compiled to WebAssembly with the WT-2 features on, so the vowel filter, chorus and drawn shapes you hear are what the keyboard plays.

What is on the page: wavetables and noise samples; scope and spectrum; voice (including **unison on the top note only**) and macros; the device panel (USB); the sequencer; oscillators A and B (warp adds ASYM, QUANT and PWM); sub and noise; filter (with VOWEL); scale; synth effects; **chorus and HYPER**; the **compressor**; delay and reverb; three envelopes; **the LFO shape editor**; LFO 1 and 2; the modulation matrix (the same destinations as the keyboard, including the ones added for WT-2); a keyboard. There are no drum, sidechain, master-distortion or P37 panels. The WT-2 WebAssembly build does not contain the drum engines at all (the WT-1 editor's wasm still does).

### 9.1 The shape editor

- Click empty space on the graph to **add a point** (up to 16). **Drag a point** to move it (the first and last stay at the ends of the cycle; the others stay between their neighbours).
- **Drag the diamond** between two points up or down to **curve** that segment; **double-click** a point to delete it, or a diamond to straighten it.
- The blue line is the table the synth's LFO really reads. **Use in LFO 1 / LFO 2** sets the LFO to this shape. Buttons: **Load factory shape** (start the slot from any of the 8 drawings), **Flip up/down**, **Reverse**, **Straighten curves**, **Reset**, **snap to grid** (16 columns, 8 rows). **Save shapes… / Load shapes…** write and read all 8 shapes as a file (`wt2-shapes.json`).
- The editor remembers your shapes in the browser. A saved patch file (`wt2-patch`) carries the shapes; loading it restores them.
- A drawn shape in the editor is **not sent to the keyboard** over USB. To use an editor design on the keyboard, redraw it with the SHAPE DRAW / CURVE pages (the editor's graph shows each point's height and time to copy).

### 9.2 Patches and USB

Patch files are JSON (`wt2-patch`; the editor also opens `wt1-patch` files and ignores their drums). The device panel connects to the FM-1 over USB-C and saves, plays, reads, erases and backs up slots as in the FM-1 manual, chapter 12; the editor and firmware must come from the same build (the parameter count is 206).

---

## 10. Troubleshooting

| Problem | What to do |
|---|---|
| No sound from the filter | The default patch has the filter OFF: choose a TYPE. |
| SEL does not do what the old manual says | In WT-2 SEL is oscillator A. There is no assign mode: use LFO / ENV GOES TO or MOD pages. |
| I cannot find the drums, or a DRUM CHANNEL knob | WT-2 has none. Channel 10 plays the synth when IN CHANNEL is ALL. Install `wt1.fwsc` for drums. |
| Double tap on LFO shows LFO 2 | Tap faster (two presses within about a third of a second). |
| SHAPE CURVE does nothing | Pick point 2 or higher with K2: a curve belongs to the segment that arrives at a point. |
| My shape is gone after power-off | Save with SAVE, WHAT = SETUP+SHAPES (or ALL). |
| A patch sounds different on the keyboard than in the editor | The editor and keyboard shapes are different banks (section 9.1), or the keyboard still has the previous build. |
| Install fails in M-UPGRADE | The identity matches the installed one: run `tools/rename_pkg.py` with a new number. |
| Crackle or LIMIT in the header | Too many voices: use POLY 3 or POLY 4, lower UNISON, turn on TOP NOTE ONLY, or CPU MODE AUTO / SAVE. New notes lose unison before a note is dropped. |
| A chord is quieter than it was, or a loud patch pumps | Section 7.5. The sum is scaled by about 1/sqrt(notes), and the limiter rides instead of a fixed curve. A quiet single note should be essentially the same. |
| An old patch is wider, or only the top note is unison | HYPER and TOP NOTE ONLY reuse WT-1's drum-distortion and drum-kit parameters. Set HYPER to 0 and TOP NOTE ONLY to OFF. |

---

## 11. Appendices

### Appendix A: parameters that are new or changed in WT-2

| Parameter | Range | Default | Where |
|---|---|---|---|
| F_TYPE | 0 OFF, 1 LP12, 2 LP24, 3 HP12, 4 BP12, 5 NOTCH, 6 VOWEL | 0 | FILTER K1 |
| F_CUTOFF | 0 to 1350 (a vowel position when F_TYPE = 6) | 1000 | FILTER K2 |
| LFOn_SHAPE | 0 to 14: 0 to 6 classic, 7 to 14 drawn shapes 1 to 8 | 0 | LFO n K1 |
| VOICEMODE | 0 POLY, 1 MONO, 2 LEGATO, 3 to 8 POLY 2 to POLY 7 | 0 | VOICE K1 |
| CH_MIX | 0 to 1000 | 0 | CHORUS K1 |
| CH_RATE | 0 to 1000 (0.1 to 4.0 Hz) | 350 | CHORUS K2 |
| CH_DEPTH | 0 to 1000 | 500 | CHORUS K3 |
| A_WARPMODE, B_WARPMODE | 0 OFF, 1 BEND, 2 SYNC, 3 MIRROR, 4 ASYM, 5 QUANT, 6 PWM | 0 | A WARP / B WARP K1 |
| UNI_TOP | 0 off, 1 top note only (WT-1's DR_KIT slot; a stored 1 was the 808 kit) | 0 | UNISON K1 |
| HYPER | 0 off, 1 to 7 width (WT-1's DR_DIST slot; a stored non-zero was a drum-distortion type) | 0 | CHORUS K4 |
| FX_COMP | 0 to 127, 0 is bypass (already in the effects; now on the COMPRESS page) | 0 | COMPRESS K1 |

The chorus parameters take the places of three parameters that WT-1 used for the sidechain and the drum level, so the patch layout (206 parameters) is the same as WT-1's. In a patch file they appear as CH_MIX, CH_RATE and CH_DEPTH. Everything else is as in the FM-1 manual, Appendix A.

### Appendix B: the drawn shape format

Each shape: n (2 to 16 points), then for each point x (0 to 255 along the cycle), y (-127 to 127) and c (-100 to 100, the curve of the segment that ends at the point). The synth builds a 128-point table from them (linear interpolation between table points, three passes of a bend `t + c t (1 - t) / 100`), so the editor's blue line and the keyboard's LFO are the same. In flash the bank is a 396-byte `FLS1` block at the end of the setup store.

### Appendix C: factory patches

init, pad, bass, pluck, lead, sweep, fmbell, tmorph, reece1, space, crunch (F09 is reece1, imported from a wt2-patch; see factory/README.md).

---

## 12. Notes to verify

These are what the Mac tests and the code say but a real FM-1 has not yet confirmed. Tell me which of them is wrong.

1. **The button layout** (section 3.2) is tested on the Mac model of the UI (every button, the LFO double tap, the MOD ring) and not yet by hand.
2. **The shape editor on the keyboard**: the K1 to K4 behaviour, the black-key edit keys, and especially the curve tools. The earlier build's curve tools reportedly did not seem to work; the cause was not found, and four likely causes were changed (point 1 has no curve, the bend was weak, no visible feedback, unclear keys). If curves still do nothing, note which page, which point and what you pressed.
3. **Shapes surviving a power cycle** (SETUP+SHAPES) has not been run on hardware.
4. **The CPU** with a 4-note chord on a heavy patch (both oscillators with unison, vowel filter, chorus on) has not been measured on the keyboard.
5. **The vowel filter's level and sound** are checked on the Mac by spectrum (energy at the two formants), not by ear; the output is scaled up (x3) to match the other filters, so lower DRIVE or the patch level if it is too loud.
6. **The editor's USB path** with WT-2: patch transfer uses the same SysEx as WT-1 (206 parameters), but drawn shapes are not transferred.
7. **The RAM figure in section 1.4** (about 30 KB free) is the previous linker reading plus the sizes of the buffers that moved. It has not been read back from a new `wt2f.fwsc` link map. The chord-headroom work adds on the order of a few hundred bytes to the synth object (measured on the host), not another buffer.
8. **Chord level, the rider, HYPER, the new warp modes and the new modulation destinations** are checked by rendering on the Mac. They have not been heard on a keyboard, and this build was not flashed. The WT-2 WebAssembly was not rebuilt here (no emscripten), so the editor checks in `web/test_wasm2.mjs` are written but not executed.
