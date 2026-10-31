/* SPDX-License-Identifier: GPL-3.0-only
 * WT-1 core, host side: the lookup tables built with libm, spectral morph, the generated wavetables and samples.
 * Not part of the device build: there the same tables are const data made by tools/gen_wt_data (which runs this code),
 * and the integer builder in wt_build.c does the rest. */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "wt_int.h"

int16_t wt_sin1024[1024];
uint32_t wt_inc_tbl[193];
uint32_t wt_g_tbl[1351];
uint32_t wt_atk_step[1001], wt_coef[1001], wt_samples[1001], wt_lfo_inc[1001];
int16_t wt_pan_l[129], wt_pan_r[129];
static int g_init;

void wt_global_init(void)
{
    int i;
    double fs = WT_FS;
    if (g_init) return;
    g_init = 1;
    for (i = 0; i < 1024; i++) wt_sin1024[i] = (int16_t)lround(32767.0 * sin(2.0 * M_PI * i / 1024.0));
    for (i = 0; i <= 192; i++)
        wt_inc_tbl[i] = (uint32_t)llround(8.175798915643707 / fs * 4294967296.0 * pow(2.0, i / 192.0));
    for (i = 0; i <= 1350; i++) {
        double hz = 8.175798915643707 * pow(2.0, (i / 10.0) / 12.0);
        if (hz > 0.45 * fs) hz = 0.45 * fs;
        wt_g_tbl[i] = (uint32_t)llround(tan(M_PI * hz / fs) * 16777216.0);
    }
    for (i = 0; i <= 1000; i++) {
        /* times: 0.5 ms .. 10 s (exponential); delay and hold: 0 at 0 */
        double ms = 0.5 * pow(10.0, i * 4.3 / 1000.0);
        double n = ms * fs / 1000.0;
        wt_atk_step[i] = (uint32_t)(1073741824.0 / n);
        wt_coef[i] = (uint32_t)llround((1.0 - exp(-4.6 / n)) * 16777216.0);
        if (wt_coef[i] < 1) wt_coef[i] = 1;
        wt_samples[i] = i == 0 ? 0u : (uint32_t)llround(n);
        wt_lfo_inc[i] = (uint32_t)llround(0.05 * pow(10.0, i * 2.9 / 1000.0) / fs * 4294967296.0);
    }
    for (i = 0; i <= 128; i++) {
        double a = (i / 128.0) * (M_PI / 2.0);
        wt_pan_l[i] = (int16_t)lround(32767.0 * cos(a));
        wt_pan_r[i] = (int16_t)lround(32767.0 * sin(a));
    }
}

/* ---- spectral morph ----
 * A frame is sum_k a_k cos(k x) + b_k sin(k x) = sum_k M_k cos(k x - phi_k). Two frames are morphed by blending the
 * magnitudes M_k and the phases. To keep the blend from depending on where in the cycle each frame happens to start,
 * phases are first expressed relative to the fundamental's: psi_k = phi_k - k phi_1 (when the fundamental is strong
 * enough to define it). Serum's "spectral morphing (frequency + phase blend)" is the same idea.
 * The in-between frames are made here, in the time domain, so the builder (wt_build.c) sees plain internal frames. */
typedef struct { int32_t a[WT_N / 2 + 1], b[WT_N / 2 + 1]; } coef_t;

static double wrap_pi(double x) { while (x > M_PI) x -= 2 * M_PI; while (x < -M_PI) x += 2 * M_PI; return x; }

