/* SPDX-License-Identifier: GPL-3.0-only */
/* WT-1 screens and controls. Included by wt_app.c on the device and by tests/ui_host.c on the Mac (which draws the same
 * screens into a picture and runs button / knob scripts under a memory checker), so it uses only: the synth and its tables
 * (synth, wtab), the command queue (wq_put, WC_*, wpatch, wpatch_pending), plat.h, and gfx.c's drawing calls.
 *
 * Pages: a button opens its first page and flips to the next one when pressed again; SELECT steps through all of them.
 * Each page shows up to four dials (K1..K4), big and readable. A dial's amber arc is the value; a blue ring outside it
 * shows how far modulation can move it, and a bright blue marker follows the modulated value live (as in Serum).
 *
 * SEQUENCER: SEQ opens STEPS (the 16 white keys are 16 steps: tap one to switch it on or off; hold one and turn K1 note, K2 velocity,
 * K3 gate, K4 chance; with a step held ENV = accent, LFO = slide, ARP = tie, SAVE = clear it; OCT -/+ page through up to 64 steps (with a step held:
 * its note an octave down/up); the black keys pick the pattern), PATTERN and PATTERN 2 (length, rate, swing, direction, transpose, pattern, clear) and
 * PLAY SETUP (tempo, accent, slide time). PLAY starts and stops; REC makes the keys a keyboard that writes steps (stopped: step by step; playing: live).
 *
 * SAVE: the SAVE button opens the SAVE page: K1 picks a user slot (U1..U20), K2 what to store (the patch, the 16 patterns, both, or erase the slot);
 * press SAVE again to do it. ALGORITHM steps through the factory patches and the saved ones. Stored in the flash store (plat_store_*).
 *
 * MODULATION BY HAND: hold SEL, choose the source with ALGORITHM, and turn the knob of the parameter you want it on
 * (on any page): the dial then shows that source's amount on it. SEL tapped (not held) opens the list pages MOD 1-8. */
#include "wt_int.h"
#if WT_P37
#define KEYVEL(i) plat_key_vel(i)         /* the keybed's velocity */
#else
#define KEYVEL(i) 100
#endif

/* WT2 on the FM-1: one button per place (SEL OSC A, EDIT OSC B, HOME FILTER, LFO and a fast double tap for MOD, ENV, ARP, FX, SEQ, GLOBAL, SAVE); SEL is no longer a mode */
#define WT_W2 (WT_NODRUMS && !WT_P37)
#if WT_W2
#define BT_OSCA B_SEL
#define BT_OSCB B_EDIT
#define BT_FILT B_HOME
#define BT_MOD  B_LFO                      /* reached by a fast double tap on LFO */
#define BT_PERF B_GLO
#define BT_ARP  B_ARP
#else
#define BT_OSCA B_EDIT
#define BT_OSCB B_EDIT
#define BT_FILT B_EDIT
#define BT_MOD  B_LFO
#define BT_PERF B_HOME
#define BT_ARP  B_ARP
#endif
#if WT_NODRUMS
#define WT_DTRACKS 11              /* only so the virtual-parameter ranges below stay the same numbers; no drum state is kept */
#define WT_NDPAT 8
#define WT_DSTEPS 64
#endif
typedef struct { const char *title; int8_t btn; uint8_t kind; const char *p[4]; const char *l[4]; } wpage_t;
enum { K_DIALS, K_OSC_A, K_OSC_B, K_STEPS, K_DSTEPS, K_DSOUND, K_PERF, K_MIXER, K_P37, K_LFOED, K_ABOUT };
static const wpage_t PAGES[] = {
    {"PERFORM", BT_PERF, K_DIALS, {"MACRO1", "MACRO2", "MACRO3", "MACRO4"}, {"MACRO 1", "MACRO 2", "MACRO 3", "MACRO 4"}},
#if !WT_NODRUMS
    {"MIXER", B_HOME, K_MIXER, {0, 0, 0, 0}, {0, 0, 0, 0}},
    {"DRUMS", B_HOME, K_DSTEPS, {0, 0, 0, 0}, {0, 0, 0, 0}},
    {"DRUM KIT", B_HOME, K_DIALS, {"DR_KIT", "DR_LEVEL", "DR_SEQ", "VP_DPAT"}, {"KIT", "LEVEL", "WITH PLAY", "PATTERN"}},
    {"DRUM ACCENT", B_HOME, K_DIALS, {"DR_ACCENT", "DR_SOFT", 0, 0}, {"LOUD ACCENT", "SOFT ACCENT", 0, 0}},
    {"DRUM LENGTH", B_HOME, K_DIALS, {"VP_LDBL", "VP_LHALF", "VP_LLOOP", 0}, {"DOUBLE", "HALVE", "LOOP BAR", 0}},
    {"DRUM REMOVE BARS", B_HOME, K_DIALS, {"VP_RFROM", "VP_RCNT", "VP_RGO", 0}, {"FROM BAR", "HOW MANY", "REMOVE", 0}},
    {"DRUM PATTERN", B_HOME, K_DIALS, {"VP_DLEN", "VP_DRATE", "VP_DSWING", "VP_DCLEAR"}, {"LENGTH", "RATE", "SWING", "CLEAR"}},
    {"DRUM GENERATE", B_HOME, K_DIALS, {"VP_DGSTYLE", "VP_DGDENS", "VP_DGROLL", 0}, {"STYLE", "EXTRAS", "ROLL", 0}},
    {"SOUND 1", B_HOME, K_DSOUND, {0, 0, 0, 0}, {0, 0, 0, 0}},
    {"SOUND 2", B_HOME, K_DSOUND, {0, 0, 0, 0}, {0, 0, 0, 0}},
    {"SOUND 3", B_HOME, K_DSOUND, {0, 0, 0, 0}, {0, 0, 0, 0}},
    {"SOUND 4", B_HOME, K_DSOUND, {0, 0, 0, 0}, {0, 0, 0, 0}},
#endif
    {"OSC A", BT_OSCA, K_OSC_A, {"A_WTPOS", "A_DETUNE", "A_UNI", "A_LEVEL"}, {"WT POS", "DETUNE", "UNISON", "LEVEL"}},
    {"A WARP", BT_OSCA, K_OSC_A, {"A_WARPMODE", "A_WARP", "A_FM", "A_PD"}, {"WARP", "AMOUNT", "FM FROM B", "PD FROM B"}},
    {"A TABLE", BT_OSCA, K_OSC_A, {"A_TABLE", "A_TABLE2", "A_MORPH", "A_PHASE"}, {"TABLE", "MORPH TO", "MORPH", "PHASE"}},
    {"A TUNE", BT_OSCA, K_OSC_A, {"A_OCT", "A_SEMI", "A_FINE", "A_PAN"}, {"OCTAVE", "SEMITONE", "FINE", "PAN"}},
    {"FILTER", BT_FILT, K_DIALS, {"F_TYPE", "F_CUTOFF", "F_RESO", "F_DRIVE"}, {"TYPE", "CUTOFF", "RESONANCE", "DRIVE"}},
    {"FILTER 2", BT_FILT, K_DIALS, {"F_KEYTRK", "F_MIX", 0, 0}, {"KEY TRACK", "MIX", 0, 0}},
    {"OSC B", BT_OSCB, K_OSC_B, {"B_ON", "B_WTPOS", "B_UNI", "B_LEVEL"}, {"OSC B", "WT POS", "UNISON", "LEVEL"}},
    {"B WARP", BT_OSCB, K_OSC_B, {"B_WARPMODE", "B_WARP", "B_FM", "B_PD"}, {"WARP", "AMOUNT", "FM FROM A", "PD FROM A"}},
    {"B TABLE", BT_OSCB, K_OSC_B, {"B_TABLE", "B_TABLE2", "B_MORPH", "B_DETUNE"}, {"TABLE", "MORPH TO", "MORPH", "DETUNE"}},
    {"B TUNE", BT_OSCB, K_OSC_B, {"B_OCT", "B_SEMI", "B_FINE", "B_PAN"}, {"OCTAVE", "SEMITONE", "FINE", "PAN"}},
    {"SUB", BT_FILT, K_DIALS, {"SUB_ON", "SUB_WAVE", "SUB_PW", "SUB_LEVEL"}, {"ON", "WAVE", "PULSE W", "LEVEL"}},
    {"SUB 2", BT_FILT, K_DIALS, {"SUB_OCT", "SUB_CRS", "SUB_PAN", 0}, {"OCTAVE", "COARSE", "PAN", 0}},
    {"NOISE", BT_FILT, K_DIALS, {"NOI_ON", "NOI_TYPE", "NOI_LEVEL", "NOI_STEREO"}, {"ON", "COLOUR", "LEVEL", "STEREO"}},
    {"NOISE 2", BT_FILT, K_DIALS, {"NOI_FILTER", "NOI_PAN", "NOI_SMP", "NOI_SMP_LEVEL"}, {"FILTER", "PAN", "SAMPLE", "SMP LEVEL"}},
    {"AMP ENV", B_ENV, K_DIALS, {"AMP_ATK", "AMP_DEC", "AMP_SUS", "AMP_REL"}, {"ATTACK", "DECAY", "SUSTAIN", "RELEASE"}},
    {"AMP ENV 2", B_ENV, K_DIALS, {"AMP_DLY", "AMP_HLD", 0, 0}, {"DELAY", "HOLD", 0, 0}},
    {"ENV 2", B_ENV, K_DIALS, {"ENV2_ATK", "ENV2_DEC", "ENV2_SUS", "ENV2_REL"}, {"ATTACK", "DECAY", "SUSTAIN", "RELEASE"}},
    {"ENV 3", B_ENV, K_DIALS, {"ENV3_ATK", "ENV3_DEC", "ENV3_SUS", "ENV3_REL"}, {"ATTACK", "DECAY", "SUSTAIN", "RELEASE"}},
    {"ENV 2 GOES TO", B_ENV, K_DIALS, {"VP_MD2", "VP_MA2", 0, 0}, {"GOES TO", "AMOUNT", 0, 0}},
    {"ENV 3 GOES TO", B_ENV, K_DIALS, {"VP_MD3", "VP_MA3", 0, 0}, {"GOES TO", "AMOUNT", 0, 0}},
    {"LFO 1", B_LFO, K_DIALS, {"LFO1_SHAPE", "LFO1_RATE", "LFO1_SYNC", "LFO1_KEY"}, {"SHAPE", "RATE", "SYNC TO TEMPO", "RESTART"}},
    {"LFO 1 GOES TO", B_LFO, K_DIALS, {"VP_MD0", "VP_MA0", "LFO1_POL", 0}, {"GOES TO", "AMOUNT", "POLARITY", 0}},
    {"LFO 2", B_LFO, K_DIALS, {"LFO2_SHAPE", "LFO2_RATE", "LFO2_SYNC", "LFO2_KEY"}, {"SHAPE", "RATE", "SYNC TO TEMPO", "RESTART"}},
    {"LFO 2 GOES TO", B_LFO, K_DIALS, {"VP_MD1", "VP_MA1", "LFO2_POL", 0}, {"GOES TO", "AMOUNT", "POLARITY", 0}},
#if WT_LFOSHAPES
    {"SHAPE DRAW", B_LFO, K_LFOED, {0, 0, 0, 0}, {"SHAPE", "POINT", "HEIGHT", "TIME"}},
    {"SHAPE CURVE", B_LFO, K_LFOED, {0, 0, 0, 0}, {"SHAPE", "POINT", "CURVE", 0}},
#endif
    {"MOD 1", BT_MOD, K_DIALS, {"MX1_SRC", "MX1_DST", "MX1_AMT", 0}, {"SOURCE", "DEST", "AMOUNT", 0}},
    {"MOD 2", BT_MOD, K_DIALS, {"MX2_SRC", "MX2_DST", "MX2_AMT", 0}, {"SOURCE", "DEST", "AMOUNT", 0}},
    {"MOD 3", BT_MOD, K_DIALS, {"MX3_SRC", "MX3_DST", "MX3_AMT", 0}, {"SOURCE", "DEST", "AMOUNT", 0}},
    {"MOD 4", BT_MOD, K_DIALS, {"MX4_SRC", "MX4_DST", "MX4_AMT", 0}, {"SOURCE", "DEST", "AMOUNT", 0}},
    {"MOD 5", BT_MOD, K_DIALS, {"MX5_SRC", "MX5_DST", "MX5_AMT", 0}, {"SOURCE", "DEST", "AMOUNT", 0}},
    {"MOD 6", BT_MOD, K_DIALS, {"MX6_SRC", "MX6_DST", "MX6_AMT", 0}, {"SOURCE", "DEST", "AMOUNT", 0}},
    {"MOD 7", BT_MOD, K_DIALS, {"MX7_SRC", "MX7_DST", "MX7_AMT", 0}, {"SOURCE", "DEST", "AMOUNT", 0}},
    {"MOD 8", BT_MOD, K_DIALS, {"MX8_SRC", "MX8_DST", "MX8_AMT", 0}, {"SOURCE", "DEST", "AMOUNT", 0}},
    {"STEPS", B_SEQ, K_STEPS, {0, 0, 0, 0}, {0, 0, 0, 0}},
    {"PATTERN", B_SEQ, K_DIALS, {"VP_LEN", "VP_RATE", "VP_SWING", "VP_DIR"}, {"LENGTH", "RATE", "SWING", "DIRECTION"}},
    {"GENERATE", B_SEQ, K_DIALS, {"VP_GDENS", "VP_GRANGE", "VP_GSPICE", "VP_GROLL"}, {"DENSITY", "RANGE", "SPICE", "ROLL"}},
    {"PATTERN LENGTH", B_SEQ, K_DIALS, {"VP_LDBL", "VP_LHALF", "VP_LLOOP", 0}, {"DOUBLE", "HALVE", "LOOP BAR", 0}},
    {"REMOVE BARS", B_SEQ, K_DIALS, {"VP_RFROM", "VP_RCNT", "VP_RGO", 0}, {"FROM BAR", "HOW MANY", "REMOVE", 0}},
    {"PATTERN 2", B_SEQ, K_DIALS, {"VP_PAT", "VP_TRANS", "VP_CLEAR", 0}, {"PATTERN", "TRANSPOSE", "CLEAR", 0}},
    {"PLAY SETUP", B_SEQ, K_DIALS, {"BPM", "ACCENT", "SLIDE", 0}, {"TEMPO", "ACCENT", "SLIDE TIME", 0}},
    {"SAVE", B_SAVE, K_DIALS, {"VP_SLOT", "VP_WHAT", "VP_NPOS", "VP_NCH"}, {"SLOT", "WHAT", "PLACE", "LETTER"}},
#if !WT_W2
    {"MASTER FX", B_FX, K_DIALS, {"FX_DIST", "FX_DRIVE", "FX_COMP", "FX_VOL"}, {"DISTORT", "DRIVE", "COMPRESS", "OUTPUT"}},
#endif
    {"SYNTH FX", B_FX, K_DIALS, {"SYN_DIST", "SYN_DRIVE", "DL_SEND", "RV_SEND"}, {"DISTORT", "DRIVE", "DELAY SEND", "REVERB SEND"}},
#if !WT_NODRUMS
    {"SIDECHAIN", B_FX, K_DIALS, {"SC_AMT", "SC_REL", 0, 0}, {"DUCK", "RECOVER", 0, 0}},
#endif
#if !WT_NODRUMS
    {"DRUM FX", B_FX, K_DIALS, {"DR_DIST", "DR_DRIVE", "DR_DLSEND", "DR_RVSEND"}, {"DISTORT", "DRIVE", "DELAY SEND", "REVERB SEND"}},
#endif
#if WT_LFOSHAPES
#if WT_NODRUMS
    {"CHORUS", B_FX, K_DIALS, {"CH_MIX", "CH_RATE", "CH_DEPTH", "HYPER"}, {"MIX", "RATE", "DEPTH", "HYPER"}},
    {"COMPRESS", B_FX, K_DIALS, {"FX_COMP", 0, 0, 0}, {"AMOUNT", 0, 0, 0}},
#else
    {"CHORUS", B_FX, K_DIALS, {"CH_MIX", "CH_RATE", "CH_DEPTH", 0}, {"MIX", "RATE", "DEPTH", 0}},
#endif
#endif
    {"DELAY", B_FX, K_DIALS, {"DL_SEND", "DL_TIME", "DL_FDBK", "BPM"}, {"SYNTH SEND", "TIME", "FEEDBACK", "TEMPO"}},
    {"DELAY 2", B_FX, K_DIALS, {"DL_TONE", "DL_LEVEL", "DL_HPF", "DL_TYPE"}, {"TONE", "RETURN", "HI-PASS", "TYPE"}},
    {"TAPE", B_FX, K_DIALS, {"DL_WEAR", "DL_PING", 0, 0}, {"WEAR", "PING-PONG", 0, 0}},
    {"REVERB", B_FX, K_DIALS, {"RV_SEND", "RV_DECAY", "RV_TONE", "RV_LEVEL"}, {"SYNTH SEND", "DECAY", "TONE", "RETURN"}},
    {"REVERB 2", B_FX, K_DIALS, {"RV_HPF", 0, 0, 0}, {"HI-PASS", 0, 0, 0}},
    {"EQ", B_FX, K_DIALS, {"EQ_LOW", "EQ_MID", "EQ_HIGH", "EQ_ON"}, {"LOW", "MID", "HIGH", "EQ"}},
    {"EQ 2", B_FX, K_DIALS, {"EQ_LOWF", "EQ_MIDF", "EQ_Q", "EQ_HIGHF"}, {"LOW FREQ", "MID FREQ", "MID Q", "HIGH FREQ"}},
    {"ARP", BT_ARP, K_DIALS, {"ARP_ON", "ARP_MODE", "ARP_RATE", "ARP_OCT"}, {"ARP", "MODE", "RATE", "OCTAVES"}},
    {"ARP 2", BT_ARP, K_DIALS, {"ARP_GATE", "ARP_SWING", "ARP_LATCH", 0}, {"GATE", "SWING", "LATCH", 0}},
    {"VOICE", B_GLO, K_DIALS, {"VOICEMODE", "GLIDE", "BEND", "TUNE"}, {"MODE", "GLIDE", "BEND RANGE", "TUNE"}},
#if WT_NODRUMS
    {"UNISON", B_GLO, K_DIALS, {"UNI_TOP", 0, 0, 0}, {"TOP NOTE ONLY", 0, 0, 0}},
#endif
    {"SCALE", B_GLO, K_DIALS, {"SCALE", "ROOT", "SCALE_SEQ", 0}, {"SCALE", "ROOT", "HOLD SEQ TO IT", 0}},
#if WT_NODRUMS
    {"MIDI", B_GLO, K_DIALS, {"MIDI_CH", "MIDI_OUTCH", "USB_AUDIO", 0}, {"IN CHANNEL", "OUT CHANNEL", "USB AUDIO", 0}},
#else
    {"MIDI", B_GLO, K_DIALS, {"MIDI_CH", "MIDI_DRCH", "MIDI_OUTCH", "USB_AUDIO"}, {"IN CHANNEL", "DRUM CHANNEL", "OUT CHANNEL", "USB AUDIO"}},
#endif
    {"SYNC", B_GLO, K_DIALS, {"CLK_SRC", "CLK_SS", 0, 0}, {"CLOCK FROM", "START / STOP", 0, 0}},
    {"PERFORMANCE", B_GLO, K_PERF, {0, 0, 0, 0}, {0, 0, 0, 0}},
    {"LEVELS", B_GLO, K_DIALS, {"VELAMP", "MASTER", "CPU_MODE", "CORE2"}, {"VELOCITY", "MASTER", "CPU MODE", "2ND CORE"}},
#if WT_NODRUMS
    {"ABOUT", B_GLO, K_ABOUT, {0, 0, 0, 0}, {0, 0, 0, 0}},
#endif
#if WT_P37
    {"P37 CONTROLS", -1, K_P37, {0, 0, 0, 0}, {0, 0, 0, 0}},          /* what the faders, wheels, pedal, pads and buttons read: for bring-up */
#endif
};
#define NPAGE ((int)(sizeof PAGES / sizeof PAGES[0]))

