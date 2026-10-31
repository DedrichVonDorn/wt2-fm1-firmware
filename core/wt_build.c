/* SPDX-License-Identifier: GPL-3.0-only
 * WT-1 core: the band-limited mip builder. Integer only, no libc, no allocation: it runs on the device
 * (and on the Mac and in the browser) and gives the same samples everywhere. The caller owns the memory. */
#include "wt_int.h"

void (*wt_idle)(void);                 /* the device feeds its watchdog here between frames while tables build */

wt_cfg_t wt_cfg = {3, 1, 32, 50, 4};   /* cubic; spectral morph, up to 32 internal frames; top half of each level's harmonics tapered; 4 samples per top harmonic */

/* Harmonics a_k (cosine) and b_k (sine), k = 1..kmax, of one frame of WT_N samples; in the frame's own units. */
void wt_analyse(const int16_t *x, int32_t *a, int32_t *b, int kmax)
{
    int k, n;
    for (k = 1; k <= kmax; k++) {
        int64_t sc = 0, ss = 0;
        for (n = 0; n < WT_N; n++) {
            int idx = (int)(((long)k * n * (1024 / WT_N)) & 1023);
            sc += (int64_t)x[n] * wt_sin1024[(idx + 256) & 1023];
            ss += (int64_t)x[n] * wt_sin1024[idx];
        }
        a[k] = (int32_t)((sc >> 15) >> (WT_LOGN - 1));
        b[k] = (int32_t)((ss >> 15) >> (WT_LOGN - 1));
    }
}

/* One cycle of M samples from harmonics 1..H; the top taper_pct percent of them roll off on a cos^2 slope. */
void wt_synth_cycle(int16_t *out, const int32_t *a, const int32_t *b, int H, int M, int taper_pct)
{
    int k, n, k0 = H - H * taper_pct / 100;
    int32_t w[WT_N / 2 + 1];
    for (k = 1; k <= H; k++) {
        if (k <= k0 || k0 >= H) w[k] = 32767;
        else {      /* x = (k - k0) / (H - k0 + 1) of the way round a quarter wave: w = cos(x pi / 2)^2 */
            int32_t c = wt_sin1024[(256 + ((k - k0) * 256 + (H - k0 + 1) / 2) / (H - k0 + 1)) & 1023];
            w[k] = (c * c) >> 15;
        }
    }
    for (n = 0; n < M; n++) {
        int64_t acc = 0;
        for (k = 1; k <= H; k++) {
            int idx = (int)(((long)k * n * (1024 / M)) & 1023);
            acc += (((int64_t)a[k] * wt_sin1024[(idx + 256) & 1023] + (int64_t)b[k] * wt_sin1024[idx]) >> 15) * w[k];
        }
        acc >>= 15;
        out[n] = (int16_t)(acc > 32767 ? 32767 : acc < -32768 ? -32768 : acc);
    }
}

/* Fills the level layout (harmonics, size) of t for nint frames; returns the samples all levels need. */
int wt_levels_layout(wt_table_t *t, int nint)
{
    int L, total = 0, bw = wt_cfg.bwdiv;
    t->nlev = 0;
    for (L = 0; L < WT_MAXLEV; L++) {
        int H = (WT_N >> L) / bw - 1, lg = WT_LOGN - L;
        if (H < 1) break;
        if (lg < WT_MINLG) lg = WT_MINLG;
        t->H[L] = H; t->lg[L] = lg;
        t->nlev = L + 1;
        total += nint << lg;
    }
    return total;
}

/* frames: nint frames of WT_N samples (the internal frames: stored ones and, with morph, the in-between ones).
 * mem: cap samples of memory for the levels. Returns 0, or -1 if cap is too small. */
int wt_table_build_levels(wt_table_t *t, const char *name, const int16_t *frames, int nint, int nsrc, int expand,
                          int morph, int16_t *mem, int cap)
{
    int L, f, i, total;
    int32_t a[WT_N / 2 + 1], b[WT_N / 2 + 1];
    int16_t *p = mem;
    for (i = 0; i < (int)sizeof *t; i++) ((char *)t)[i] = 0;
    for (i = 0; i < 11 && name[i]; i++) t->name[i] = name[i];
    t->nframes = nint; t->nsrc = nsrc; t->expand = expand; t->morph = morph;
    total = wt_levels_layout(t, nint);
    if (total > cap) return -1;
    t->mem = mem;
    for (L = 0; L < t->nlev; L++) { t->lev[L] = p; p += nint << t->lg[L]; }
    for (f = 0; f < nint; f++) {
        if (wt_idle) wt_idle();
        wt_analyse(frames + (size_t)f * WT_N, a, b, t->H[0]);
        for (L = 0; L < t->nlev; L++)
            wt_synth_cycle(t->lev[L] + ((size_t)f << t->lg[L]), a, b, t->H[L], 1 << t->lg[L], wt_cfg.taper);
    }
    return 0;
}

int wt_table_bytes(const wt_table_t *t)
{
    int L, n = 0;
    for (L = 0; L < t->nlev; L++) n += (t->nframes << t->lg[L]) * (int)sizeof(int16_t);
    return n;
}

uint32_t wt_isqrt(uint64_t x)
{
    uint64_t r = 0, b = 1ull << 62;
    while (b > x) b >>= 2;
    while (b) {
        if (x >= r + b) { x -= r + b; r = (r >> 1) + b; } else r >>= 1;
        b >>= 2;
    }
    return (uint32_t)r;
}

uint32_t wt_pitch_inc(int32_t s)
{
    uint32_t oct, rem, i, f, a, b, v;
    if (s < 0) s = 0;
    if (s > 135 * 256) s = 135 * 256;
    oct = (uint32_t)s / 3072u; rem = (uint32_t)s % 3072u;
    i = rem >> 4; f = rem & 15u;
    a = wt_inc_tbl[i]; b = wt_inc_tbl[i + 1];
    v = a + (uint32_t)(((uint64_t)(b - a) * f) >> 4);
    v <<= oct;
    return v > 0x7FFFFFFFu ? 0x7FFFFFFFu : v;
}
