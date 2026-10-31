/* SPDX-License-Identifier: GPL-3.0-only
 * WT-1 core: a portable wavetable synth engine. No hardware or OS dependency.
 * Audio path is integer only; tables are built at init (host libm; to be replaced by generated
 * headers for the device build). Render in multiples of WT_CTL (16) frames. */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "wt_fx.h"

#ifndef WT_N
#define WT_N 512                    /* samples per frame at mip level 0 (power of two, 256..1024) */
#endif
#if WT_N == 256
#define WT_LOGN 8
#elif WT_N == 512
#define WT_LOGN 9
#elif WT_N == 1024
#define WT_LOGN 10
#else
#error "WT_N must be 256, 512 or 1024"
#endif
#define WT_MAXLEV 8
#ifndef WT_MINLG
#define WT_MINLG 6                  /* smallest stored cycle: 64 samples */
#endif
#define WT_MAXFRAMES 64
#define WT_NTABLES 3
#define WT_NVOICES 8
#define WT_MAXUNI 5
#define WT_CTL 16                   /* control block, samples */
#define WT_FS 44100
#define WT_DLY_LEN 32768            /* the delay line, samples (0.74 s); on the device it lives in the pool */
#ifndef WT_POOL
#define WT_POOL
#endif

/* ---- runtime configuration (experiments; set before building tables) ---- */
typedef struct {
    int interp;                     /* 1 = linear, 3 = cubic (Catmull-Rom) */
    int morph;                      /* 1 = spectral frame morph (tables get extra in-between frames), 0 = waveform crossfade */
    int morph_cap;                  /* with morph: at most this many internal frames per table (RAM) */
    int taper;                      /* percent of each level's harmonics (the top ones) rolled off with a cos^2 slope; 0 = none */
    int bwdiv;                      /* mip level L keeps harmonics up to (N>>L)/bwdiv - 1: 2 = none above Nyquist of the table, 4, 8 = more oversampling */
} wt_cfg_t;
extern wt_cfg_t wt_cfg;

/* ---- wavetables ---- */
typedef struct {
    char name[12];
    int nframes, nlev;              /* nframes = internal frames read by WT POS (>= nsrc when morph expands) */
    int nsrc, expand, morph;        /* stored frames, in-between frames per gap + 1, spectral morph on */
    int H[WT_MAXLEV];               /* harmonics kept at each level */
    int lg[WT_MAXLEV];              /* log2 of the level's samples per frame (never below WT_MINLG: tiny tables interpolate badly) */
    int16_t *lev[WT_MAXLEV];        /* level L: nframes x (1 << lg[L]) samples, peak about 0.9 */
    int16_t *mem;
    int owned;                      /* mem was malloc'ed by wt_table_build (the host); wt_table_free frees it */
} wt_table_t;

void wt_global_init(void);          /* host: builds the shared lookup tables (the device has them as const data); safe to call twice */
/* The portable builder (wt_build.c): internal frames in, band-limited mip levels out, into memory the caller owns. */
int wt_levels_layout(wt_table_t *t, int nint);       /* returns the samples of memory the levels need */
int wt_table_build_levels(wt_table_t *t, const char *name, const int16_t *frames, int nint, int nsrc, int expand,
                          int morph, int16_t *mem, int cap);
/* frames: nframes x WT_N int16. Copies and builds the band-limited mip levels. */
int wt_table_build(wt_table_t *t, const char *name, const int16_t *frames, int nframes);   /* uses wt_cfg.morph */
int wt_table_build_ex(wt_table_t *t, const char *name, const int16_t *frames, int nframes, int morph);
void wt_table_free(wt_table_t *t);
int wt_table_bytes(const wt_table_t *t);    /* RAM used by the built levels */
/* looping samples for the noise oscillator's sample layer (mono 44.1 kHz int16, at most 65535 samples) */
#define WT_NSAMPLES 4
typedef struct { char name[12]; const int16_t *d; int n; int owned; } wt_sample_t;
int wt_gen_samples(wt_sample_t out[3]);     /* CRACKLE, AIR, RUMBLE: generated, seamless loops */
void wt_sample_free(wt_sample_t *s);