static void spectral_mix(const coef_t *x, const coef_t *y, double t, int H, coef_t *out)
{
    double mx[WT_N / 2 + 1], my[WT_N / 2 + 1], px[WT_N / 2 + 1], py[WT_N / 2 + 1], big_x = 0, big_y = 0, f1x = 0, f1y = 0, f1;
    int k;
    for (k = 1; k <= H; k++) {
        mx[k] = hypot((double)x->a[k], (double)x->b[k]); px[k] = atan2((double)x->b[k], (double)x->a[k]);
        my[k] = hypot((double)y->a[k], (double)y->b[k]); py[k] = atan2((double)y->b[k], (double)y->a[k]);
        if (mx[k] > big_x) big_x = mx[k];
        if (my[k] > big_y) big_y = my[k];
    }
    if (mx[1] > 0.05 * big_x) f1x = px[1];                 /* fundamental strong enough to anchor the phases */
    if (my[1] > 0.05 * big_y) f1y = py[1];
    f1 = f1x + t * wrap_pi(f1y - f1x);
    for (k = 1; k <= H; k++) {
        double m = mx[k] + t * (my[k] - mx[k]), sx = px[k] - k * f1x, sy = py[k] - k * f1y, psi, phi;
        if (mx[k] < 0.002 * big_x) psi = sy;                /* a vanishing harmonic has no meaningful phase */
        else if (my[k] < 0.002 * big_y) psi = sx;
        else psi = sx + t * wrap_pi(sy - sx);
        phi = psi + k * f1;
        out->a[k] = (int32_t)lround(m * cos(phi));
        out->b[k] = (int32_t)lround(m * sin(phi));
    }
}

static int peak16(const int16_t *x)
{
    int n, p = 1;
    for (n = 0; n < WT_N; n++) if (abs(x[n]) > p) p = abs(x[n]);
    return p;
}

/* stored frames -> internal frames (nint of them: E - 1 spectral in-between frames per gap), loudness-matched */
static void expand_frames(const int16_t *frames, int nsrc, int E, int H, int16_t *out)
{
    int f, j, n;
    coef_t *ca = (coef_t *)calloc(1, sizeof *ca), *cb = (coef_t *)calloc(1, sizeof *cb), *cm = (coef_t *)calloc(1, sizeof *cm);
    for (f = 0; f < nsrc; f++) memcpy(out + (size_t)f * E * WT_N, frames + (size_t)f * WT_N, WT_N * sizeof(int16_t));
    for (f = 0; f + 1 < nsrc && E > 1; f++) {
        int pa = peak16(frames + (size_t)f * WT_N), pb = peak16(frames + (size_t)(f + 1) * WT_N);
        wt_analyse(frames + (size_t)f * WT_N, ca->a, ca->b, H);
        wt_analyse(frames + (size_t)(f + 1) * WT_N, cb->a, cb->b, H);
        for (j = 1; j < E; j++) {
            double tt = (double)j / E, want = pa + tt * (pb - pa);
            int16_t *o = out + (size_t)(f * E + j) * WT_N;
            int pk;
            spectral_mix(ca, cb, tt, H, cm);
            wt_synth_cycle(o, cm->a, cm->b, H, WT_N, 0);
            pk = peak16(o);
            for (n = 0; n < WT_N; n++) o[n] = (int16_t)lround(o[n] * want / pk);
        }
    }
    free(ca); free(cb); free(cm);
}

int wt_table_build_ex(wt_table_t *t, const char *name, const int16_t *frames, int nframes, int morph)
{
    int E = 1, nint, rc, need;
    int16_t *internal, *mem;
    wt_table_t probe;
    wt_global_init();
    memset(t, 0, sizeof *t);
    if (nframes < 1 || nframes > WT_MAXFRAMES) return -1;
    if (morph && nframes > 1) {
        E = (wt_cfg.morph_cap - 1) / (nframes - 1);
        if (E < 1) E = 1;
    } else morph = 0;
    nint = (nframes - 1) * E + 1;
    if (nint > WT_MAXFRAMES) return -1;
    need = wt_levels_layout(&probe, nint);
    internal = (int16_t *)malloc((size_t)nint * WT_N * sizeof(int16_t));
    mem = (int16_t *)malloc((size_t)need * sizeof(int16_t));
    if (!internal || !mem) { free(internal); free(mem); return -1; }
    expand_frames(frames, nframes, E, probe.H[0], internal);
    rc = wt_table_build_levels(t, name, internal, nint, nframes, E, morph, mem, need);
    free(internal);
    if (rc) { free(mem); return -1; }
    t->owned = 1;
    return 0;
}

int wt_table_build(wt_table_t *t, const char *name, const int16_t *frames, int nframes)
{
    return wt_table_build_ex(t, name, frames, nframes, wt_cfg.morph);
}

void wt_table_free(wt_table_t *t) { if (t->owned) free(t->mem); memset(t, 0, sizeof *t); }

