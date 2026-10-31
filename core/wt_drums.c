/* SPDX-License-Identifier: GPL-3.0-only
 * WT-1 drums: see wt_drums.h. This unit also holds the engines (X0X's 909 and 808) and their data, as one translation unit.
 * WT-2 (WT_NODRUMS) does not compile this file: the engines, the sample ROMs and the pattern player are not in that product. */
#if !WT_NODRUMS
#include "drums/dr_tables.c"
#include "drums/dr_samples.c"
#include "drums/drum909.c"
#include "drums/drum808.c"
#include "wt_drums.h"
/* wt_par_run / wt_par_wait are defined in wt_synth.c (WT-2 links that and not this file) */

static const uint8_t DRATE[SR_N] = {4, 6, 8, 3};          /* steps per beat: 16th, 16th triplet, 32nd, 8th triplet */
static const char *const NAME9[WT_DTRACKS] = {"BD", "SD", "LT", "MT", "HT", "RS", "CP", "CH", "OH", "CR", "RD"};
static const char *const NAME8[WT_DTRACKS] = {"BD", "SD", "LT", "MT", "HT", "RS", "CP", "CB", "CY", "OH", "CH"};

const char *wt_drums_track_name(int kit, int track) { track = track < 0 ? 0 : track >= WT_DTRACKS ? WT_DTRACKS - 1 : track; return kit ? NAME8[track] : NAME9[track]; }
int wt_drums_nparams(int kit, int track) { return kit ? drum808_nparams(track) : drum909_nparams(track); }
const x0x_param_t *wt_drums_param(int kit, int track, int i) { return kit ? drum808_param(track, i) : drum909_param(track, i); }
int wt_drums_get(const wt_drums_t *D, int kit, int track, int i) { return kit ? drum808_get(&D->d8, track, i) : drum909_get(&D->d9, track, i); }
void wt_drums_set(wt_drums_t *D, int kit, int track, int i, int v) { if (kit) drum808_set(&D->d8, track, i, v); else drum909_set(&D->d9, track, i, v); }

void wt_drums_clear(wt_dpat_t *p)
{
    int t, i;
    for (t = 0; t < WT_DTRACKS; t++) for (i = 0; i < WT_DSTEPS; i++) { p->st[t][i] = 0; dx_set(p, t, i, 0); }
    p->len = 16; p->rate = SR_16; p->swing = 50; p->lstart = p->llen = 0; p->rsv[0] = p->rsv[1] = p->rsv[2] = 0;
}

void wt_drums_init(wt_drums_t *D)
{
    int i, t;
    uint8_t *b = (uint8_t *)D;
    for (i = 0; i < (int)sizeof *D; i++) b[i] = 0;
    drum909_init(&D->d9);
    drum808_init(&D->d8);
    for (i = 0; i < WT_NDPAT; i++) wt_drums_clear(&D->pat[i]);
    D->next = 255; D->pos = -1; D->rng = 2463534242u;
    /* pattern 1: a four-on-the-floor beat to hear the kits with */
    for (i = 0; i < 16; i += 4) D->pat[0].st[0][i] = 110 | DS_ACCENT * (i == 0);
    D->pat[0].st[1][4] = 100; D->pat[0].st[1][12] = 100 | DS_ACCENT;
    for (i = 2; i < 16; i += 4) D->pat[0].st[7][i] = 80;
    D->pat[0].st[8][14] = 70;
    for (t = 0; t < 16; t += 8) D->pat[0].st[6][t + 4] = 90;
}

void wt_synth_set_drums(wt_synth_t *s, wt_drums_t *D) { s->dr = D; }

void wt_drums_hit(wt_synth_t *s, int track, int vel)
{
    wt_drums_t *D = s->dr;
    if (!D || track < 0 || track >= WT_DTRACKS) return;
    if (D->nhit < 16) { D->hit_t[D->nhit] = (uint8_t)track; D->hit_v[D->nhit] = (uint8_t)(vel < 1 ? 1 : vel > 127 ? 127 : vel); D->nhit++; }
}

static uint32_t dstep_len_q8(const wt_synth_t *s, const wt_dpat_t *p, uint32_t n)
{
    uint32_t base = (uint32_t)(((uint64_t)2646000u << 12) / wt_bpm_x16(s)) / DRATE[p->rate % SR_N];
    uint32_t sw = p->swing < 50 ? 50 : p->swing > 75 ? 75 : p->swing;
    return (n & 1u) ? (uint32_t)(((uint64_t)base * 2u * (100u - sw)) / 100u) : (uint32_t)(((uint64_t)base * 2u * sw) / 100u);
}