/* generated tables (original shapes, not Xfer's) */
int wt_gen_basic_shapes(wt_table_t *t);     /* 16 frames, see SPEC.md section 4 */
int wt_gen_fold_sine(wt_table_t *t);        /* 16 frames */
int wt_gen_internal(int which, int morph, int16_t *out, int *nsrc, int *expand);   /* internal frames of factory table 0..3 (BASIC, FOLD, ODD-EVEN, SAW): returns their count */
int wt_gen_saw(wt_table_t *t);              /* 1 frame, used by the sub oscillator's square and pulse */
int wt_gen_odd_even(wt_table_t *t);         /* 16 frames, odd harmonics to even harmonics */

/* ---- parameters (all int16; ranges in wt_param_desc) ---- */
enum { MOD_NONE, S_LFO1, S_LFO2, S_ENV2, S_ENV3, S_VEL, S_NOTE, S_MAC1, S_MAC2, S_MAC3, S_MAC4, S_RAND, S_MOD, S_ACCENT, S_COUNT };
enum { D_NONE, D_WTPOS, D_WTPOS_B, D_WARP, D_WARP_B, D_DETUNE, D_DETUNE_B, D_LEVEL, D_LEVEL_B, D_PAN, D_PAN_B,
       D_PITCH, D_PITCH_B, D_FM, D_FM_B, D_MORPH, D_MORPH_B, D_CUTOFF, D_RESO, D_DRIVE, D_SUBPW,
#if WT_NODRUMS
       D_SUBLVL, D_NOILVL, D_AMPATK, D_AMPDEC, D_AMPREL, D_LFO1R, D_LFO2R, D_CHMIX, D_DLSEND, D_RVSEND, D_MASTER,
#endif
       D_COUNT };
                                               /* osc dest = base + osc (0 = A, 1 = B) */
extern const char *const wt_src_names[S_COUNT], *const wt_dst_names[D_COUNT];

enum { O_ON, O_TABLE, O_WTPOS, O_WARPMODE, O_WARP, O_OCT, O_SEMI, O_FINE, O_PHASE, O_RAND, O_UNI, O_DETUNE,
       O_BLEND, O_SPREAD, O_LEVEL, O_PAN, O_FM, O_PD, O_TABLE2, O_MORPH, O_FLT, O_SIZE };
/* Mapped to Serum 2:
 *  O_WARPMODE 0 off, 1 bend, 2 sync, 3 mirror. WT-2 also has 4 asym, 5 quantize, 6 PWM.
 *  O_FM  frequency modulation by the OTHER oscillator (Serum "FM (B)": linear, thru-zero). The other oscillator may be
 *        turned down to LEVEL 0 and still modulates. O_PD is phase distortion from the other oscillator ("PD (B)").
 *  O_TABLE2 / O_MORPH: crossfade to a second wavetable read at the same WT POS (the same as the morph a sweep
 *        between two wavetables needs: modulate WT POS and MORPH from one source).
 *  O_FLT 1 = through the filter, 0 = straight to the output (Serum's filter routing switches). */