/* ---- generators ---- */
/* Frames are defined by a function of the phase x in [0,1) or by a harmonic series. A naive
 * (aliased) time-domain shape is sampled at 8192 points and its first WT_N/2-1 harmonics are
 * kept, so the stored frame is band-limited. Each frame is normalised to a peak of 0.9. */
typedef double (*shape_fn)(int frame, double x);

static double frac(double x) { return x - floor(x); }
static double tri(double x) { x = frac(x); return x < 0.25 ? 4 * x : x < 0.75 ? 2 - 4 * x : 4 * x - 4; }
static double saw(double x) { return 2 * frac(x + 0.5) - 1; }

static void store_frame(int16_t *dst, const double *ak, const double *bk, int kmax)
{
    double y[WT_N], pk = 1e-9;
    int n, k;
    for (n = 0; n < WT_N; n++) {
        double acc = 0;
        for (k = 1; k <= kmax; k++) {
            double ph = 2.0 * M_PI * k * n / WT_N;
            acc += ak[k] * cos(ph) + bk[k] * sin(ph);
        }
        y[n] = acc;
        if (fabs(acc) > pk) pk = fabs(acc);
    }
    for (n = 0; n < WT_N; n++) dst[n] = (int16_t)lround(y[n] / pk * 0.9 * 32767.0);
}

static void frame_from_shape(int16_t *dst, shape_fn fn, int frame)
{
    enum { NP = 8192 };
    static double v[NP];
    double ak[WT_N / 2 + 1], bk[WT_N / 2 + 1];
    int n, k, kmax = WT_N / 2 - 1;
    for (n = 0; n < NP; n++) v[n] = fn(frame, (double)n / NP);
    for (k = 1; k <= kmax; k++) {
        double sc = 0, ss = 0;
        for (n = 0; n < NP; n++) {
            double ph = 2.0 * M_PI * ((long)k * n % NP) / NP;
            sc += v[n] * cos(ph); ss += v[n] * sin(ph);
        }
        ak[k] = 2.0 * sc / NP; bk[k] = 2.0 * ss / NP;
    }
    store_frame(dst, ak, bk, kmax);
}

/* BASIC SHAPES, in the order of Serum's table of that name: triangle, saw, square, sine, then the pulse width narrowing. Nine clean
 * shapes (no stepped or rectified ones); the table sweeps smoothly between them (spectral morph). */
static double basic(int f, double x)
{
    static const double PW[5] = {0.375, 0.25, 0.1667, 0.10, 0.05};
    switch (f) {
    case 0: return tri(x);
    case 1: return saw(x);
    case 2: return frac(x) < 0.5 ? 1 : -1;
    case 3: return sin(2 * M_PI * x);
    default: return frac(x) < PW[(f - 4) % 5] ? 1 : -1;
    }
}
static double fold(int f, double x) { return sin((0.5 + f * 0.55) * sin(2 * M_PI * x)); }
static double sawone(int f, double x) { (void)f; return saw(x); }

/* the stored frames of a factory table: 0 BASIC SHAPES, 1 FOLD-SINE, 2 ODD-EVEN, 3 SAW; returns the frame count */
static int gen_stored(int which, int16_t *fr)
{
    int f, nf = which == 3 ? 1 : which == 0 ? 9 : 16;
    if (which == 2) {
        int k, kmax = WT_N / 2 - 1;
        for (f = 0; f < 16; f++) {
            double ak[WT_N / 2 + 1], bk[WT_N / 2 + 1], w = f / 15.0;
            for (k = 1; k <= kmax; k++) {
                ak[k] = 0;
                bk[k] = ((k & 1) ? (1.0 - w) : w) / k;
            }
            store_frame(fr + (size_t)f * WT_N, ak, bk, kmax);
        }
        return 16;
    }
    for (f = 0; f < nf; f++) frame_from_shape(fr + (size_t)f * WT_N, which == 0 ? basic : which == 1 ? fold : sawone, f);
    return nf;
}

static const char *const FAC_NAME[4] = {"BASIC SHP", "FOLD-SINE", "ODD-EVEN", "SAW"};

/* The internal frames (stored plus spectral in-between ones, as the builder takes them) of a factory table. out must hold
 * WT_MAXFRAMES frames. The device build ships exactly these as const data (tools/gen_wt_data.c). */