void wt_drums_play(wt_synth_t *s, int on)
{
    wt_drums_t *D = s->dr;
    if (!D) return;
    if (on) {
        if (D->run || !s->p[P_DR_SEQ]) return;       /* the DR SEQ switch: whether the drums follow PLAY */
        D->pos = -1; D->n = 0;
        if (s->p[P_LFO1_SYNC] && !s->seq.run) s->lfo_ph[0] = 0;
        if (s->p[P_LFO2_SYNC] && !s->seq.run) s->lfo_ph[1] = 0;
        D->len_q8 = dstep_len_q8(s, &D->pat[D->cur % WT_NDPAT], 0);
        D->acc_q8 = D->len_q8;                              /* the first step fires in the first block */
        D->run = 1;
    } else { D->run = 0; D->pos = -1; }
}

void wt_drums_resume(wt_synth_t *s) { wt_drums_t *D = s->dr; if (D && !D->run && s->p[P_DR_SEQ]) D->run = 1; }

void wt_drums_panic(wt_synth_t *s) { wt_drums_t *D = s->dr; if (D) { D->run = 0; D->pos = -1; D->nhit = 0; } }

void wt_drums_queue(wt_synth_t *s, int pat)
{
    wt_drums_t *D = s->dr;
    if (!D) return;
    pat = pat < 0 ? 0 : pat >= WT_NDPAT ? WT_NDPAT - 1 : pat;
    if (!D->run) { D->cur = (uint8_t)pat; D->next = 255; } else D->next = (uint8_t)pat;
}

static void dev_add(wt_synth_t *s, wt_drums_t *D, int blk, int track, int v, int soft)       /* v: 1..127, + DS_ACCENT; soft: the soft accent */
{
    float vel;
    if (track == 0 && blk < 32) D->kickmask |= 1u << blk;                    /* the sidechain ducks for a muted kick too (a ghost kick) */
    if (D->nev >= 96 || !wt_track_audible(s, track)) return;          /* muted, or not the solo */
    vel = (float)(v & 127) * (0.75f / 127.0f);
    if ((v & DS_ACCENT) || soft) {               /* an accent: the level it sets (fixed) or adds (relative), on a scale that reaches full at 127 */
        int a = s->p[(v & DS_ACCENT) ? P_DR_ACCENT : P_DR_SOFT], n = a >= 128 ? a - 128 : (v & 127) + a;
        float x;
        n = n < 1 ? 1 : n > 127 ? 127 : n;
        x = (float)n * (1.0f / 127.0f);
        vel = x * (0.75f + 0.25f * x);
    }
    D->ev[D->nev].blk = (uint8_t)blk; D->ev[D->nev].track = (uint8_t)track; D->ev[D->nev].vel = vel;
    D->nev++;
}

/* the drum sequencer and the pads for one chunk of nblk control blocks: which hits fall in which block */
static void drums_events(wt_synth_t *s, wt_drums_t *D, int nblk)
{
    int b, i, t;
    D->nev = 0; D->kickmask = 0;
    for (i = 0; i < D->nhit; i++) dev_add(s, D, 0, D->hit_t[i], D->hit_v[i], 0);
    D->nhit = 0;
    if (!D->run) return;
    for (b = 0; b < nblk; b++) {
        int guard = 2;
        D->acc_q8 += s->blk_inc[b & 15];
        while (D->acc_q8 >= D->len_q8 && D->len_q8 && guard--) {
            const wt_dpat_t *p = &D->pat[D->cur % WT_NDPAT];
            int len = p->len < 1 ? 1 : p->len > WT_DSTEPS ? WT_DSTEPS : p->len, lo = 0, hi = len;
            D->acc_q8 -= D->len_q8;
            if (p->llen && p->lstart < len) { lo = p->lstart; hi = lo + p->llen > len ? len : lo + p->llen; }       /* looping one bar */
            if (D->pos < 0) D->pos = (int16_t)lo;
            else {
                int pos = D->pos + 1;
                if (pos >= hi || pos < lo) {
                    pos = lo;
                    if (D->next != 255) {
                        D->cur = D->next; D->next = 255; p = &D->pat[D->cur % WT_NDPAT];
                        len = p->len < 1 ? 1 : p->len > WT_DSTEPS ? WT_DSTEPS : p->len;
                        pos = (p->llen && p->lstart < len) ? p->lstart : 0;
                    }
                }
                D->pos = (int16_t)pos;
            }
            for (t = 0; t < WT_DTRACKS; t++)
                if (D->pos < WT_DSTEPS && p->st[t][D->pos]) {
                    int e = dx_get(p, t, D->pos), skip = e >> 1;
                    if (skip) {                                          /* a probability: the dice */
                        D->rng ^= D->rng << 13; D->rng ^= D->rng >> 17; D->rng ^= D->rng << 5;
                        if ((int)(D->rng % 100u) < skip * 2) continue;
                    }
                    dev_add(s, D, b, t, p->st[t][D->pos], e & DX_SOFT);
                }
            D->len_q8 = dstep_len_q8(s, p, D->n);
            D->n++;
        }
        D->phase8 = (uint8_t)(D->len_q8 ? ((uint64_t)D->acc_q8 * 255u) / D->len_q8 : 0);
    }
}