enum { E_DLY, E_ATK, E_HLD, E_DEC, E_SUS, E_REL, E_SIZE };
enum {
    P_MASTER, P_TUNE, P_VOICEMODE, P_GLIDE, P_BEND, P_VELAMP,
    P_OSC0,                                     /* OSC A: P_OSC0 + O_*, OSC B: P_OSC0 + O_SIZE + O_* */
    P_SUB_ON = P_OSC0 + 2 * O_SIZE, P_SUB_WAVE, P_SUB_OCT, P_SUB_CRS, P_SUB_PW, P_SUB_PHASE, P_SUB_CONT, P_SUB_LEVEL,
    P_SUB_PAN, P_SUB_FLT,                       /* SUB_WAVE 0 sine, 1 triangle, 2 square, 3 pulse (SUB_PW 0 = 50 % .. 1000 = 4 %);
                                                   SUB_CRS in cents; SUB_CONT 1 = a new note continues the previous note's phase */
    P_NOI_ON, P_NOI_TYPE, P_NOI_STEREO, P_NOI_FILTER, P_NOI_LEVEL, P_NOI_PAN, P_NOI_FLT,
                                                /* NOI_TYPE 0 white, 1 pink, 2 brown, 3 geiger, 4 off (sample layer only);
                                                   NOI_STEREO 0 = mono .. 1000 = uncorrelated; NOI_FILTER < 0 low-pass, > 0 high-pass */
    P_NOI_SMP, P_NOI_SMP_LEVEL, P_NOI_SMP_PITCH, P_NOI_SMP_FINE, P_NOI_SMP_KEY, P_NOI_SMP_START, P_NOI_SMP_RAND,
    P_NOI_SMP_LOOP,                             /* looping sample layer: NOI_SMP 0 = none, 1..WT_NSAMPLES; LOOP 1 = loop, 0 = one shot */
    P_F_TYPE, P_F_CUTOFF, P_F_RESO, P_F_DRIVE, P_F_KEYTRK, P_F_MIX,
    P_ENV0,                                     /* AMP, ENV2, ENV3: P_ENV0 + n * E_SIZE + E_* */
    P_LFO0 = P_ENV0 + 3 * E_SIZE,               /* LFO1, LFO2: P_LFO0 + n * 2 + (0 shape, 1 rate) */
    P_MAC0 = P_LFO0 + 4,                        /* 4 macros */
    P_MX0 = P_MAC0 + 4,                         /* 8 slots x (src, dst, amount) */
    P_FX0 = P_MX0 + 8 * 3,                      /* X0X's effects, as its pots: P_FX0 + FX_* (wt_fx.h): volume, distortion type and drive,
                                                   compressor, reverb (decay, tone, HPF, level), delay (time, feedback, tone, level, HPF,
                                                   type, wear, ping-pong) */
    P_FX_RVSEND = P_FX0 + FX_NPARAMS, P_FX_DLSEND,   /* how much of the synth goes to the reverb and the delay, 0..127 */
    P_BPM,                                      /* tempo for the delay time and the sequencer, 40..240 */
    P_ACCENT,                                   /* how much an accented step lifts the velocity (towards 127), 0..1000 */
    P_CPUMODE,                                  /* 0 FULL: no load governor; 1 AUTO: sheds unison lanes when the CPU runs out; 2 SAVE: always lean (linear reads, 32 lanes) */
    P_SLIDE,                                    /* the glide time of a sequencer slide (an envelope-time curve), 0..1000 */
    /* appended after the first saved patches: new parameters go at the end so stored patches keep their meaning */
    P_ARP_ON, P_ARP_MODE, P_ARP_RATE, P_ARP_OCT, P_ARP_GATE, P_ARP_SWING, P_ARP_LATCH,   /* the arpeggiator */
    P_EQ_ON, P_EQ_LOW, P_EQ_LOWF, P_EQ_MID, P_EQ_MIDF, P_EQ_Q, P_EQ_HIGH, P_EQ_HIGHF,    /* the EQ: gains in 0.1 dB, frequencies in Hz, Q in tenths */
    P_DR_KIT, P_DR_LEVEL, P_DR_SEQ, P_CORE2,
    P_SYN_DIST, P_SYN_DRIVE, P_DR_DIST, P_DR_DRIVE, P_DR_RVSEND, P_DR_DLSEND, P_SYN_LEVEL,
    P_SC_AMT, P_SC_REL,                         /* sidechain: how far the bass drum ducks the synth (0..1000) and how long it takes to come back */
    P_SCALE, P_ROOT, P_SCALE_SEQ,
    P_SYN_MUTE, P_DR_MUTE,                      /* the mixer: 0 normal, 1 muted, 2 solo (the synth; the drum machine as a whole). A drum track's own state is in wt_drums_t.mute */
    P_MIDI_CH, P_MIDI_DRCH, P_MIDI_OUTCH,       /* USB MIDI: the channel the synth listens on (0 = all), the drums' channel (0 = off, 10 default), the channel it sends played notes on (0 = off) */
    P_USB_AUDIO,                                /* the stereo mix over USB audio: 1 on, 0 off (device) */
    P_CLK_SRC, P_CLK_SS,                        /* the sequencers' clock: 0 internal, 1 MIDI (the TRS jack), 2 USB; and whether Start / Continue / Stop from it start and stop them */
    P_LFO1_SYNC, P_LFO2_SYNC,                   /* an LFO's rate as a note value of the tempo: 0 free (its RATE), 1..15 from 4 bars to 1/32 (wt_lfo_div_names) */               /* scale mode: the scale (0 off), its root note (0 = C), and whether the sequencer's playback is held to it too */        /* per source: distortion on the synth / on the drums, and the drums' own sends (the synth's are P_FX_RVSEND / _DLSEND) */                                                           /* the drums: kit (909 / 808), level, the drum sequencer follows PLAY; the second core renders them (device) */
    P_LFO1_KEY, P_LFO2_KEY,                     /* an LFO restarting at a note: 0 free-running, 1 at every note, 2 at the first note of a phrase (none held) */
    P_LFO1_POL, P_LFO2_POL,                     /* an LFO's swing: 0 both ways, 1 positive only (0 .. top), 2 negative only (bottom .. 0); each at full strength */
    P_FDR0,                                     /* the P37's faders, 4 banks x 4: P_FDR0 .. P_FDR0 + 7, two assignments each (a parameter number + 1, 0 = none; low byte, high byte) */
    P_FDR7 = P_FDR0 + 7,
    P_DR_ACCENT,                                /* the LOUD drum accent: -127..127 = relative to the step's velocity, 128..255 = fixed velocity (value - 128) */
    P_DR_SOFT,                                  /* the SOFT drum accent, the same coding */
    P_COUNT
};
_Static_assert(P_COUNT == 206, "the patch store is 206 parameters");
/* Drawn LFO shapes (WT2): LFO SHAPE 7.. = user shape 0.. of a bank the editor fills. A shape is up to 16 breakpoints; the synth keeps
 * each as a 128-sample table. x 0..255 across the cycle, y -127..127, c = the bend of the segment that ends at the point (-100..100). */