/* parameters the matrix can modulate: the destination, and how a matrix unit becomes a parameter unit (num / den) */
static const struct { const char *name; uint8_t dest; int16_t num, den; } MODMAP[] = {
    {"A_WTPOS", D_WTPOS, 1, 1}, {"B_WTPOS", D_WTPOS_B, 1, 1}, {"A_WARP", D_WARP, 1, 1}, {"B_WARP", D_WARP_B, 1, 1},
    {"A_DETUNE", D_DETUNE, 1, 1}, {"B_DETUNE", D_DETUNE_B, 1, 1}, {"A_LEVEL", D_LEVEL, 1, 1}, {"B_LEVEL", D_LEVEL_B, 1, 1},
    {"A_PAN", D_PAN, 1, 1}, {"B_PAN", D_PAN_B, 1, 1}, {"A_SEMI", D_PITCH, 24, 1000}, {"B_SEMI", D_PITCH_B, 24, 1000},
    {"A_FM", D_FM, 1, 1}, {"B_FM", D_FM_B, 1, 1}, {"A_MORPH", D_MORPH, 1, 1}, {"B_MORPH", D_MORPH_B, 1, 1},
    {"F_CUTOFF", D_CUTOFF, 6, 10}, {"F_RESO", D_RESO, 1, 1}, {"F_DRIVE", D_DRIVE, 1, 1}, {"SUB_PW", D_SUBPW, 1, 1},
#if WT_NODRUMS
    {"SUB_LEVEL", D_SUBLVL, 1, 1}, {"NOI_LEVEL", D_NOILVL, 1, 1},
    {"AMP_ATK", D_AMPATK, 1, 1}, {"AMP_DEC", D_AMPDEC, 1, 1}, {"AMP_REL", D_AMPREL, 1, 1},
    {"LFO1_RATE", D_LFO1R, 1, 1}, {"LFO2_RATE", D_LFO2R, 1, 1},
    {"CH_MIX", D_CHMIX, 1, 1}, {"DL_SEND", D_DLSEND, 127, 1000}, {"RV_SEND", D_RVSEND, 127, 1000},
    {"MASTER", D_MASTER, 1, 1},
#endif
};
#define NMODMAP ((int)(sizeof MODMAP / sizeof MODMAP[0]))
/* the pattern's own settings are shown on dials too: "virtual" parameters after the synth's (ids P_COUNT ..), kept in the pattern */
/* the characters a saved sound's name is typed from (SAVE page: one knob picks the place, one the letter) */
static const char NCHARS[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-";
#define NNCH ((int)sizeof NCHARS - 1)
enum { VP_LEN = P_COUNT, VP_RATE, VP_SWING, VP_DIR, VP_TRANS, VP_PAT, VP_CLEAR, VP_SLOT, VP_WHAT, VP_DPAT, VP_DLEN, VP_DRATE, VP_DSWING, VP_DCLEAR, VP_MD0, VP_MD1, VP_MD2, VP_MD3, VP_MA0, VP_MA1, VP_MA2, VP_MA3, VP_GDENS, VP_GRANGE, VP_GSPICE, VP_GROLL, VP_DGSTYLE, VP_DGDENS, VP_DGROLL, VP_LDBL, VP_LHALF, VP_LLOOP, VP_RFROM, VP_RCNT, VP_RGO, VP_NPOS, VP_NCH, VP_END };
#define NVP (VP_END - P_COUNT)
#define NID VP_END
static const char *const VP_NAME[NVP] = {"VP_LEN", "VP_RATE", "VP_SWING", "VP_DIR", "VP_TRANS", "VP_PAT", "VP_CLEAR", "VP_SLOT", "VP_WHAT", "VP_DPAT", "VP_DLEN", "VP_DRATE", "VP_DSWING", "VP_DCLEAR", "VP_MD0", "VP_MD1", "VP_MD2", "VP_MD3", "VP_MA0", "VP_MA1", "VP_MA2", "VP_MA3", "VP_GDENS", "VP_GRANGE", "VP_GSPICE", "VP_GROLL", "VP_DGSTYLE", "VP_DGDENS", "VP_DGROLL", "VP_LDBL", "VP_LHALF", "VP_LLOOP", "VP_RFROM", "VP_RCNT", "VP_RGO", "VP_NPOS", "VP_NCH"};
static const int16_t VP_MIN[NVP] = {1, 0, 50, 0, -24, 1, 0, 1, 0, 1, 1, 0, 50, 0, 0, 0, 0, 0, -1000, -1000, -1000, -1000, 0, 1, 0, 0, 0, 0, 0, 0, -1, 0, 1, 1, 0, 1, 0}, VP_MAX[NVP] = {64, 3, 75, 3, 24, 16, 1, 20, 4, WT_NDPAT, WT_DSTEPS, 3, 75, 1, D_COUNT - 1, D_COUNT - 1, D_COUNT - 1, D_COUNT - 1, 1000, 1000, 1000, 1000, 100, 3, 100, 1, 2, 100, 1, 1, 1, 4, 4, 3, 1, 11, NNCH - 1};
static int pmin(int id) { return id < P_COUNT ? wt_param_desc[id].min : VP_MIN[id - P_COUNT]; }
static int pmax(int id) { return id < P_COUNT ? wt_param_desc[id].max : VP_MAX[id - P_COUNT]; }
static const char *pname(int id) { return id < P_COUNT ? wt_param_desc[id].name : VP_NAME[id - P_COUNT] + 3; }
static int ui_find(const char *n)
{
    int i;
    if (n[0] == 'V' && n[1] == 'P' && n[2] == '_') { for (i = 0; i < NVP; i++) { const char *a = VP_NAME[i], *b = n; while (*a && *a == *b) { a++; b++; } if (!*a && !*b) return P_COUNT + i; } return -1; }
    return wt_param_find(n);
}
static int8_t dest_of[NID];                     /* parameter id -> destination (0 = cannot be modulated) */
static int16_t dnum[NID], dden[NID];

static int16_t pid[NPAGE][4];                   /* parameter ids, -1 = none */
static int16_t ui_p[NID];                       /* the main loop's mirror of the synth's parameters (and the pattern's) */
static uint32_t vp_t[NVP];                      /* when a pattern setting was last changed here: its mirror is not overwritten for a moment */
static struct {
    int8_t page, patch, oct;
    uint8_t dirty, anim, rec, sp;               /* sp: the 16-step page of the step grid */
    int8_t held;                                /* the step key held on the STEPS page, -1 none */
    uint8_t held_used, last_note, rec_pos, clear_msg;
    uint8_t rfrom, rcnt, opf;                                                      /* the remove-bars settings; flashes of the edit dials (bit 0 double, 1 first half, 2 second half, 3 remove) */
    uint8_t gdens, grange, gspice, groll, dgstyle, dgdens, dgroll;     /* the generators' settings and the "rolled" flashes */
    int8_t navbtn; uint8_t nav_used; uint32_t nav_t;                  /* a page button being held: its keys jump to its pages */
    uint8_t dtrack, dsp, dclear, mch;           /* ... mch: the mixer channel (0 synth, 1 drums, 2.. the drum tracks) */                /* the drum track edited, the drum grid's 16-step page, the "cleared" flash */
    int8_t dheld;                               /* the drum step key held, -1 none */
    uint8_t dheld_used, dhits;
    uint8_t slot, what, armed, msg;             /* SAVE page: the slot (0..19), what to store, waiting for the second press, the last result (0 none, 1 saved, 2 erased, 3 failed) */
    uint32_t armed_t, msg_t;
    char toast[24], toast2[24]; uint32_t toast_t;           /* a message over the screen for a moment (the table PRESETS chose) */
    char sname[12]; uint8_t npos;               /* SAVE page: the name being typed (11 places, space padded) and the place being edited */
    char pname[14];                             /* the loaded patch's name */
    int8_t mod_src;                             /* the source a held SEL assigns (S_LFO1 ..), */
    uint8_t sel_down, sel_used, sel_latched, sel_ignore;                 /* SEL held / a knob or ALGORITHM was used while it was */
    uint32_t last_draw, keys, bprev, sel_t0, clear_t;
    uint8_t keynote[NKEYS];
    uint8_t recst[NKEYS];                       /* REC live: the step a key wrote (+1), and when */
    uint32_t rec8[NKEYS], rt0, play_t, lfo_t;
    int8_t rpk; uint8_t ri;                     /* REC step by step: the key held and the step it wrote */
#if WT_P37
    uint8_t fbank, fb_down, fb_used, fpend;     /* the fader bank 0..3; FADER BANK held / a knob went to the assignment meanwhile; faders waiting to catch their value (bit each) */
    uint32_t fb_t0, ft_t;                       /* FADER BANK pressed at; the fader readout shown since */
    int16_t ft_id;                              /* the parameter of that readout, -1 none */
    uint8_t flast[4], extnote[37], wcal;        /* the faders' last readings; the outer keys' notes; wheel calibration frames */
    int16_t wrest[2], whi[2], wlo[2], bend_sent, mod_sent;      /* the wheels: rest reading, the extremes seen, what was sent */
    uint8_t lastbtn, lastvel;                   /* the last button / key velocity (the P37 CONTROLS page) */
    uint16_t pdown;                             /* the pads held (bit per pad, top row first) */
    uint8_t pmode, ntap, nextused, pheld_used, psave_st, psave_ex, pstep, pflash_g, plit_v[16];     /* pads: 0 play the sounds / 1 step entry; > taps counted; the pad held in entry mode and its step / what the step was before the tap; the flash colour */
    int8_t pheld, pl_pad, pl_step;              /* the pad held (-1 none); the last tapped pad and its step (for the double tap) */
    uint32_t pk_prev, ntap_t, pl_t, pflash_t, plit_t[16];     /* pads held a frame ago; when > was tapped; when the pad was tapped; when the mode flash began; when each pad was struck */
#endif
} wui;
#if !WT_NODRUMS
static int8_t dmir[2][WT_DTRACKS][12];            /* the drum pots as the screen knows them (the engines are only written through the queue) */
static uint8_t dflash[WT_DTRACKS];
#endif
static int8_t slot_key[16], bslot_key[11];     /* the key of grid step j / of pattern j */
static int8_t white_slot[NKEYS], black_slot[NKEYS];      /* a key's step in the grid (white keys 0..15) or pattern (black keys 0..10); -1 */

static const char *const E_TABLES[3] = {"BASIC SHP", "FOLD-SINE", "ODD-EVEN"};
static const char *const E_SUBW[4] = {"SINE", "TRIANGLE", "SQUARE", "PULSE"};
static const char *const E_NOISE[5] = {"WHITE", "PINK", "BROWN", "GEIGER", "OFF"};
static const char *const E_FILT[7] = {"OFF", "LP12", "LP24", "HP12", "BP12", "NOTCH", "VOWEL"};
static const char *const E_VMODE[9] = {"POLY", "MONO", "LEGATO", "POLY 2", "POLY 3", "POLY 4", "POLY 5", "POLY 6", "POLY 7"};
static const char *const E_ONOFF[2] = {"OFF", "ON"};
static const char *const E_SMP[5] = {"NONE", "CRACKLE", "AIR", "RUMBLE", "(EMPTY)"};
#if WT_LFOSHAPES
#define LFO_NAME(v) ((v) < 7 ? E_LFO[(v)] : wt_lfoshape_name((v) - 7))
#else
#define LFO_NAME(v) E_LFO[(v) % 7]
#endif
static const char *const E_LFO[7] = {"SINE", "TRIANGLE", "SAW UP", "SQUARE", "S&H", "SMOOTH", "SAW DOWN"};
#if WT_NODRUMS
static const char *const E_WARP[7] = {"OFF", "BEND", "SYNC", "MIRROR", "ASYM", "QUANT", "PWM"};
#else
static const char *const E_WARP[4] = {"OFF", "BEND", "SYNC", "MIRROR"};
#endif

static const char *const E_CPU[3] = {"FULL", "AUTO", "SAVE"};
#if WT_NODRUMS
static const char *const E_WHAT[5] = {"PATCH", "PATTERNS", "SETUP+SHAPES", "ALL", "ERASE"};
#else
static const char *const E_WHAT[5] = {"PATCH", "PATTERNS", "DRUMS+SETUP", "ALL", "ERASE"};
#endif
/* BASIC SHAPES' 9 stored frames (the table sweeps smoothly between them; WT POS 0-100 % passes frame i at i / 8) */
static const char *const SHAPE_NAME[9] = {"TRIANGLE", "SAW", "SQUARE", "SINE", "PULSE 37", "PULSE 25", "PULSE 17", "PULSE 10", "PULSE 5"};
static const char *const E_DGS[3] = {"FOUR FLOOR", "BREAK", "RANDOM"};
static const char *const E_ROOT[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const char *const E_CLK[3] = {"INTERNAL", "MIDI IN", "USB"};
#if !WT_NODRUMS
static const char *const E_KIT[2] = {"909", "808"};
#endif
static const char *const E_AMODE[AM_N] = {"UP", "DOWN", "UP-DOWN", "DOWN-UP", "RANDOM", "PLAYED", "CHORD"};
static const char *const E_ARATE[6] = {"1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32"};
static const char *const E_RATE[4] = {"1/16", "1/16T", "1/32", "1/8T"};
static const char *const E_DIR[4] = {"FORWARD", "REVERSE", "PING-PONG", "RANDOM"};

static const char *wui_enum(int id, int v)      /* the parameter's value as text, or 0 for a number */
{
    if (id == VP_RATE) return E_RATE[v & 3];
    if (id == VP_DIR) return E_DIR[v & 3];
    if (id == VP_WHAT) return E_WHAT[v > 4 ? 4 : v];
    if (id == VP_DRATE) return E_RATE[v & 3];
    if (id == VP_GROLL || id == VP_DGROLL) return v ? "ROLLED" : "TURN >";
    if (id == VP_DGSTYLE) return E_DGS[v > 2 ? 2 : v];
    if (id == VP_LDBL) return v ? "DOUBLED" : "TURN >";
    if (id == VP_LHALF) return v > 0 ? "1ST HALF" : v < 0 ? "2ND HALF" : "< TURN >";
    if (id == VP_RGO) return v ? "REMOVED" : "TURN >";
    if (id == VP_LLOOP) { static char lb[8]; if (!v) return "OFF"; lb[0] = 'B'; lb[1] = 'A'; lb[2] = 'R'; lb[3] = ' '; lb[4] = (char)('0' + v); lb[5] = 0; return lb; }
    if (id == VP_RFROM) { static char rb[8]; rb[0] = 'B'; rb[1] = 'A'; rb[2] = 'R'; rb[3] = ' '; rb[4] = (char)('0' + v); rb[5] = 0; return rb; }
    if (id == VP_RCNT) { static char cb[10]; cb[0] = (char)('0' + v); cb[1] = ' '; cb[2] = 'B'; cb[3] = 'A'; cb[4] = 'R'; cb[5] = v > 1 ? 'S' : 0; cb[6] = 0; return cb; }
    if (id == P_SCALE) return wt_scale_names[v % WT_NSCALES];
    if (id == P_MIDI_CH) return v ? 0 : "ALL";
    if (id == P_MIDI_DRCH || id == P_MIDI_OUTCH) return v ? 0 : "OFF";
    if (id == P_USB_AUDIO || id == P_CLK_SS) return E_ONOFF[v & 1];
    if (id == P_CLK_SRC) return E_CLK[v > 2 ? 2 : v];
    if (id == P_LFO1_POL || id == P_LFO2_POL) { static const char *const LP[3] = {"UP + DOWN", "UP ONLY", "DOWN ONLY"}; return LP[v > 2 ? 2 : v]; }
    if (id == P_LFO1_KEY || id == P_LFO2_KEY) { static const char *const LK[3] = {"FREE", "EVERY NOTE", "FIRST NOTE"}; return LK[v > 2 ? 2 : v]; }
    if (id == P_LFO1_SYNC || id == P_LFO2_SYNC) return wt_lfo_div_names[v > 15 ? 15 : v];
    if (id == P_ROOT) return E_ROOT[v % 12];
    if (id >= VP_MD0 && id <= VP_MD3) return wt_dst_names[v < 0 ? 0 : v % D_COUNT];
    if (id == P_SYN_DIST) return wt_fx_dist_names[v & 7];
#if !WT_NODRUMS
    if (id == P_DR_DIST) return wt_fx_dist_names[v & 7];
#endif
    if (id == VP_DCLEAR) return v ? "DONE" : "TURN >";
#if WT_NODRUMS
    if (id == P_DR_KIT) return E_ONOFF[v & 1];          /* UNI_TOP */
#else
    if (id == P_DR_KIT) return E_KIT[v & 1];
#endif
    if (id == P_DR_SEQ || id == P_CORE2) return E_ONOFF[v & 1];
    if (id == VP_CLEAR) return v ? "DONE" : "TURN >";
    if (id >= P_COUNT) return 0;
    if (id == P_OSC(0, O_TABLE) || id == P_OSC(0, O_TABLE2) || id == P_OSC(1, O_TABLE) || id == P_OSC(1, O_TABLE2)) return E_TABLES[v % 3];
    if (id == P_CPUMODE) return E_CPU[v % 3];
    if (id == P_ARP_MODE) return E_AMODE[v % AM_N];
    if (id == P_ARP_RATE) return E_ARATE[v % 6];
    if (id == P_ARP_ON || id == P_ARP_LATCH || id == P_EQ_ON || id == P_SCALE_SEQ) return E_ONOFF[v & 1];
    if (id == P_SUB_WAVE) return E_SUBW[v & 3];
    if (id == P_NOI_TYPE) return E_NOISE[v > 4 ? 4 : v];
    if (id == P_F_TYPE) return E_FILT[v < 0 ? 0 : v > 6 ? 6 : v];
    if (id == P_VOICEMODE) return E_VMODE[v < 0 ? 0 : v > 8 ? 8 : v];
    if (id == P_NOI_SMP) return E_SMP[v > 4 ? 4 : v];
    if (id == P_LFO(0, 0) || id == P_LFO(1, 0)) return LFO_NAME(v < 0 ? 0 : v);
    if (id == P_OSC(0, O_WARPMODE) || id == P_OSC(1, O_WARPMODE)) {
#if WT_NODRUMS
        return E_WARP[v < 0 ? 0 : v > 6 ? 6 : v];
#else
        return E_WARP[v & 3];
#endif
    }
    if (id == P_OSC(0, O_ON) || id == P_OSC(1, O_ON) || id == P_SUB_ON || id == P_NOI_ON || id == P_FX0 + FX_DL_PING) return E_ONOFF[v & 1];
    if (id == P_FX0 + FX_DIST) return wt_fx_dist_names[v & 7];
    if (id == P_FX0 + FX_DL_TIME) return wt_fx_div_names[v > 12 ? 12 : v];
    if (id == P_FX0 + FX_DL_TYPE) return wt_fx_type_names[v & 1];
    if (id >= P_MX0 && id < P_FX0 && (id - P_MX0) % 3 == 0) return wt_src_names[v % S_COUNT];
    if (id >= P_MX0 && id < P_FX0 && (id - P_MX0) % 3 == 1) return wt_dst_names[v % D_COUNT];
    return 0;
}

/* a number and a % sign: the length is taken once (measuring again after the '%' replaces the end mark scans past the buffer) */
static void fmt_pct(char *b, int v)
{
    uint32_t n;
    fmt_int(b, v);
    n = str_len(b);
    b[n] = '%'; b[n + 1] = 0;
}

static void wui_value(char *b, int id, int v)   /* the value as the screen shows it (b: 16 bytes) */
{
    if (id == P_DR_ACCENT || id == P_DR_SOFT) {  /* an accent: a velocity it sets (FIX 100) or adds (+50 / -50) */
        int n = 0;
        if (v >= 128) { str_cpy(b, "FIX ", 15); fmt_int(b + 4, v - 128); }
        else { if (v > 0) b[n++] = '+'; fmt_int(b + n, v); }
        return;
    }
    const char *s = wui_enum(id, v);
    if (s) { str_cpy(b, s, 15); return; }
#if WT_LFOSHAPES
    if (id == P_CH_MIX || id == P_CH_DEPTH) { fmt_pct(b, v / 10); return; }
    if (id == P_CH_RATE) { int hz10 = 1 + v * 39 / 1000; fmt_int(b, hz10 / 10); { int n = (int)str_len(b); b[n++] = '.'; b[n++] = (char)('0' + hz10 % 10); b[n++] = 'H'; b[n++] = 'z'; b[n] = 0; } return; }
#endif
    if (id == P_F_CUTOFF && ui_p[P_F_TYPE] == 6) {                        /* a vowel filter: the cutoff is where it is between A E I O U */
        int pos = v * 1024 / 1350, sg = pos >> 8, n;
        if (sg > 3) sg = 3;
        b[0] = "AEIOU"[sg]; b[1] = '>'; b[2] = "AEIOU"[sg + 1]; b[3] = ' ';
        fmt_int(b + 4, (pos - sg * 256) * 100 / 256); n = (int)str_len(b); b[n++] = '%'; b[n] = 0;
        return;
    }
    if (id == P_EQ_LOW || id == P_EQ_MID || id == P_EQ_HIGH) {           /* dB with one decimal */
        int a = v < 0 ? -v : v, n = 0;
        if (v < 0) b[n++] = '-'; else if (v > 0) b[n++] = '+';
        fmt_int(b + n, a / 10); n = (int)str_len(b);
        b[n++] = '.'; b[n++] = (char)('0' + a % 10); b[n] = 0;
        return;
    }
    if (id == P_EQ_LOWF || id == P_EQ_MIDF || id == P_EQ_HIGHF) {        /* Hz, or kHz with one decimal */
        if (v >= 1000) { fmt_int(b, v / 1000); { int n = (int)str_len(b); b[n++] = '.'; b[n++] = (char)('0' + (v % 1000) / 100); b[n++] = 'k'; b[n] = 0; } }
        else fmt_int(b, v);
        return;
    }
    if (id == P_EQ_Q) { fmt_int(b, v / 10); { int n = (int)str_len(b); b[n++] = '.'; b[n++] = (char)('0' + v % 10); b[n] = 0; } return; }
    if (id == P_ARP_GATE || id == P_ARP_SWING || id == VP_DSWING || id == VP_GDENS || id == VP_GSPICE || id == VP_DGDENS) { fmt_pct(b, v); return; }
    if (id >= VP_MA0 && id <= VP_MA3) { if (v > 0) { b[0] = '+'; fmt_pct(b + 1, v / 10); } else fmt_pct(b, v / 10); return; }
    if (id == P_SYN_DRIVE || id == P_DR_DRIVE || id == P_DR_RVSEND || id == P_DR_DLSEND) { fmt_pct(b, v * 100 / 127); return; }
    if (id == VP_NCH) { b[0] = NCHARS[v % NNCH]; if (b[0] == ' ') b[0] = '_'; b[1] = 0; return; }
    if (id == VP_SLOT) { b[0] = 'U'; fmt_int(b + 1, v); return; }
    if (id == P_BPM || id >= P_COUNT) { if (id == VP_SWING) fmt_pct(b, v); else fmt_int(b, v); return; }
    if (id >= P_FX0 && id <= P_FX_DLSEND && wt_param_desc[id].max == 127) {      /* the effects' pots: percent */
        fmt_pct(b, v * 100 / 127);
        return;
    }
    if (wt_param_desc[id].max >= 1000 && wt_param_desc[id].min >= -1000) {      /* per mille: percent */
        fmt_pct(b, v / 10);
        return;
    }
    fmt_int(b, v);
}

static int ui_snap(int n) { n = n < 0 ? 0 : n > 127 ? 127 : n; return ui_p[P_SCALE] ? wt_scale_snap(ui_p[P_SCALE], ui_p[P_ROOT], n) : n; }      /* a pitch, held to the scale when scale mode is on */
static wt_pat_t *cur_pat(void) { return &synth.seq.pat[synth.seq.cur % WT_NPAT]; }
#if WT_NODRUMS
static int ops_on_drums(void) { return 0; }
#else
static int ops_on_drums(void) { const char *t = PAGES[wui.page].title; return t[0] == 'D' && t[1] == 'R' && t[2] == 'U'; }       /* the length / remove pages of the DRUM group edit the drum pattern, the others the synth's */
#endif
static void ui_toast(const char *a, const char *b) { str_cpy(wui.toast, a, 23); str_cpy(wui.toast2, b, 23); wui.toast_t = fm1_ms ? fm1_ms : 1; wui.dirty = 1; }
static void ui_toast_len(const char *a, int len)
{
    char b[24];
    int n;
    fmt_int(b, len); n = (int)str_len(b);
    b[n++] = ' '; b[n++] = 'S'; b[n++] = 'T'; b[n++] = 'E'; b[n++] = 'P'; b[n++] = 'S'; b[n] = 0;
    ui_toast(a, b);
}

/* the LFOs' and envelopes' own pages: where each one goes and how much, kept in the first matrix slot it is the source of */
static const uint8_t MSRC[4] = {S_LFO1, S_LFO2, S_ENV2, S_ENV3};
static int src_slot(int src, int create)
{
    int i;
    for (i = 0; i < 8; i++) if (ui_p[P_MX(i, 0)] == src) return i;
    if (!create) return -1;
    for (i = 0; i < 8; i++) if (ui_p[P_MX(i, 0)] == 0 || ui_p[P_MX(i, 2)] == 0 || ui_p[P_MX(i, 1)] == 0) {
        ui_p[P_MX(i, 0)] = (int16_t)src; ui_p[P_MX(i, 1)] = 0; ui_p[P_MX(i, 2)] = 0;
        wq_put(WC_PARAM, (uint8_t)P_MX(i, 0), (int16_t)src); wq_put(WC_PARAM, (uint8_t)P_MX(i, 1), 0); wq_put(WC_PARAM, (uint8_t)P_MX(i, 2), 0);
        return i;
    }
    return -1;
}

/* the pattern's settings into the mirror (a setting just changed here is left alone for a moment: the audio side confirms it later) */
static void wui_vsync(void)
{
    const wt_pat_t *q = cur_pat();
    int i;
    int16_t v[NVP];
    v[0] = q->len; v[1] = q->rate; v[2] = q->swing; v[3] = q->dir; v[4] = (int16_t)((int)q->trans - 24);
    v[5] = (int16_t)((synth.seq.next != 255 ? synth.seq.next : synth.seq.cur) + 1);
    v[6] = (int16_t)(wui.clear_msg ? 1 : 0);
    v[7] = (int16_t)(wui.slot + 1); v[8] = wui.what;
#if WT_NODRUMS
    v[9] = v[10] = v[11] = v[12] = v[13] = 0;
#else
    { const wt_dpat_t *dp = &drums.pat[drums.cur % WT_NDPAT];
      v[9] = (int16_t)((drums.next != 255 ? drums.next : drums.cur) + 1); v[10] = dp->len; v[11] = dp->rate; v[12] = dp->swing; v[13] = (int16_t)(wui.dclear ? 1 : 0); }
#endif
    v[22] = wui.gdens; v[23] = wui.grange; v[24] = wui.gspice; v[25] = (int16_t)(wui.groll ? 1 : 0); v[26] = wui.dgstyle; v[27] = wui.dgdens; v[28] = (int16_t)(wui.dgroll ? 1 : 0);
    {   int dr = ops_on_drums();                    /* the pattern edit pages: the loop bar of whichever pattern they edit, their own settings, the flashes */
        v[29] = (int16_t)(wui.opf & 1); v[30] = (int16_t)((wui.opf & 2) ? 1 : (wui.opf & 4) ? -1 : 0);
#if WT_NODRUMS
        v[31] = (int16_t)wt_pat_loop_bar(cur_pat());
        (void)dr;
#else
        v[31] = (int16_t)(dr ? wt_dpat_loop_bar(&drums.pat[drums.cur % WT_NDPAT]) : wt_pat_loop_bar(cur_pat()));
#endif
        v[32] = wui.rfrom; v[33] = wui.rcnt; v[34] = (int16_t)((wui.opf & 8) ? 1 : 0);
        v[35] = (int16_t)(wui.npos + 1);
        { const char *cp = NCHARS; int k = 0; while (cp[k] && cp[k] != wui.sname[wui.npos]) k++; v[36] = (int16_t)(cp[k] ? k : 0); }
    }
    for (i = 0; i < 4; i++) {                       /* where each LFO / envelope goes (the first matrix slot it is the source of) */
        int sl = src_slot(MSRC[i], 0);
        v[14 + i] = sl >= 0 ? ui_p[P_MX(sl, 1)] : 0;
        v[18 + i] = sl >= 0 ? ui_p[P_MX(sl, 2)] : 0;
    }
    for (i = 0; i < NVP; i++)
        if (fm1_ms - vp_t[i] > 250u && ui_p[P_COUNT + i] != v[i]) { ui_p[P_COUNT + i] = v[i]; wui.dirty = 1; }
}

static void wui_sync(void) { uint32_t i; for (i = 0; i < P_COUNT; i++) ui_p[i] = synth.p[i]; for (i = 0; i < NVP; i++) vp_t[i] = 0; wui_vsync(); wui.dirty = 1; }

/* ---- the flash store: 20 user patches (10 in each of two objects), the 16 patterns (6 + 6 + 4 in three objects) ---- */
#define UPER ((int)((PLAT_STORE_MAX - 8) / (12 + 2 * P_COUNT)))      /* patches per store object: as many as fit (9 now) */
#define NUSER (2 * UPER)
#define NPLIST (WT_NPATCHES + NUSER)
#define WTP_MAGIC 0x31505457u                   /* "WTP1" */
#define WTQ_MAGIC 0x31515457u                   /* "WTQ1" */
typedef struct { uint32_t magic; uint16_t nparams, n; } st_head_t;
typedef struct { char name[12]; int16_t v[P_COUNT]; } urec_t;
_Static_assert(sizeof(st_head_t) + UPER * sizeof(urec_t) <= PLAT_STORE_MAX, "a patch object does not fit the store");
_Static_assert(sizeof(st_head_t) + 6 * sizeof(wt_pat_t) <= PLAT_STORE_MAX, "a pattern object does not fit the store");
#if WT_NODRUMS
static uint8_t sbuf[PLAT_STORE_MAX] WT_POOL;   /* flash scratch: the pool, not the 96 KB RAM */
#else
static uint8_t sbuf[PLAT_STORE_MAX];
#endif
static char uname[NUSER][12];                   /* the saved patches' names; "" = an empty slot */
static const uint8_t UOBJ[2] = {OBJ_SOUND, OBJ_MOTION};
static const uint8_t QOBJ[3] = {OBJ_PAT0, OBJ_PAT1, OBJ_SONG};
static void bcopy_(void *d, const void *s_, uint32_t n) { uint8_t *a = d; const uint8_t *c = s_; while (n--) *a++ = *c++; }

static int uobj_load(int k)                     /* object k of the patch store into sbuf; 1 = it holds patches, else sbuf is empty */
{
    int n = plat_store_load(UOBJ[k], sbuf, sizeof sbuf);
    const st_head_t *h = (const st_head_t *)sbuf;
    if (n >= (int)sizeof(st_head_t) && h->magic == WTP_MAGIC && h->nparams > 0 && h->nparams <= 400 && h->n >= UPER &&
        (uint32_t)n >= sizeof(st_head_t) + (uint32_t)UPER * (12u + 2u * h->nparams)) return 1;
    { uint32_t i; for (i = 0; i < sizeof sbuf; i++) sbuf[i] = 0; }
    ((st_head_t *)sbuf)->magic = WTP_MAGIC; ((st_head_t *)sbuf)->nparams = P_COUNT; ((st_head_t *)sbuf)->n = UPER;
    return 0;
}
/* record i of the object in sbuf (the stored parameter count may differ from this build's: records are stored at the stored width) */
static uint8_t *urec_at(int i) { return sbuf + sizeof(st_head_t) + (uint32_t)i * (12u + 2u * ((st_head_t *)sbuf)->nparams); }

static int16_t store_np[2], store_len[2], store_named[2], store_nrec[2];             /* what each patch object held at power-on: parameters per patch, its length in bytes, records, named records */
static int8_t store_stat[4];                    /* what the flash held at power-on: the two patch objects, the patterns, the drums: 1 readable, 0 something there that was not understood, -1 nothing */
static void user_scan(void)
{
    int k, i;
    for (i = 0; i < NUSER; i++) uname[i][0] = 0;
    for (k = 0; k < 2; k++) {
        int raw = plat_store_load(UOBJ[k], sbuf, sizeof sbuf);
        store_stat[k] = (int8_t)(raw < 0 ? -1 : 0);
        store_len[k] = (int16_t)(raw < 0 ? 0 : raw); store_np[k] = store_nrec[k] = store_named[k] = 0;
        if (raw >= (int)sizeof(st_head_t)) {                                                /* count what is in there, whatever the layout */
            const st_head_t *hh = (const st_head_t *)sbuf;
            int r2;
            store_np[k] = (int16_t)hh->nparams; store_nrec[k] = (int16_t)hh->n;
            if (hh->magic == WTP_MAGIC && hh->nparams > 0 && hh->nparams <= 400)
                for (r2 = 0; r2 < hh->n && r2 < 32; r2++) {
                    uint32_t off = sizeof(st_head_t) + (uint32_t)r2 * (12u + 2u * hh->nparams);
                    if (off < (uint32_t)raw && sbuf[off]) store_named[k]++;
                }
        }
    }
    for (k = 0; k < 2; k++)
        if (uobj_load(k)) {
            store_stat[k] = 1;
            for (i = 0; i < UPER; i++) { const char *nm = (const char *)urec_at(i); if (nm[0]) { bcopy_(uname[k * UPER + i], nm, 12); uname[k * UPER + i][11] = 0; } }
        }
}
static int user_read(int slot, int16_t *out)    /* 0 = ok */
{
    int k = slot / UPER, i = slot % UPER, np, j;
    if (!uobj_load(k) || !((const char *)urec_at(i))[0]) return -1;
    np = ((st_head_t *)sbuf)->nparams;
    wt_patch_values(wt_patch_names[0], out);    /* what a newer build adds starts at the init patch's value */
    for (j = 0; j < np && j < P_COUNT; j++) { int16_t v; bcopy_(&v, urec_at(i) + 12 + 2 * j, 2); out[j] = v; }
    return 0;
}
static int user_write(int slot, const char *name, const int16_t *vals)     /* name 0 = erase the slot; 0 = ok */
{
    int k = slot / UPER, i = slot % UPER, j, rc;
    uint8_t *r;
    char nm[12] = {0};
    if (uobj_load(k) && ((st_head_t *)sbuf)->nparams != P_COUNT) {
        int np = ((st_head_t *)sbuf)->nparams;
        if (np < P_COUNT) {                     /* patches saved by an older build: widen each record (the new parameters at the init patch's values), last first */
            int16_t init[P_COUNT];
            int r2, e;
            wt_patch_values(wt_patch_names[0], init);
            for (r2 = UPER - 1; r2 >= 0; r2--) {
                uint8_t *src = sbuf + sizeof(st_head_t) + (uint32_t)r2 * (12u + 2u * (uint32_t)np);
                uint8_t *dst = sbuf + sizeof(st_head_t) + (uint32_t)r2 * (12u + 2u * P_COUNT);
                int used = src[0] != 0;
                for (e = 12 + 2 * np - 1; e >= 0; e--) dst[e] = used ? src[e] : 0;
                for (e = np; e < P_COUNT; e++) { int16_t v = used ? init[e] : 0; bcopy_(dst + 12 + 2 * e, &v, 2); }
            }
            ((st_head_t *)sbuf)->nparams = P_COUNT; ((st_head_t *)sbuf)->n = UPER;
        } else {                                /* from a newer build: start afresh */
            for (j = 0; j < (int)sizeof sbuf; j++) sbuf[j] = 0;
            ((st_head_t *)sbuf)->magic = WTP_MAGIC; ((st_head_t *)sbuf)->nparams = P_COUNT; ((st_head_t *)sbuf)->n = UPER;
        }
    }
    r = urec_at(i);
    for (j = 0; j < 12 + 2 * P_COUNT; j++) r[j] = 0;
    if (name) {
        str_cpy(nm, name, 11);
        bcopy_(r, nm, 12);
        bcopy_(r + 12, vals, 2u * P_COUNT);
    }
    rc = plat_store_save(UOBJ[k], sbuf, sizeof(st_head_t) + UPER * (12u + 2u * P_COUNT));
    user_scan();
    return rc;
}
static int pat_save(void)
{
    int g, rc = 0;
    for (g = 0; g < 3; g++) {
        int first = g * 6, n = 16 - first < 6 ? 16 - first : 6;
        st_head_t *h = (st_head_t *)sbuf;
        h->magic = WTQ_MAGIC; h->nparams = sizeof(wt_pat_t); h->n = (uint16_t)n;
        bcopy_(sbuf + sizeof *h, &synth.seq.pat[first], (uint32_t)n * sizeof(wt_pat_t));
        rc |= plat_store_save(QOBJ[g], sbuf, (uint32_t)(sizeof *h + (uint32_t)n * sizeof(wt_pat_t)));
    }
    return rc;
}
static int pat_load(void)                       /* the saved patterns into the sequencer (before audio runs, or with it stopped); patterns found */
{
    int g, found = 0;
    for (g = 0; g < 3; g++) {
        int first = g * 6, n = 16 - first < 6 ? 16 - first : 6, got = plat_store_load(QOBJ[g], sbuf, sizeof sbuf);
        const st_head_t *h = (const st_head_t *)sbuf;
        if (got == (int)(sizeof *h + (uint32_t)n * sizeof(wt_pat_t)) && h->magic == WTQ_MAGIC && h->nparams == sizeof(wt_pat_t) && h->n == n) {
            bcopy_(&synth.seq.pat[first], sbuf + sizeof *h, (uint32_t)n * sizeof(wt_pat_t));
            found += n;
        }
    }
    return found;
}

/* the drums: the 8 patterns and every pot of both kits, in the settings object (3.1 KB of 3.8) */
#define WTD_MAGIC 0x31445457u                   /* "WTD1" */
static const uint8_t SETUP_ID[] = {P_MIDI_CH, P_MIDI_DRCH, P_MIDI_OUTCH, P_USB_AUDIO, P_SCALE, P_ROOT, P_SCALE_SEQ, P_CLK_SRC, P_CLK_SS};
#define NSETUP ((int)sizeof SETUP_ID)
#if WT_LFOSHAPES
/* (the editor's state, first) */
static wt_lfoshape_t ushp[WT_NUSHAPE];
static uint8_t ed_slot, ed_pt;
static int on_lfoed(void) { return PAGES[wui.page].kind == K_LFOED; }
static int ed_clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
#define LFS_MAGIC 0x31534C46u                   /* "FLS1": 8 fixed records (n, x[16], y[16], c[16]) at the very end of the setup object */
#define LFS_BYTES (4 + WT_NUSHAPE * (1 + 3 * WT_SHP_PTS))
#define LFS_RESERVE LFS_BYTES
static void lfs_init(void) { int k; for (k = 0; k < WT_NUSHAPE; k++) { wt_lfoshape_default(k, &ushp[k]); wt_synth_set_shape(&synth, k, &ushp[k]); } }
static uint8_t *lfs_put(uint8_t *q)
{
    int k, i;
    q[0] = 'F'; q[1] = 'L'; q[2] = 'S'; q[3] = '1'; q += 4;
    for (k = 0; k < WT_NUSHAPE; k++) {
        *q++ = ushp[k].n;
        for (i = 0; i < WT_SHP_PTS; i++) *q++ = ushp[k].x[i];
        for (i = 0; i < WT_SHP_PTS; i++) *q++ = (uint8_t)ushp[k].y[i];
        for (i = 0; i < WT_SHP_PTS; i++) *q++ = (uint8_t)ushp[k].c[i];
    }
    return q;
}
static int lfs_load(void)
{
    int got = plat_store_load(OBJ_SET, sbuf, sizeof sbuf), k, i;
    const uint8_t *q;
    if (got < LFS_BYTES + (int)sizeof(st_head_t)) return 0;
    q = sbuf + got - LFS_BYTES;
    if (q[0] != 'F' || q[1] != 'L' || q[2] != 'S' || q[3] != '1') return 0;
    q += 4;
    for (k = 0; k < WT_NUSHAPE; k++) {
        wt_lfoshape_t *sh = &ushp[k];
        sh->n = (uint8_t)ed_clamp(q[0], 2, WT_SHP_PTS); q++;
        for (i = 0; i < WT_SHP_PTS; i++) sh->x[i] = *q++;
        for (i = 0; i < WT_SHP_PTS; i++) sh->y[i] = (int8_t)ed_clamp((int8_t)*q++, -127, 127);
        for (i = 0; i < WT_SHP_PTS; i++) sh->c[i] = (int8_t)ed_clamp((int8_t)*q++, -100, 100);
        for (i = 1; i < sh->n; i++) if (sh->x[i] < sh->x[i - 1]) sh->x[i] = sh->x[i - 1];
        wt_synth_set_shape(&synth, k, sh);
    }
    return 1;
}
#else
#define LFS_RESERVE 0
#endif
#if WT_NODRUMS
/* WT-2 keeps MIDI / scale / clock (and the drawn shapes) in the setup object, and nothing of the drum machine.
 * Old WT-1 / early WT-2 objects (WTD1..WTD3) still yield those setup bytes: the pot block is a fixed 193 bytes,
 * which is what dr_npots() returned for both kits. The patterns are skipped. */
#define WTS_MAGIC 0x31535457u                   /* "WTS1": setup bytes, then the FLS1 shape footer */
#define WTD2_MAGIC 0x32445457u
#define WTD3_MAGIC 0x33445457u
#define WT_OLD_NPOTS 193
static void setup_take(const uint8_t *q, const uint8_t *end)
{
    int i;
    for (i = 0; i < NSETUP && q < end; i++, q++) wt_synth_set_param(&synth, SETUP_ID[i], *q);
}
static int drums_save(void)                     /* the name is historical: this stores the setup, not drums */
{
    st_head_t *h = (st_head_t *)sbuf;
    uint8_t *q = sbuf + sizeof *h;
    int i;
    h->magic = WTS_MAGIC; h->nparams = (uint16_t)NSETUP; h->n = 0;
    for (i = 0; i < NSETUP; i++) *q++ = (uint8_t)ui_p[SETUP_ID[i]];
#if WT_LFOSHAPES
    q = lfs_put(q);
#endif
    return plat_store_save(OBJ_SET, sbuf, (uint32_t)(q - sbuf));
}
static void dmir_init(void) { }
static int drums_load(void)
{
    int got = plat_store_load(OBJ_SET, sbuf, sizeof sbuf);
    const st_head_t *h = (const st_head_t *)sbuf;
    const uint8_t *q = sbuf + sizeof *h, *end = sbuf + (got > 0 ? got : 0);
    if (got < (int)sizeof *h) return 0;
    if (h->magic == WTS_MAGIC && h->nparams == NSETUP) { setup_take(q, end); return 1; }
    if ((h->magic == WTD2_MAGIC || h->magic == WTD3_MAGIC) && h->n == WT_NDPAT && h->nparams == WT_OLD_NPOTS) {
        if (q + WT_OLD_NPOTS > end) return 0;
        setup_take(q + WT_OLD_NPOTS, end);          /* pots, then the setup; patterns after that are ignored */
        return 1;
    }
    if (h->magic == WTD_MAGIC) {                    /* patterns (8 x 356), then pots, then setup */
        int base = (int)(sizeof *h + WT_NDPAT * 356u + WT_OLD_NPOTS);
        if (got != base && got != base + NSETUP && got != base + NSETUP - 2) return 0;
        setup_take(sbuf + sizeof *h + WT_NDPAT * 356u + WT_OLD_NPOTS, end);
        return 1;
    }
    return 0;
}
#else
static int dr_npots(void) { int n = 0, k, t; for (k = 0; k < 2; k++) for (t = 0; t < WT_DTRACKS; t++) n += wt_drums_nparams(k, t); return n; }
/* the drums: the kits' pots, the setup, then each pattern as its non-empty hits only (a 64-step pattern is 704 bytes dense, most of it zeros) */
#define WTD2_MAGIC 0x32445457u                  /* "WTD2" */
#define WTD3_MAGIC 0x33445457u                  /* "WTD3": as WTD2, and each hit also carries its extras (soft accent, probability) */
static int drums_save(void)
{
    st_head_t *h = (st_head_t *)sbuf;
    uint8_t *q = sbuf + sizeof *h, *end = sbuf + PLAT_STORE_MAX - LFS_RESERVE;
    int k, t, i, n, pi;
    if (sizeof *h + (uint32_t)dr_npots() + NSETUP + WT_NDPAT * 8u + LFS_RESERVE > PLAT_STORE_MAX) return -1;
    h->magic = WTD3_MAGIC; h->nparams = (uint16_t)dr_npots(); h->n = WT_NDPAT;
    for (k = 0; k < 2; k++) for (t = 0; t < WT_DTRACKS; t++) { n = wt_drums_nparams(k, t); for (i = 0; i < n; i++) *q++ = (uint8_t)dmir[k][t][i]; }
    for (i = 0; i < NSETUP; i++) *q++ = (uint8_t)ui_p[SETUP_ID[i]];                              /* the setup: MIDI channels, USB audio, scale, clock */
    for (pi = 0; pi < WT_NDPAT; pi++) {
        const wt_dpat_t *pp = &drums.pat[pi];
        uint8_t *cnt;
        int nh = 0;
        if (q + 8 > end) return -2;
        *q++ = pp->len; *q++ = pp->rate; *q++ = pp->swing; *q++ = pp->lstart; *q++ = pp->llen;
        cnt = q; q += 2;
        for (t = 0; t < WT_DTRACKS; t++)
            for (i = 0; i < WT_DSTEPS; i++)
                if (pp->st[t][i]) {
                    if (q + 4 > end) return -2;                                                 /* too many hits for one store object */
                    *q++ = (uint8_t)t; *q++ = (uint8_t)i; *q++ = pp->st[t][i]; *q++ = (uint8_t)dx_get(pp, t, i); nh++;
                }
        cnt[0] = (uint8_t)nh; cnt[1] = (uint8_t)(nh >> 8);
    }
#if WT_LFOSHAPES
    q = lfs_put(q);
#endif
    return plat_store_save(OBJ_SET, sbuf, (uint32_t)(q - sbuf));
}
static void dmir_init(void)
{
    int k, t, i;
    for (k = 0; k < 2; k++) for (t = 0; t < WT_DTRACKS; t++) { int n = wt_drums_nparams(k, t); for (i = 0; i < n && i < 12; i++) dmir[k][t][i] = (int8_t)wt_drums_get(&drums, k, t, i); }
}
static int drums_load(void)                     /* before the audio runs */
{
    int got = plat_store_load(OBJ_SET, sbuf, sizeof sbuf), k, t, i, n, pi;
    const st_head_t *h = (const st_head_t *)sbuf;
    const uint8_t *q = sbuf + sizeof *h, *end = sbuf + (got > 0 ? got : 0);
    if (got < (int)sizeof *h || h->n != WT_NDPAT || h->nparams != dr_npots()) return 0;
    if (h->magic == WTD2_MAGIC || h->magic == WTD3_MAGIC) {
        int rl = h->magic == WTD3_MAGIC ? 4 : 3;
        if (q + dr_npots() + NSETUP - 2 > end) return 0;
        for (k = 0; k < 2; k++) for (t = 0; t < WT_DTRACKS; t++) { n = wt_drums_nparams(k, t); for (i = 0; i < n; i++) { wt_drums_set(&drums, k, t, i, *q); dmir[k][t][i] = (int8_t)*q; q++; } }
        for (i = 0; i < NSETUP; i++) { wt_synth_set_param(&synth, SETUP_ID[i], *q); q++; }
        for (pi = 0; pi < WT_NDPAT; pi++) {
            wt_dpat_t *pp = &drums.pat[pi];
            int nh;
            if (q + 7 > end) return 1;
            wt_drums_clear(pp);
            pp->len = q[0]; pp->rate = q[1]; pp->swing = q[2]; pp->lstart = q[3]; pp->llen = q[4]; nh = q[5] | q[6] << 8; q += 7;
            if (pp->len < 1 || pp->len > WT_DSTEPS) pp->len = 16;
            for (i = 0; i < nh && q + rl <= end; i++, q += rl)
                if (q[0] < WT_DTRACKS && q[1] < WT_DSTEPS) { pp->st[q[0]][q[1]] = q[2]; if (rl == 4) dx_set(pp, q[0], q[1], q[3]); }
        }
        return 1;
    }
    if (h->magic == WTD_MAGIC) {                /* the older save: 32-step patterns, all of them, then the pots and the setup */
        int base = (int)(sizeof *h + WT_NDPAT * 356u + (uint32_t)dr_npots());
        if (got != base && got != base + NSETUP && got != base + NSETUP - 2) return 0;
        for (pi = 0; pi < WT_NDPAT; pi++) {
            wt_dpat_t *pp = &drums.pat[pi];
            wt_drums_clear(pp);
            for (t = 0; t < WT_DTRACKS; t++) for (i = 0; i < 32; i++) pp->st[t][i] = q[t * 32 + i];
            pp->len = q[352]; pp->rate = q[353]; pp->swing = q[354];
            q += 356;
        }
        for (k = 0; k < 2; k++) for (t = 0; t < WT_DTRACKS; t++) { n = wt_drums_nparams(k, t); for (i = 0; i < n; i++) { wt_drums_set(&drums, k, t, i, *q); dmir[k][t][i] = (int8_t)*q; q++; } }
        for (i = 0; i < NSETUP && q < end; i++) { wt_synth_set_param(&synth, SETUP_ID[i], *q); q++; }
        return 1;
    }
    return 0;
}
#endif /* WT_NODRUMS: setup store / the drum store */

static void wui_set_sname(const char *nm)       /* the name to start from: the loaded one (an old "U3 " prefix dropped) */
{
    int i = 0, k = 0;
    if (nm[0] == 'U' && nm[1] >= '0' && nm[1] <= '9') { int j = 1; while (nm[j] >= '0' && nm[j] <= '9') j++; if (nm[j] == ' ') i = j + 1; }
    for (; k < 11; k++) {
        char c = nm[i] ? nm[i++] : ' ';
        const char *cp = NCHARS;
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        while (*cp && *cp != c) cp++;
        wui.sname[k] = *cp ? c : ' ';
    }
    wui.sname[11] = 0;
}

static void wui_load_patch(int n)
{
    const char *nm;
    if (wpatch_pending) return;
    if (n < 0 || n >= NPLIST) n = 0;
    if (n < WT_NPATCHES) { wt_patch_values(wt_patch_names[n], wpatch); nm = wt_patch_names[n]; }
    else {
        if (user_read(n - WT_NPATCHES, wpatch)) return;
        nm = uname[n - WT_NPATCHES];
    }
    wui.patch = (int8_t)n;
    str_cpy(wui.pname, nm, 13);
    wui_set_sname(nm);
    wpatch_pending = 1;
    wq_put(WC_PATCH, 0, 0);
    {   uint32_t t0 = fm1_ms;                   /* the ISR takes it within a block (11 ms) */
        while (wpatch_pending && fm1_ms - t0 < 100u) fm1_wdt_feed();
    }
    wui_sync();
}
static void wui_step_patch(int dir)             /* the next factory or saved patch (empty slots are skipped) */
{
    int n = wui.patch, k;
    for (k = 0; k < NPLIST; k++) {
        n = (n + dir + NPLIST) % NPLIST;
        if (n < WT_NPATCHES || uname[n - WT_NPATCHES][0]) { wui_load_patch(n); return; }
    }
}

/* ---- patches over USB: SysEx  F0 7D 'W' 'T' cmd args F7  (7-bit bytes; an int16 is three). The editor sends one request and waits for the answer.
 *   01 HELLO                      -> 01 version (2) nparams-lo nparams-hi nslots
 *   02 LIST                       -> 02 nslots, then 11 name characters per slot (0 = empty)
 *   03 PUT offlo offhi cnt vals.. -> 03 status      (the patch is staged: off 0 starts it, the next off follows the last)
 *   05 COMMIT slot mode name[11]  -> 05 status      (mode 0 save to the slot, 1 play it now, 2 both)
 *   06 GET slot offlo offhi cnt   (slot 127: the sound playing now) -> 06 status offlo offhi cnt vals..   (offsets are 14 bits: a byte holds 7)
 *   07 ERASE slot                 -> 07 status
 * status 0 ok, 1 empty slot, 2 incomplete, 3 out of order, 4 bad slot, 5 busy, 6 flash failed */
static int16_t xstage[P_COUNT];
static int xstaged;
static int16_t xtmp[P_COUNT];
static int sx_rd16(const uint8_t *q) { return (int16_t)(uint16_t)(q[0] | (q[1] << 7) | (q[2] << 14)); }
static void sx_wr16(uint8_t *q, int v) { uint16_t u = (uint16_t)v; q[0] = (uint8_t)(u & 127); q[1] = (uint8_t)((u >> 7) & 127); q[2] = (uint8_t)(u >> 14); }
static uint32_t wui_sysex(const uint8_t *q, uint32_t n, uint8_t *o)       /* q: after the 7D 'W' 'T'; o: the whole answer F0 .. F7 (length returned, 0 = none) */
{
    uint32_t m = 0, i;
    int st = 0;
    o[m++] = 0xF0; o[m++] = 0x7D; o[m++] = 'W'; o[m++] = 'T';
    if (!n) return 0;
    switch (q[0]) {
    case 1: o[m++] = 1; o[m++] = 2; o[m++] = (uint8_t)(P_COUNT & 127); o[m++] = (uint8_t)(P_COUNT >> 7); o[m++] = NUSER; break;
    case 2:
        o[m++] = 2; o[m++] = NUSER;
        { int k; for (k = 0; k < NUSER; k++) for (i = 0; i < 11; i++) o[m++] = (uint8_t)(uname[k][i] & 127); }
        break;
    case 3: {
        int off = n > 3 ? q[1] | (q[2] << 7) : 0, cnt = n > 3 ? q[3] : 0, k;
        if (n != 4u + 3u * (uint32_t)cnt || off + cnt > P_COUNT) st = 3;
        else if (off != 0 && off != xstaged) st = 3;
        else { for (k = 0; k < cnt; k++) xstage[off + k] = (int16_t)sx_rd16(q + 4 + 3 * k); xstaged = off + cnt; }
        o[m++] = 3; o[m++] = (uint8_t)st; break; }
    case 5: {
        int slot = n >= 14 ? q[1] : 255, mode = n >= 14 ? q[2] : 0;
        char nm[12];
        if (n < 14u || slot >= NUSER) st = 4;
        else if (xstaged != P_COUNT) st = 2;
        else if (wpatch_pending) st = 5;
        else {
            int e = 11;
            for (i = 0; i < 11; i++) nm[i] = (char)(q[3 + i] ? q[3 + i] : ' ');
            nm[11] = 0;
            while (e > 0 && nm[e - 1] == ' ') e--;
            if (!e) { str_cpy(nm, "PATCH", 12); } else nm[e] = 0;
            if (mode == 0 || mode == 2) { wq_put(WC_ALLOFF, 0, 0); if (user_write(slot, nm, xstage)) st = 6; }
            if (!st && (mode == 1 || mode == 2)) {
                for (i = 0; i < P_COUNT; i++) wpatch[i] = xstage[i];
                str_cpy(wui.pname, nm, 13); wui_set_sname(nm);
                if (mode == 2) wui.patch = (int8_t)(WT_NPATCHES + slot);
                wpatch_pending = 1;
                wq_put(WC_PATCH, 0, 0);
                {   uint32_t t0 = fm1_ms; while (wpatch_pending && fm1_ms - t0 < 100u) fm1_wdt_feed(); }
                wui_sync();
            }
            wui.dirty = 1;
        }
        o[m++] = 5; o[m++] = (uint8_t)st; break; }
    case 6: {
        int slot = n > 4 ? q[1] : 255, off = n > 4 ? q[2] | (q[3] << 7) : 0, cnt = n > 4 ? q[4] : 0, k;
        if (n < 5u || (slot >= NUSER && slot != 127)) st = 4;
        else if (slot != 127 && !uname[slot][0]) st = 1;
        else if (off + cnt > P_COUNT || cnt > 64) st = 3;
        else if (slot == 127) { for (k = 0; k < P_COUNT; k++) xtmp[k] = synth.p[k]; }            /* 127: the sound playing now, as the knobs have it */
        else if (user_read(slot, xtmp)) st = 6;
        o[m++] = 6; o[m++] = (uint8_t)st;
        if (!st) { o[m++] = (uint8_t)(off & 127); o[m++] = (uint8_t)(off >> 7); o[m++] = (uint8_t)cnt; for (k = 0; k < cnt; k++) { sx_wr16(o + m, xtmp[off + k]); m += 3; } }
        break; }
    case 7: {
        int slot = n > 1 ? q[1] : 255;
        if (slot >= NUSER) st = 4; else { wq_put(WC_ALLOFF, 0, 0); if (user_write(slot, 0, 0)) st = 6; }
        o[m++] = 7; o[m++] = (uint8_t)st; break; }
    default: o[m++] = 0x7F; o[m++] = q[0]; break;
    }
    o[m++] = 0xF7;
    return m;
}

static void wui_save_press(void)                /* the SAVE page's button: the first press arms, the second does it */
{
    int rc = 0;
    if (!wui.armed) { wui.armed = 1; wui.armed_t = fm1_ms; wui.msg = 0; wui.dirty = 1; return; }
    wui.armed = 0;
    wq_put(WC_ALLOFF, 0, 0);
    if (wui.what == 4) { rc = user_write(wui.slot, 0, 0); wui.msg = (uint8_t)(rc ? 3 : 2); }
    else {
        if (wui.what == 0 || wui.what == 3) {
            char nm[12];
            int e = 11, k;
            while (e > 0 && wui.sname[e - 1] == ' ') e--;
            for (k = 0; k < e; k++) nm[k] = wui.sname[k];
            if (!e) { str_cpy(nm, "PATCH", 12); } else nm[e] = 0;
            rc |= user_write(wui.slot, nm, ui_p);
            if (!rc) { str_cpy(wui.pname, nm, 13); wui.patch = (int8_t)(WT_NPATCHES + wui.slot); }
        }
        if (wui.what == 1 || wui.what == 3) rc |= pat_save();
        if (wui.what == 2 || wui.what == 3) rc |= drums_save();
        wui.msg = (uint8_t)(rc ? 3 : 1);
    }
    wui.msg_t = fm1_ms; wui.dirty = 1;
}

#if WT_NODRUMS
static void mixer_init(void) { }
static int on_dsteps(void) { return 0; }
static int on_drum_page(void) { return 0; }
#else
static void mixer_init(void);
#endif
static void wui_init(void)
{
    int p, i;
    for (p = 0; p < NPAGE; p++)
        for (i = 0; i < 4; i++) pid[p][i] = PAGES[p].p[i] ? (int16_t)ui_find(PAGES[p].p[i]) : (int16_t)-1;
    for (i = 0; i < NMODMAP; i++) {
        int id = wt_param_find(MODMAP[i].name);
        if (id >= 0) { dest_of[id] = (int8_t)MODMAP[i].dest; dnum[id] = MODMAP[i].num; dden[id] = MODMAP[i].den; }
    }
    {   int w = 0, bl = 0;                      /* the 27 keys F3..G5: 16 white ones (the steps), 11 black ones (the patterns) */
        for (i = 0; i < NKEYS; i++) {
            int pc = (53 + i) % 12, white = pc == 0 || pc == 2 || pc == 4 || pc == 5 || pc == 7 || pc == 9 || pc == 11;
            if (white && w < 16) slot_key[w] = (int8_t)i;
            if (!white && bl < 11) bslot_key[bl] = (int8_t)i;
            white_slot[i] = (int8_t)(white ? w++ : -1);
            black_slot[i] = (int8_t)(white ? -1 : bl++);
        }
    }
    {   int i2;                                 /* before the audio runs: no queue, set the first patch directly */
        wt_patch_values(wt_patch_names[0], wpatch);
        for (i2 = 0; i2 < P_COUNT; i2++) wt_synth_set_param(&synth, i2, wpatch[i2]);
        wui_sync();
    }
    dmir_init();
#if WT_LFOSHAPES
    lfs_init();
#endif
    mixer_init();
    user_scan();
    store_stat[2] = (int8_t)(pat_load() > 0 ? 1 : -1);
#if WT_LFOSHAPES
    lfs_load();
#endif
    store_stat[3] = (int8_t)(drums_load() ? 1 : (plat_store_load(OBJ_SET, sbuf, sizeof sbuf) < 0 ? -1 : 0));
    wui_sync();
    str_cpy(wui.pname, wt_patch_names[0], 13);
    wui_set_sname(wt_patch_names[0]);
    wui.patch = 0; wui.page = 0; wui.oct = 0; wui.dirty = 1; wui.mod_src = S_LFO1; wui.held = -1; wui.dheld = -1; wui.last_note = 48; wui.rpk = -1;
#if WT_P37
    wui.fpend = 15; wui.ft_id = -1; wui.pheld = -1; wui.pl_pad = -1;             /* the faders do nothing until they meet their parameter's value */
#endif
    wui.gdens = 60; wui.grange = 2; wui.gspice = 30; wui.dgstyle = 0; wui.dgdens = 40; wui.navbtn = -1; wui.rfrom = 1; wui.rcnt = 1;
}

/* ---- modulation by hand ---- */
static int wui_slot_for(int src, int dest, int create)      /* the matrix slot holding src -> dest; a free one if create; -1 */
{
    int i, free_slot = -1;
    for (i = 0; i < 8; i++) {
        int s = ui_p[P_MX(i, 0)], d = ui_p[P_MX(i, 1)], a = ui_p[P_MX(i, 2)];
        if (s == src && d == dest) return i;
        if (free_slot < 0 && (s == 0 || d == 0 || a == 0)) free_slot = i;
    }
    if (create && free_slot >= 0) {
        ui_p[P_MX(free_slot, 0)] = (int16_t)src; ui_p[P_MX(free_slot, 1)] = (int16_t)dest; ui_p[P_MX(free_slot, 2)] = 0;
        wq_put(WC_PARAM, (uint8_t)P_MX(free_slot, 0), (int16_t)src);
        wq_put(WC_PARAM, (uint8_t)P_MX(free_slot, 1), (int16_t)dest);
        wq_put(WC_PARAM, (uint8_t)P_MX(free_slot, 2), 0);
        return free_slot;
    }
    return -1;
}

static void wui_mod_knob(int id, int32_t d)     /* hold SEL and turn: the selected source's amount on this parameter */
{
    int dest = dest_of[id], slot, v;
    if (!dest) return;
    slot = wui_slot_for(wui.mod_src, dest, 1);
    if (slot < 0) return;                       /* all eight slots are in use */
    if (d > 1 || d < -1) d *= 2;
    v = ui_p[P_MX(slot, 2)] + (int)d * 10;
    if (v < -1000) v = -1000;
    if (v > 1000) v = 1000;
    ui_p[P_MX(slot, 2)] = (int16_t)v;
    wq_put(WC_PARAM, (uint8_t)P_MX(slot, 2), (int16_t)v);
    wui.dirty = 1;
}

/* a pattern setting turned on a dial */
static void wui_vset(int id, int v)
{
    wt_pat_t *q = cur_pat();
    switch (id) {
    case VP_LEN: q->len = (uint8_t)v; if (wui.sp * 16 >= v) wui.sp = (uint8_t)((v - 1) / 16); break;
    case VP_RATE: q->rate = (uint8_t)v; break;
    case VP_SWING: q->swing = (uint8_t)v; break;
    case VP_DIR: q->dir = (uint8_t)v; break;
    case VP_TRANS: q->trans = (uint8_t)(v + 24); break;
    case VP_PAT: wq_put(WC_QUEUE, (uint8_t)(v - 1), 0); wui.sp = 0; break;
    case VP_GDENS: wui.gdens = (uint8_t)v; break;
    case VP_GRANGE: wui.grange = (uint8_t)v; break;
    case VP_GSPICE: wui.gspice = (uint8_t)v; break;
    case VP_GROLL: if (v) { wt_seq_generate(&synth, synth.seq.cur, wui.gdens, wui.grange, wui.gspice); wui.groll = 1; wui.clear_t = fm1_ms; } break;
    case VP_DGSTYLE: wui.dgstyle = (uint8_t)v; break;
    case VP_LDBL: case VP_LHALF: case VP_RGO: {
        int dr = ops_on_drums(), r = 0, len;
        if (!v) break;
        if (id == VP_LDBL) {
#if WT_NODRUMS
            r = wt_pat_double(cur_pat());
#else
            r = dr ? wt_dpat_double(&drums.pat[drums.cur % WT_NDPAT]) : wt_pat_double(cur_pat());
#endif
            if (r) { wui.opf |= 1; ui_toast_len("DOUBLED", r); } else ui_toast("ALREADY AT THE MOST", "64 STEPS, 4 BARS");
        } else if (id == VP_LHALF) {
#if WT_NODRUMS
            r = wt_pat_halve(cur_pat(), v < 0);
#else
            r = dr ? wt_dpat_halve(&drums.pat[drums.cur % WT_NDPAT], v < 0) : wt_pat_halve(cur_pat(), v < 0);
#endif
            wui.opf |= (uint8_t)(v < 0 ? 4 : 2); ui_toast_len(v < 0 ? "KEPT THE 2ND HALF" : "KEPT THE 1ST HALF", r);
        } else {
#if WT_NODRUMS
            r = wt_pat_remove(cur_pat(), wui.rfrom, wui.rcnt);
#else
            r = dr ? wt_dpat_remove(&drums.pat[drums.cur % WT_NDPAT], wui.rfrom, wui.rcnt) : wt_pat_remove(cur_pat(), wui.rfrom, wui.rcnt);
#endif
            if (r) { wui.opf |= 8; ui_toast_len("BARS REMOVED", r); } else ui_toast("NOTHING REMOVED", "THAT BAR IS NOT THERE");
        }
#if WT_NODRUMS
        len = cur_pat()->len;
        (void)dr;
#else
        len = dr ? drums.pat[drums.cur % WT_NDPAT].len : cur_pat()->len;
#endif
        if (dr) { if (wui.dsp * 16 >= len) wui.dsp = (uint8_t)((len - 1) / 16); } else if (wui.sp * 16 >= len) wui.sp = (uint8_t)((len - 1) / 16);
        wui.clear_t = fm1_ms;
        break; }
    case VP_LLOOP: {
#if WT_NODRUMS
        int ok = wt_pat_loop(cur_pat(), v);
#else
        int ok = ops_on_drums() ? wt_dpat_loop(&drums.pat[drums.cur % WT_NDPAT], v) : wt_pat_loop(cur_pat(), v);
#endif
        if (!ok) ui_toast("NO SUCH BAR", "THE PATTERN IS SHORTER");
        break; }
    case VP_RFROM: wui.rfrom = (uint8_t)v; break;
    case VP_RCNT: wui.rcnt = (uint8_t)v; break;
    case VP_DGDENS: wui.dgdens = (uint8_t)v; break;
#if !WT_NODRUMS
    case VP_DGROLL: if (v) { wt_drums_generate(&synth, drums.cur, wui.dgstyle, wui.dgdens); wui.dgroll = 1; wui.clear_t = fm1_ms; } break;
#endif
    case VP_MD0: case VP_MD1: case VP_MD2: case VP_MD3: {
        int sl = src_slot(MSRC[id - VP_MD0], 1);
        if (sl < 0) break;                          /* all eight matrix slots are in use */
        ui_p[P_MX(sl, 1)] = (int16_t)v; wq_put(WC_PARAM, (uint8_t)P_MX(sl, 1), (int16_t)v);
        if (v && ui_p[P_MX(sl, 2)] == 0) { ui_p[P_MX(sl, 2)] = 500; wq_put(WC_PARAM, (uint8_t)P_MX(sl, 2), 500); }     /* a destination with nothing going to it would be silent */
        break; }
    case VP_MA0: case VP_MA1: case VP_MA2: case VP_MA3: {
        int sl = src_slot(MSRC[id - VP_MA0], 0);
        if (sl < 0) break;                          /* choose where it goes first */
        ui_p[P_MX(sl, 2)] = (int16_t)v; wq_put(WC_PARAM, (uint8_t)P_MX(sl, 2), (int16_t)v);
        break; }
#if !WT_NODRUMS
    case VP_DPAT: wq_put(WC_DQUEUE, (uint8_t)(v - 1), 0); wui.dsp = 0; break;
    case VP_DLEN: drums.pat[drums.cur % WT_NDPAT].len = (uint8_t)v; if (wui.dsp * 16 >= v) wui.dsp = (uint8_t)((v - 1) / 16); break;
    case VP_DRATE: drums.pat[drums.cur % WT_NDPAT].rate = (uint8_t)v; break;
    case VP_DSWING: drums.pat[drums.cur % WT_NDPAT].swing = (uint8_t)v; break;
    case VP_DCLEAR: if (v) { wt_drums_clear(&drums.pat[drums.cur % WT_NDPAT]); wui.dclear = 1; wui.clear_t = fm1_ms; } break;
#endif
    case VP_NPOS: wui.npos = (uint8_t)(v - 1); break;
    case VP_NCH: wui.sname[wui.npos % 11] = NCHARS[v % NNCH]; wui.armed = 0; wui.msg = 0; break;
    case VP_SLOT: wui.slot = (uint8_t)(v - 1); wui.armed = 0; wui.msg = 0; break;
    case VP_WHAT: wui.what = (uint8_t)v; wui.armed = 0; wui.msg = 0; break;
    default:                                    /* VP_CLEAR: turned up, the pattern is emptied */
        if (v) { wt_seq_clear(q); wui.clear_msg = 1; wui.clear_t = fm1_ms; }
        break;
    }
}

static int on_mod_page(void) { const char *t = PAGES[wui.page].title; return t[0] == 'M' && t[1] == 'O' && t[2] == 'D'; }   /* the matrix list pages: their knobs edit the slots */

#if !WT_NODRUMS
static void dsound_knob(int k, int32_t d);
#endif
#if WT_LFOSHAPES
/* ---- the LFO shape editor: breakpoints (x, height, curve) of the 8 drawn shapes ---- */
static void lfoed_apply(void) { wt_synth_set_shape(&synth, ed_slot, &ushp[ed_slot]); wui.dirty = 1; }
static void lfoed_knob(int k, int32_t d)
{
    wt_lfoshape_t *sh;
    int step = d > 0 ? 1 : -1, draw = PAGES[wui.page].l[3] != 0, n;
    if (!d) return;
    if (k == 0) { ed_slot = (uint8_t)((ed_slot + step) & (WT_NUSHAPE - 1)); wui.dirty = 1; }
    sh = &ushp[ed_slot]; n = sh->n;
    if (ed_pt >= n) ed_pt = (uint8_t)(n - 1);
    if (k == 1) { ed_pt = (uint8_t)ed_clamp(ed_pt + step, 0, n - 1); wui.dirty = 1; }
    else if (k == 2 && draw) { sh->y[ed_pt] = (int8_t)ed_clamp(sh->y[ed_pt] + (int)d * 4, -127, 127); lfoed_apply(); }
    else if (k == 2) { if (ed_pt == 0) ed_pt = 1; sh->c[ed_pt] = (int8_t)ed_clamp(sh->c[ed_pt] + (int)d * 8, -100, 100); lfoed_apply(); }
    else if (k == 3 && draw && ed_pt > 0 && ed_pt < n - 1) { sh->x[ed_pt] = (uint8_t)ed_clamp(sh->x[ed_pt] + (int)d * 3, sh->x[ed_pt - 1], sh->x[ed_pt + 1]); lfoed_apply(); }
}
static void lfoed_key(int slot)                 /* the black keys: add a point, delete it, reset the shape, flip it, flatten its curves */
{
    wt_lfoshape_t *sh = &ushp[ed_slot];
    int i, b;
    if (ed_pt >= sh->n) ed_pt = (uint8_t)(sh->n - 1);
    switch (slot) {
    case 0:
        if (sh->n >= WT_SHP_PTS) break;
        b = ed_pt < sh->n - 1 ? ed_pt : sh->n - 2;
        for (i = sh->n; i > b + 1; i--) { sh->x[i] = sh->x[i - 1]; sh->y[i] = sh->y[i - 1]; sh->c[i] = sh->c[i - 1]; }
        sh->x[b + 1] = (uint8_t)((sh->x[b] + sh->x[b + 2]) / 2); sh->y[b + 1] = (int8_t)((sh->y[b] + sh->y[b + 2]) / 2); sh->c[b + 1] = 0;
        sh->n++; ed_pt = (uint8_t)(b + 1); break;
    case 1:
        if (sh->n <= 2 || ed_pt == 0 || ed_pt >= sh->n - 1) break;
        for (i = ed_pt; i < sh->n - 1; i++) { sh->x[i] = sh->x[i + 1]; sh->y[i] = sh->y[i + 1]; sh->c[i] = sh->c[i + 1]; }
        sh->n--; break;
    case 2: wt_lfoshape_default(ed_slot, sh); ed_pt = 0; break;
    case 3: for (i = 0; i < sh->n; i++) sh->y[i] = (int8_t)-sh->y[i]; break;
    case 4: for (i = 0; i < sh->n; i++) sh->c[i] = 0; break;
    case 5: case 6:                                          /* bend the segment that ends at this point: up, down */
        if (ed_pt == 0) ed_pt = 1;
        sh->c[ed_pt] = (int8_t)ed_clamp(sh->c[ed_pt] + (slot == 5 ? 25 : -25), -100, 100); break;
    default:                                                 /* 7, 8: LFO 1 / LFO 2 play this shape */
        ui_p[P_LFO(slot - 7, 0)] = (int16_t)(7 + ed_slot); wq_put(WC_PARAM, (uint8_t)P_LFO(slot - 7, 0), (int16_t)(7 + ed_slot));
        str_cpy(wui.toast, slot == 7 ? "LFO 1" : "LFO 2", 23); str_cpy(wui.toast2, wt_lfoshape_name(ed_slot), 23); wui.toast_t = fm1_ms ? fm1_ms : 1;
        break;
    }
    lfoed_apply();
}
#endif
#if !WT_NODRUMS
static void dsteps_knob(int k, int32_t d);
static void mixer_knob(int k, int32_t d);
#endif

static void wui_knob(int k, int32_t d)
{
    int id = pid[wui.page][k], span, step, v;
#if !WT_NODRUMS
    if (PAGES[wui.page].kind == K_DSOUND) { dsound_knob(k, d); return; }
    if (PAGES[wui.page].kind == K_MIXER) { mixer_knob(k, d); return; }
#endif
#if WT_LFOSHAPES
    if (PAGES[wui.page].kind == K_LFOED) { lfoed_knob(k, d); return; }
#endif
#if !WT_NODRUMS
    if (PAGES[wui.page].kind == K_DSTEPS) { dsteps_knob(k, d); return; }
#endif
    if (id < 0) return;
    if (id == VP_LDBL || id == VP_LHALF || id == VP_RGO) {      /* the pattern edit dials act on every turn (clockwise; HALVE: either way, picking the half to keep) */
        if (id == VP_LHALF) wui_vset(id, d > 0 ? 1 : -1); else if (d > 0) wui_vset(id, 1);
        wui.dirty = 1;
        return;
    }
    if (wui.sel_down && (dest_of[id] || !on_mod_page())) { wui.sel_used = 1; wui_mod_knob(id, d); return; }
    span = pmax(id) - pmin(id);
    step = span >= 200 ? span / 100 : 1;
    if (d > 1 || d < -1) d *= 2;
    v = ui_p[id] + (int)d * step;
    if (v < pmin(id)) v = pmin(id);
    if (v > pmax(id)) v = pmax(id);
    if (id == P_OSC(0, O_WTPOS) || id == P_OSC(1, O_WTPOS)) {   /* WT POS stops on each stored frame (a clean saw, square ...) when the knob passes it */
        const wt_table_t *tb = &wtab[ui_p[id - O_WTPOS + O_TABLE] % WT_NTABLES];
        int fi, old = ui_p[id];
        for (fi = 0; fi < tb->nsrc && tb->nsrc > 1 && (v > old ? v - old : old - v) <= 3 * step; fi++) {        /* (a fast spin passes through) */
            int fp = fi * 1000 / (tb->nsrc - 1);
            if ((old < fp && v >= fp) || (old > fp && v <= fp)) { v = fp; break; }
        }
    }
    if (v == ui_p[id]) return;
    ui_p[id] = (int16_t)v;
    if (id >= P_COUNT) { vp_t[id - P_COUNT] = fm1_ms; wui_vset(id, v); }
    else wq_put(WC_PARAM, (uint8_t)id, (int16_t)v);
    wui.dirty = 1;
}

static int on_steps_page(void) { return PAGES[wui.page].kind == K_STEPS; }

/* hold a page button and press a white key: jump straight to one of its pages (the keys light, the screen lists them) */
static const struct { int8_t btn; const char *name; const char *t[9]; } NAV[] = {
#if WT_W2
    {B_SEL, "OSC A", {"OSC A", "A WARP", "A TABLE", "A TUNE", 0, 0, 0, 0}},
    {B_EDIT, "OSC B", {"OSC B", "B WARP", "B TABLE", "B TUNE", 0, 0, 0, 0}},
    {B_HOME, "FILTER", {"FILTER", "FILTER 2", "SUB", "SUB 2", "NOISE", "NOISE 2", 0, 0}},
    {B_ARP, "ARP", {"ARP", "ARP 2", 0, 0, 0, 0, 0, 0}},
#else
    {B_EDIT, "EDIT", {"OSC A", "OSC B", "FILTER", "SUB", "NOISE", 0, 0, 0}},
#endif
#if WT_NODRUMS
#if WT_LFOSHAPES
    {B_FX, "FX", {"SYNTH FX", "CHORUS", "COMPRESS", "DELAY", "REVERB", "EQ", 0, 0}},
#else
    {B_FX, "FX", {"MASTER FX", "SYNTH FX", "DELAY", "REVERB", "EQ", 0, 0, 0}},
#endif
#else
    {B_FX, "FX", {"MASTER FX", "SYNTH FX", "DRUM FX", "SIDECHAIN", "DELAY", "REVERB", "EQ", 0}},
#endif
    {B_ENV, "ENV", {"AMP ENV", "ENV 2", "ENV 3", "ENV 2 GOES TO", 0, 0, 0, 0}},
#if WT_W2
    {B_LFO, "LFO", {"LFO 1", "LFO 1 GOES TO", "LFO 2", "LFO 2 GOES TO", "SHAPE DRAW", "SHAPE CURVE", "MOD 1", "MOD 2", "MOD 3"}},
#else
    {B_LFO, "LFO", {"LFO 1", "LFO 1 GOES TO", "LFO 2", "LFO 2 GOES TO", "MOD 1", 0, 0, 0}},
#endif
#if WT_NODRUMS
#else
    {B_HOME, "HOME", {"PERFORM", "MIXER", "DRUMS", "DRUM KIT", "SOUND 1", "DRUM GENERATE", "DRUM ACCENT", "DRUM LENGTH", "DRUM REMOVE BARS"}},
#endif
    {B_SEQ, "SEQ", {"STEPS", "PATTERN", "PATTERN LENGTH", "REMOVE BARS", "GENERATE", "PLAY SETUP", 0, 0}},
#if WT_W2
    {B_GLO, "GLOBAL", {"PERFORM", "VOICE", "UNISON", "LEVELS", "SCALE", "MIDI", "SYNC", "PERFORMANCE", "ABOUT"}},
#else
    {B_GLO, "GLOBAL", {"VOICE", "LEVELS", "SCALE", "MIDI", "SYNC", "PERFORMANCE", 0, 0}},
#endif
};
#define NNAV ((int)(sizeof NAV / sizeof NAV[0]))
static int nav_index(int btn) { int i; for (i = 0; i < NNAV; i++) if (NAV[i].btn == btn) return i; return -1; }
static int nav_count(int ni) { int n = 0; while (n < 9 && NAV[ni].t[n]) n++; return n; }
static int title_page(const char *t)
{
    int i;
    for (i = 0; i < NPAGE; i++) { const char *a = PAGES[i].title, *b = t; while (*a && *a == *b) { a++; b++; } if (!*a && !*b) return i; }
    return -1;
}
static int page_or(const char *t) { int p = title_page(t); return p < 0 ? wui.page : p; }      /* a page this build may not have: stay where we are */
static void nav_jump(int slot)
{
    int ni = nav_index(wui.navbtn), pg;
    if (ni < 0 || slot >= nav_count(ni)) return;
    pg = title_page(NAV[ni].t[slot]);
    if (pg >= 0) { wui.page = (int8_t)pg; wui.held = -1; wui.dheld = -1; wui.dirty = 1; }
    wui.nav_used = 1;
}

#if WT_W2
static int is_mod_page(int pg) { const char *t = PAGES[pg].title; return t[0] == 'M' && t[1] == 'O' && t[2] == 'D'; }
#endif
static void wui_button(int btn)                 /* a button was pressed: its first page, or its next one if it is already showing */
{
    int i, first = -1, next = -1;
#if WT_W2
    int in_mod = btn == B_LFO && is_mod_page(wui.page);      /* LFO's pages and its MOD pages are two rings */
#endif
    for (i = 0; i < NPAGE; i++)
        if (PAGES[i].btn == btn
#if WT_W2
            && (btn != B_LFO || is_mod_page(i) == in_mod)
#endif
            ) {
            if (first < 0) first = i;
            if (next < 0 && i > wui.page && PAGES[wui.page].btn == btn) next = i;
        }
    if (first < 0) return;                      /* ARP, SAVE: not yet */
    wui.page = (int8_t)(PAGES[wui.page].btn == btn ? (next >= 0 ? next : first) : first);
    wui.dirty = 1;
}

/* ---- the step grid ---- */
/* A note's length is stored in its step (flags bits 4..7: 1..16 steps). It sounds that long whatever the steps after it hold, so a long
 * note rings on under later notes (poly mode); the gate says how much of its last step it sounds. */
static int step_len(int r) { return ((cur_pat()->step[r % WT_NSTEP].flags >> 4) & 15) + 1; }
static int step_covered(int i)                  /* a note started before step i is still sounding in it */
{
    const wt_pat_t *q = cur_pat();
    int k;
    for (k = i > 15 ? i - 15 : 0; k < i; k++) { const wt_step_t *st = &q->step[k]; if ((st->flags & SF_ON) && st->note[0] && k + (((st->flags >> 4) & 15) + 1) > i) return 1; }
    return 0;
}
static void step_press(int slot, uint32_t b)    /* a white key went down on the STEPS page */
{
    int i = wui.sp * 16 + slot;
    wt_step_t *st;
    if (i >= WT_NSTEP) return;
    st = &cur_pat()->step[i];
    wui.held = (int8_t)i; wui.held_used = 0;
    if (b & (1u << B_ENV)) { st->flags ^= SF_ACCENT; wui.held_used = 1; }                    /* ENV held: accent */
    else if (b & (1u << B_LFO)) { st->flags ^= SF_SLIDE; wui.held_used = 1; }                /* LFO held: slide */
    else if (!st->note[0] && !(st->flags & SF_TIE)) {                                          /* an empty step: on at once, with the last note */
        st->note[0] = wui.last_note; st->vel = 100; st->gate = 60; st->prob = 100; st->flags = SF_ON; wui.held_used = 1;
        wq_put(WC_ON, st->note[0], st->vel); wui.keynote[slot_key[slot < 0 ? 0 : slot % 16]] = st->note[0];
    }
    wui.dirty = 1;
}

static void step_release(void)
{
    if (wui.held >= 0 && !wui.held_used) {      /* a tap on a step that had a note: switch it (and its ties) on / off */
        wt_step_t *st = &cur_pat()->step[wui.held];
        st->flags ^= SF_ON;
    }
    if (wui.held >= 0) { int k = slot_key[wui.held % 16]; if (wui.keynote[k]) { wq_put(WC_OFF, wui.keynote[k], 0); wui.keynote[k] = 0; } }
    wui.held = -1; wui.dirty = 1;
}

/* a step is held: K1 pitch, K2 velocity, K3 gate (how much of its last step it sounds; with ARP held, the chance), K4 length (steps the note lasts) */
static void step_knob(int k, int32_t d)
{
    wt_pat_t *q = cur_pat();
    wt_step_t *st = &q->step[wui.held];
    int v;
    wui.held_used = 1;
    if (k == 3) {                               /* length: clockwise longer, anticlockwise shorter (1..16 steps) */
        int L = step_len(wui.held) + (d > 0 ? 1 : -1) * (int)(d > 0 ? d : -d);
        L = L < 1 ? 1 : L > 16 ? 16 : L;
        st->flags = (uint8_t)((st->flags & 15) | ((L - 1) << 4));
        wui.dirty = 1;
        return;
    }
    if (d > 1 || d < -1) d *= 2;
    if (k == 0) {
        v = st->note[0] ? st->note[0] : wui.last_note;
        if (ui_p[P_SCALE]) { int c2 = d > 0 ? (int)d : (int)-d; while (c2--) v = wt_scale_step(ui_p[P_SCALE], ui_p[P_ROOT], v, d > 0 ? 1 : -1); }       /* scale mode: a detent is the next note of the scale */
        else v += (int)d;
        st->note[0] = (uint8_t)(v < 1 ? 1 : v > 127 ? 127 : v);
        st->flags |= SF_ON; wui.last_note = st->note[0];
        wq_put(WC_ON, st->note[0], st->vel); wq_put(WC_OFF, st->note[0], 0);       /* a blip to hear it */
    } else if (k == 1) { v = st->vel + (int)d * 3; st->vel = (uint8_t)(v < 1 ? 1 : v > 127 ? 127 : v); }
    else if ((wui.bprev >> B_ARP) & 1u) { v = st->prob + (int)d * 5; st->prob = (uint8_t)(v < 5 ? 5 : v > 100 ? 100 : v); }
    else { v = st->gate + (int)d * 5; st->gate = (uint8_t)(v < 5 ? 5 : v > 100 ? 100 : v); }
    wui.dirty = 1;
}

/* REC: a key played goes into the pattern, and how long you hold it is the note's length. Playing: the length is the steps that passed
 * between press and release. Stopped (step by step): the time held in step lengths, then the next note goes after it. */
static int steps_per_beat(void) { static const uint8_t SPB[4] = {4, 6, 8, 3}; return SPB[cur_pat()->rate & 3]; }
static void rec_finish_pending(uint32_t now)    /* the step-by-step note being held ends: its length, and where the next one goes */
{
    wt_pat_t *q = cur_pat();
    uint32_t step_ms, held, L;
    wt_step_t *st;
    if (wui.rpk < 0) return;
    st = &q->step[wui.ri % WT_NSTEP];
    step_ms = 60000u / ((uint32_t)(ui_p[P_BPM] < 20 ? 20 : ui_p[P_BPM]) * (uint32_t)steps_per_beat());
    held = now - wui.rt0;
    L = (held + step_ms / 2u) / (step_ms ? step_ms : 1u);
    L = L < 1u ? 1u : L > 16u ? 16u : L;
    st->flags = (uint8_t)((st->flags & 15u) | ((L - 1u) << 4));
    wui.rec_pos = (uint8_t)((wui.ri + L) % q->len);
    wui.rpk = -1; wui.dirty = 1;
}
static void record_note(int note, int key)
{
    wt_pat_t *q = cur_pat();
    int i;
    wt_step_t *st;
    if (synth.seq.run && synth.seq.pos >= 0) {      /* live: the nearer step (a note a little early belongs to the next one) */
        i = synth.seq.pos + (synth.seq.phase8 > 128 ? 1 : 0);
        if (i >= q->len) i = 0;
        wui.recst[key] = (uint8_t)(i + 1); wui.rec8[key] = (uint32_t)synth.seq.pos * 256u + synth.seq.phase8;
    } else {                                        /* stopped: step by step */
        rec_finish_pending(fm1_ms);                 /* the note still held (a chord of notes pressed one after another) ends here */
        i = wui.rec_pos;
        if (i >= q->len) i = 0;
        wui.rpk = (int8_t)key; wui.ri = (uint8_t)i; wui.rt0 = fm1_ms;
        wui.rec_pos = (uint8_t)(i + 1 >= q->len ? 0 : i + 1);
    }
    st = &q->step[i];
    st->note[0] = (uint8_t)note; st->note[1] = st->note[2] = st->note[3] = 0;
    st->vel = 100; st->gate = 70; st->prob = 100; st->flags = SF_ON;
    wui.last_note = (uint8_t)note;
    wui.dirty = 1;
}
static void rec_release(int key)                    /* a recorded key is let go */
{
    wt_pat_t *q = cur_pat();
    if (wui.rpk == key) { rec_finish_pending(fm1_ms); return; }
    if (wui.recst[key]) {
        int idx = wui.recst[key] - 1;
        uint32_t span = (uint32_t)q->len * 256u, now = (uint32_t)(synth.seq.pos < 0 ? 0 : synth.seq.pos) * 256u + synth.seq.phase8, delta, L;
        wt_step_t *st = &q->step[idx % WT_NSTEP];
        wui.recst[key] = 0;
        if (!synth.seq.run || !span || !(st->flags & SF_ON)) return;
        delta = (now + span - wui.rec8[key] % span) % span;
        L = (delta + 128u) / 256u;
        L = L < 1u ? 1u : L > 16u ? 16u : L;
        st->flags = (uint8_t)((st->flags & 15u) | ((L - 1u) << 4));
        wui.dirty = 1;
    }
}

#if !WT_NODRUMS
/* ---- the drums ---- */
static int on_dsteps(void) { return PAGES[wui.page].kind == K_DSTEPS; }
static int on_drum_page(void) { const wpage_t *g = &PAGES[wui.page]; return g->kind == K_DSTEPS || g->kind == K_DSOUND || g->kind == K_MIXER || (g->title[0] == 'D' && g->title[1] == 'R' && g->title[2] == 'U'); }
static wt_dpat_t *dpat(void) { return &drums.pat[drums.cur % WT_NDPAT]; }

static void dpad(int t, int vel, int write)                       /* a black key on a drum page: select the track, hit it; with REC on and the drums playing, write it */
{
    wui.dtrack = (uint8_t)t; wui.mch = (uint8_t)(t + 2);
    wq_put(WC_DHIT, (uint8_t)t, (int16_t)vel);
    if (write && wui.rec && drums.run && drums.pos >= 0) {
        wt_dpat_t *q = dpat();
        int i = drums.pos + (drums.phase8 > 128 ? 1 : 0);
        if (i >= q->len) i = 0;
        q->st[t][i] = (uint8_t)vel; dx_set(q, t, i, 0);
    }
    wui.dirty = 1;
}
static void dstep_press(int slot, uint32_t b)
{
    int i = wui.dsp * 16 + slot;
    uint8_t *st;
    if (i >= WT_DSTEPS) return;
    st = &dpat()->st[wui.dtrack][i];
    wui.dheld = (int8_t)i; wui.dheld_used = 0;
    if (b & (1u << B_ENV)) { *st = (uint8_t)(*st ? *st ^ DS_ACCENT : 100 | DS_ACCENT); wui.dheld_used = 1; }
    else if (!*st) { *st = 100; wui.dheld_used = 1; }
    wui.dirty = 1;
}
static void dstep_release(void)
{
    if (wui.dheld >= 0 && !wui.dheld_used) dpat()->st[wui.dtrack][wui.dheld] = 0;     /* a tap on a step that was on: off */
    wui.dheld = -1; wui.dirty = 1;
}
static void dsteps_knob(int k, int32_t d)       /* DRUMS page: K1 the track; with a step held K2 its velocity */
{
    if (d > 1 || d < -1) d *= 2;
    if (k == 0 && wui.dheld < 0) { int t = wui.dtrack + (int)d; wui.dtrack = (uint8_t)(t < 0 ? 0 : t >= WT_DTRACKS ? WT_DTRACKS - 1 : t); wui.dirty = 1; }
    else if (k == 1 && wui.dheld >= 0) {
        uint8_t *st = &dpat()->st[wui.dtrack][wui.dheld];
        int v = (*st & 127) + (int)d * 3;
        if (!*st) return;
        *st = (uint8_t)((*st & DS_ACCENT) | (v < 1 ? 1 : v > 127 ? 127 : v));
        wui.dheld_used = 1; wui.dirty = 1;
    }
}
/* ---- the mixer: the synth, the drum machine, and each drum track (its Level and Pan pots) ---- */
static int8_t lvl_idx[2][WT_DTRACKS], pan_idx[2][WT_DTRACKS];
static int pot_named(int kit, int t, const char *nm)
{
    int n = wt_drums_nparams(kit, t), i;
    for (i = 0; i < n; i++) { const char *a = wt_drums_param(kit, t, i)->name, *b = nm; while (*a && ((*a | 32) == (*b | 32))) { a++; b++; } if (!*a && !*b) return i; }
    return -1;
}
static void mixer_init(void)
{
    int k, t;
    for (k = 0; k < 2; k++) for (t = 0; t < WT_DTRACKS; t++) { lvl_idx[k][t] = (int8_t)pot_named(k, t, "level"); pan_idx[k][t] = (int8_t)pot_named(k, t, "pan"); }
}
static int mixer_value(int c)                   /* a channel's level, 0..1000 */
{
    int kit = ui_p[P_DR_KIT] & 1, t = c - 2;
    if (c == 0) return ui_p[P_SYN_LEVEL];
    if (c == 1) return ui_p[P_DR_LEVEL];
    return lvl_idx[kit][t] >= 0 ? dmir[kit][t][lvl_idx[kit][t]] * 1000 / 127 : 0;
}
static int mixer_state(int c)                   /* 0 normal, 1 muted, 2 solo */
{
    return c == 0 ? ui_p[P_SYN_MUTE] : c == 1 ? ui_p[P_DR_MUTE] : drums.mute[c - 2];
}
static void mixer_set_state(int c, int v)
{
    v = v < 0 ? 0 : v > 2 ? 2 : v;
    if (c == 0) { ui_p[P_SYN_MUTE] = (int16_t)v; wq_put(WC_PARAM, P_SYN_MUTE, (int16_t)v); }
    else if (c == 1) { ui_p[P_DR_MUTE] = (int16_t)v; wq_put(WC_PARAM, P_DR_MUTE, (int16_t)v); }
    else { drums.mute[c - 2] = (uint8_t)v; wq_put(WC_DMUTE, (uint8_t)(c - 2), (int16_t)v); }
}
static void mixer_knob(int k, int32_t d)
{
    int kit = ui_p[P_DR_KIT] & 1, c = wui.mch, t = c - 2, v;
    if (k == 0) { c += d > 0 ? 1 : -1; wui.mch = (uint8_t)(c < 0 ? 0 : c > 12 ? 12 : c); if (wui.mch >= 2) wui.dtrack = (uint8_t)(wui.mch - 2); wui.dirty = 1; return; }
    if (k == 3) {                               /* K4: normal -> MUTE -> SOLO (clockwise), and back (anticlockwise) */
        mixer_set_state(c, mixer_state(c) + (d > 0 ? 1 : -1));
        wui.dirty = 1;
        return;
    }
    if (d > 1 || d < -1) d *= 2;
    if (k == 1) {                               /* level */
        if (c < 2) {
            int id = c == 0 ? P_SYN_LEVEL : P_DR_LEVEL;
            v = ui_p[id] + (int)d * 20; v = v < 0 ? 0 : v > 1000 ? 1000 : v;
            if (v != ui_p[id]) { ui_p[id] = (int16_t)v; wq_put(WC_PARAM, (uint8_t)id, (int16_t)v); }
        } else if (lvl_idx[kit][t] >= 0) {
            int i = lvl_idx[kit][t];
            v = dmir[kit][t][i] + (int)d * 2; v = v < 0 ? 0 : v > 127 ? 127 : v;
            if (v != dmir[kit][t][i]) { dmir[kit][t][i] = (int8_t)v; wq_put(WC_DSET, (uint8_t)(kit << 4 | t), (int16_t)(i << 8 | v)); }
        }
    } else if (k == 2 && c >= 2 && pan_idx[kit][t] >= 0) {      /* pan */
        int i = pan_idx[kit][t];
        v = dmir[kit][t][i] + (int)d * 2; v = v < 0 ? 0 : v > 127 ? 127 : v;
        if (v != dmir[kit][t][i]) { dmir[kit][t][i] = (int8_t)v; wq_put(WC_DSET, (uint8_t)(kit << 4 | t), (int16_t)(i << 8 | v)); }
    }
    wui.dirty = 1;
}
static int dsound_first(void) { int i; for (i = 0; i < NPAGE; i++) if (PAGES[i].kind == K_DSOUND) return i; return 0; }
static void dsound_knob(int k, int32_t d)       /* SOUND pages: K1 the track, K2-K4 its pots (3 a page) */
{
    int kit = ui_p[P_DR_KIT] & 1, t = wui.dtrack;
    if (d > 1 || d < -1) d *= 2;
    if (k == 0) { t += (int)d; wui.dtrack = (uint8_t)(t < 0 ? 0 : t >= WT_DTRACKS ? WT_DTRACKS - 1 : t); wui.dirty = 1; return; }
    {   int i = (wui.page - dsound_first()) * 3 + (k - 1), n = wt_drums_nparams(kit, t), v;
        const x0x_param_t *pp;
        if (i >= n || i >= 12) return;
        pp = wt_drums_param(kit, t, i);
        v = dmir[kit][t][i] + (int)d * (pp->max >= 100 ? 2 : 1);
        v = v < 0 ? 0 : v > pp->max ? pp->max : v;
        if (v == dmir[kit][t][i]) return;
        dmir[kit][t][i] = (int8_t)v;
        wq_put(WC_DSET, (uint8_t)(kit << 4 | t), (int16_t)(i << 8 | v));
        wui.dirty = 1;
    }
}
#endif /* !WT_NODRUMS: drum pages */

#if WT_P37
/* ---- the P37: faders in four banks, the wheels, the outer keys, cutoff / resonance knobs ---- */
static int fdr_get(int bank, int slot)          /* the parameter on fader `slot` of bank `bank`, -1 none */
{
    int n = (bank & 3) * 4 + (slot & 3), v = (int)(uint16_t)ui_p[P_FDR0 + n / 2];
    v = (n & 1) ? v >> 8 : v & 255;
    return v ? v - 1 : -1;
}
static void fdr_set(int bank, int slot, int id)  /* id -1 = none */
{
    int n = (bank & 3) * 4 + (slot & 3), r = P_FDR0 + n / 2, v = (int)(uint16_t)ui_p[r], a = id + 1;
    v = (n & 1) ? (v & 255) | (a << 8) : (v & 0xFF00) | a;
    ui_p[r] = (int16_t)(uint16_t)v;
    wq_put(WC_PARAM, (uint8_t)r, ui_p[r]);
}
static int fdr_ok(int id) { return id >= 0 && id < P_FDR0 && pmax(id) > pmin(id); }      /* a parameter a fader may take */
static void ft_show(int id) { wui.ft_id = (int16_t)id; wui.ft_t = fm1_ms ? fm1_ms : 1; wui.dirty = 1; }
static void param_set(int id, int v)
{
    v = v < pmin(id) ? pmin(id) : v > pmax(id) ? pmax(id) : v;
    if (v == ui_p[id]) return;
    ui_p[id] = (int16_t)v;
    wq_put(WC_PARAM, (uint8_t)id, (int16_t)v);
    wui.dirty = 1;
}
static void param_bump(int id, int32_t d)       /* a knob's turn on a parameter that is not on the page (cutoff, resonance) */
{
    int span = pmax(id) - pmin(id), step = span >= 200 ? span / 100 : 1;
    if (d > 1 || d < -1) d *= 2;
    param_set(id, ui_p[id] + (int)d * step);
    ft_show(id);
}
static void fdr_pending_all(void) { wui.fpend = 15; }
/* ---- the pads: they play the drum sounds (the first 8 on the top row, left to right, the rest wrapping to the second row);
 *      > tapped twice (>>) turns them into the drum step entry for the selected sound ---- */
static int pad_of_slot(int slot) { return slot >= 8 ? slot - 8 : slot + 8; }       /* a white key's slot (bottom row first) -> the pad, top row first */
static int acc_level(int st, int ex)            /* the velocity a step sounds at, 1..127 */
{
    int v = st & 127, a;
    if (st & DS_ACCENT) a = ui_p[P_DR_ACCENT]; else if (ex & DX_SOFT) a = ui_p[P_DR_SOFT]; else return v < 1 ? 1 : v;
    a = a >= 128 ? a - 128 : v + a;
    return a < 1 ? 1 : a > 127 ? 127 : a;
}
static void pad_flash(int green) { wui.pflash_t = fm1_ms ? fm1_ms : 1; wui.pflash_g = (uint8_t)green; wui.dirty = 1; }
static void pad_edit(int fader, int raw)        /* a held pad: fader 1 = velocity 1..127, fader 2 = probability 0..100 % (steps of 2) */
{
    wt_dpat_t *q = dpat();
    uint8_t *st = &q->st[wui.dtrack][wui.pstep];
    if (!*st) return;
    if (fader == 0) *st = (uint8_t)((*st & DS_ACCENT) | (raw < 1 ? 1 : raw));
    else dx_set(q, wui.dtrack, wui.pstep, (dx_get(q, wui.dtrack, wui.pstep) & DX_SOFT) | (((100 - raw * 100 / 127) / 2) << 1));
    wui.pheld_used = 1; wui.dirty = 1;
}
static void pad_down(int p, int vel, uint32_t b)
{
    uint32_t now = fm1_ms ? fm1_ms : 1;
    wt_dpat_t *q = dpat();
    wui.plit_t[p] = now; wui.plit_v[p] = (uint8_t)vel; wui.pdown |= (uint16_t)(1u << p);
    if (wui.navbtn >= 0 && nav_index(wui.navbtn) >= 0) { nav_jump(p); return; }
    if (b & (1u << B_NEXT)) { wui.ntap = 0; if (p < WT_DTRACKS) dpad(p, vel, 0); return; }          /* > held: this pad's sound is the one the steps are entered for */
    if (!wui.pmode) { if (p < WT_DTRACKS) dpad(p, vel, 1); return; }
    {   int i = wui.dsp * 16 + p;
        uint8_t *st;
        int ex;
        if (i >= q->len || i >= WT_DSTEPS) return;
        st = &q->st[wui.dtrack][i]; ex = dx_get(q, wui.dtrack, i);
        wui.pheld = (int8_t)p; wui.pstep = (uint8_t)i; wui.pheld_used = 0;
        if (wui.pl_pad == p && wui.pl_step == i && now - wui.pl_t < 350u) {         /* a double tap: undo the single tap, then none -> loud -> soft -> none */
            *st = wui.psave_st; ex = wui.psave_ex;
            if (!*st) { *st = 100 | DS_ACCENT; ex = 0; }
            else if (*st & DS_ACCENT) { *st &= 127; ex |= DX_SOFT; }
            else if (ex & DX_SOFT) ex &= ~DX_SOFT;
            else *st |= DS_ACCENT;
            wui.pheld_used = 1; wui.pl_pad = -1;
        } else {
            wui.psave_st = *st; wui.psave_ex = (uint8_t)ex; wui.pl_pad = (int8_t)p; wui.pl_step = (int8_t)i; wui.pl_t = now;
            if (!*st) { *st = 100; ex = 0; wui.pheld_used = 1; }
        }
        dx_set(q, wui.dtrack, i, ex);
        if (*st) wq_put(WC_DHIT, wui.dtrack, (int16_t)acc_level(*st, ex));
        wui.dirty = 1;
    }
}
static void pad_up(int p)
{
    wui.pdown &= (uint16_t)~(1u << p);
    if (wui.pmode && wui.pheld == p) {          /* a tap on a step that was on: off (unless it was edited, or was the first of a double tap) */
        if (!wui.pheld_used) { wt_dpat_t *q = dpat(); q->st[wui.dtrack][wui.pstep] = 0; dx_set(q, wui.dtrack, wui.pstep, 0); }
        wui.pheld = -1; wui.dirty = 1;
    }
}
static void p37_pads(uint32_t pk, uint32_t b)
{
    uint32_t ch = pk ^ wui.pk_prev;
    int i;
    for (i = 0; i < NKEYS; i++)
        if ((ch >> i) & 1u) {
            int sl = white_slot[i];
            if (sl < 0) continue;
            if ((pk >> i) & 1u) pad_down(pad_of_slot(sl), KEYVEL(i), b); else pad_up(pad_of_slot(sl));
        }
}
static void pad_col(uint8_t *o, int r, int g, int bl, int lvl)    /* a pad colour at a velocity: brightest at 127, dimmer below (and never quite dark) */
{
    int k = 45 + lvl * 210 / 127;
    k = k * 40 / 100;                           /* (the LEDs are bright: a good deal less than full) */
    o[0] = (uint8_t)(r * k / 255); o[1] = (uint8_t)(g * k / 255); o[2] = (uint8_t)(bl * k / 255);
}
static void p37_leds(void)                      /* the 16 pad lights: off by default; a struck pad; in step entry the steps (blue, loud accent magenta, soft accent violet) */
{
    uint8_t rgb[16][3];
    uint32_t now = fm1_ms;
    const wt_dpat_t *q = dpat();
    int p, nexth = (int)((wui.bprev >> B_NEXT) & 1u);
    for (p = 0; p < 16; p++) rgb[p][0] = rgb[p][1] = rgb[p][2] = 0;
    if (wui.pflash_t && now - wui.pflash_t < 480u) {            /* entering (red) / leaving (green) step entry: two quick flashes */
        uint32_t ph = now - wui.pflash_t;
        if (ph < 120u || (ph >= 240u && ph < 360u))
            for (p = 0; p < 16; p++) pad_col(rgb[p], wui.pflash_g ? 0 : 255, wui.pflash_g ? 255 : 0, 0, 127);
    } else if (!on_steps_page()) {
        for (p = 0; p < 16; p++) {
            if (nexth) { if (p < WT_DTRACKS) pad_col(rgb[p], 255, 150, 0, p == wui.dtrack ? 127 : 25); continue; }       /* > held: the sounds, the chosen one bright */
            if (wui.pmode) {
                int i = wui.dsp * 16 + p;
                if (i < q->len && i < WT_DSTEPS && q->st[wui.dtrack][i]) {
                    int st = q->st[wui.dtrack][i], ex = dx_get(q, wui.dtrack, i), lv = acc_level(st, ex);
                    if (st & DS_ACCENT) pad_col(rgb[p], 255, 0, 200, lv); else if (ex & DX_SOFT) pad_col(rgb[p], 130, 70, 255, lv); else pad_col(rgb[p], 0, 90, 255, lv);
                }
            } else if (((wui.pdown >> p) & 1u) || (wui.plit_t[p] && now - wui.plit_t[p] < 140u)) pad_col(rgb[p], 0, 220, 170, wui.plit_v[p]);
        }
    }
    plat_pad_leds(rgb);
}
static void p37_faders(void)
{
    int s;
    for (s = 0; s < 4; s++) {
        int raw = plat_fader(s), id = fdr_get(wui.fbank, s), cur;
        if (s < 2 && wui.pmode && wui.pheld >= 0) {   /* a pad held in step entry: fader 1 is its velocity, fader 2 its probability */
            if (raw - wui.flast[s] > 1 || wui.flast[s] - raw > 1) pad_edit(s, raw);
            wui.flast[s] = (uint8_t)raw; wui.fpend |= (uint8_t)(1u << s);
            continue;
        }
        if (!fdr_ok(id) || wui.fb_down) { wui.flast[s] = (uint8_t)raw; continue; }
        cur = (ui_p[id] - pmin(id)) * 127 / (pmax(id) - pmin(id));
        if ((wui.fpend >> s) & 1u) {            /* after a bank change the fader does nothing until it reaches the value */
            if (raw - cur > 3 || cur - raw > 3) { wui.flast[s] = (uint8_t)raw; continue; }
            wui.fpend &= (uint8_t)~(1u << s);
        }
        if (raw - wui.flast[s] > 1 || wui.flast[s] - raw > 1 || (raw != wui.flast[s] && (raw == 0 || raw == 127))) {
            wui.flast[s] = (uint8_t)raw;
            param_set(id, pmin(id) + ((pmax(id) - pmin(id)) * raw + 63) / 127);
            ft_show(id);
        }
    }
}
static void p37_wheels(void)
{
    int w, raw[2];
    raw[0] = plat_wheel(0); raw[1] = plat_wheel(1);
    if (wui.wcal < 30) {                        /* the first frames: where the wheels rest */
        for (w = 0; w < 2; w++) { wui.wrest[w] = (int16_t)(wui.wcal ? (wui.wrest[w] * 3 + raw[w]) / 4 : raw[w]); wui.whi[w] = (int16_t)(wui.wrest[w] + 100); wui.wlo[w] = (int16_t)(wui.wrest[w] - 100); }
        wui.wcal++;
        return;
    }
    for (w = 0; w < 2; w++) { if (raw[w] > wui.whi[w]) wui.whi[w] = (int16_t)raw[w]; if (raw[w] < wui.wlo[w]) wui.wlo[w] = (int16_t)raw[w]; }
    {   int dev = raw[0] - wui.wrest[0], rng = dev > 0 ? wui.whi[0] - wui.wrest[0] : wui.wrest[0] - wui.wlo[0], bend;
        if (dev < 8 && dev > -8) dev = 0;
        bend = dev * 8191 / rng;
        bend = bend < -8192 ? -8192 : bend > 8191 ? 8191 : bend;
        if (bend - wui.bend_sent > 40 || wui.bend_sent - bend > 40 || (bend == 0 && wui.bend_sent != 0)) { wui.bend_sent = (int16_t)bend; wq_put(WC_BEND, 0, (int16_t)bend); }
    }
    {   int dev = raw[1] - wui.wrest[1], a, rng, m;
        a = dev < 0 ? -dev : dev;
        rng = wui.whi[1] - wui.wrest[1]; if (wui.wrest[1] - wui.wlo[1] > rng) rng = wui.wrest[1] - wui.wlo[1];
        m = a < 8 ? 0 : a * 127 / rng;
        m = m > 127 ? 127 : m;
        if (m != wui.mod_sent) { wui.mod_sent = (int16_t)m; wq_put(WC_MOD, 0, (int16_t)m); }
    }
}
#endif

static void wui_input(void)
{
    uint32_t k = plat_keys(), diff = k ^ wui.keys, i, b = plat_buttons(), edge = b & ~wui.bprev, rel = wui.bprev & ~b;
    int32_t d;
#if WT_NODRUMS
    int steps = on_steps_page(), stepkeys = steps && !wui.rec, dpg = on_dsteps();
#else
    int steps = on_steps_page(), stepkeys = steps && !wui.rec, dpg = on_dsteps(), dany = on_drum_page();
#endif
#if WT_LFOSHAPES
    int lfoed = on_lfoed();
#endif
#if WT_P37
    {   int32_t ev;                             /* the keybed's outer keys (C3 .. E3, G#5 .. C6): plain notes, with their velocity */
        while ((ev = plat_extkey_take()) >= 0) {
            int key = (int)(ev >> 8) % 37, vel = (int)(ev & 255);
            if (vel) {
                int n = 48 + key + wui.oct * 12;
                n = n < 0 ? 0 : n > 127 ? 127 : n;
                if (wui.extnote[key]) wq_put(WC_OFF, wui.extnote[key], 0);
                wui.extnote[key] = (uint8_t)n; wui.lastvel = (uint8_t)vel;
                wq_put(WC_ON, (uint8_t)n, (int16_t)vel);
            } else if (wui.extnote[key]) { wq_put(WC_OFF, wui.extnote[key], 0); wui.extnote[key] = 0; }
        }
        p37_wheels();
        p37_faders();
    }
#endif
#if WT_P37
    {   uint32_t pk = plat_pads();              /* the pads play the drum sounds / enter drum steps; only on the synth's STEPS page do they stay white keys */
        if (!steps) { p37_pads(pk, b); k &= ~(pk | wui.pk_prev); }
        wui.pk_prev = pk;
        diff = k ^ wui.keys;
    }
#endif
    for (i = 0; i < NKEYS; i++)
        if ((diff >> i) & 1u) {
            int down = (k >> i) & 1u;
            if (!down) {                            /* a key let go: whatever it started ends, on whatever page it is now (a note must never be left sounding) */
                if (wui.keynote[i]) { wq_put(WC_OFF, wui.keynote[i], 0); wui.keynote[i] = 0; }
                if (wui.recst[i] || wui.rpk == (int)i) rec_release((int)i);
            }
            if (down && wui.navbtn >= 0 && white_slot[i] >= 0 && nav_index(wui.navbtn) >= 0) { nav_jump(white_slot[i]); continue; }       /* a page button held: this key is a shortcut to one of its pages */
#if !WT_P37
#if WT_LFOSHAPES
            if (lfoed && black_slot[i] >= 0) { if (down) lfoed_key(black_slot[i]); continue; }
#endif
#if !WT_NODRUMS
            if (dany && black_slot[i] >= 0) { if (down) dpad(black_slot[i], 100, 1); continue; }              /* on the drum pages the black keys are the pads */
#endif
#endif
#if !WT_P37 && !WT_NODRUMS                      /* (the P37's pads are the steps; its keybed plays the synth) */
            if (dpg && white_slot[i] >= 0) { if (down) dstep_press(white_slot[i], b); else if (wui.dheld == wui.dsp * 16 + white_slot[i]) dstep_release(); continue; }
#endif
            if (stepkeys && wui.held >= 0 && down && (white_slot[i] < 0 || wui.sp * 16 + white_slot[i] != wui.held)) {
                /* a step is held and another key goes down: that key's pitch is the step's note (it sounds, so you hear what you set) */
                wt_step_t *hs = &cur_pat()->step[wui.held];
                int n = ui_snap(53 + wui.oct * 12 + (int)i);
                hs->note[0] = (uint8_t)n; hs->flags |= SF_ON; wui.last_note = (uint8_t)n; wui.held_used = 1; wui.dirty = 1;
                wq_put(WC_ON, (uint8_t)n, hs->vel); wui.keynote[i] = (uint8_t)n;
            } else if (stepkeys) {                  /* on the STEPS page the keys are the steps (white) and the patterns (black) */
                if (white_slot[i] >= 0) { if (down) step_press(white_slot[i], b); else if (wui.held == wui.sp * 16 + white_slot[i]) step_release(); }
                else if (down && black_slot[i] >= 0 && black_slot[i] < WT_NPAT && wui.held < 0) { wq_put(WC_QUEUE, (uint8_t)black_slot[i], 0); wui.sp = 0; wui.dirty = 1; }
            } else if (down) {
                int n = 53 + wui.oct * 12 + (int)i;
                if (n < 0) n = 0;
                if (n > 127) n = 127;
                wui.keynote[i] = (uint8_t)n;
                wq_put(WC_ON, (uint8_t)n, KEYVEL((int)i));
                if (wui.rec) record_note(ui_snap(n), (int)i);
            }
        }
    wui.keys = k;
    if (steps && wui.held >= 0 && !wui.rec) {       /* a step is held: these buttons edit it */
        wt_step_t *st = &cur_pat()->step[wui.held];
        if (edge & (1u << B_ENV)) { st->flags ^= SF_ACCENT; wui.held_used = 1; wui.dirty = 1; }
        if (edge & (1u << B_LFO)) { st->flags ^= SF_SLIDE; wui.held_used = 1; wui.dirty = 1; }
        if (edge & (1u << B_SAVE)) { for (i = 0; i < 4; i++) st->note[i] = 0; st->flags = 0; st->vel = 100; st->gate = 60; st->prob = 100; wui.held_used = 1; wui.dirty = 1; }
        if (edge & (1u << B_OCTUP)) { if (st->note[0] && st->note[0] + 12 <= 127) st->note[0] = (uint8_t)(st->note[0] + 12); wui.held_used = 1; wui.dirty = 1; edge &= ~(1u << B_OCTUP); }
        if (edge & (1u << B_OCTDN)) { if (st->note[0] > 12) st->note[0] = (uint8_t)(st->note[0] - 12); wui.held_used = 1; wui.dirty = 1; edge &= ~(1u << B_OCTDN); }
    }
#if !WT_NODRUMS
    if (dpg && wui.dheld >= 0) {
        uint8_t *st = &dpat()->st[wui.dtrack][wui.dheld];
        if (edge & (1u << B_ENV)) { if (*st) *st ^= DS_ACCENT; wui.dheld_used = 1; wui.dirty = 1; }
        if (edge & (1u << B_SAVE)) { *st = 0; wui.dheld_used = 1; wui.dirty = 1; }
    }
    if (dpg && wui.dheld < 0) {                     /* the drum grid's second page */
        int maxp = (dpat()->len - 1) / 16;
        if ((edge & (1u << B_OCTUP)) && wui.dsp < maxp) { wui.dsp++; wui.dirty = 1; edge &= ~(1u << B_OCTUP); }
        if ((edge & (1u << B_OCTDN)) && wui.dsp > 0) { wui.dsp--; wui.dirty = 1; edge &= ~(1u << B_OCTDN); }
    }
#endif
    if (steps && wui.held < 0) {                    /* the grid's pages */
        int maxp = (cur_pat()->len - 1) / 16;
        if ((edge & (1u << B_OCTUP)) && wui.sp < maxp) { wui.sp++; wui.dirty = 1; edge &= ~(1u << B_OCTUP); }
        if ((edge & (1u << B_OCTDN)) && wui.sp > 0) { wui.sp--; wui.dirty = 1; edge &= ~(1u << B_OCTDN); }
    }
    if ((edge & (1u << B_OCTUP)) && wui.oct < 3 && !steps && !dpg) { wui.oct++; wui.dirty = 1; }
    if ((edge & (1u << B_OCTDN)) && wui.oct > -3 && !steps && !dpg) { wui.oct--; wui.dirty = 1; }
    if (edge & (1u << B_PLAY)) {                /* PLAY toggles the transport; a second press within half a second is the panic: everything stops and every voice is cut */
        if (wui.play_t && fm1_ms - wui.play_t < 450u) {
            wq_put(WC_PANIC, 0, 0); wui.play_t = 0;
            { int kk; for (kk = 0; kk < NKEYS; kk++) { wui.keynote[kk] = 0; wui.recst[kk] = 0; } wui.rpk = -1; wui.held = -1; wui.dheld = -1; }
            str_cpy(wui.toast, "PANIC", 23); str_cpy(wui.toast2, "ALL STOPPED", 23); wui.toast_t = fm1_ms ? fm1_ms : 1; wui.dirty = 1;
#if WT_NODRUMS
        } else { wui.play_t = fm1_ms ? fm1_ms : 1; wq_put(WC_PLAY, synth.seq.run ? 0 : 1, 0); }
#else
        } else { wui.play_t = fm1_ms ? fm1_ms : 1; wq_put(WC_PLAY, (synth.seq.run || drums.run) ? 0 : 1, 0); }
#endif
    }
    if (edge & (1u << B_REC)) { rec_finish_pending(fm1_ms); wui.rec = (uint8_t)!wui.rec; wui.rec_pos = 0; wui.dirty = 1; }
    if (edge & (1u << B_SEL)) {
        if (wui.sel_latched) { wui.sel_latched = 0; wui.sel_down = 0; wui.sel_ignore = 1; }       /* a press while the mode stays on: it ends */
#if !WT_P37 && !WT_W2                           /* (on the P37 KNOB BANK is only a shift: no mode, no page) */
        else { wui.sel_down = 1; wui.sel_used = 0; wui.sel_t0 = fm1_ms; }
#endif
        wui.dirty = 1;
    }
    for (i = 0; i < (uint32_t)B_OCTDN; i++)
        if ((WT_W2 || i != (uint32_t)B_SEL) && i != (uint32_t)B_PLAY && i != (uint32_t)B_REC && ((edge >> i) & 1u)) {
            if ((steps || (dpg && wui.dheld >= 0)) && (i == (uint32_t)B_ENV || i == (uint32_t)B_LFO || i == (uint32_t)B_ARP || i == (uint32_t)B_SAVE)) continue;   /* modifiers here, not pages */
            if (i == (uint32_t)B_SAVE && PAGES[wui.page].btn == B_SAVE) { wui_save_press(); continue; }
#if WT_W2
            if (i == (uint32_t)B_LFO) {                  /* a fast double tap: the MOD pages (and back to the LFOs from them) */
                if (wui.lfo_t && fm1_ms - wui.lfo_t < 350u) {
                    wui.lfo_t = 0;
                    wui.page = (int8_t)(is_mod_page(wui.page) ? title_page("LFO 1") : title_page("MOD 1"));
                    wui.held = -1; wui.dheld = -1; wui.dirty = 1;
                    wui.navbtn = (int8_t)i; wui.nav_t = fm1_ms ? fm1_ms : 1; wui.nav_used = 1;
                    continue;
                }
                wui.lfo_t = fm1_ms ? fm1_ms : 1;
            }
#endif
            wui_button((int)i);
            if (nav_index((int)i) >= 0) { wui.navbtn = (int8_t)i; wui.nav_t = fm1_ms ? fm1_ms : 1; wui.nav_used = 0; wui.dirty = 1; }
        }
    if (wui.navbtn >= 0 && !((b >> wui.navbtn) & 1u)) { wui.navbtn = -1; wui.dirty = 1; }
    if (rel & (1u << B_SEL)) {                  /* SEL held while turning knobs: the mode ends on release. SEL tapped (nothing turned): the mode stays on until SEL is pressed again */
        if (wui.sel_ignore) wui.sel_ignore = 0;
#if !WT_P37 && !WT_W2
        else if (!wui.sel_used && fm1_ms - wui.sel_t0 < 600u) wui.sel_latched = 1;
#endif
        else wui.sel_down = 0;
        wui.dirty = 1;
    }
    wui.bprev = b;
#if WT_P37
    if (edge) wui.lastbtn = (uint8_t)(__builtin_ctz(edge) + 1);
    if (edge & (1u << B_SCALE)) { wui.page = (int8_t)page_or("SCALE"); wui.held = -1; wui.dheld = -1; wui.dirty = 1; }
    if (edge & (1u << B_FBANK)) { wui.fb_down = 1; wui.fb_used = 0; wui.fb_t0 = fm1_ms ? fm1_ms : 1; wui.dirty = 1; }
    if (rel & (1u << B_FBANK)) {                /* a tap: the next fader bank; held, with knobs turned: the assignment */
        if (!wui.fb_used && fm1_ms - wui.fb_t0 < 450u) { wui.fbank = (uint8_t)((wui.fbank + 1) & 3); ft_show(-1); }
        wui.fb_down = 0; fdr_pending_all(); wui.dirty = 1;
    }
    if (wui.fb_down) {                          /* FADER BANK held: the four knobs choose what the four faders do (a parameter, in the order of the list) */
        for (i = 0; i < 4u; i++) {
            d = plat_enc(EN_K1 + (int)i);
            if (d) {
                int id = fdr_get(wui.fbank, (int)i), dir = d > 0 ? 1 : -1, n;
                for (n = 0; n < P_FDR0 + 1; n++) { id += dir; if (id >= P_FDR0) id = 0; if (id < 0) id = P_FDR0 - 1; if (fdr_ok(id)) break; }
                fdr_set(wui.fbank, (int)i, id);
                wui.fb_used = 1; wui.dirty = 1;
            }
        }
    } else if (wui.fb_used) wui.fb_used = 0;
    if (edge & (1u << B_PREV)) { wui.page = (int8_t)page_or("OSC A"); wui.held = -1; wui.dheld = -1; wui.dirty = 1; }       /* < : OSC A */
    if (edge & (1u << B_NEXT)) {                /* > : the drum sequencer; tapped twice (>>) the pads' step entry on / off, three times the accent page */
        wui.page = (int8_t)page_or("DRUMS"); wui.held = -1; wui.dheld = -1; wui.dirty = 1;
        wui.ntap = (uint8_t)(wui.ntap && fm1_ms - wui.ntap_t < 400u ? wui.ntap + 1 : 1);
        wui.ntap_t = fm1_ms ? fm1_ms : 1;
    }
    if (wui.ntap && fm1_ms - wui.ntap_t >= 400u) {
        if (wui.ntap == 2) { wui.pmode = (uint8_t)!wui.pmode; wui.pheld = -1; wui.pl_pad = -1; pad_flash(!wui.pmode); }
        else if (wui.ntap >= 3) { wui.page = (int8_t)page_or("DRUM ACCENT"); wui.dirty = 1; }
        wui.ntap = 0;
    }
    {   d = plat_enc(EN_CUT); if (d) param_bump(P_F_CUTOFF, d);
        d = plat_enc(EN_RES); if (d) param_bump(P_F_RESO, d);
    }
    if (wui.ft_t && fm1_ms - wui.ft_t > 1200u && !wui.fb_down) { wui.ft_t = 0; wui.ft_id = -1; wui.dirty = 1; }
    if (!wui.fb_down)
#endif
    for (i = 0; i < 4u; i++) {
        d = plat_enc(EN_K1 + (int)i);
        if (d) { if (steps && wui.held >= 0 && !wui.rec) step_knob((int)i, d); else wui_knob((int)i, d); }
    }
    d = plat_enc(EN_SELECT);
#if WT_P37
    if (d && (b & (1u << B_SEL))) { param_bump(P_MASTER, d); d = 0; }      /* KNOB BANK is the shift: with it, K1 is the master volume */
#endif
    if (d) { wui.page = (int8_t)(((int)wui.page + (d > 0 ? 1 : NPAGE - 1)) % NPAGE); wui.held = -1; wui.dheld = -1; wui.dirty = 1; }
    d = plat_enc(EN_ALGO);
    if (d) {
        if (wui.sel_down) {                     /* with SEL held, ALGORITHM chooses the modulation source */
            int s = wui.mod_src + (d > 0 ? 1 : -1);
            wui.sel_used = 1;
            wui.mod_src = (int8_t)(s < 1 ? S_COUNT - 1 : s >= S_COUNT ? 1 : s);
            wui.dirty = 1;
        } else wui_step_patch(d > 0 ? 1 : -1);
    }
    d = plat_enc(EN_PRESET);
    if (d) {                                    /* PRESETS: the wavetable of the oscillator whose page this is (OSC B pages: B, anything else: A) */
        int o = PAGES[wui.page].kind == K_OSC_B ? 1 : 0, id = P_OSC(o, O_TABLE), v = (ui_p[id] + (d > 0 ? 1 : 2)) % 3;
        ui_p[id] = (int16_t)v;
        wq_put(WC_PARAM, (uint8_t)id, (int16_t)v);
        str_cpy(wui.toast, o ? "OSC B TABLE" : "OSC A TABLE", 23); str_cpy(wui.toast2, E_TABLES[v], 23);
        wui.toast_t = fm1_ms ? fm1_ms : 1;
        wui.dirty = 1;
    }
    if (wui.toast_t && fm1_ms - wui.toast_t > 1500u) { wui.toast_t = 0; wui.dirty = 1; }
    if (wui.armed && fm1_ms - wui.armed_t > 5000u) { wui.armed = 0; wui.dirty = 1; }
    if (wui.msg && fm1_ms - wui.msg_t > 3000u) { wui.msg = 0; wui.dirty = 1; }
    if (wui.clear_msg && fm1_ms - wui.clear_t > 700u) { wui.clear_msg = 0; wui.dirty = 1; }
#if !WT_NODRUMS
    if (wui.dclear && fm1_ms - wui.clear_t > 700u) { wui.dclear = 0; wui.dirty = 1; }
#endif
    if ((wui.groll || wui.dgroll || wui.opf) && fm1_ms - wui.clear_t > 700u) { wui.groll = wui.dgroll = 0; wui.opf = 0; wui.dirty = 1; }
#if !WT_NODRUMS
    {   uint16_t hit = drums.last_hit;               /* tracks that sounded: their label flashes for a moment */
        int t, any = 0;
        drums.last_hit = 0;
        for (t = 0; t < WT_DTRACKS; t++) { if ((hit >> t) & 1u) dflash[t] = 6; else if (dflash[t]) dflash[t]--; any |= dflash[t]; }
        if (any && on_dsteps()) wui.dirty = 1;
    }
#endif
    wui_vsync();
    {   uint32_t lit = PAGES[wui.page].btn >= 0 ? 1u << PAGES[wui.page].btn : 0u;   /* the page's button is lit, and the keys held */
        if (wui.sel_down || wui.bprev & (1u << B_SEL)) lit |= 1u << B_SEL;
#if WT_P37
        if (wui.fbank) lit |= 1u << B_FBANK;
        if (PAGES[wui.page].title[0] == 'S' && PAGES[wui.page].title[1] == 'C' && !PAGES[wui.page].title[5]) lit |= 1u << B_SCALE;
#endif
#if WT_NODRUMS
        if (synth.seq.run) lit |= 1u << B_PLAY;
#else
        if (synth.seq.run || drums.run) lit |= 1u << B_PLAY;
#endif
        if (wui.rec) lit |= 1u << B_REC;
        if (ui_p[P_ARP_ON] && !WT_W2) lit |= 1u << B_ARP;
        {   uint32_t kl = k, kd = 0;                   /* the keys' lights: on the grid pages a key is lit when its step has a note */
            if (on_steps_page() && !wui.rec) {
                const wt_pat_t *q = cur_pat();
                int pos = synth.seq.run ? synth.seq.pos : -1, j;
                kl = 0;
                for (j = 0; j < 16; j++) {
                    int si = wui.sp * 16 + j;
                    const wt_step_t *st = &q->step[si % WT_NSTEP];
                    int on = si < q->len && (st->flags & SF_ON) && st->note[0], tie = si < q->len && step_covered(si);
                    if (si == pos) kd |= 1u << slot_key[j];             /* the step playing dips to dim (or shows dim if it is empty) */
                    else if (on) kl |= 1u << slot_key[j];
                    else if (tie) kd |= 1u << slot_key[j];              /* a note ringing over this step: dim */
                }
                if (wui.held >= 0 && wui.held / 16 == wui.sp) kl |= 1u << slot_key[wui.held % 16];
                if ((synth.seq.next != 255 ? synth.seq.next : synth.seq.cur) < 11) kd |= 1u << bslot_key[(synth.seq.next != 255 ? synth.seq.next : synth.seq.cur)];
#if !WT_NODRUMS
            } else if (on_dsteps()) {
                const wt_dpat_t *q = dpat();
                int pos = drums.run ? drums.pos : -1, j;
                kl = 0;
                for (j = 0; j < 16; j++) {
                    int si = wui.dsp * 16 + j;
                    int on = si < q->len && si < WT_DSTEPS && q->st[wui.dtrack][si];
                    if (si == pos) kd |= 1u << slot_key[j];
                    else if (on) kl |= 1u << slot_key[j];
                }
                kd |= 1u << bslot_key[wui.dtrack % 11];
#endif
            }
            if (wui.navbtn >= 0 && nav_index(wui.navbtn) >= 0) {       /* a page button held: the keys that jump to its pages light */
                int ni = nav_index(wui.navbtn), j;
                kl = 0; kd = 0;
                for (j = 0; j < nav_count(ni); j++) kl |= 1u << slot_key[j];
            }
            plat_keys_dim(kd);
            plat_leds(lit, kl);
        }
    }
}

/* ---- drawing: the screen is drawn in four 240 x 60 bands (the canvas is small: the pool holds the wavetables and the effects);
 * every band draws the whole picture and what falls outside it is clipped ---- */
#define C_MODB RGB(90, 170, 255)                /* modulation: blue (the marker and the amount) */
#define C_MODD RGB(40, 100, 200)                /* the range ring */

static void arc_dot(int32_t cx, int32_t cy, int32_t r, int32_t deg, int32_t sz, uint16_t c)    /* deg: clockwise from 12 o'clock */
{
    int32_t idx = ((deg * 1024) / 360) & 1023;
    int32_t sx = wt_sin1024[idx], cs = wt_sin1024[(idx + 256) & 1023];
    cv_rect(cx + (r * sx) / 32768 - sz / 2, cy - (r * cs) / 32768 - sz / 2, sz, sz, c);
}
static void arc(int32_t cx, int32_t cy, int32_t r, int32_t d0, int32_t d1, int32_t sz, uint16_t c)
{
    int32_t d;
    if (d1 < d0) return;
    for (d = d0; d <= d1; d += 2) arc_dot(cx, cy, r, d, sz, c);
    arc_dot(cx, cy, r, d1, sz, c);
}
static int32_t frac_deg(int32_t f) { return -135 + (f < 0 ? 0 : f > 1000 ? 1000 : f) * 270 / 1000; }     /* per mille -> degrees */

/* one dial of the page: id, its label, the cell's centre */
static void wui_dial(int id, const char *label, int32_t cx, int32_t cy, int32_t r)
{
    char b[16] = {0};
    int32_t span = pmax(id) - pmin(id), mn = pmin(id), v = ui_p[id], f, f0, lo = -1, hi = -1, now = -1;
    int dest = dest_of[id], i;
    int shown = v, modmode = wui.sel_down;
    const char *e = wui_enum(id, v);
    if (span <= 0) span = 1;
    if (dest) {
        int32_t depth = 0, slot_amt = 0;
        for (i = 0; i < 8; i++)
            if (ui_p[P_MX(i, 0)] && ui_p[P_MX(i, 1)] == dest && ui_p[P_MX(i, 2)]) {
                int32_t a = ui_p[P_MX(i, 2)];
                depth += a < 0 ? -a : a;
                if (ui_p[P_MX(i, 0)] == wui.mod_src) slot_amt = a;
            }
        if (modmode) {
            fmt_pct(b, slot_amt / 10);
            depth = slot_amt < 0 ? -slot_amt : slot_amt;
            e = 0;
        }
        if (depth) {
            int32_t off = depth * dnum[id] / dden[id], vm = synth.mod_view[dest] * dnum[id] / dden[id];
            lo = (v - off - mn) * 1000 / span;
            hi = (v + off - mn) * 1000 / span;
            now = (v + vm - mn) * 1000 / span;
            wui.anim = 1;
        }
    }
    f = (v - mn) * 1000 / span;
    f0 = mn < 0 ? 500 : 0;                      /* a bipolar value's arc starts at 12 o'clock */
    arc(cx, cy, r, -135, 135, r > 25 ? 3 : 2, C_LINE);
    if (f >= f0) arc(cx, cy, r, frac_deg(f0), frac_deg(f), r > 25 ? 3 : 2, C_AMB); else arc(cx, cy, r, frac_deg(f), frac_deg(f0), r > 25 ? 3 : 2, C_AMB);
    arc_dot(cx, cy, r, frac_deg(f), r > 25 ? 7 : 5, C_HI);
    if (lo >= 0 || hi >= 0) {
        arc(cx, cy, r + (r > 25 ? 7 : 5), frac_deg(lo), frac_deg(hi), 2, modmode ? C_MODB : C_MODD);
        if (now >= 0 && !modmode) arc_dot(cx, cy, r + (r > 25 ? 7 : 5), frac_deg(now), r > 25 ? 7 : 5, RGB(170, 215, 255));
    }
    if (!(modmode && dest)) wui_value(b, id, shown);
    if (modmode && !dest) { b[0] = '-'; b[1] = '-'; b[2] = 0; }
    {   int32_t mw = r * 3 / 2;              /* the value inside the dial, in the biggest face that fits */
        const felucca_font_t *ft = r > 25 && text_w(&FONT_B, b) <= mw ? &FONT_B : text_w(&FONT_S, b) <= mw ? &FONT_S : &FONT_XS;
        cv_text(cx - text_w(ft, b) / 2, cy - ft->h / 2 + 1, ft, b, modmode && dest ? C_MODB : C_HI);
    }
    (void)e;
    {   const felucca_font_t *lf = text_w(&FONT_S, label) <= (r > 25 ? 110 : 58) ? &FONT_S : &FONT_XS;
        cv_text(cx - text_w(lf, label) / 2, cy + r + (r > 25 ? 6 : 8), lf, label, C_GRAY);
    }
}

/* an oscillator's table at its WT POS, where modulation has it right now (it moves with the LFO, as in Serum) */
static void wui_wave(int o, int32_t y0, int32_t h)
{
    const wt_table_t *t = &wtab[ui_p[P_OSC(o, O_TABLE)] % WT_NTABLES];
    int32_t pos = ui_p[P_OSC(o, O_WTPOS)], fp, f0, fr, f1, x, py, lastx = 0, lasty = 0;
    int on = o == 0 || ui_p[P_OSC(1, O_ON)];
    uint16_t col = on ? C_HI : C_DIM;
    int i;
    for (i = 0; i < 8; i++)
        if (ui_p[P_MX(i, 0)] && ui_p[P_MX(i, 1)] == D_WTPOS + o && ui_p[P_MX(i, 2)]) {
            pos += synth.mod_view[D_WTPOS + o];
            wui.anim = 1;
            break;
        }
    pos = pos < 0 ? 0 : pos > 1000 ? 1000 : pos;
    fp = pos * (t->nframes - 1) * 256 / 1000; f0 = fp >> 8; fr = fp & 255;
    if (f0 >= t->nframes - 1) { f0 = t->nframes - 1; fr = 0; }
    f1 = f0 + 1 < t->nframes ? f0 + 1 : f0;
    cv_rect(6, y0, 228, h, RGB(12, 12, 18));
    cv_line(8, y0 + h / 2, 231, y0 + h / 2, C_LINE);
    for (x = 0; x < 224; x++) {
        int32_t k = x * WT_N / 224, a = t->lev[0][f0 * WT_N + k], c = t->lev[0][f1 * WT_N + k];
        int32_t y = (a * (256 - fr) + c * fr) >> 8;
        py = y0 + h / 2 - ((y * (h / 2 - 3)) >> 15);
        if (x) cv_line(8 + lastx, lasty, 8 + x, py, col);
        lastx = x; lasty = py;
    }
}

#if WT_LFOSHAPES
static void wui_lfoed(void)
{
    static int16_t tb[WT_LTAB];
    const wt_lfoshape_t *sh = &ushp[ed_slot];
    const int y0 = 44, h = 100;
    int i, draw = PAGES[wui.page].l[3] != 0, lx = 0, ly = 0, pt = ed_pt >= sh->n ? sh->n - 1 : ed_pt;
    char b[24];
    wt_lfoshape_table(sh, tb);
    cv_rect(6, y0, 228, h, RGB(12, 12, 18));
    cv_line(8, y0 + h / 2, 231, y0 + h / 2, C_LINE);
    for (i = 0; i < WT_LTAB; i++) {
        int x = 8 + i * 222 / (WT_LTAB - 1), y = y0 + h / 2 - ((tb[i] * (h / 2 - 5)) >> 15);
        if (i) cv_line(lx, ly, x, y, !draw && pt > 0 && i * 255 / (WT_LTAB - 1) > sh->x[pt - 1] && i * 255 / (WT_LTAB - 1) <= sh->x[pt] ? C_AMB : C_HI);
        lx = x; ly = y;
    }
    for (i = 0; i < sh->n; i++) {
        int x = 8 + sh->x[i] * 222 / 255, y = y0 + h / 2 - (sh->y[i] * (h / 2 - 5)) / 127;
        if (i == pt) cv_rect(x - 3, y - 3, 7, 7, C_AMB); else cv_rect(x - 2, y - 2, 5, 5, C_WHITE);
    }
    str_cpy(b, "USER ", 8); fmt_int(b + str_len(b), ed_slot + 1);
    cv_text(6, 152, &FONT_S, b, C_GRAY); cv_text(60, 152, &FONT_S, wt_lfoshape_name(ed_slot), C_HI);
    str_cpy(b, "POINT ", 8); fmt_int(b + str_len(b), pt + 1); { int n = (int)str_len(b); b[n++] = '/'; fmt_int(b + n, sh->n); }
    cv_text(150, 152, &FONT_S, b, C_WHITE);
    if (draw) {
        str_cpy(b, "HEIGHT ", 10); fmt_int(b + str_len(b), sh->y[pt]); cv_text(6, 172, &FONT_S, b, C_WHITE);
        str_cpy(b, "TIME ", 10); fmt_int(b + str_len(b), sh->x[pt] * 100 / 255); { int n = (int)str_len(b); b[n++] = '%'; b[n] = 0; } cv_text(120, 172, &FONT_S, b, C_WHITE);
    } else {
        str_cpy(b, "CURVE ", 10); fmt_int(b + str_len(b), pt ? sh->c[pt] : 0); cv_text(6, 172, &FONT_S, pt ? b : "K2: PICK POINT 2 OR MORE", pt ? C_AMB : C_DIM);
    }
    cv_text(6, 190, &FONT_XS, "BLACK KEYS FROM THE LEFT:", C_GRAY);
    cv_text(6, 204, &FONT_XS, "ADD DEL RESET FLIP FLAT BEND+ BEND-", C_GRAY);
    cv_text(6, 218, &FONT_XS, "THEN LFO 1 / LFO 2 PLAY THIS SHAPE", C_GRAY);
    if (ui_p[P_LFO(0, 0)] == 7 + ed_slot) cv_text(212, 152 - 14, &FONT_XS, "L1", C_HI);
    if (ui_p[P_LFO(1, 0)] == 7 + ed_slot) cv_text(224, 152 - 14, &FONT_XS, "L2", C_HI);
}
#endif

static void note_name(char *b, int n)
{
    static const char *const NN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    str_cpy(b, NN[n % 12], 4);
    fmt_int(b + str_len(b), n / 12 - 1);
}

/* the 16 steps of the page: note, velocity bar, accent bar, slide / tie / chance marks, the playhead in blue, the held step outlined */
static void wui_steps(void)
{
    const wt_pat_t *q = cur_pat();
    char b[24];
    int j, len = q->len, pos = synth.seq.run ? synth.seq.pos : -1, held = wui.held;
    for (j = 0; j < 16; j++) {
        int i = wui.sp * 16 + j, x = (j & 7) * 30, y = 44 + (j >> 3) * 64;
        const wt_step_t *st = &q->step[i % WT_NSTEP];
        int in = i < len, outside = in && q->rsv[1] && (i < q->rsv[0] || i >= q->rsv[0] + q->rsv[1]);          /* outside the bar being looped: it does not play */
        int on = in && !outside && (st->flags & SF_ON) && (st->note[0] || (st->flags & SF_TIE));
        uint16_t edge = !in ? C_BG : i == pos ? C_YEL : i == held ? C_WHITE : on ? C_VIOLET : C_LINE;
        cv_rect(x + 2, y + 1, 26, 60, !in || outside ? C_BG : on ? C_VTINT : C_CELL);           /* a card: tinted when it has a note */
        cv_rect(x + 2, y + 1, 26, 2, edge);
        if (i == pos || i == held) { cv_rect(x + 2, y + 59, 26, 2, edge); cv_rect(x + 2, y + 1, 2, 60, edge); cv_rect(x + 26, y + 1, 2, 60, edge); }
        if (in && !outside && step_covered(i)) cv_rect(x + 2, y + 56, 26, 3, C_MINT);                 /* ... and through the steps it lasts over */
        if (!in) continue;
        if (!on) {
            fmt_int(b, i + 1);
            cv_text(x + 15 - text_w(&FONT_XS, b) / 2, y + 44, &FONT_XS, b, outside ? C_LINE : C_DIM);
            continue;
        }
        if (st->flags & SF_ACCENT) cv_rect(x + 4, y + 5, 22, 3, C_AMB);
        if (st->note[0]) {
            const felucca_font_t *ft;
            note_name(b, st->note[0] + (int)q->trans - 24);
            ft = text_w(&FONT_S, b) <= 26 ? &FONT_S : &FONT_XS;
            cv_text(x + 15 - text_w(ft, b) / 2, y + 9, ft, b, C_INK);
            {   int hgt = 6 + (st->vel * 30) / 127;
                cv_rect(x + 7, y + 54 - hgt, 16, hgt, (st->flags & SF_ACCENT) ? C_AMB : C_MINT);
            }
        }
        if (st->flags & SF_SLIDE) { cv_line(x + 4, y + 57, x + 26, y + 51, C_MODB); cv_line(x + 4, y + 58, x + 26, y + 52, C_MODB); }
        if ((st->flags & SF_ON) && st->note[0] && ((st->flags >> 4) & 15)) cv_rect(x + 14, y + 56, 16, 3, C_YEL);      /* a long note: a bar along the bottom ... */
        if (st->prob < 100) cv_rect(x + 23, y + 10, 4, 4, C_MODB);
    }
    if (held >= 0) {                                    /* the held step's values */
        const wt_step_t *st = &q->step[held % WT_NSTEP];
        int n;
        note_name(b, st->note[0] ? st->note[0] : wui.last_note);
        cv_text(6, 176, &FONT_S, b, C_HI);
        n = 44; cv_text(n, 176, &FONT_S, "VEL", C_GRAY); fmt_int(b, st->vel); cv_text(n + 30, 176, &FONT_S, b, C_HI);
        n = 100; cv_text(n, 176, &FONT_S, "GATE", C_GRAY); fmt_pct(b, st->gate); cv_text(n + 38, 176, &FONT_S, b, C_HI);
        n = 172; cv_text(n, 176, &FONT_S, "LEN", C_GRAY); fmt_int(b, step_len(held)); cv_text(n + 30, 176, &FONT_S, b, C_HI);
        cv_text(6, 194, &FONT_XS, "PRESS A KEY: THAT PITCH", C_HI);
        cv_text(6, 208, &FONT_XS, "K1 PITCH  K2 VEL  K3 GATE  K4 LENGTH", C_GRAY);
        cv_text(6, 222, &FONT_XS, "ENV ACCENT   LFO SLIDE   SAVE CLEAR", C_GRAY);
    } else if (wui.rec) {
        cv_text(6, 176, &FONT_S, "REC", RGB(255, 80, 70));
        cv_text(46, 176, &FONT_S, synth.seq.run ? "LIVE: KEYS WRITE THE NEAREST STEP" : "KEYS WRITE STEP BY STEP", C_HI);
        fmt_int(b, wui.rec_pos + 1); cv_text(6, 200, &FONT_XS, "NEXT STEP", C_GRAY); cv_text(70, 200, &FONT_XS, b, C_HI);
    } else {
        fmt_int(b, synth.seq.cur + 1);
        cv_text(6, 176, &FONT_S, "PATTERN", C_GRAY); cv_text(76, 176, &FONT_S, b, C_HI);
        cv_text(100, 176, &FONT_S, "LEN", C_GRAY); fmt_int(b, len); cv_text(132, 176, &FONT_S, b, C_HI);
        cv_text(160, 176, &FONT_S, "BPM", C_GRAY); fmt_int(b, ui_p[P_BPM]); cv_text(194, 176, &FONT_S, b, C_HI);
        if (q->rsv[1]) { fmt_int(b, wt_pat_loop_bar(q)); cv_text(6, 200, &FONT_XS, "LOOPING BAR", C_AMB); cv_text(80, 200, &FONT_XS, b, C_HI); cv_text(98, 200, &FONT_XS, "OF THE PATTERN", C_GRAY); }
        else cv_text(6, 200, &FONT_XS, "TAP A STEP  HOLD IT AND TURN K1-K4", C_GRAY);
        cv_text(6, 214, &FONT_XS, "BLACK KEYS PICK A PATTERN  PLAY STARTS", C_GRAY);
        if (len > 16) { fmt_int(b, wui.sp + 1); cv_text(200, 200, &FONT_XS, "PAGE", C_GRAY); cv_text(226 - text_w(&FONT_XS, b) + 6, 214, &FONT_XS, b, C_HI); }
    }
}

#if !WT_NODRUMS
/* a dial drawn from a text and a fraction (per mille): the drum pots */
static void wui_dial_raw(const char *label, const char *val, int f, int bipolar, int32_t cx, int32_t cy, int32_t r, uint16_t col)
{
    int f0 = bipolar ? 500 : 0;
    arc(cx, cy, r, -135, 135, 3, C_LINE);
    if (f >= f0) arc(cx, cy, r, frac_deg(f0), frac_deg(f), 3, C_AMB); else arc(cx, cy, r, frac_deg(f), frac_deg(f0), 3, C_AMB);
    arc_dot(cx, cy, r, frac_deg(f), 7, C_HI);
    {   int32_t mw = r * 3 / 2;
        const felucca_font_t *ft = text_w(&FONT_B, val) <= mw ? &FONT_B : text_w(&FONT_S, val) <= mw ? &FONT_S : &FONT_XS;
        cv_text(cx - text_w(ft, val) / 2, cy - ft->h / 2 + 1, ft, val, col);
    }
    {   const felucca_font_t *lf = text_w(&FONT_S, label) <= 110 ? &FONT_S : &FONT_XS;
        cv_text(cx - text_w(lf, label) / 2, cy + r + 6, lf, label, C_GRAY);
    }
}

static void wui_dsound(void)                    /* SOUND pages: the track, then three of its pots */
{
    int kit = ui_p[P_DR_KIT] & 1, t = wui.dtrack, k, n = wt_drums_nparams(kit, t);
    char b[16];
    wui_dial_raw("TRACK", wt_drums_track_name(kit, t), t * 1000 / (WT_DTRACKS - 1), 0, 60, 84, 34, C_HI);
    for (k = 0; k < 3; k++) {
        int i = (wui.page - dsound_first()) * 3 + k;
        int32_t cx = 60 + ((k + 1) & 1) * 120, cy = 84 + ((k + 1) >> 1) * 100;
        if (i < n && i < 12) {
            const x0x_param_t *pp = wt_drums_param(kit, t, i);
            int v = dmir[kit][t][i];
            if (pp->names && v <= pp->max) str_cpy(b, pp->names[v], 15);
            else if (pp->max == 127) fmt_pct(b, v * 100 / 127);
            else fmt_int(b, v);
            wui_dial_raw(pp->name, b, pp->max ? v * 1000 / pp->max : 0, 0, cx, cy, 34, C_HI);
        }
    }
}

/* the drum grid: 11 tracks x 16 steps, the playhead blue, the selected track marked, a track's label flashes when it sounds */
static void wui_dsteps(void)
{
    const wt_dpat_t *q = dpat();
    int kit = ui_p[P_DR_KIT] & 1, t, j, pos = drums.run ? drums.pos : -1, len = q->len;
    char b[24];
    for (t = 0; t < WT_DTRACKS; t++) {
        int y = 44 + t * 13, sel = t == wui.dtrack, mute = drums.mute[t] == 1, solo = drums.mute[t] == 2;
        const char *nm = wt_drums_track_name(kit, t);
        if (sel) cv_rect(0, y, 28, 12, C_DIM);
        cv_text(3, y - 1, &FONT_XS, nm, dflash[t] ? C_WHITE : solo ? RGB(120, 255, 140) : sel ? C_HI : mute ? C_LINE : C_GRAY);
        for (j = 0; j < 16; j++) {
            int i = wui.dsp * 16 + j, x = 30 + j * 13, in = i < len && i < WT_DSTEPS, v = in ? q->st[t][i] : 0;
            int outside = in && q->llen && (i < q->lstart || i >= q->lstart + q->llen);                       /* outside the bar being looped */
            uint16_t c = !in || outside ? RGB(14, 14, 18) : (i & 3) == 0 ? RGB(34, 34, 42) : RGB(24, 24, 30);
            if (outside) v = 0;
            if (v) c = (v & DS_ACCENT) ? C_HI : dx_get(q, t, i) & DX_SOFT ? RGB(130, 90, 230) : C_AMB;
            cv_rect(x, y, 12, 11, c);
            if (v) cv_rect(x + 1, y + 10 - (((v & 127) * 8) >> 7), 10, 1, C_WHITE);               /* its velocity, a line */
            if (i == pos) cv_rect(x, y + 11, 12, 1, C_MODB);
            if (i == wui.dheld && t == wui.dtrack) { cv_rect(x - 1, y - 1, 14, 1, C_WHITE); cv_rect(x - 1, y + 11, 14, 1, C_WHITE); }
        }
    }
    if (pos >= wui.dsp * 16 && pos < wui.dsp * 16 + 16) cv_rect(30 + (pos - wui.dsp * 16) * 13, 42, 12, 2, C_MODB);
    str_cpy(b, E_KIT[kit], 6); cv_text(6, 192, &FONT_S, b, C_AMB);
    cv_text(40, 192, &FONT_S, wt_drums_track_name(kit, wui.dtrack), C_HI);
    fmt_int(b, (drums.next != 255 ? drums.next : drums.cur) + 1); cv_text(76, 192, &FONT_S, "PAT", C_GRAY); cv_text(104, 192, &FONT_S, b, C_HI);
    cv_text(130, 192, &FONT_S, "LEN", C_GRAY); fmt_int(b, len); cv_text(158, 192, &FONT_S, b, C_HI);
    if (len > 16) { fmt_int(b, wui.dsp + 1); cv_text(184, 192, &FONT_S, "PG", C_GRAY); cv_text(204, 192, &FONT_S, b, C_HI); }
#if WT_P37
    if (wui.pmode && wui.pheld >= 0) {
        int v = q->st[wui.dtrack][wui.pstep], pr = 100 - (dx_get(q, wui.dtrack, wui.pstep) >> 1) * 2;
        cv_text(6, 214, &FONT_XS, "FADER 1 VELOCITY  FADER 2 PROBABILITY", C_GRAY);
        if (v) { fmt_int(b, v & 127); cv_text(190, 192, &FONT_S, b, C_HI); fmt_pct(b, pr); cv_text(214, 192, &FONT_XS, b, C_AMB); }
    } else
#endif
    if (wui.dheld >= 0) {
        int v = q->st[wui.dtrack][wui.dheld];
        cv_text(6, 214, &FONT_XS, v ? "K2 VELOCITY  ENV ACCENT  SAVE CLEAR" : "RELEASE: OFF", C_GRAY);
        if (v) { fmt_int(b, v & 127); cv_text(190, 192, &FONT_S, b, C_HI); if (v & DS_ACCENT) cv_text(214, 192, &FONT_S, "ACC", C_HI); }
    } else if (q->llen) { fmt_int(b, wt_dpat_loop_bar(q)); cv_text(6, 214, &FONT_XS, "LOOPING BAR", C_AMB); cv_text(80, 214, &FONT_XS, b, C_HI); cv_text(96, 214, &FONT_XS, "(HOME > DRUM LENGTH)", C_GRAY); }
#if WT_P37
    else if (wui.pmode) cv_text(6, 214, &FONT_XS, "STEP ENTRY: TAP PADS  > + PAD: SOUND  >> EXIT", C_AMB);
    else cv_text(6, 214, &FONT_XS, "PADS PLAY THE SOUNDS   >> STEP ENTRY", C_GRAY);
#else
    else cv_text(6, 214, &FONT_XS, wui.rec ? "REC: HIT THE PADS WHILE IT PLAYS" : "BLACK KEYS: PADS   WHITE KEYS: STEPS", C_GRAY);
#endif
}
#endif /* !WT_NODRUMS: drum dials and the drum grid */

/* the performance page: where the audio time goes (the shares are of the time the audio block takes, scaled to the CPU % shown) */
static void wui_perf(void)
{
    static uint32_t acc[4], last_t, cpu_peak;
    static int pct[4], lanes_shown;
    char b[24];
    int i, cpu = (int)plat_cpu_pct();
    uint32_t tot = 0;
    if (fm1_ms - last_t > 500u) {                           /* twice a second: what the synth timed since last time */
        for (i = 0; i < 4; i++) { acc[i] = synth.prof[i]; synth.prof[i] = 0; tot += acc[i]; }
        for (i = 0; i < 4; i++) pct[i] = tot ? (int)((uint64_t)acc[i] * cpu / tot) : 0;
        { uint32_t pk = plat_cpu_peak_pct(); cpu_peak = pk > cpu_peak * 7 / 8 ? pk : cpu_peak * 7 / 8; }
        lanes_shown = (int)synth.lane_budget;
        last_t = fm1_ms;
    }
    {   static const char *const NM[4] = {"VOICES",
#if WT_NODRUMS
            "2ND CORE",
#else
            "DRUMS (WAIT)",
#endif
            "EFFECTS + EQ", "CONTROL"};
        for (i = 0; i < 4; i++) {
            int y = 56 + i * 24, w = pct[i] * 120 / 100;
            cv_text(6, y, &FONT_S, NM[i], C_GRAY);
            cv_rect(120, y + 3, w > 110 ? 110 : w, 10, i == 0 ? C_AMB : C_DIM);
            fmt_pct(b, pct[i]); cv_text(236 - text_w(&FONT_S, b), y, &FONT_S, b, C_HI);
        }
    }
    fmt_pct(b, cpu); cv_text(6, 156, &FONT_M, "CPU", C_GRAY); cv_text(66, 156, &FONT_M, b, cpu >= 90 ? RGB(255, 90, 70) : C_HI);
    fmt_pct(b, (int)cpu_peak); cv_text(134, 160, &FONT_S, "PEAK", C_GRAY); cv_text(180, 160, &FONT_S, b, C_HI);
    fmt_int(b, synth.nact); cv_text(6, 190, &FONT_S, "VOICES", C_GRAY); cv_text(70, 190, &FONT_S, b, C_HI);
    fmt_int(b, lanes_shown); { int n = (int)str_len(b); b[n++] = '/'; fmt_int(b + n, WT_NVOICES * 2 * WT_MAXUNI); }
    cv_text(104, 190, &FONT_S, "LANES", C_GRAY); cv_text(150, 190, &FONT_S, b, C_HI);
    cv_text(6, 214, &FONT_S, "2ND CORE", C_GRAY); cv_text(80, 214, &FONT_S, plat_cpu2_on() && ui_p[P_CORE2] ? "ON" : "OFF", C_HI);
    cv_text(116, 214, &FONT_S, "MODE", C_GRAY); cv_text(160, 214, &FONT_S, E_CPU[ui_p[P_CPUMODE] % 3], C_HI);
    wui.anim = 1;
}

#if WT_NODRUMS
#ifndef FELUCCA_ID
#define FELUCCA_ID "FM-1_922"          /* the device build sets this from the package identity; a --release build passes the release form */
#endif
#ifdef X0X_VERSION
#define WT_FW_VER X0X_VERSION
#else
#define WT_FW_VER "dev"
#endif
static const char *const ABOUT_LINE[4] = {"WT-2 by Dedrich", "Web Editor at thatsoundshot.com", WT_FW_VER, FELUCCA_ID};
static void wui_about(void)
{
    cv_text(120 - text_w(&FONT_S, ABOUT_LINE[0]) / 2, 78, &FONT_S, ABOUT_LINE[0], C_HI);
    cv_text(120 - text_w(&FONT_S, ABOUT_LINE[1]) / 2, 104, &FONT_S, ABOUT_LINE[1], C_WHITE);
    cv_text(120 - text_w(&FONT_XS, "VERSION") / 2, 146, &FONT_XS, "VERSION", C_GRAY);
    cv_text(120 - text_w(&FONT_S, ABOUT_LINE[2]) / 2, 164, &FONT_S, ABOUT_LINE[2], C_HI);
    cv_text(120 - text_w(&FONT_XS, "IDENTITY") / 2, 196, &FONT_XS, "IDENTITY", C_GRAY);
    cv_text(120 - text_w(&FONT_S, ABOUT_LINE[3]) / 2, 214, &FONT_S, ABOUT_LINE[3], C_HI);
}
#endif

#if !WT_NODRUMS
/* the mixer: 13 faders (synth, drums, the 11 tracks), the selected one outlined, a muted track dimmed, a track flashes when it sounds */
static void wui_mixer(void)
{
    int kit = ui_p[P_DR_KIT] & 1, c;
    char b[24];
    for (c = 0; c < 13; c++) {
        int x = 4 + c * 18, lv = mixer_value(c), t = c - 2, sel = c == wui.mch, st = mixer_state(c);
        int h = lv * 96 / 1000;
        uint16_t col = st == 1 ? C_LINE : c == 0 ? C_HI : c == 1 ? C_AMB : (dflash[t] ? C_WHITE : C_AMB);
        const char *nm = c == 0 ? "SY" : c == 1 ? "DR" : wt_drums_track_name(kit, t);
        cv_rect(x + 3, 62, 10, 98, RGB(16, 16, 22));
        cv_rect(x + 3, 160 - h, 10, h, col);
        cv_rect(x + 1, 160 - h - 1, 14, 2, sel ? C_WHITE : col);                          /* the fader cap */
        if (sel) { cv_rect(x, 44, 16, 1, C_WHITE); cv_rect(x, 161, 16, 1, C_WHITE); cv_rect(x, 44, 1, 118, C_WHITE); cv_rect(x + 15, 44, 1, 118, C_WHITE); }
        if (st) cv_text(x + 8 - text_w(&FONT_S, st == 1 ? "M" : "S") / 2, 44, &FONT_S, st == 1 ? "M" : "S", st == 1 ? RGB(255, 90, 70) : RGB(120, 255, 140));
        cv_text(x + 8 - text_w(&FONT_XS, nm) / 2, 164, &FONT_XS, nm, sel ? C_HI : st == 1 ? C_LINE : C_GRAY);
    }
    {   int c2 = wui.mch, t = c2 - 2;
        cv_text(6, 186, &FONT_S, c2 == 0 ? "SYNTH" : c2 == 1 ? "DRUMS" : wt_drums_track_name(kit, t), C_HI);
        fmt_pct(b, mixer_value(c2) / 10); cv_text(62, 186, &FONT_S, b, C_HI);
        if (c2 >= 2) {
            int pi = pan_idx[kit][t], pv = pi >= 0 ? dmir[kit][t][pi] : 64;
            if (pv == 64) str_cpy(b, "C", 3); else if (pv < 64) { b[0] = 'L'; fmt_int(b + 1, (64 - pv) * 100 / 64); } else { b[0] = 'R'; fmt_int(b + 1, (pv - 64) * 100 / 63); }
            cv_text(112, 186, &FONT_S, "PAN", C_GRAY); cv_text(144, 186, &FONT_S, b, C_HI);
        }
        { int st = mixer_state(c2); if (st) cv_text(190, 186, &FONT_S, st == 1 ? "MUTED" : "SOLO", st == 1 ? RGB(255, 90, 70) : RGB(120, 255, 140)); }
    }
    cv_text(6, 210, &FONT_XS, "K1 CHANNEL  K2 LEVEL  K3 PAN  K4 MUTE / SOLO", C_GRAY);
    cv_text(6, 224, &FONT_XS, "BLACK KEYS: HIT AND SELECT A DRUM TRACK", C_GRAY);
}
#endif /* !WT_NODRUMS: the mixer page */

#if WT_P37
static void wui_p37page(void);
#endif
static void wui_content_page(void)
{
    char b[16] = {0};
    const wpage_t *pg = &PAGES[wui.page];
    int i;
    if (wui.sel_down) {                         /* modulation by hand: the source is on show */
        cv_text(6, 3, &FONT_M, "MOD", C_MODB);
        cv_text(66, 3, &FONT_M, wt_src_names[wui.mod_src], C_HI);
        cv_text(6, 26, &FONT_XS, wui.sel_latched ? "TURN ANY KNOB  SEL ENDS" : "TURN ANY KNOB WHILE HOLDING SEL", C_GRAY);
    } else {
        cv_text(6, 3, &FONT_M, pg->title, C_HI);
        cv_text(234 - text_w(&FONT_S, wui.pname), 8, &FONT_S, wui.pname, C_GRAY);
        fmt_int(b, wui.page + 1); i = (int)str_len(b); b[i] = '/'; fmt_int(b + i + 1, NPAGE);
        cv_text(6, 26, &FONT_XS, b, C_DIM);
        if (ui_p[P_SCALE]) { str_cpy(b, E_ROOT[ui_p[P_ROOT] % 12], 3); { int ln = (int)str_len(b); b[ln++] = ' '; str_cpy(b + ln, wt_scale_names[ui_p[P_SCALE] % WT_NSCALES], 14 - ln); } cv_text(60, 26, &FONT_XS, b, C_AMB); }
        fmt_pct(b, (int32_t)plat_cpu_pct());
        cv_text(176, 26, &FONT_XS, ui_p[P_CPUMODE] == 2 ? "SAVE" : synth.lane_budget < WT_NVOICES * 2 * WT_MAXUNI ? "LIMIT" : "", C_AMB);
        cv_text(190, 26, &FONT_XS, "CPU", C_DIM); cv_text(212, 26, &FONT_XS, b, C_GRAY);
    }
    if (pg->kind == K_STEPS) { wui_steps(); if (synth.seq.run) wui.anim = 1; return; }
    if (pg->kind == K_PERF) { wui_perf(); return; }
#if WT_NODRUMS
    if (pg->kind == K_ABOUT) { wui_about(); return; }
#endif
#if WT_P37
    if (pg->kind == K_P37) { wui_p37page(); wui.anim = 1; return; }
#endif
#if WT_LFOSHAPES
    if (pg->kind == K_LFOED) { wui_lfoed(); return; }
#endif
#if !WT_NODRUMS
    if (pg->kind == K_MIXER) { wui_mixer(); return; }
    if (pg->kind == K_DSTEPS) { wui_dsteps(); if (drums.run) wui.anim = 1; return; }
    if (pg->kind == K_DSOUND) { wui_dsound(); return; }
#endif
    if (pg->kind == K_OSC_A || pg->kind == K_OSC_B) {       /* the oscillator's own waveform on every one of its pages, its dials below */
        int o = pg->kind == K_OSC_B;
        cv_text(6, 44, &FONT_S, E_TABLES[ui_p[P_OSC(o, O_TABLE)] % 3], C_AMB);
        if (ui_p[P_OSC(o, O_TABLE)] % 3 == 0) {           /* BASIC SHAPES: the shape at this WT POS */
            int fr = (ui_p[P_OSC(o, O_WTPOS)] * 8 + 500) / 1000;
            cv_text(8 + text_w(&FONT_S, E_TABLES[0]) + 10, 44, &FONT_S, SHAPE_NAME[fr < 0 ? 0 : fr > 8 ? 8 : fr], C_HI);
        }
        if (o && !ui_p[P_OSC(1, O_ON)]) { const char *off = "OFF"; cv_text(234 - text_w(&FONT_S, off), 44, &FONT_S, off, RGB(255, 120, 90)); }
        wui_wave(o, 62, 84);
        for (i = 0; i < 4; i++)
            if (pid[wui.page][i] >= 0) wui_dial(pid[wui.page][i], pg->l[i], 30 + i * 60, 184, 21);
        return;
    }
    if (pg->btn == B_SAVE) {
        for (i = 0; i < 4; i++) if (pid[wui.page][i] >= 0) wui_dial(pid[wui.page][i], pg->l[i], 30 + i * 60, 112, 21);
    } else
    for (i = 0; i < 4; i++)
        if (pid[wui.page][i] >= 0) wui_dial(pid[wui.page][i], pg->l[i], 60 + (i & 1) * 120, 84 + (i >> 1) * 100, 34);
    if (pg->title[0] == 'S' && pg->title[1] == 'Y' && pg->title[2] == 'N' && pg->title[3] == 'C' && !pg->title[4]) {      /* the SYNC page: what the clock is doing */
        char tb[24];
        if (!ui_p[P_CLK_SRC]) cv_text(6, 150, &FONT_S, "OWN TEMPO (PLAY SETUP)", C_GRAY);
        else if (!synth.clk.seen) { cv_text(6, 150, &FONT_S, "WAITING FOR CLOCK ...", RGB(255, 190, 70)); wui.anim = 1; }
        else { int bt = (int)(synth.clk.bpm * 10.0f + 0.5f); fmt_int(tb, bt / 10); { int n = (int)str_len(tb); tb[n++] = '.'; tb[n++] = (char)('0' + bt % 10); tb[n] = 0; }
               cv_text(6, 150, &FONT_S, "LOCKED", C_HI); cv_text(76, 150, &FONT_M, tb, C_HI); cv_text(76 + text_w(&FONT_M, tb) + 6, 156, &FONT_S, "BPM", C_GRAY); wui.anim = 1; }
        cv_text(6, 190, &FONT_XS, "MIDI IN: THE TRS JACK.  USB: THE USB-C PORT.", C_GRAY);
        cv_text(6, 204, &FONT_XS, "START / STOP ON: START, CONTINUE AND STOP", C_GRAY);
        cv_text(6, 218, &FONT_XS, "FROM THE CLOCK CONTROL THE SEQUENCERS.", C_GRAY);
    }
    if (pg->btn == B_SAVE) {                                                          /* what the flash held at power-on (to see what an update left behind) */
        static const char *const SN[3] = {"NONE", "BAD", "OK"};
        char sb[40];
        { uint32_t n; const char *p1 = "PATCH STORE ", *p2;
          n = 0; for (p2 = p1; *p2; p2++) sb[n++] = *p2;
          for (p2 = SN[store_stat[0] + 1]; *p2; p2++) sb[n++] = *p2;
          sb[n++] = ' '; for (p2 = SN[store_stat[1] + 1]; *p2; p2++) sb[n++] = *p2;
          sb[n] = 0; }
        cv_text(6, 200, &FONT_XS, sb, C_DIM);
        {   char ib[40]; uint32_t n = 0; const char *p2; char t2[8];
            for (p2 = "OBJ A: "; *p2; p2++) ib[n++] = *p2;
            fmt_int(t2, store_np[0]); for (p2 = t2; *p2; p2++) ib[n++] = *p2;
            for (p2 = " PARAMS  "; *p2; p2++) ib[n++] = *p2;
            fmt_int(t2, store_named[0]); for (p2 = t2; *p2; p2++) ib[n++] = *p2;
            for (p2 = " / "; *p2; p2++) ib[n++] = *p2;
            fmt_int(t2, store_nrec[0]); for (p2 = t2; *p2; p2++) ib[n++] = *p2;
            for (p2 = " NAMED  "; *p2; p2++) ib[n++] = *p2;
            fmt_int(t2, store_len[0]); for (p2 = t2; *p2; p2++) ib[n++] = *p2;
            ib[n++] = 'B'; ib[n] = 0;
            cv_text(6, 226, &FONT_XS, ib, C_DIM); }
        { uint32_t n = 0; const char *p2;
          for (p2 = "PATTERNS "; *p2; p2++) sb[n++] = *p2;
          for (p2 = SN[store_stat[2] + 1]; *p2; p2++) sb[n++] = *p2;
#if WT_NODRUMS
          for (p2 = "   SETUP "; *p2; p2++) sb[n++] = *p2;
#else
          for (p2 = "   DRUMS "; *p2; p2++) sb[n++] = *p2;
#endif
          for (p2 = SN[store_stat[3] + 1]; *p2; p2++) sb[n++] = *p2;
          sb[n] = 0; }
        cv_text(6, 214, &FONT_XS, sb, C_DIM);
    }
    if (pg->btn == B_SAVE) {
        const char *nm = uname[wui.slot % NUSER];
        if (wui.what == 0 || wui.what == 3) {               /* the name being typed, the place being edited underlined */
            int k, x = 6; char c1[2];
            cv_text(6, 40, &FONT_XS, "NAME", C_DIM);
            for (k = 0; k < 11; k++) {
                c1[0] = wui.sname[k] == ' ' ? '_' : wui.sname[k]; c1[1] = 0;
                cv_text(x, 52, &FONT_M, c1, k == wui.npos ? C_HI : C_WHITE);
                if (k == wui.npos) cv_rect(x, 72, 16, 2, C_HI);
                x += 20;
            }
        }
        cv_text(6, 160, &FONT_XS, wui.what == 1 ? "STORES ALL 16 PATTERNS" : wui.what == 2 ? 
#if WT_NODRUMS
            "STORES MIDI, SCALE, CLOCK, SHAPES"
#else
            "STORES DRUMS, KITS, MIDI SETUP"
#endif
            : wui.what == 4 ? "EMPTIES THE SLOT" : nm[0] ? "SLOT IN USE:" : "SLOT IS EMPTY", C_GRAY);
        if (nm[0] && (wui.what == 0 || wui.what == 3 || wui.what == 4)) cv_text(100, 160, &FONT_XS, nm, C_HI);
        if (wui.armed) cv_text(6, 176, &FONT_M, "PRESS SAVE AGAIN", C_AMB);
        else if (wui.msg) cv_text(6, 176, &FONT_M, wui.msg == 1 ? "SAVED" : wui.msg == 2 ? "ERASED" : "FAILED", wui.msg == 3 ? RGB(255, 80, 70) : C_HI);
        else cv_text(6, 176, &FONT_S, "SAVE TWICE TO CONFIRM", C_DIM);
        if (wui.armed || wui.msg) wui.anim = 1;
    }
}

#if WT_P37
static void p37_row(int y, const char *label, int a, int b2, int c, int d, int n)      /* a label and up to four numbers */
{
    char t[12];
    int v[4], i, x = 104;
    v[0] = a; v[1] = b2; v[2] = c; v[3] = d;
    cv_text(6, y, &FONT_S, label, C_GRAY);
    for (i = 0; i < n; i++) { fmt_int(t, v[i]); cv_text(x, y, &FONT_S, t, C_HI); x += 34; }
}
static void wui_p37page(void)                   /* what the controls report: the faders, the wheels, the pedal, the pads, the last button and key */
{
    int p, mx = 0;
    for (p = 0; p < 16; p++) { int q = plat_pad_press(p); if (q > mx) mx = q; }
    p37_row(46, "FADERS", plat_fader(0), plat_fader(1), plat_fader(2), plat_fader(3), 4);
    p37_row(66, "WHEEL RAW", plat_wheel(0), plat_wheel(1), 0, 0, 2);
    p37_row(86, "BEND MOD", wui.bend_sent, wui.mod_sent, 0, 0, 2);
    p37_row(106, "PEDAL", plat_pedal(), 0, 0, 0, 1);
    p37_row(126, "PAD PRESS", mx, 0, 0, 0, 1);
    p37_row(146, "LAST KEY V", wui.lastvel, 0, 0, 0, 1);
    p37_row(166, "LAST BUTTON", wui.lastbtn, 0, 0, 0, 1);
    p37_row(186, "FADER BANK", wui.fbank + 1, 0, 0, 0, 1);
    cv_text(6, 214, &FONT_XS, "THE BUTTON NUMBER IS ITS ROLE + 1", C_DIM);
}
static void wui_p37overlay(void)                 /* the fader assignment while FADER BANK is held, or the readout of a fader / cutoff knob just moved */
{
    char b[16];
    int i;
    if (wui.fb_down && fm1_ms - wui.fb_t0 > 450u) {
        cv_rect(8, 60, 224, 140, RGB(10, 10, 16)); cv_rect(8, 60, 224, 2, C_HI); cv_rect(8, 198, 224, 2, C_HI);
        fmt_int(b, wui.fbank + 1);
        cv_text(14, 66, &FONT_S, "FADER BANK", C_HI); cv_text(104, 66, &FONT_S, b, C_AMB);
        cv_text(124, 70, &FONT_XS, "TURN K5-K8", C_GRAY);
        for (i = 0; i < 4; i++) {
            int id = fdr_get(wui.fbank, i);
            fmt_int(b, i + 1);
            cv_text(14, 92 + i * 24, &FONT_S, "F", C_GRAY); cv_text(24, 92 + i * 24, &FONT_S, b, C_GRAY);
            cv_text(44, 92 + i * 24, &FONT_S, id >= 0 ? pname(id) : "-", C_WHITE);
        }
        wui.anim = 1;
    } else if (wui.ft_t) {
        if (wui.ft_id >= 0 && wui.ft_id < P_COUNT) {
            wui_value(b, wui.ft_id, ui_p[wui.ft_id]);
            cv_rect(0, 214, 240, 26, RGB(10, 10, 16)); cv_rect(0, 214, 240, 2, C_HI);
            cv_text(8, 220, &FONT_S, pname(wui.ft_id), C_GRAY); cv_text(232 - text_w(&FONT_S, b), 220, &FONT_S, b, C_HI);
        } else {                                                 /* a bank change: the four assignments */
            int k;
            cv_rect(0, 196, 240, 44, RGB(10, 10, 16)); cv_rect(0, 196, 240, 2, C_HI);
            fmt_int(b, wui.fbank + 1);
            cv_text(8, 200, &FONT_XS, "FADERS", C_GRAY); cv_text(54, 200, &FONT_XS, b, C_AMB);
            for (k = 0; k < 4; k++) { int id = fdr_get(wui.fbank, k); cv_text(8 + (k & 1) * 118, 214 + (k >> 1) * 12, &FONT_XS, id >= 0 ? pname(id) : "-", C_WHITE); }
        }
        wui.anim = 1;
    }
}
#endif

static void wui_content(void)
{
    wui_content_page();
#if WT_P37
    wui_p37overlay();
#endif
    if (wui.navbtn >= 0 && !wui.nav_used && fm1_ms - wui.nav_t > 200u && nav_index(wui.navbtn) >= 0) {       /* the shortcut list */
        int ni = nav_index(wui.navbtn), n = nav_count(ni), j;
        char nb[8];
        cv_rect(8, 38, 224, 24 + n * 20, RGB(10, 10, 16)); cv_rect(8, 38, 224, 2, C_HI); cv_rect(8, 60 + n * 20, 224, 2, C_HI);
        cv_text(14, 44, &FONT_XS, NAV[ni].name, C_HI); cv_text(60, 44, &FONT_XS, "PRESS A WHITE KEY:", C_GRAY);
        for (j = 0; j < n; j++) {
            fmt_int(nb, j + 1);
            cv_text(16, 62 + j * 20, &FONT_S, nb, C_AMB);
            cv_text(40, 62 + j * 20, &FONT_S, NAV[ni].t[j], title_page(NAV[ni].t[j]) == wui.page ? C_HI : C_WHITE);
        }
        wui.anim = 1;
    }
    if (wui.toast_t) {                          /* the table PRESETS just chose, over the picture */
        const char *tn = wui.toast2;
        cv_rect(10, 84, 220, 66, RGB(10, 10, 16)); cv_rect(10, 84, 220, 2, C_HI); cv_rect(10, 148, 220, 2, C_HI);
        cv_text(120 - text_w(&FONT_S, wui.toast) / 2, 92, &FONT_S, wui.toast, C_GRAY);
        cv_text(120 - text_w(&FONT_M, tn) / 2, 112, &FONT_M, tn, C_HI);
        wui.anim = 1;
    }
}

static void wui_draw(void)
{
    int band;
    wui.anim = 0;
    for (band = 0; band < 4; band++) {
        cv_begin(240, 60, C_BG);
        cv_oy = -band * 60;
        wui_content();
        cv_oy = 0;
        cv_blit(0, (uint32_t)band * 60u);
    }
    lcd_sync();
}

static void wui_frame(void)
{
    wui_input();
#if WT_P37
    p37_leds();
#endif
    if (wui.dirty || fm1_ms - wui.last_draw > (wui.anim ? 70u : 250u)) {
        wui.dirty = 0;
        wui.last_draw = fm1_ms;
        wui_draw();
    }
}