static wt_drums_t *job_dr;
static int job_n;

static void drums_stretch(wt_drums_t *D, int from, int len)
{
    float *l = D->L + from, *r = D->R + from, *rv = D->rev + from, *dy = D->dly + from;
    if (len <= 0) return;
    if (D->kit) {
        if (!drum808_active(&D->d8)) { drum808_pan_settle(&D->d8); return; }
        drum808_render_st(&D->d8, l, r, rv, dy, len);
    } else {
        if (!drum909_active(&D->d9)) { drum909_pan_settle(&D->d9); return; }
        drum909_render_st(&D->d9, l, r, rv, dy, len);
    }
}

/* the whole chunk: stretches between the hits, each hit triggered where it falls */
static void drums_job(uint32_t arg)
{
    wt_drums_t *D = job_dr;
    int n = job_n, pos = 0, e = 0, i;
    (void)arg;
    for (i = 0; i < n; i++) D->L[i] = D->R[i] = D->rev[i] = D->dly[i] = 0.0f;
    while (e < D->nev) {
        int at = D->ev[e].blk * WT_CTL;
        drums_stretch(D, pos, at - pos);
        pos = at;
        while (e < D->nev && D->ev[e].blk * WT_CTL == at) {
            if (D->kit) drum808_trigger(&D->d8, D->ev[e].track, D->ev[e].vel); else drum909_trigger(&D->d9, D->ev[e].track, D->ev[e].vel);
            e++;
        }
    }
    drums_stretch(D, pos, n - pos);
}

/* ---- called by the synth's render ---- */
int wt_drums_chunk_start(wt_synth_t *s, int n)            /* 1 = the job was handed to the other core */
{
    wt_drums_t *D = s->dr;
    int i;
    D->kit = (uint8_t)(s->p[P_DR_KIT] ? 1 : 0);
    drums_events(s, D, n / WT_CTL);
    for (i = 0; i < D->nev; i++) D->last_hit |= (uint16_t)(1u << D->ev[i].track);
    D->nsamp = n;
    D->active = D->nev > 0 || (D->kit ? drum808_active(&D->d8) : drum909_active(&D->d9));
    if (!D->active) {                                      /* nothing sounding, nothing starting: zeros, no work */
        for (i = 0; i < n; i++) D->L[i] = D->R[i] = D->rev[i] = D->dly[i] = 0.0f;
        if (D->kit) drum808_pan_settle(&D->d8); else drum909_pan_settle(&D->d9);
        return 0;
    }
    job_dr = D; job_n = n;
    return wt_par_run && wt_par_run(drums_job, 0);
}

void wt_drums_chunk_finish(wt_synth_t *s, int started)    /* the job done, here or there */
{
    wt_drums_t *D = s->dr;
    if (!D->active) return;
    if (!started || (wt_par_wait && wt_par_wait())) { job_dr = D; job_n = D->nsamp; drums_job(0); }
}