#ifndef WT_LFOSHAPES
#ifdef WT_NODRUMS
#define WT_LFOSHAPES WT_NODRUMS
#else
#define WT_LFOSHAPES 0
#endif
#endif
/* WT2 has no drums, so three of the drums' parameters are the chorus's (patch storage has no room for new ones) */
#if WT_LFOSHAPES
#define P_CH_MIX P_SC_AMT
#define P_CH_RATE P_SC_REL
#define P_CH_DEPTH P_DR_LEVEL
#endif
#define WT_NUSHAPE 8
#define WT_SHP_PTS 16
#define WT_LTAB 128
typedef struct { uint8_t n; uint8_t x[WT_SHP_PTS]; int8_t y[WT_SHP_PTS], c[WT_SHP_PTS]; } wt_lfoshape_t;
void wt_lfoshape_default(int k, wt_lfoshape_t *o);
const char *wt_lfoshape_name(int k);
void wt_lfoshape_table(const wt_lfoshape_t *sh, int16_t out[WT_LTAB]);
#define P_OSC(o, f) (P_OSC0 + (o) * O_SIZE + (f))
#define P_ENV(n, f) (P_ENV0 + (n) * E_SIZE + (f))
#define P_LFO(n, f) (P_LFO0 + (n) * 2 + (f))
#define P_MX(i, f) (P_MX0 + (i) * 3 + (f))

typedef struct { const char *name; int16_t min, max, def; } wt_param_desc_t;
extern wt_param_desc_t wt_param_desc[P_COUNT];
int wt_param_find(const char *name);           /* case-insensitive; -1 if unknown */

