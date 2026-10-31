/* SPDX-License-Identifier: GPL-3.0-only
 * WT-1 drums: X0X's TR-909 and TR-808 engines (9W9 / 8W8, GPL-3.0; core/drums/) with a drum step sequencer that shares the synth's tempo.
 * The drums render a whole chunk (up to 256 frames) at once into their own buffers, on the second core when there is one, while the
 * first core renders the synth; the two are added afterwards, so the result is the same on one core or two (bit for bit). */
#pragma once
#include "wt_core.h"
#include "drums/drum909.h"
#include "drums/drum808.h"

#define WT_DTRACKS 11
#define WT_DSTEPS 64
#define WT_NDPAT 8
#define DS_ACCENT 0x80                  /* a drum step: 0 = off, 1..127 = velocity, + DS_ACCENT = loud accent */
#define DX_SOFT 1                       /* a step's extras (dx_get / dx_set): bit 0 = soft accent (when DS_ACCENT is not set); bits 1..7 = (100 - probability %) / 2, 0..50 (0 = always plays) */
typedef struct {
    uint8_t st[WT_DTRACKS][WT_DSTEPS];
    uint8_t len, rate, swing;           /* 1..64; SR_*; 50..75 */
    uint8_t lstart, llen, rsv[3];       /* the bar being looped: its first step and length (llen 0 = the whole pattern) */
    uint8_t xp[WT_DTRACKS][WT_DSTEPS / 2];  /* the extras, packed (memory is tight): a 4-bit probability per step ... */
    uint8_t xs[WT_DTRACKS][WT_DSTEPS / 8];  /* ... and a soft-accent bit; after everything else so the fields above keep their offsets */
} wt_dpat_t;
static inline int dx_get(const wt_dpat_t *p, int t, int i)
{
    int n = (p->xp[t][i >> 1] >> ((i & 1) * 4)) & 15;
    return ((p->xs[t][i >> 3] >> (i & 7)) & 1) | ((n * 50 / 15) << 1);
}
static inline void dx_set(wt_dpat_t *p, int t, int i, int e)
{
    int n = ((e >> 1) * 15 + 25) / 50, sh = (i & 1) * 4;
    p->xp[t][i >> 1] = (uint8_t)((p->xp[t][i >> 1] & ~(15 << sh)) | (n << sh));
    p->xs[t][i >> 3] = (uint8_t)((p->xs[t][i >> 3] & ~(1 << (i & 7))) | ((e & 1) << (i & 7)));
}

struct wt_drums {
    drum909_t d9;
    drum808_t d8;
    wt_dpat_t pat[WT_NDPAT];
    uint8_t cur, next, run, kit;        /* the pattern playing; queued (255 none); playing; the kit rendered (0 909, 1 808) */
    volatile int16_t pos;               /* the step that sounded last (-1 none): the playhead */
    volatile uint8_t phase8;
    uint32_t acc_q8, len_q8, n;
    uint8_t mute[WT_DTRACKS];           /* 0 normal, 1 muted, 2 solo */
    uint8_t hit_t[16], hit_v[16], nhit; /* pad hits queued for the next chunk (track, velocity 1..127) */
    volatile uint16_t last_hit;         /* bit per track: set when it sounded (the screen flashes it) */
    float L[256], R[256], rev[256], dly[256];   /* the chunk's render */
    uint32_t kickmask;                  /* control blocks of this chunk where the bass drum is hit (muted or not): the sidechain's trigger */
    uint32_t rng;                       /* the probability's dice (advanced only when a step has a probability) */
    int nsamp, active;
    struct { uint8_t blk, track; float vel; } ev[96];
    int nev;
};
typedef struct wt_drums wt_drums_t;

void wt_drums_init(wt_drums_t *D);
void wt_synth_set_drums(wt_synth_t *s, wt_drums_t *D);
void wt_drums_hit(wt_synth_t *s, int track, int vel);     /* a pad: sounds at the start of the next chunk */
void wt_drums_play(wt_synth_t *s, int on);
void wt_drums_resume(wt_synth_t *s);
void wt_drums_generate(wt_synth_t *s, int pat, int style, int density);   /* style 0 four on the floor, 1 breakbeat, 2 random; density % of the extra hits */
void wt_drums_panic(wt_synth_t *s);                       /* stop the sequencer and drop the queued hits */
void wt_drums_queue(wt_synth_t *s, int pat);
void wt_drums_clear(wt_dpat_t *p);
int wt_dpat_double(wt_dpat_t *p);               /* the same edits as the synth patterns (wt_pat_*): a bar is 16 steps, 4 bars at most */
int wt_dpat_halve(wt_dpat_t *p, int keep_second);
int wt_dpat_remove(wt_dpat_t *p, int from_bar, int nbars);
int wt_dpat_loop(wt_dpat_t *p, int bar);
int wt_dpat_loop_bar(const wt_dpat_t *p);
const char *wt_drums_track_name(int kit, int track);
/* the engine's pots of a track in the kit, as the screen shows them */
int wt_drums_nparams(int kit, int track);
const x0x_param_t *wt_drums_param(int kit, int track, int i);
int wt_drums_get(const wt_drums_t *D, int kit, int track, int i);
void wt_drums_set(wt_drums_t *D, int kit, int track, int i, int v);
/* the second core: set by the device (null = everything on this core) */
/* the synth's render: start the chunk's drums (1 = on the other core), and finish them (waits, or renders them here) */
int wt_drums_chunk_start(wt_synth_t *s, int n);
void wt_drums_chunk_finish(wt_synth_t *s, int started);
extern int (*wt_par_run)(void (*fn)(uint32_t), uint32_t arg);   /* 1 = started on the other core */
extern int (*wt_par_wait)(void);                                  /* 0 = done; nonzero = it failed: run the job here */