/* a random drum pattern: the kit's backbone for the style, then extras at `density` % */
void wt_drums_generate(wt_synth_t *s, int pat, int style, int density)
{
    wt_drums_t *D = s->dr;
    wt_dpat_t *p;
    uint32_t x;
    int i, t, kit, ch, oh, len;
    if (!D || pat < 0 || pat >= WT_NDPAT) return;
    p = &D->pat[pat];
    x = D->n * 2654435761u + 0x9E3779B9u + (uint32_t)pat * 40503u + (uint32_t)(s->seq.t + 1u);          /* a new roll each time: the steps played and the sequencer's clock */
    #define DRND() (x ^= x << 13, x ^= x >> 17, x ^= x << 5, x)
    density = density < 0 ? 0 : density > 100 ? 100 : density;
    kit = s->p[P_DR_KIT] ? 1 : 0;
    ch = kit ? 10 : 7; oh = kit ? 9 : 8;                           /* closed / open hat: the tracks differ between the kits */
    len = p->len < 1 ? 16 : p->len > WT_DSTEPS ? WT_DSTEPS : p->len;
    for (t = 0; t < WT_DTRACKS; t++) for (i = 0; i < WT_DSTEPS; i++) { p->st[t][i] = 0; dx_set(p, t, i, 0); }
    for (i = 0; i < len; i++) {
        int beat = i % 16;
        uint8_t vel = (uint8_t)(86 + DRND() % 30u);
        if (style == 0) {                                          /* four on the floor */
            if (beat % 4 == 0) p->st[0][i] = (uint8_t)(110 | (beat == 0 ? DS_ACCENT : 0));
            if (beat == 4 || beat == 12) p->st[1][i] = (uint8_t)(100 | DS_ACCENT * (beat == 12));
            if (beat % 2 == 0) p->st[ch][i] = (uint8_t)(70 + DRND() % 30u);
            if (beat == 6 || beat == 14) p->st[oh][i] = 80;
        } else if (style == 1) {                                   /* breakbeat */
            if (beat == 0 || beat == 10) p->st[0][i] = (uint8_t)(112 | (beat == 0 ? DS_ACCENT : 0));
            if (beat == 4 || beat == 12) p->st[1][i] = (uint8_t)(104 | DS_ACCENT);
            if (beat % 2 == 0 || (int)(DRND() % 100u) < density / 3) p->st[ch][i] = (uint8_t)(66 + DRND() % 34u);
        } else {                                                   /* random */
            if ((int)(DRND() % 100u) < density / 2 + 10) p->st[0][i] = vel;
            if ((int)(DRND() % 100u) < density / 3) p->st[1][i] = vel;
            if ((int)(DRND() % 100u) < density / 2) p->st[ch][i] = (uint8_t)(60 + DRND() % 40u);
        }
        if ((int)(DRND() % 100u) < density / 4 && i % 4 != 0) p->st[0][i] = (uint8_t)(90 + DRND() % 20u);                       /* a ghost kick */
        if ((int)(DRND() % 100u) < density / 5) p->st[1][i] = p->st[1][i] ? p->st[1][i] : (uint8_t)(50 + DRND() % 30u);      /* a ghost snare */
        if ((int)(DRND() % 100u) < density / 6) { int tt = 2 + (int)(DRND() % 3u); p->st[tt][i] = (uint8_t)(80 + DRND() % 40u); }       /* a tom */
        if ((int)(DRND() % 100u) < density / 7) { int tt = 5 + (int)(DRND() % 2u); p->st[tt][i] = (uint8_t)(80 + DRND() % 30u); }       /* rim or clap */
    }
    #undef DRND
}

/* ---- editing a drum pattern: the same as the synth's (wt_seq.c) ---- */
static void dp_clear_step(wt_dpat_t *p, int i) { int t; for (t = 0; t < WT_DTRACKS; t++) p->st[t][i] = 0, dx_set(p, t, i, 0); }
static void dp_move(wt_dpat_t *p, int to, int from) { int t; for (t = 0; t < WT_DTRACKS; t++) { p->st[t][to] = p->st[t][from]; dx_set(p, t, to, dx_get(p, t, from)); } }

int wt_dpat_double(wt_dpat_t *p)
{
    int len = p->len < 1 ? 1 : p->len > WT_DSTEPS ? WT_DSTEPS : p->len, nl, i;
    if (len >= WT_DSTEPS) return 0;
    nl = len * 2 > WT_DSTEPS ? WT_DSTEPS : len * 2;
    for (i = len; i < nl; i++) dp_move(p, i, i - len);
    p->len = (uint8_t)nl; p->lstart = p->llen = 0;
    return nl;
}
int wt_dpat_halve(wt_dpat_t *p, int keep_second)
{
    int len = p->len < 1 ? 1 : p->len > WT_DSTEPS ? WT_DSTEPS : p->len, h = len / 2, i;
    if (h < 1) h = 1;
    if (keep_second) for (i = 0; i < h; i++) dp_move(p, i, len - h + i);
    for (i = h; i < WT_DSTEPS; i++) dp_clear_step(p, i);
    p->len = (uint8_t)h; p->lstart = p->llen = 0;
    return h;
}
int wt_dpat_remove(wt_dpat_t *p, int from_bar, int nbars)
{
    int len = p->len < 1 ? 1 : p->len > WT_DSTEPS ? WT_DSTEPS : p->len, s = (from_bar - 1) * 16, c = nbars * 16, i;
    if (from_bar < 1 || nbars < 1 || s >= len) return 0;
    if (c > len - s) c = len - s;
    if (len - c < 1) return 0;
    for (i = s; i + c < len; i++) dp_move(p, i, i + c);
    for (i = len - c; i < len; i++) dp_clear_step(p, i);
    p->len = (uint8_t)(len - c); p->lstart = p->llen = 0;
    return len - c;
}
int wt_dpat_loop(wt_dpat_t *p, int bar)
{
    int len = p->len < 1 ? 1 : p->len > WT_DSTEPS ? WT_DSTEPS : p->len, s;
    if (bar <= 0) { p->lstart = p->llen = 0; return 1; }
    s = (bar - 1) * 16;
    if (s >= len) return 0;
    p->lstart = (uint8_t)s; p->llen = (uint8_t)(len - s < 16 ? len - s : 16);
    return 1;
}
int wt_dpat_loop_bar(const wt_dpat_t *p) { return p->llen ? p->lstart / 16 + 1 : 0; }
#endif /* !WT_NODRUMS */