/* ---- synth ---- */
typedef struct { uint8_t st; int32_t lvl; uint32_t cnt; } wt_env_t;
typedef struct {
    uint8_t active, gate, note, accent, slide;
    uint32_t age;
    int32_t pitch, target;                     /* 1/256 semitone */
    int32_t vel;                               /* Q15 */
    int32_t rnd;                               /* Q15 bipolar, per note */
    uint32_t ph[2][WT_MAXUNI], phsub;
    int32_t last[2];                           /* previous mono osc output, for FM */
    wt_env_t env[3];
    int32_t fs[2][2][2];                       /* filter state [stage][channel][ic1, ic2] */
    /* per block */
    struct {
        int n, mode, warp, fm, pd, t2, on, flt;
        uint32_t inc[WT_MAXUNI];
        int32_t wl[WT_MAXUNI], wr[WT_MAXUNI];  /* Q15 gains with level, unison weight and pan */
        const int16_t *p0[WT_MAXUNI], *p1[WT_MAXUNI], *p2[WT_MAXUNI], *p3[WT_MAXUNI];
        int sh[WT_MAXUNI];
        int32_t ff, ff2, morph, rawnorm;
    } o[2];
    int32_t subinc, sub_wl, sub_wr;
    int subw, subflt, noiflt;
    uint32_t subthr;                           /* pulse: phase distance between the two saws */
    const int16_t *subp; int subsh; int32_t subsc;   /* band-limited saw (mip level), its size, pulse scale */
    int32_t noi_wl, noi_wr, smp_wl, smp_wr;
    int ntype, nstereo, nfilt;                 /* noise colour, stereo width 0..1000, filter amount -1000..1000 */
    int32_t nfa;                               /* noise filter coefficient, Q15 */
    int32_t nz_pk[2][3], nz_brown[2], nz_geig[2], nz_lp[2];
    const int16_t *sd; int sn;                 /* sample layer: data, length */
    uint32_t spos, sinc;                       /* position and step, Q16 samples */
    int sdone;
    int ftype;
    int32_t fa1, fa2, fa3, fk, fmix, fdrive;
    int32_t fb1, fb2, fb3, fkb;                /* VOWEL: the second formant's filter (the first is fa1..fk) */
    int32_t velgain;
#if WT_NODRUMS
    int16_t amp_atk, amp_dec, amp_rel;   /* amp envelope times after modulation (the patch value when nothing is routed) */
    int16_t gd[6];                       /* global destinations from this voice: LFO rates, chorus, sends, master */
#endif
} wt_voice_t;


/* ---- the step sequencer (wt_seq.c): X0X's design for a polyphonic synth ----
 * The UI edits patterns in place from the main loop while the audio code reads them: every field is a single byte, so a read
 * never sees half a write (a step changed between two of its bytes plays a little odd for one pass at worst). */
#define WT_NPAT 16
#define WT_NSTEP 64
enum { SF_ON = 1, SF_ACCENT = 2, SF_SLIDE = 4, SF_TIE = 8 };      /* a step's flags: plays, accented, glides into the next, held from the one before */
typedef struct {
    uint8_t note[4];                /* MIDI notes (a chord); 0 = unused; note[0] = 0 with SF_ON = a rest-like empty step */
    uint8_t vel;                    /* 1..127 */
    uint8_t gate;                   /* % of the step the note is held: 5..100 */
    uint8_t flags;                  /* SF_* */
    uint8_t prob;                   /* chance it plays, 5..100 % */
} wt_step_t;
enum { SR_16, SR_16T, SR_32, SR_8T, SR_N };                       /* step length: 16th, 16th triplet, 32nd, 8th triplet */
enum { SD_FWD, SD_REV, SD_PING, SD_RAND, SD_N };
typedef struct {
    wt_step_t step[WT_NSTEP];
    uint8_t len;                    /* 1..64 */
    uint8_t rate;                   /* SR_* */
    uint8_t swing;                  /* 50 = straight .. 75: the share of a pair of steps the first one takes, % */
    uint8_t dir;                    /* SD_* */
    uint8_t trans;                  /* semitones + 24 (24 = none) */
    uint8_t rsv[3];
} wt_pat_t;
typedef struct {
    wt_pat_t pat[WT_NPAT];
    volatile uint8_t run;           /* 1 = playing */
    volatile uint8_t cur;           /* the pattern playing (and the one the editor shows when stopped) */
    volatile uint8_t next;          /* queued pattern, takes over when the playing one wraps; 255 = none */
    volatile int16_t pos;           /* the step that sounded last (-1 = none yet): the playhead */
    volatile uint8_t phase8;        /* how far through that step, 0..255 (to put a live-recorded note on the nearer step) */
    int8_t pp;                      /* ping-pong direction */
    uint32_t acc_q8, len_q8, n, rng, t;     /* time in the step, the step's length (samples << 8), steps played, a random state, samples since start */
    struct { uint8_t note, used; int32_t off_in; } held[8];   /* notes the sequencer holds: samples until their note-off, -1 = until the next step */
    int16_t slide_from;             /* the note a slide starts from (the previous step had SF_SLIDE), -1 */
} wt_seq_t;