int wt_gen_internal(int which, int morph, int16_t *out, int *nsrc, int *expand)
{
    static int16_t fr[16 * WT_N];
    int E = 1, n;
    wt_table_t probe;
    wt_global_init();
    n = gen_stored(which, fr);
    if (morph && n > 1) { E = (wt_cfg.morph_cap - 1) / (n - 1); if (E < 1) E = 1; }
    wt_levels_layout(&probe, 1);
    expand_frames(fr, n, E, probe.H[0], out);
    *nsrc = n; *expand = E;
    return (n - 1) * E + 1;
}

static int gen_table(wt_table_t *t, int which)
{
    static int16_t fr[16 * WT_N];
    int n = gen_stored(which, fr);
    return wt_table_build(t, FAC_NAME[which], fr, n);
}

int wt_gen_basic_shapes(wt_table_t *t) { return gen_table(t, 0); }
int wt_gen_fold_sine(wt_table_t *t) { return gen_table(t, 1); }
int wt_gen_odd_even(wt_table_t *t) { return gen_table(t, 2); }
int wt_gen_saw(wt_table_t *t) { return gen_table(t, 3); }

/* ---- generated looping samples (original, procedural) ----
 * Each is generated over N + X samples and its first X samples are crossfaded (equal power) with the X samples that
 * follow the end, so the loop point is a continuous stretch of the same process. */
#define SMP_N 16384
#define SMP_X 1024

static uint32_t sm_rng;
static double sm_noise(void)       /* white, -1..1 */
{
    sm_rng ^= sm_rng << 13; sm_rng ^= sm_rng >> 17; sm_rng ^= sm_rng << 5;
    return (double)(int32_t)sm_rng / 2147483648.0;
}

static int16_t *make_loop(const char *kind)
{
    static double x[SMP_N + SMP_X];
    int16_t *o = (int16_t *)malloc(SMP_N * sizeof(int16_t));
    int i;
    double lp1 = 0, lp2 = 0, env = 0, rms = 0, g;
    if (!o) return NULL;
    sm_rng = 0x9E3779B9u;
    for (i = 0; i < SMP_N + SMP_X; i++) {
        double w = sm_noise(), v;
        if (!strcmp(kind, "CRACKLE")) {          /* sparse pops with a quiet hiss under them */
            if ((sm_noise() * 0.5 + 0.5) < 1.0 / 900) env = 0.4 + 0.6 * (sm_noise() * 0.5 + 0.5);
            v = env * w + 0.02 * w;
            env *= 0.9;
        } else if (!strcmp(kind, "AIR")) {       /* high-passed noise with a slow swell (two swells per loop) */
            lp1 += 0.12 * (w - lp1);
            v = (w - lp1) * (0.65 + 0.35 * sin(2 * M_PI * 2 * i / SMP_N));
        } else {                                 /* RUMBLE: noise through two low-pass stages */
            lp1 += 0.012 * (w - lp1); lp2 += 0.03 * (lp1 - lp2);
            v = lp2 * 6.0;
        }
        x[i] = v;
    }
    for (i = 0; i < SMP_N; i++) {
        double v = x[i];
        if (i < SMP_X) {
            double a = (double)i / SMP_X;
            v = x[i] * sin(a * M_PI / 2) + x[SMP_N + i] * cos(a * M_PI / 2);
        }
        x[i] = v;
        rms += v * v;
    }
    rms = sqrt(rms / SMP_N);
    g = rms > 1e-9 ? 0.12 / rms : 1.0;           /* about -18 dBFS rms */
    for (i = 0; i < SMP_N; i++) {
        double v = x[i] * g * 32767.0;
        o[i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : lround(v));
    }
    return o;
}

int wt_gen_samples(wt_sample_t out[3])
{
    static const char *nm[3] = {"CRACKLE", "AIR", "RUMBLE"};
    int i;
    for (i = 0; i < 3; i++) {
        memset(&out[i], 0, sizeof out[i]);
        strncpy(out[i].name, nm[i], sizeof out[i].name - 1);
        out[i].d = make_loop(nm[i]);
        out[i].n = SMP_N;
        out[i].owned = 1;
        if (!out[i].d) return -1;
    }
    return 0;
}

void wt_sample_free(wt_sample_t *s)
{
    if (s->owned) free((void *)s->d);
    memset(s, 0, sizeof *s);
}