/* ---- the arpeggiator (wt_seq.c): the keys you hold are played one at a time, in time with P_BPM ---- */
enum { AM_UP, AM_DOWN, AM_UPDOWN, AM_DOWNUP, AM_RANDOM, AM_PLAYED, AM_CHORD, AM_N };
typedef struct {
    struct { uint8_t note, vel, down; } h[16];   /* held notes in the order played; down = the key is physically down (a latched note is not) */
    uint8_t nheld, run, nsnd;
    uint8_t snd[16];                  /* notes the arpeggiator is sounding now */
    int16_t pos;                      /* index into the expanded note list */
    int8_t pp;                        /* up-down direction */
    uint32_t acc_q8, len_q8, n, rng;
    int32_t off_in;                   /* samples until the sounding notes are released */
    volatile uint8_t step;            /* counts steps, for the screen's blink */
} wt_arp_t;

/* ---- the EQ: low shelf, one peak, high shelf (RBJ biquads), on the master ---- */
typedef struct { d9_shape_t sh; float st[2][2]; int16_t key; } wt_sdist_t;     /* a distortion on one source (the synth, or the drums) */
typedef struct { float b0, b1, b2, a1, a2; } eq_coef_t;
typedef struct {
    eq_coef_t c[3];
    float z[2][3][2];
    int16_t key[8];                   /* the parameters the coefficients were made from */
    uint8_t on;
} wt_eq_t;

#define WT_NSCALES 16
extern const char *const wt_scale_names[WT_NSCALES];
extern const char *const wt_lfo_div_names[16];   /* FREE, 4 BAR .. 1/32 */
int wt_scale_snap(int scale, int root, int note);                /* the nearest note of the scale (the one below on a tie); scale 0 = off: unchanged */
int wt_scale_step(int scale, int root, int note, int dir);       /* the next note of the scale above (dir > 0) or below */
/* an external MIDI clock (24 ticks to the beat): a phase-locked loop turns the ticks into a steady position, so the sequencers step smoothly
 * although the ticks reach the audio code in bursts every few milliseconds */
typedef struct {
    float pos, r, period, corr;     /* position in ticks since Start, ticks per sample, samples per tick, a phase correction to spread over the next chunk */
    uint32_t ticks, smp, mark_ticks, mark_smp, last_rx, last_plan, pending;
    uint8_t seen, started, bpm_ok;
    float bpm;
} wt_clk_t;
struct wt_drums;
typedef struct {
    int16_t p[P_COUNT];
    const wt_table_t *table[WT_NTABLES];
    wt_sample_t smp[WT_NSAMPLES];
    wt_voice_t v[WT_NVOICES];
    uint32_t lfo_ph[2], lfo_rnd[2][2];
#if WT_LFOSHAPES
    int16_t ch_buf[2][512];                    /* the chorus's delay lines (WT2) */
    uint32_t ch_ph; uint16_t ch_w;
#endif
#if WT_LFOSHAPES
    int16_t lfo_tab[WT_NUSHAPE][WT_LTAB];      /* the drawn shapes, built by wt_synth_set_shape */
#endif
    int32_t lfo_val[2];
    uint32_t rng, age;
    uint32_t last_subph;                       /* for SUB_CONT */
    int bend, modwheel;                        /* bend -8192..8191, mod wheel 0..127 */
    uint8_t stack[16]; int nstack;             /* mono note stack */
    fxbus_t fx;
    wt_seq_t seq;
    wt_arp_t arp;
#if !WT_NODRUMS
    struct wt_drums *dr;                      /* the drum machine (wt_drums.h); null = none */
#endif
    int32_t chunkL[256], chunkR[256];         /* the synth voices of one chunk, before the effects */
    wt_eq_t eq;
#if WT_NODRUMS
    wt_sdist_t sdist[1];                      /* the synth's own distortion (WT-2 has no drum distortion) */
#else
    wt_sdist_t sdist[2];                      /* 0 the synth's voices, 1 the drums */
#endif
    uint32_t prof[4];                         /* clock ticks spent: voices, the other core (drums on WT-1, the voice job on WT-2), effects / EQ / output, control */
    wt_clk_t clk;
    uint32_t blk_inc[16];                     /* per control block of this chunk: how far the sequencers' clocks move (sample << 8): 16 samples when the clock is internal */
    int cur_blk;
    float mix_syn_g;                          /* the synth's mute / solo gain as last applied (it ramps, no clicks) */
#if !WT_NODRUMS
    float mix_dr_g;                           /* the drum machine's mute / solo gain */
#endif
    uint8_t no_midi_out;                      /* set while a note from USB MIDI is played: it is not sent back out */
#if !WT_NODRUMS
    float sc_env, sc_gain;                    /* sidechain: the duck's envelope (1 = just kicked) and the gain last applied */
#endif
    uint8_t keymap[128];                      /* the note a key was played as (scale mode), so its release ends the right one */
    int32_t gov_hold;                         /* halves spent below 60 % since the last lane was won back */
    int32_t lane_budget;                      /* load governor: unison lanes (oscillator voices) the synth may run at once; the device lowers it when the CPU runs out */
    int32_t nact;                             /* voices sounding in this block */
    volatile int32_t mod_view[D_COUNT];       /* the modulation of each destination in the last voice rendered, for the screen (units of the matrix) */
    int fx_ran;                                /* the effects ran in the last block (they idle when off and silent) */
    int32_t cpu_samples;                       /* voice-samples rendered (for benchmarks) */
#if WT_NODRUMS
    int32_t vc_g, lim_g;                   /* Q15: smoothed 1/sqrt(voices), and the output rider (32768 = unity) */
    int16_t hold_gd[6];                    /* global mods from the newest sounding voice */
    int8_t lane_of[WT_NVOICES];            /* unison lanes (both oscillators together) this voice may run */
#endif
} wt_synth_t;

void wt_synth_init(wt_synth_t *s);
void wt_synth_load(wt_synth_t *s, uint32_t pct);   /* the audio side tells the synth how busy the last block left the CPU: it sheds unison lanes when over, wins them back when idle */             /* default patch; no tables: call wt_synth_set_table */
void wt_synth_set_table(wt_synth_t *s, int slot, const wt_table_t *t);
void wt_synth_set_shape(wt_synth_t *s, int k, const wt_lfoshape_t *sh);   /* build user shape k (a no-op without WT_LFOSHAPES) */
void wt_synth_set_sample(wt_synth_t *s, int slot, const wt_sample_t *smp);   /* slot 0..WT_NSAMPLES-1; the synth does not own it */
void wt_synth_defaults(wt_synth_t *s);         /* parameters to the descriptor defaults */
void wt_synth_set_param(wt_synth_t *s, int id, int value);   /* clamps */
void wt_synth_note_on(wt_synth_t *s, int note, int vel);
void wt_synth_note_on_ex(wt_synth_t *s, int note, int vel, int accent);   /* accent: lifts the velocity by P_ACCENT and feeds the ACCENT source */
void wt_synth_slide(wt_synth_t *s, int from, int to, int vel, int accent);  /* glide a sounding note to another (no new attack); else a new note */
void wt_synth_note_off(wt_synth_t *s, int note);
/* the player's keys (keyboard, USB MIDI): through the arpeggiator when it is on, else straight to wt_synth_note_on / _off */
void wt_synth_key_on(wt_synth_t *s, int note, int vel);
void wt_synth_key_off(wt_synth_t *s, int note);
void wt_arp_block(wt_synth_t *s);              /* called by the synth every WT_CTL samples */
void wt_arp_stop(wt_synth_t *s);
/* random patterns: notes of the scale (minor pentatonic when it is off), density % of steps, range in octaves 1..3, spice % (accents, slides, long notes) */
void wt_seq_generate(wt_synth_t *s, int pat, int density, int range, int spice);
void wt_seq_init(wt_seq_t *q);                 /* patterns empty, pattern 1 a bass line */
void wt_seq_play(wt_synth_t *s, int on);       /* start (from step 1) or stop; stopping releases the sequencer's notes */
void wt_seq_queue(wt_synth_t *s, int pat);     /* take pattern pat when the playing one wraps (at once when stopped) */
void wt_seq_clear(wt_pat_t *p);
/* editing a pattern (a bar = 16 steps, at most 64 steps = 4 bars) */
int wt_pat_double(wt_pat_t *p);
int wt_pat_halve(wt_pat_t *p, int keep_second);
int wt_pat_remove(wt_pat_t *p, int from_bar, int nbars);
int wt_pat_loop(wt_pat_t *p, int bar);
int wt_pat_loop_bar(const wt_pat_t *p);
/* a hook for tests: every note the sequencer starts or ends (kind 1 on, 0 off, 2 slide), with the sample it happened on */
extern void (*wt_seq_log)(int kind, int note, int vel, int accent, uint32_t t);
void wt_seq_block(wt_synth_t *s);              /* called by the synth every WT_CTL samples */
void wt_synth_all_off(wt_synth_t *s);
void wt_synth_panic(wt_synth_t *s);
void wt_clk_plan(wt_synth_t *s, int nblk);     /* once per chunk: take the ticks received, fill blk_inc */
uint32_t wt_bpm_x16(const wt_synth_t *s);      /* the tempo the sequencers use, x 16: P_BPM, or the external clock's once it is locked */
void wt_seq_resume(wt_synth_t *s);             /* Continue: run on from where it stopped */
extern void (*wt_midi_out)(uint32_t pkt);      /* the device sets it: a 4-byte USB-MIDI event packet to send */
void wt_midi_in(wt_synth_t *s, uint32_t pkt);  /* a USB-MIDI event packet received: notes, bend, mod wheel, all notes off, on the channels set; the drums' channel plays the drum tracks (General MIDI drum notes) */
int wt_syn_audible(const wt_synth_t *s);       /* mute / solo: is the synth heard now */
#if !WT_NODRUMS
int wt_drums_audible(const wt_synth_t *s);
int wt_track_audible(const wt_synth_t *s, int t);
#endif
/* the second core: the device sets these (null = this core does the work). Defined with the synth so WT-2, which does not link the drums, still has them. */
extern int (*wt_par_run)(void (*fn)(uint32_t), uint32_t arg);   /* 1 = started on the other core */
extern int (*wt_par_wait)(void);                                /* 0 = done; nonzero = it failed: run the job here */
void wt_synth_bend(wt_synth_t *s, int v);
void wt_synth_modwheel(wt_synth_t *s, int v);
int wt_synth_active_voices(const wt_synth_t *s);
int wt_voice_lanes(const wt_synth_t *s, int i);   /* WT-2: unison lanes given to voice i this block (0 if it is silent). WT-1: 0 */
/* interleaved stereo int16; frames must be a multiple of WT_CTL */
extern uint32_t (*wt_clock)(void);            /* a free-running tick counter for the performance page (the device sets it; null = off) */
void wt_synth_render(wt_synth_t *s, int16_t *out, int frames);
