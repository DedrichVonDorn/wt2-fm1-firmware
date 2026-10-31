/* SPDX-License-Identifier: GPL-3.0-only
 * WT-1 core tests (Mac). Usage: wt_test [--interp 1|3] [--bwdiv 2|4] [--matrix] [--golden FILE] [--update] */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../core/wt_core.h"
#include "../core/wt_patch.h"

static int fails, checks, verbose;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static wt_table_t tb[3];
static wt_sample_t smp[3];
static wt_synth_t *S;
static int master_override = -1;

static int setp(void *ctx, const char *name, const char *val)
{
    int id = wt_param_find(name), v = atoi(val), i;
    wt_synth_t *s = (wt_synth_t *)ctx;
    if (id < 0) { printf("unknown param %s\n", name); exit(2); }
    if (id >= P_MX0 && (id - P_MX0) % 3 == 0) for (i = 0; i < S_COUNT; i++) if (!strcmp(wt_src_names[i], val)) v = i;
    if (id >= P_MX0 && (id - P_MX0) % 3 == 1) for (i = 0; i < D_COUNT; i++) if (!strcmp(wt_dst_names[i], val)) v = i;
    wt_synth_set_param(s, id, v);
    return 0;
}
static void P(const char *n, int v) { char b[16]; snprintf(b, sizeof b, "%d", v); setp(S, n, b); }

static void fresh(void)
{
    int i;
    memset(S, 0, sizeof *S);
    wt_synth_init(S);
    for (i = 0; i < 3; i++) { wt_synth_set_table(S, i, &tb[i]); wt_synth_set_sample(S, i, &smp[i]); }
    patch_apply("init", setp, S);
    if (master_override >= 0) wt_synth_set_param(S, P_MASTER, master_override);
}

/* ---- FFT ---- */
static void fft(double *re, double *im, int n)
{
    int i, j = 0, k, m;
    for (i = 1; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { double t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
    }
    for (m = 2; m <= n; m <<= 1) {
        double a = -2 * M_PI / m;
        for (k = 0; k < n; k += m)
            for (i = 0; i < m / 2; i++) {
                double wr = cos(a * i), wi = sin(a * i);
                double xr = re[k + i + m / 2] * wr - im[k + i + m / 2] * wi, xi = re[k + i + m / 2] * wi + im[k + i + m / 2] * wr;
                re[k + i + m / 2] = re[k + i] - xr; im[k + i + m / 2] = im[k + i] - xi;
                re[k + i] += xr; im[k + i] += xi;
            }
    }
}

static double bh(int i, int n)      /* 4-term Blackman-Harris: sidelobes below -92 dB, main lobe +-4 bins */
{
    double x = 2 * M_PI * i / n;
    return 0.35875 - 0.48829 * cos(x) + 0.14128 * cos(2 * x) - 0.01168 * cos(3 * x);
}

/* Render the held note and return aliased energy relative to total, in dB. */
static double alias_db(int note, double *f0_out)
{
    enum { N = 16384, SETTLE = 4096 };
    static int16_t buf[2 * (N + SETTLE)];
    static double re[N], im[N];
    double f0 = 440.0 * pow(2.0, (note - 69) / 12.0), df = (double)WT_FS / N, tot = 0, al = 0;
    int i, k, h, nh;
    wt_synth_note_on(S, note, 100);
    wt_synth_render(S, buf, N + SETTLE);
    for (i = 0; i < N; i++) { re[i] = buf[2 * (SETTLE + i)] * bh(i, N); im[i] = 0; }
    fft(re, im, N);
    nh = (int)((WT_FS / 2) / f0);
    for (k = 6; k < N / 2; k++) {
        double e = re[k] * re[k] + im[k] * im[k];
        int near = 0;
        tot += e;
        for (h = 1; h <= nh + 1; h++) if (fabs(k - h * f0 / df) <= 5.0) { near = 1; break; }
        if (!near) al += e;
    }
    wt_synth_all_off(S);
    if (f0_out) *f0_out = f0;
    return 10 * log10((al + 1e-9) / (tot + 1e-9));
}

static double worst_alias(int frame_a, int lo, int hi, int unison)
{
    int n;
    double worst = -200;
    for (n = lo; n <= hi; n += 3) {
        double a;
        fresh();
        P("A_WTPOS", frame_a * 1000 / 8); P("A_UNI", unison);
        a = alias_db(n, NULL);
        if (verbose) printf("%4d:%.0f ", n, a);
        if (a > worst) worst = a;
    }
    return worst;
}

/* ---- tests ---- */
static double harm(const int16_t *x, int M, int k)       /* amplitude of harmonic k of an M-point cycle, in LSB */
{
    double sc = 0, ss = 0;
    int n;
    for (n = 0; n < M; n++) { sc += x[n] * cos(2 * M_PI * k * n / M); ss += x[n] * sin(2 * M_PI * k * n / M); }
    return 2.0 * sqrt(sc * sc + ss * ss) / M;
}

static void test_tables(void)
{
    int t, f, L, k;
    printf("tables\n");
    for (t = 0; t < 3; t++)
        printf("  %-10s %d frames, %d levels, %.1f KiB (%.1f KiB at 32 frames)\n", tb[t].name, tb[t].nframes, tb[t].nlev,
               wt_table_bytes(&tb[t]) / 1024.0, wt_table_bytes(&tb[t]) / 1024.0 * 32 / tb[t].nframes);
    for (t = 0; t < 3; t++) {
        { int ns = t == 0 ? 9 : 16; CHECK(tb[t].nsrc == ns && tb[t].nframes == (wt_cfg.morph ? (ns - 1) * ((wt_cfg.morph_cap - 1) / (ns - 1)) + 1 : ns), "table %d: %d stored, %d internal frames", t, tb[t].nsrc, tb[t].nframes); }
        for (f = 0; f < tb[t].nframes; f++) {
            int16_t *x = tb[t].lev[0] + (size_t)f * WT_N;
            int n, pk = 0;
            for (n = 0; n < WT_N; n++) if (abs(x[n]) > pk) pk = abs(x[n]);
            CHECK(pk > 24000 && pk < 32768, "%s frame %d peak %d", tb[t].name, f, pk);
        }
        for (L = 0; L < tb[t].nlev; L++) {
            int M = 1 << tb[t].lg[L], H = tb[t].H[L];
            double worst = 0;
            for (k = H + 1; k < M / 2; k++) {
                double a = harm(tb[t].lev[L] + (size_t)6 * tb[t].expand * M, M, k);    /* stored frame 6 */
                if (a > worst) worst = a;
            }
            CHECK(worst < 3.0, "%s level %d has %.1f LSB above harmonic %d", tb[t].name, L, worst, H);
        }
    }
    /* BASIC SHAPES (Serum's order): frame 3 is a sine, frame 1 a saw (harmonic k about 1/k), frame 2 a square (odd only) */
    {
        const int16_t *x = tb[0].lev[0] + (size_t)3 * tb[0].expand * WT_N;
        double h1 = harm(x, WT_N, 1);
        CHECK(harm(x, WT_N, 2) < 0.01 * h1 && harm(x, WT_N, 3) < 0.01 * h1, "frame 3 is not a pure sine");
        x = tb[0].lev[0] + (size_t)1 * tb[0].expand * WT_N;
        h1 = harm(x, WT_N, 1);
        CHECK(fabs(harm(x, WT_N, 2) / h1 - 0.5) < 0.03 && fabs(harm(x, WT_N, 5) / h1 - 0.2) < 0.03, "frame 1 is not a saw");
        x = tb[0].lev[0] + (size_t)2 * tb[0].expand * WT_N;
        h1 = harm(x, WT_N, 1);
        CHECK(harm(x, WT_N, 2) < 0.02 * h1 && fabs(harm(x, WT_N, 3) / h1 - 1.0 / 3) < 0.03, "frame 2 is not a square");
    }
    {   /* a mip level must stay close to the full-band frame where both have the harmonic */
        const int16_t *a = tb[0].lev[0] + (size_t)1 * tb[0].expand * WT_N;
        CHECK(fabs(harm(a, WT_N, 3) - harm(tb[0].lev[2] + ((size_t)(1 * tb[0].expand) << tb[0].lg[2]), 1 << tb[0].lg[2], 3)) < 6.0, "mip level 2 changes harmonic 3");
    }
}

static void test_voices(void)
{
    static int16_t buf[2 * 4096];
    int i, nz = 0;
    printf("voices\n");
    fresh();
    wt_synth_render(S, buf, 4096);
    for (i = 0; i < 8192; i++) nz |= buf[i];
    CHECK(nz == 0, "silence is not silent");
    for (i = 0; i < 12; i++) wt_synth_note_on(S, 40 + i, 100);
    CHECK(wt_synth_active_voices(S) == WT_NVOICES, "poly: %d voices after 12 notes", wt_synth_active_voices(S));
    wt_synth_all_off(S);
    for (i = 0; i < 30; i++) wt_synth_render(S, buf, 4096);
    CHECK(wt_synth_active_voices(S) == 0, "voices not freed after release (%d)", wt_synth_active_voices(S));
    P("VOICEMODE", 1);
    wt_synth_note_on(S, 40, 100); wt_synth_note_on(S, 47, 100);
    CHECK(wt_synth_active_voices(S) == 1, "mono: %d voices", wt_synth_active_voices(S));
    wt_synth_note_off(S, 47);
    CHECK(S->v[0].target == 40 * 256 && S->v[0].gate, "mono: last-note priority did not return to the held note");
    wt_synth_note_off(S, 40);
    for (i = 0; i < 30; i++) wt_synth_render(S, buf, 4096);
    CHECK(wt_synth_active_voices(S) == 0, "mono voice not freed");
    fresh();
    P("AMP_DEC", 300); P("AMP_SUS", 0);
    wt_synth_note_on(S, 60, 100);
    for (i = 0; i < 30; i++) wt_synth_render(S, buf, 4096);
    CHECK(wt_synth_active_voices(S) == 0, "sustain 0 note did not end by itself");
}

static double band_rms(int note, int type, int cutoff, int reso, double lo_hz, double hi_hz)
{
    enum { N = 8192 };
    static int16_t buf[2 * (N + 2048)];
    static double re[N], im[N];
    double df = (double)WT_FS / N, e = 0;
    int i, k;
    fresh();
    P("F_TYPE", type); P("F_CUTOFF", cutoff); P("F_RESO", reso);
    wt_synth_note_on(S, note, 100);
    wt_synth_render(S, buf, N + 2048);
    for (i = 0; i < N; i++) { re[i] = buf[2 * (2048 + i)] * (0.5 - 0.5 * cos(2 * M_PI * i / N)); im[i] = 0; }
    fft(re, im, N);
    for (k = 1; k < N / 2; k++) if (k * df >= lo_hz && k * df < hi_hz) e += re[k] * re[k] + im[k] * im[k];
    return 10 * log10(e + 1e-9);
}

static void test_filter(void)
{
    double open, lp, hp;
    printf("filter\n");
    open = band_rms(36, 0, 1000, 200, 4000, 12000);
    lp = band_rms(36, 1, 600, 200, 4000, 12000);          /* cutoff note 60 = 262 Hz */
    CHECK(open - lp > 25, "LP12 at 262 Hz only attenuates 4-12 kHz by %.1f dB", open - lp);
    lp = band_rms(36, 2, 600, 200, 4000, 12000);
    CHECK(open - lp > 45, "LP24 at 262 Hz only attenuates 4-12 kHz by %.1f dB", open - lp);
    open = band_rms(36, 0, 1000, 200, 30, 120);
    hp = band_rms(36, 3, 1000, 200, 30, 120);              /* cutoff 2.6 kHz */
    CHECK(open - hp > 25, "HP at 2.6 kHz only attenuates 30-120 Hz by %.1f dB", open - hp);
    {   /* maximum resonance and drive must not ring on after the note has ended */
        static int16_t buf[2 * 16384];
        int i, pk = 0, b;
        fresh();
        P("F_TYPE", 2); P("F_CUTOFF", 700); P("F_RESO", 1000); P("F_DRIVE", 800); P("AMP_REL", 100);
        wt_synth_note_on(S, 36, 127);
        wt_synth_render(S, buf, 16384);
        wt_synth_note_off(S, 36);
        for (b = 0; b < 12; b++) wt_synth_render(S, buf, 16384);
        wt_synth_note_on(S, 36, 127);                     /* a fresh note after the old one: state must have been cleared */
        wt_synth_note_off(S, 36);
        for (b = 0; b < 12; b++) wt_synth_render(S, buf, 16384);
        for (i = 0; i < 32768; i++) if (abs(buf[i]) > pk) pk = abs(buf[i]);
        CHECK(pk == 0 && wt_synth_active_voices(S) == 0, "resonant filter still sounding after release (peak %d)", pk);
    }
}

static void test_unison_warp(void)
{
    static int16_t buf[2 * 8192];
    int i;
    double l = 0, r = 0, l1 = 0;
    printf("unison / warp / matrix\n");
    fresh();
    P("A_UNI", 5); P("A_SPREAD", 1000); P("A_DETUNE", 400);
    wt_synth_note_on(S, 48, 100);
    wt_synth_render(S, buf, 8192);
    for (i = 4096; i < 8192; i++) { l += (double)buf[2 * i] * buf[2 * i]; r += (double)buf[2 * i + 1] * buf[2 * i + 1]; }
    CHECK(fabs(10 * log10(l / r)) < 2.0, "unison spread is lopsided: L/R %.1f dB", 10 * log10(l / r));
    fresh();
    wt_synth_note_on(S, 48, 100);
    wt_synth_render(S, buf, 8192);
    for (i = 4096; i < 8192; i++) l1 += (double)buf[2 * i] * buf[2 * i];
    CHECK(fabs(10 * log10(l / l1)) < 6.0, "5 unison voices change the level by %.1f dB", 10 * log10(l / l1));
    {   /* mod matrix: LFO on WT POS changes the sound; a macro at zero does not */
        int16_t a[2 * 4096], b[2 * 4096];
        int diff = 0;
        fresh(); P("A_WTPOS", 100); P("MX1_SRC", 7); P("MX1_DST", 1); P("MX1_AMT", 800); P("MACRO1", 0);
        wt_synth_note_on(S, 48, 100); wt_synth_render(S, a, 4096);
        fresh(); P("A_WTPOS", 100);
        wt_synth_note_on(S, 48, 100); wt_synth_render(S, b, 4096);
        CHECK(!memcmp(a, b, sizeof a), "macro 0 modulation changed the sound");
        fresh(); P("A_WTPOS", 100); P("MX1_SRC", 7); P("MX1_DST", 1); P("MX1_AMT", 800); P("MACRO1", 800);
        wt_synth_note_on(S, 48, 100); wt_synth_render(S, a, 4096);
        for (i = 0; i < 8192; i++) diff |= a[i] != b[i];
        CHECK(diff, "macro modulation of WT POS has no effect");
    }
    {   /* every warp mode renders something bounded and different from no warp */
        int m;
        int16_t ref[2 * 4096], w[2 * 4096];
        fresh(); P("B_ON", 1); P("B_WTPOS", 0); P("B_LEVEL", 300);
        wt_synth_note_on(S, 48, 100); wt_synth_render(S, ref, 4096);
        for (m = 1; m <= 3; m++) {
            int diff = 0;
            fresh(); P("B_ON", 1); P("B_WTPOS", 0); P("B_LEVEL", 300); P("A_WARPMODE", m); P("A_WARP", 600);
            wt_synth_note_on(S, 48, 100); wt_synth_render(S, w, 4096);
            for (i = 0; i < 8192; i++) diff |= w[i] != ref[i];
            CHECK(diff, "warp mode %d has no effect", m);
        }
    }
}

static void test_alias(void)
{
    double saw, sq, p25;
    printf("aliasing (worst case over C3..C8, single voice, aliased energy vs total)\n");
    saw = worst_alias(1, 48, 108, 1);
    sq = worst_alias(2, 48, 108, 1);
    p25 = worst_alias(5, 48, 108, 1);
    printf("  saw %.1f dB, square %.1f dB, pulse25 %.1f dB\n", saw, sq, p25);
    CHECK(saw < -50, "saw aliasing %.1f dB", saw);
    CHECK(sq < -50, "square aliasing %.1f dB", sq);
    CHECK(p25 < -50, "pulse aliasing %.1f dB", p25);
}


/* ---- feature tests (Serum-mapped additions) ---- */
enum { FN = 16384 };
static int16_t fbuf[2 * (FN + 4096)];

static void render_n(int n) { wt_synth_render(S, fbuf, n); }
static double rms_block(const int16_t *b, int n, int ch)
{
    double e = 0; int i;
    for (i = 0; i < n; i++) e += (double)b[2 * i + ch] * b[2 * i + ch];
    return sqrt(e / n);
}
/* power spectral density (arbitrary units per Hz) averaged over [lo, hi] Hz of the left channel of fbuf[start..] */
static double psd(int start, double lo, double hi)
{
    static double re[FN], im[FN];
    double df = (double)WT_FS / FN, e = 0; int i, k, c = 0;
    for (i = 0; i < FN; i++) { re[i] = fbuf[2 * (start + i)] * bh(i, FN); im[i] = 0; }
    fft(re, im, FN);
    for (k = 1; k < FN / 2; k++) if (k * df >= lo && k * df <= hi) { e += re[k] * re[k] + im[k] * im[k]; c++; }
    return c ? e / c : 0;
}
static double db(double x) { return 10 * log10(x + 1e-9); }

static void harm_levels(int note, int nh, double *out)    /* level of harmonics 1..nh relative to the strongest, dB */
{
    static double re[FN], im[FN];
    double f0 = 440.0 * pow(2.0, (note - 69) / 12.0), df = (double)WT_FS / FN, top = 1e-9;
    int i, h, k;
    wt_synth_note_on(S, note, 100);
    render_n(FN + 4096);
    for (i = 0; i < FN; i++) { re[i] = fbuf[2 * (4096 + i)] * bh(i, FN); im[i] = 0; }
    fft(re, im, FN);
    for (h = 1; h <= nh; h++) {
        double m = 0;
        for (k = (int)(h * f0 / df) - 4; k <= (int)(h * f0 / df) + 4; k++)
            if (k > 0 && k < FN / 2) { double e = re[k] * re[k] + im[k] * im[k]; if (e > m) m = e; }
        out[h] = m;
        if (m > top) top = m;
    }
    for (h = 1; h <= nh; h++) out[h] = 10 * log10(out[h] / top + 1e-12);
    wt_synth_all_off(S);
}

static void test_spectral_morph(void)
{
    static int16_t fr[2 * WT_N];
    wt_table_t m1, m0;
    int i;
    double r1, r0, rend;
    printf("spectral morph\n");
    for (i = 0; i < WT_N; i++) {
        fr[i] = (int16_t)lround(0.9 * 32767 * sin(2 * M_PI * i / WT_N));
        fr[WT_N + i] = (int16_t)-fr[i];                     /* the same sine upside down */
    }
    wt_table_build_ex(&m1, "SPEC", fr, 2, 1);
    wt_table_build_ex(&m0, "XFADE", fr, 2, 0);
    CHECK(m1.nframes > 2 && m1.expand > 1 && m0.nframes == 2, "morph expansion: %d internal frames, x%d", m1.nframes, m1.expand);
    fresh();
    P("A_WTPOS", 0); wt_synth_set_table(S, 0, &m1); wt_synth_note_on(S, 48, 100); render_n(8192);
    rend = rms_block(fbuf + 2 * 4096, 4096, 0); wt_synth_all_off(S);
    fresh();
    P("A_WTPOS", 500); wt_synth_set_table(S, 0, &m1); wt_synth_note_on(S, 48, 100); render_n(8192);
    r1 = rms_block(fbuf + 2 * 4096, 4096, 0);
    fresh();
    P("A_WTPOS", 500); wt_synth_set_table(S, 0, &m0); wt_synth_note_on(S, 48, 100); render_n(8192);
    r0 = rms_block(fbuf + 2 * 4096, 4096, 0);
    CHECK(r1 > 0.8 * rend, "spectral morph midpoint is %.0f%% of the end frames' level", 100 * r1 / rend);
    CHECK(r0 < 0.1 * rend, "waveform crossfade of opposite sines should cancel (%.0f%%)", 100 * r0 / rend);
    wt_table_free(&m1); wt_table_free(&m0);
    /* in-between frames stay about as loud as their neighbours (a sweep has no dips or bumps) */
    {
        int tt, f, j, bad = 0;
        for (tt = 0; tt < 3; tt++) {
            const wt_table_t *T = &tb[tt];
            for (f = 0; f + 1 < T->nsrc; f++) {
                double ra = 0, rb = 0;
                int n;
                for (n = 0; n < WT_N; n++) {
                    double x = T->lev[0][(size_t)f * T->expand * WT_N + n], y = T->lev[0][(size_t)(f + 1) * T->expand * WT_N + n];
                    ra += x * x; rb += y * y;
                }
                ra = sqrt(ra / WT_N); rb = sqrt(rb / WT_N);
                for (j = 1; j < T->expand; j++) {
                    double r = 0, lo = ra < rb ? ra : rb, hi = ra < rb ? rb : ra;
                    for (n = 0; n < WT_N; n++) { double x = T->lev[0][(size_t)(f * T->expand + j) * WT_N + n]; r += x * x; }
                    r = sqrt(r / WT_N);
                    if (r < 0.7 * lo || r > 1.3 * hi) { bad++; printf("    %s frames %d-%d: in-between rms %.0f, neighbours %.0f and %.0f\n", T->name, f, f + 1, r, ra, rb); }
                }
            }
        }
        CHECK(bad == 0, "%d in-between frames are much quieter or louder than both neighbours", bad);
    }
}

static void test_table_morph(void)
{
    static int16_t a[2 * 4096], b[2 * 4096], c[2 * 4096];
    int i, diff = 0;
    printf("table morph\n");
    fresh(); P("A_WTPOS", 400); wt_synth_note_on(S, 52, 100); wt_synth_render(S, a, 4096);
    fresh(); P("A_WTPOS", 400); P("A_TABLE2", 1); P("A_MORPH", 0); wt_synth_note_on(S, 52, 100); wt_synth_render(S, b, 4096);
    CHECK(!memcmp(a, b, sizeof a), "MORPH 0 changed the sound");
    fresh(); P("A_WTPOS", 400); P("A_TABLE", 1); wt_synth_note_on(S, 52, 100); wt_synth_render(S, a, 4096);
    fresh(); P("A_WTPOS", 400); P("A_TABLE", 0); P("A_TABLE2", 1); P("A_MORPH", 1000); wt_synth_note_on(S, 52, 100); wt_synth_render(S, b, 4096);
    CHECK(!memcmp(a, b, sizeof a), "MORPH 1000 is not the second table");
    fresh(); P("A_WTPOS", 400); P("A_TABLE", 0); P("A_TABLE2", 1); P("A_MORPH", 500); wt_synth_note_on(S, 52, 100); wt_synth_render(S, c, 4096);
    for (i = 0; i < 8192; i++) diff |= c[i] != b[i];
    CHECK(diff, "MORPH 500 equals the second table");
    {   /* sweeping WT POS and MORPH together moves smoothly: no sample-to-sample jump beyond the signal's own slope */
        static int16_t s[2 * 16384];
        int maxjump = 0;
        fresh(); P("A_TABLE2", 2); P("A_WTPOS", 0); P("LFO1_SHAPE", 2); P("LFO1_RATE", 150);
        P("MX1_SRC", 1); P("MX1_DST", D_MORPH); P("MX1_AMT", 1000);
        wt_synth_note_on(S, 36, 100);
        wt_synth_render(S, s, 16384);
        for (i = 4096; i < 16383; i++) { int d = abs(s[2 * i + 2] - s[2 * i]); if (d > maxjump) maxjump = d; }
        CHECK(maxjump < 12000, "morph sweep jumps by %d between samples", maxjump);
    }
}

static void test_fm(void)
{
    static int16_t base[2 * 8192], x[2 * 8192];
    int i, diff, bmod;
    double h[8];
    printf("FM / PD between A and B\n");
    fresh(); P("B_ON", 1); P("B_LEVEL", 0); P("B_WTPOS", 0); P("B_OCT", 1);
    wt_synth_note_on(S, 48, 100); wt_synth_render(S, base, 8192);
    fresh(); P("B_ON", 1); P("B_LEVEL", 0); P("B_WTPOS", 0); P("B_OCT", 1); P("A_FM", 300);
    wt_synth_note_on(S, 48, 100); wt_synth_render(S, x, 8192);
    for (diff = 0, i = 0; i < 16384; i++) diff |= base[i] != x[i];
    CHECK(diff, "A_FM from a silent B has no effect");
    fresh(); P("B_ON", 1); P("B_LEVEL", 0); P("B_WTPOS", 0); P("B_OCT", 1); P("A_FM", 0);
    wt_synth_note_on(S, 48, 100); wt_synth_render(S, x, 8192);
    CHECK(!memcmp(base, x, sizeof base), "FM 0 changed the sound");
    fresh(); P("B_ON", 1); P("B_LEVEL", 0); P("B_WTPOS", 0); P("B_OCT", 1); P("A_PD", 300);
    wt_synth_note_on(S, 48, 100); wt_synth_render(S, x, 8192);
    for (diff = 0, i = 0; i < 16384; i++) diff |= base[i] != x[i];
    CHECK(diff, "A_PD from a silent B has no effect");
    /* both directions at once; and B as the carrier */
    fresh(); P("B_ON", 1); P("B_WTPOS", 0); P("A_WTPOS", 0); P("A_LEVEL", 0); P("B_FM", 300);
    wt_synth_note_on(S, 48, 100); wt_synth_render(S, x, 8192);
    fresh(); P("B_ON", 1); P("B_WTPOS", 0); P("A_WTPOS", 0); P("A_LEVEL", 0);
    wt_synth_note_on(S, 48, 100); wt_synth_render(S, base, 8192);
    for (diff = 0, i = 0; i < 16384; i++) diff |= base[i] != x[i];
    CHECK(diff, "B_FM from a silent A has no effect");
    fresh(); P("B_ON", 1); P("B_WTPOS", 0); P("A_WTPOS", 0); P("A_FM", 200); P("B_FM", 200);
    wt_synth_note_on(S, 48, 100); wt_synth_render(S, x, 8192);
    {   double e = 0; for (i = 0; i < 16384; i++) e += (double)x[i] * x[i];
        CHECK(e > 0 && wt_synth_active_voices(S) == 1, "mutual FM is silent"); }
    /* FM is frequency modulation: the pitch is kept (the strongest partial stays at the note) */
    fresh(); P("A_WTPOS", 0); P("B_ON", 1); P("B_LEVEL", 0); P("B_WTPOS", 0); P("B_OCT", 1); P("A_FM", 150);
    harm_levels(48, 6, h);
    bmod = 0; for (i = 2; i <= 6; i++) if (h[i] > h[1]) bmod = 1;
    CHECK(!bmod && h[1] > -1, "FM moved the fundamental (partial levels %.0f %.0f %.0f)", h[1], h[2], h[3]);
    CHECK(h[2] > -45 || h[3] > -45, "FM produced no sidebands");
}

static void test_sub(void)
{
    double h[9];
    int w;
    printf("sub oscillator\n");
    for (w = 0; w < 4; w++) {
        fresh(); P("A_ON", 0); P("SUB_ON", 1); P("SUB_OCT", 0); P("SUB_WAVE", w); P("SUB_PW", 500); P("SUB_LEVEL", 800);
        harm_levels(48, 8, h);
        switch (w) {
        case 0: CHECK(h[2] < -60 && h[3] < -60, "sine has harmonics (%.0f %.0f dB)", h[2], h[3]); break;
        case 1: CHECK(fabs(h[3] + 19.08) < 2.0 && h[2] < -40, "triangle partials %.1f %.1f dB", h[2], h[3]); break;
        case 2: CHECK(fabs(h[3] + 9.54) < 2.0 && h[2] < -40, "square partials %.1f %.1f dB", h[2], h[3]); break;
        default: CHECK(fabs(h[2] + 3.6) < 3.0, "pulse (duty 27%%) 2nd partial %.1f dB, expected about -3.6", h[2]); break;
        }
    }
    {   /* pulse width moves the spectrum: 50 % has no even partials, a narrow pulse has */
        double a[9], b[9];
        fresh(); P("A_ON", 0); P("SUB_ON", 1); P("SUB_OCT", 0); P("SUB_WAVE", 3); P("SUB_PW", 0); harm_levels(48, 8, a);
        fresh(); P("A_ON", 0); P("SUB_ON", 1); P("SUB_OCT", 0); P("SUB_WAVE", 3); P("SUB_PW", 1000); harm_levels(48, 8, b);
        CHECK(a[2] < -40 && b[2] > -10, "PW 0: 2nd %.1f dB, PW 1000: 2nd %.1f dB", a[2], b[2]);
    }
    {   /* OCT and CRS */
        static int16_t buf[2 * 8192];
        int i, zc0 = 0, zc1 = 0;
        fresh(); P("A_ON", 0); P("SUB_ON", 1); P("SUB_OCT", 0); wt_synth_note_on(S, 48, 100); wt_synth_render(S, buf, 8192);
        for (i = 4096; i < 8191; i++) zc0 += (buf[2 * i] < 0) != (buf[2 * i + 2] < 0);
        fresh(); P("A_ON", 0); P("SUB_ON", 1); P("SUB_OCT", -1); P("SUB_CRS", 1200); wt_synth_note_on(S, 48, 100); wt_synth_render(S, buf, 8192);
        for (i = 4096; i < 8191; i++) zc1 += (buf[2 * i] < 0) != (buf[2 * i + 2] < 0);
        CHECK(abs(zc0 - zc1) <= 2 && zc0 > 20, "OCT -1 and CRS +1200 cents should cancel (%d vs %d zero crossings)", zc0, zc1);
    }
    {   /* square and pulse at high notes are anti-aliased (PolyBLEP) */
        double a;
        fresh(); P("A_ON", 0); P("SUB_ON", 1); P("SUB_OCT", 0); P("SUB_WAVE", 2); P("SUB_LEVEL", 900);
        a = alias_db(96, NULL);
        printf("  square at C7: aliasing %.1f dB\n", a);
        if (verbose) { int n; for (n = 36; n <= 108; n += 6) { fresh(); P("A_ON", 0); P("SUB_ON", 1); P("SUB_OCT", 0); P("SUB_WAVE", 2); P("SUB_LEVEL", 900); printf("   note %d: %.1f dB\n", n, alias_db(n, NULL)); } }
        CHECK(a < -40, "sub square aliasing %.1f dB", a);
    }
    {   /* SUB_CONT: a new note continues the phase of the previous one */
        static int16_t b1[2 * 256], b2[2 * 256];
        fresh(); P("A_ON", 0); P("SUB_ON", 1); P("SUB_OCT", 0); P("SUB_PHASE", 90); P("SUB_CONT", 0);
        wt_synth_note_on(S, 48, 100); wt_synth_render(S, b1, 256); wt_synth_all_off(S);
        {   int k; int16_t tmp[2 * 4096]; for (k = 0; k < 30; k++) wt_synth_render(S, tmp, 4096); }
        P("SUB_CONT", 1);
        wt_synth_note_on(S, 48, 100); wt_synth_render(S, b2, 256);
        CHECK(memcmp(b1, b2, sizeof b1) != 0, "SUB_CONT 1 restarted at the fixed phase");
    }
}

static void test_noise(void)
{
    double w, p, br, lo, hi;
    int i, l_eq_r = 1;
    printf("noise oscillator\n");
    fresh(); P("A_ON", 0); P("NOI_ON", 1); P("NOI_LEVEL", 800); P("NOI_TYPE", 0);
    wt_synth_note_on(S, 48, 100); render_n(FN + 4096);
    w = db(psd(4096, 400, 600)) - db(psd(4096, 7000, 9000));
    CHECK(fabs(w) < 2.5, "white noise tilt %.1f dB", w);
    for (i = 4096; i < 4096 + 4000; i++) if (fbuf[2 * i] != fbuf[2 * i + 1]) l_eq_r = 0;
    CHECK(l_eq_r, "NOI_STEREO 0 is not mono");
    wt_synth_all_off(S);
    fresh(); P("A_ON", 0); P("NOI_ON", 1); P("NOI_LEVEL", 800); P("NOI_TYPE", 1);
    wt_synth_note_on(S, 48, 100); render_n(FN + 4096);
    p = db(psd(4096, 400, 600)) - db(psd(4096, 7000, 9000));
    CHECK(fabs(p - 11.9) < 3.5, "pink noise: 500 Hz vs 8 kHz is %.1f dB, expected about 12", p);
    wt_synth_all_off(S);
    fresh(); P("A_ON", 0); P("NOI_ON", 1); P("NOI_LEVEL", 800); P("NOI_TYPE", 2);
    wt_synth_note_on(S, 48, 100); render_n(FN + 4096);
    br = db(psd(4096, 400, 600)) - db(psd(4096, 7000, 9000));
    CHECK(fabs(br - 23.9) < 5.0, "brown noise: 500 Hz vs 8 kHz is %.1f dB, expected about 24", br);
    wt_synth_all_off(S);
    {   /* geiger: sparse bursts, so a high crest factor */
        double pk = 0, e = 0;
        fresh(); P("A_ON", 0); P("NOI_ON", 1); P("NOI_LEVEL", 1000); P("NOI_TYPE", 3);
        wt_synth_note_on(S, 48, 100); render_n(FN + 4096);
        for (i = 4096; i < FN + 4096; i++) { double x = fbuf[2 * i]; if (fabs(x) > pk) pk = fabs(x); e += x * x; }
        CHECK(pk / sqrt(e / FN) > 6, "geiger crest factor %.1f", pk / sqrt(e / FN));
        wt_synth_all_off(S);
    }
    {   /* STEREO 1000: left and right are uncorrelated */
        double sl = 0, sr = 0, sx = 0;
        fresh(); P("A_ON", 0); P("NOI_ON", 1); P("NOI_LEVEL", 800); P("NOI_STEREO", 1000);
        wt_synth_note_on(S, 48, 100); render_n(FN + 4096);
        for (i = 4096; i < FN + 4096; i++) { double l = fbuf[2 * i], r = fbuf[2 * i + 1]; sl += l * l; sr += r * r; sx += l * r; }
        CHECK(fabs(sx / sqrt(sl * sr)) < 0.1, "NOI_STEREO 1000 correlation %.2f", sx / sqrt(sl * sr));
        wt_synth_all_off(S);
    }
    {   /* FILTER knob: negative low-passes, positive high-passes */
        double open_hi, open_lo;
        fresh(); P("A_ON", 0); P("NOI_ON", 1); P("NOI_LEVEL", 800); P("NOI_FILTER", 0);
        wt_synth_note_on(S, 48, 100); render_n(FN + 4096);
        open_hi = psd(4096, 8000, 12000); open_lo = psd(4096, 100, 300);
        wt_synth_all_off(S);
        fresh(); P("A_ON", 0); P("NOI_ON", 1); P("NOI_LEVEL", 800); P("NOI_FILTER", -700);
        wt_synth_note_on(S, 48, 100); render_n(FN + 4096);
        lo = db(open_hi) - db(psd(4096, 8000, 12000));
        CHECK(lo > 15, "NOI_FILTER -700 only cuts the highs by %.1f dB", lo);
        wt_synth_all_off(S);
        fresh(); P("A_ON", 0); P("NOI_ON", 1); P("NOI_LEVEL", 800); P("NOI_FILTER", 700);
        wt_synth_note_on(S, 48, 100); render_n(FN + 4096);
        hi = db(open_lo) - db(psd(4096, 100, 300));
        CHECK(hi > 15, "NOI_FILTER +700 only cuts the lows by %.1f dB", hi);
        wt_synth_all_off(S);
    }
}

static void test_sample_layer(void)
{
    double step = 0;
    int i, n = smp[2].n;
    printf("noise sample layer\n");
    for (i = 0; i + 1 < n; i++) { double d = smp[2].d[i + 1] - smp[2].d[i]; step += d * d; }
    step = sqrt(step / n);
    CHECK(fabs((double)smp[2].d[0] - smp[2].d[n - 1]) < 8 * step + 4, "loop point jumps by %d (typical step %.1f)", abs(smp[2].d[0] - smp[2].d[n - 1]), step);
    for (i = 0; i < 3; i++) {
        double e = 0; int j;
        for (j = 0; j < smp[i].n; j++) e += (double)smp[i].d[j] * smp[i].d[j];
        CHECK(sqrt(e / smp[i].n) > 1500 && sqrt(e / smp[i].n) < 9000, "%s sample level %.0f", smp[i].name, sqrt(e / smp[i].n));
        CHECK(fabs((double)smp[i].d[0] - smp[i].d[smp[i].n - 1]) < 14000, "%s loop point", smp[i].name);
    }
    {   /* layer sounds, loops, and one-shot stops */
        static int16_t b[2 * 4096];
        int k;
        double tail = 0, head;
        fresh(); P("A_ON", 0); P("NOI_ON", 1); P("NOI_TYPE", 4); P("NOI_SMP", 3); P("NOI_SMP_LEVEL", 900);
        wt_synth_note_on(S, 48, 100); wt_synth_render(S, b, 4096);
        head = rms_block(b, 4096, 0);
        CHECK(head > 200, "sample layer is silent (%.0f)", head);
        for (k = 0; k < 6; k++) wt_synth_render(S, b, 4096);       /* beyond one loop (16384 samples) */
        CHECK(rms_block(b, 4096, 0) > 200, "looping sample stopped");
        wt_synth_all_off(S);
        fresh(); P("A_ON", 0); P("NOI_ON", 1); P("NOI_TYPE", 4); P("NOI_SMP", 3); P("NOI_SMP_LEVEL", 900); P("NOI_SMP_LOOP", 0);
        wt_synth_note_on(S, 48, 100);
        for (k = 0; k < 6; k++) wt_synth_render(S, b, 4096);
        tail = rms_block(b, 4096, 0);
        CHECK(tail < 1, "one-shot sample still playing (%.0f)", tail);
    }
    {   /* pitch: +12 semitones doubles the rate; key tracking follows the note; START and RAND move the entry point */
        double c0, c12, k0, k12;
        #define CENTROID(first) ({ double num = 0, den = 0; int kk; static double re[FN], im[FN]; for (kk = 0; kk < FN; kk++) { re[kk] = fbuf[2*(4096+kk)] * bh(kk, FN); im[kk] = 0; } fft(re, im, FN); \
            for (kk = 1; kk < FN / 2; kk++) { double e = re[kk]*re[kk] + im[kk]*im[kk]; num += e * kk; den += e; } num / den; })
        fresh(); P("A_ON", 0); P("NOI_ON", 1); P("NOI_TYPE", 4); P("NOI_SMP", 3); P("NOI_SMP_LEVEL", 900);
        wt_synth_note_on(S, 48, 100); render_n(FN + 4096); c0 = CENTROID(0); wt_synth_all_off(S);
        fresh(); P("A_ON", 0); P("NOI_ON", 1); P("NOI_TYPE", 4); P("NOI_SMP", 3); P("NOI_SMP_LEVEL", 900); P("NOI_SMP_PITCH", 12);
        wt_synth_note_on(S, 48, 100); render_n(FN + 4096); c12 = CENTROID(0); wt_synth_all_off(S);
        CHECK(c12 / c0 > 1.6 && c12 / c0 < 2.5, "PITCH +12 changes the spectrum by %.2fx", c12 / c0);
        fresh(); P("A_ON", 0); P("NOI_ON", 1); P("NOI_TYPE", 4); P("NOI_SMP", 3); P("NOI_SMP_LEVEL", 900);
        wt_synth_note_on(S, 60, 100); render_n(FN + 4096); k0 = CENTROID(0); wt_synth_all_off(S);
        fresh(); P("A_ON", 0); P("NOI_ON", 1); P("NOI_TYPE", 4); P("NOI_SMP", 3); P("NOI_SMP_LEVEL", 900);
        wt_synth_note_on(S, 72, 100); render_n(FN + 4096); k12 = CENTROID(0); wt_synth_all_off(S);
        CHECK(fabs(k12 / k0 - 1) < 0.15, "without key tracking the note changed the sample's spectrum by %.2fx", k12 / k0);
        fresh(); P("A_ON", 0); P("NOI_ON", 1); P("NOI_TYPE", 4); P("NOI_SMP", 3); P("NOI_SMP_LEVEL", 900); P("NOI_SMP_KEY", 1);
        wt_synth_note_on(S, 72, 100); render_n(FN + 4096); k12 = CENTROID(0); wt_synth_all_off(S);
        CHECK(k12 / k0 > 1.6 && k12 / k0 < 2.5, "key tracking: an octave up changes the spectrum by %.2fx", k12 / k0);
    }
    {
        static int16_t a[2 * 512], b[2 * 512];
        fresh(); P("A_ON", 0); P("NOI_ON", 1); P("NOI_TYPE", 4); P("NOI_SMP", 1); P("NOI_SMP_LEVEL", 900); P("NOI_SMP_START", 0);
        wt_synth_note_on(S, 48, 100); wt_synth_render(S, a, 512);
        fresh(); P("A_ON", 0); P("NOI_ON", 1); P("NOI_TYPE", 4); P("NOI_SMP", 1); P("NOI_SMP_LEVEL", 900); P("NOI_SMP_START", 500);
        wt_synth_note_on(S, 48, 100); wt_synth_render(S, b, 512);
        CHECK(memcmp(a, b, sizeof a) != 0, "NOI_SMP_START has no effect");
        fresh(); P("A_ON", 0); P("NOI_ON", 1); P("NOI_TYPE", 4); P("NOI_SMP", 1); P("NOI_SMP_LEVEL", 900); P("NOI_SMP_RAND", 1000);
        wt_synth_note_on(S, 48, 100); wt_synth_render(S, a, 512); wt_synth_note_on(S, 48, 100); wt_synth_render(S, b, 512);
        CHECK(memcmp(a, b, sizeof a) != 0, "NOI_SMP_RAND gives the same start twice");
    }
}

static void test_routing(void)
{
    double thru, direct;
    printf("filter routing\n");
    fresh(); P("F_TYPE", 2); P("F_CUTOFF", 500); P("A_FLT", 1);
    wt_synth_note_on(S, 36, 100); render_n(FN + 4096); thru = db(psd(4096, 3000, 9000));
    wt_synth_all_off(S);
    fresh(); P("F_TYPE", 2); P("F_CUTOFF", 500); P("A_FLT", 0);
    wt_synth_note_on(S, 36, 100); render_n(FN + 4096); direct = db(psd(4096, 3000, 9000));
    CHECK(direct - thru > 30, "A_FLT 0 should bypass the filter: highs only %.1f dB above the filtered sound", direct - thru);
    wt_synth_all_off(S);
}


/* ---- effects (X0X's distortion, compressor, delay, reverb) ---- */
static double win_rms(const int16_t *b, double t0, double t1)       /* left channel, seconds */
{
    int i0 = (int)(t0 * WT_FS), i1 = (int)(t1 * WT_FS);
    double e = 0;
    int i;
    for (i = i0; i < i1; i++) e += (double)b[2 * i] * b[2 * i];
    return sqrt(e / (i1 - i0));
}

static void blip(void)       /* a short click-like note: the saw for a millisecond or two */
{
    P("AMP_DEC", 100); P("AMP_SUS", 0); P("AMP_REL", 50); P("A_LEVEL", 900); P("MASTER", 900);
}

static void test_fx(void)
{
    static int16_t buf[2 * 44100 * 2];
    int ty, i;
    double h[8];
    printf("effects\n");
    /* distortion: a pure sine gains harmonics, with every character; off leaves it pure */
    fresh(); P("A_ON", 0); P("SUB_ON", 1); P("SUB_OCT", 0); P("SUB_WAVE", 0); P("SUB_LEVEL", 700);
    harm_levels(48, 6, h);
    CHECK(h[2] < -60 && h[3] < -60, "dry sine is not pure (%.0f %.0f dB)", h[2], h[3]);
    for (ty = 1; ty <= 7; ty++) {
        double top;
        fresh(); P("A_ON", 0); P("SUB_ON", 1); P("SUB_OCT", 0); P("SUB_WAVE", 0); P("SUB_LEVEL", 700);
        P("FX_DIST", ty); P("FX_DRIVE", 110);
        harm_levels(48, 6, h);
        top = h[2] > h[3] ? h[2] : h[3];
        CHECK(top > -40, "distortion %s adds no harmonics (%.0f %.0f dB)", wt_fx_dist_names[ty], h[2], h[3]);
    }
    /* compressor: a 20 dB difference between a loud and a quiet note shrinks */
    {
        double d_off, d_on;
        for (i = 0; i < 2; i++) {
            double loud, quiet;
            fresh(); P("FX_COMP", i ? 127 : 0); P("A_LEVEL", 1000); P("MASTER", 1000);
            wt_synth_note_on(S, 48, 127); wt_synth_render(S, buf, 16384); loud = win_rms(buf, 0.25, 0.37);
            fresh(); P("FX_COMP", i ? 127 : 0); P("A_LEVEL", 100); P("MASTER", 1000);
            wt_synth_note_on(S, 48, 127); wt_synth_render(S, buf, 16384); quiet = win_rms(buf, 0.25, 0.37);
            if (i) d_on = 20 * log10(loud / quiet); else d_off = 20 * log10(loud / quiet);
        }
        printf("  loud/quiet: %.1f dB without the compressor, %.1f dB with it\n", d_off, d_on);
        CHECK(d_on < d_off - 3, "compressor: the loud/quiet gap only shrinks from %.1f to %.1f dB", d_off, d_on);
    }
    /* delay: an echo one 1/8 note (250 ms at 120 BPM) after a click, nothing between */
    for (i = 0; i < 2; i++) {
        double echo, between;
        fresh(); blip(); P("DL_SEND", 127); P("DL_TIME", 5); P("DL_FDBK", 20); P("DL_LEVEL", 127); P("DL_HPF", 0); P("DL_TYPE", i); P("BPM", 120);
        wt_synth_note_on(S, 60, 127); wt_synth_render(S, buf, 44100);
        echo = win_rms(buf, 0.24, 0.27); between = win_rms(buf, 0.12, 0.2);
        printf("  delay %s: echo %.0f, between %.1f\n", i ? "TAPE" : "DIGI", echo, between);
        CHECK(echo > 20 * between + 20, "%s delay: echo %.1f at 250 ms is not above the gap %.1f", i ? "tape" : "digital", echo, between);
    }
    /* reverb: a tail where there was silence, dying away */
    {
        double dry_tail, wet_tail, wet_late;
        fresh(); blip(); wt_synth_note_on(S, 60, 127); wt_synth_render(S, buf, 44100);
        dry_tail = win_rms(buf, 0.2, 0.4);
        fresh(); blip(); P("RV_SEND", 127); P("RV_LEVEL", 127); P("RV_DECAY", 110);
        wt_synth_note_on(S, 60, 127); wt_synth_render(S, buf, 2 * 44100);
        wet_tail = win_rms(buf, 0.2, 0.4); wet_late = win_rms(buf, 1.4, 1.6);
        printf("  reverb tail: dry %.1f, wet %.1f at 0.3 s, %.1f at 1.5 s\n", dry_tail, wet_tail, wet_late);
        CHECK(wet_tail > 30 * dry_tail + 20, "reverb makes no tail (%.1f vs %.1f)", wet_tail, dry_tail);
        CHECK(wet_late < wet_tail, "the reverb tail does not die away");
    }
    /* stress: random settings of every effect while notes play; then everything off: the tails must go quiet */
    {
        uint32_t rng = 12345;
        int blk, id, bad = 0, pk = 0;
        fresh(); P("A_UNI", 3);
        wt_synth_note_on(S, 48, 120); wt_synth_note_on(S, 55, 120); wt_synth_note_on(S, 60, 120);
        for (blk = 0; blk < 120; blk++) {
            for (id = P_FX0; id < P_FX_RVSEND + 3; id++) {
                rng = rng * 1664525u + 1013904223u;
                wt_synth_set_param(S, id, (int)((rng >> 16) % (unsigned)(wt_param_desc[id].max + 1)));
            }
            wt_synth_render(S, buf, 4096);
            for (i = 0; i < 8192; i++) if (abs(buf[i]) > pk) pk = abs(buf[i]);
        }
        wt_synth_all_off(S);
        P("FX_DIST", 0); P("FX_COMP", 0); P("RV_SEND", 0); P("DL_SEND", 0); P("DL_FDBK", 0);
        for (blk = 0; blk < 80; blk++) wt_synth_render(S, buf, 4096);       /* 7 s */
        for (i = 0; i < 8192; i++) bad |= buf[i] != 0;
        CHECK(!bad, "effects tails do not die away after everything is off (a stuck or NaN state?)");
        CHECK(pk < 32768, "peak");
    }
}


/* ---- the step sequencer ---- */
static struct { int kind, note, vel, acc; uint32_t t; } slog[2048];
static int nslog;
static void seq_hook(int kind, int note, int vel, int acc, uint32_t tt)
{
    if (nslog < 2048) { slog[nslog].kind = kind; slog[nslog].note = note; slog[nslog].vel = vel; slog[nslog].acc = acc; slog[nslog].t = tt; nslog++; }
}
static void seq_run(int samples)
{
    static int16_t b[2 * 4096];
    while (samples > 0) { int n = samples > 4096 ? 4096 : samples; wt_synth_render(S, b, n); samples -= n; }
}
static void seq_fresh(int bpm)
{
    fresh(); P("BPM", bpm);
    wt_seq_clear(&S->seq.pat[0]);
    S->seq.cur = 0; nslog = 0;
    wt_seq_log = seq_hook;
}
static wt_step_t *stp(int i) { return &S->seq.pat[0].step[i]; }
static void on(int i, int note, int flags) { stp(i)->note[0] = (uint8_t)note; stp(i)->flags = (uint8_t)(SF_ON | flags); }
static int near_t(uint32_t got, double want) { return fabs((double)got - want) <= 17.0; }
static int find(int kind, int note, int nth)       /* the nth log entry of a kind and note (note < 0: any) */
{
    int i, k = 0;
    for (i = 0; i < nslog; i++) if (slog[i].kind == kind && (note < 0 || slog[i].note == note) && k++ == nth) return i;
    return -1;
}

static void test_seq(void)
{
    int i, k;
    printf("sequencer\n");
    /* timing: 120 BPM, a 16th is 5512.5 samples */
    seq_fresh(120);
    on(0, 60, 0); on(4, 62, 0); on(8, 64, 0); on(12, 65, 0);
    wt_seq_play(S, 1); seq_run(88200);
    CHECK(nslog >= 8, "only %d events in two seconds", nslog);
    for (i = 0; i < 4; i++) {
        int e = find(1, 60 + (i < 1 ? 0 : i == 1 ? 2 : i == 2 ? 4 : 5), 0);
        CHECK(e >= 0 && near_t(slog[e].t, 22050.0 * i + 16), "step %d note-on at %u, expected about %.0f", i * 4, e >= 0 ? slog[e].t : 0, 22050.0 * i);
    }
    {   int a = find(1, 60, 0), b = find(0, 60, 0);        /* gate 60 %: off at 0.6 of a 16th after the on */
        CHECK(a >= 0 && b >= 0 && near_t(slog[b].t - slog[a].t, 5512.5 * 0.6), "gate 60%%: held %u samples, expected %.0f", b >= 0 && a >= 0 ? slog[b].t - slog[a].t : 0, 5512.5 * 0.6);
    }
    CHECK(S->seq.pos >= 0 && S->seq.run, "the playhead is not running");
    /* wraps: the first note comes again after 16 steps = 88200 samples */
    seq_run(2000);
    {   int a = find(1, 60, 1);
        CHECK(a >= 0 && near_t(slog[a].t, 88200.0 + 16), "the pattern did not wrap on time (%u)", a >= 0 ? slog[a].t : 0);
    }
    /* tempo and rate */
    {   static const struct { int bpm, rate; double len; } C[] = {{90, SR_16, 7350.0}, {120, SR_16T, 3675.0}, {120, SR_32, 2756.25}, {120, SR_8T, 7350.0}};
        for (i = 0; i < 4; i++) {
            int a;
            seq_fresh(C[i].bpm);
            S->seq.pat[0].rate = (uint8_t)C[i].rate;
            for (k = 0; k < 4; k++) on(k, 60 + k, 0);
            wt_seq_play(S, 1); seq_run(40000);
            a = find(1, 63, 0);
            CHECK(a >= 0 && near_t(slog[a].t, 3 * C[i].len + 16), "bpm %d rate %d: step 4 at %u, expected %.0f", C[i].bpm, C[i].rate, a >= 0 ? slog[a].t : 0, 3 * C[i].len);
        }
    }
    /* swing: the second 16th of a pair comes late (66 %: 0.66 of the pair is the first step) */
    seq_fresh(120);
    S->seq.pat[0].swing = 66;
    on(0, 60, 0); on(1, 61, 0); on(2, 62, 0); on(3, 63, 0);
    wt_seq_play(S, 1); seq_run(30000);
    {   int a = find(1, 61, 0), b = find(1, 62, 0);
        CHECK(a >= 0 && near_t(slog[a].t, 2 * 5512.5 * 0.66 + 16), "swing: step 2 at %u, expected %.0f", a >= 0 ? slog[a].t : 0, 2 * 5512.5 * 0.66);
        CHECK(b >= 0 && near_t(slog[b].t, 2 * 5512.5 + 16), "swing: step 3 at %u (the pair still takes two steps)", b >= 0 ? slog[b].t : 0);
    }
    /* accent and velocity */
    seq_fresh(120);
    on(0, 60, 0); stp(0)->vel = 60; on(2, 62, SF_ACCENT); stp(2)->vel = 60;
    wt_seq_play(S, 1); seq_run(11100);
    {   int a = find(1, 60, 0), b = find(1, 62, 0), vp = -1, va = -1;
        CHECK(a >= 0 && slog[a].vel == 60 && !slog[a].acc, "plain step velocity");
        CHECK(b >= 0 && slog[b].acc, "the accent flag was lost");
        vp = 60 * 32767 / 127;                              /* what a plain velocity 60 is */
        for (i = 0; i < WT_NVOICES; i++) if (S->v[i].note == 62 && S->v[i].active) va = S->v[i].vel;
        CHECK(vp > 0 && va > vp + 3000, "the accent does not lift the velocity (%d vs %d)", va, vp);
    }
    /* a chord, a rest, the gate */
    seq_fresh(120);
    stp(0)->note[0] = 60; stp(0)->note[1] = 64; stp(0)->note[2] = 67; stp(0)->flags = SF_ON; stp(0)->gate = 50;
    wt_seq_play(S, 1); seq_run(8000);
    {   int n3 = 0;
        for (i = 0; i < nslog; i++) n3 += slog[i].kind == 1 && slog[i].t < 100;
        CHECK(n3 == 3, "a chord step started %d notes", n3);
        CHECK(find(0, 60, 0) >= 0 && find(0, 67, 0) >= 0, "the chord's notes did not end");
    }
    /* tie: no new attack, one note, held through both steps */
    seq_fresh(120);
    on(0, 60, 0); stp(0)->gate = 50; stp(1)->flags = SF_ON | SF_TIE; stp(1)->gate = 50;
    wt_seq_play(S, 1); seq_run(20000);
    {   int a = find(1, 60, 0), b = find(0, 60, 0);
        CHECK(find(1, 60, 1) < 0 && find(1, -1, 1) < 0, "a tie retriggered");
        CHECK(a >= 0 && b >= 0 && near_t(slog[b].t - slog[a].t, 5512.5 * 1.5), "tie: held %u samples, expected %.0f", a >= 0 && b >= 0 ? slog[b].t - slog[a].t : 0, 5512.5 * 1.5);
    }
    /* slide: the next note glides on the same voice, no off between */
    seq_fresh(120);
    on(0, 60, SF_SLIDE); on(1, 67, 0); stp(1)->gate = 50;
    wt_seq_play(S, 1); seq_run(20000);
    {   int a = find(1, 60, 0), sl = find(2, 67, 0), off60 = find(0, 60, 0);
        CHECK(a >= 0 && sl >= 0, "slide events missing");
        CHECK(off60 < 0, "a slide released the first note");
        CHECK(sl >= 0 && near_t(slog[sl].t, 5512.5 + 16), "slide at %u", sl >= 0 ? slog[sl].t : 0);
        CHECK(find(1, 67, 0) < 0, "a slide made a new note");
        CHECK(find(0, 67, 0) >= 0, "the slid-to note never ended");
    }
    /* slide glides in pitch on the voice: the voice's note follows and the pitch moves over several blocks */
    seq_fresh(120);
    on(0, 60, SF_SLIDE); on(1, 72, 0);
    wt_seq_play(S, 1); seq_run(5600);                      /* just after the slide started */
    {   int vi = -1, p1;
        for (i = 0; i < WT_NVOICES; i++) if (S->v[i].active) vi = i;
        p1 = vi >= 0 ? S->v[vi].pitch : -1;
        seq_run(6000);
        CHECK(vi >= 0 && S->v[vi].note == 72 && p1 > 60 * 256 && p1 < 72 * 256 + 1 && S->v[vi].pitch >= p1 && S->v[vi].pitch <= 72 * 256, "the pitch does not glide (%d then %d)", p1, vi >= 0 ? S->v[vi].pitch : -1);
    }
    /* chance */
    seq_fresh(120);
    S->seq.pat[0].len = 1; S->seq.pat[0].rate = SR_32; on(0, 60, 0); stp(0)->prob = 50; stp(0)->gate = 20;
    wt_seq_play(S, 1); seq_run(2756 * 2000);
    {   int cnt = 0;
        for (i = 0; i < nslog; i++) cnt += slog[i].kind == 1;
        CHECK(cnt > 800 && cnt < 1200, "a 50%% step played %d of 2000 times", cnt);
    }
    seq_fresh(120);
    S->seq.pat[0].len = 1; S->seq.pat[0].rate = SR_32; on(0, 60, 0); stp(0)->gate = 20;
    wt_seq_play(S, 1); seq_run(2756 * 300);
    {   int cnt = 0;
        for (i = 0; i < nslog; i++) cnt += slog[i].kind == 1;
        CHECK(cnt >= 298, "a 100%% step played %d of 300 times", cnt);
    }
    /* direction */
    {   static const char *D[] = {"forward", "reverse", "ping-pong"};
        static const int WANT[3][8] = {{60, 61, 62, 63, 60, 61, 62, 63}, {63, 62, 61, 60, 63, 62, 61, 60}, {60, 61, 62, 63, 62, 61, 60, 61}};
        for (k = 0; k < 3; k++) {
            int ok = 1;
            seq_fresh(240);
            S->seq.pat[0].len = 4; S->seq.pat[0].dir = (uint8_t)k; S->seq.pat[0].rate = SR_32;
            for (i = 0; i < 4; i++) on(i, 60 + i, 0);
            wt_seq_play(S, 1); seq_run(1378 * 9);
            for (i = 0; i < 8; i++) { int e = find(1, -1, i); if (e < 0 || slog[e].note != WANT[k][i]) ok = 0; }
            CHECK(ok, "direction %s plays the wrong order", D[k]);
        }
        seq_fresh(240);
        S->seq.pat[0].len = 4; S->seq.pat[0].dir = SD_RAND; S->seq.pat[0].rate = SR_32;
        for (i = 0; i < 4; i++) on(i, 60 + i, 0);
        wt_seq_play(S, 1); seq_run(1378 * 200);
        {   int seen[4] = {0, 0, 0, 0}, bad = 0;
            for (i = 0; i < nslog; i++) if (slog[i].kind == 1) { if (slog[i].note < 60 || slog[i].note > 63) bad = 1; else seen[slog[i].note - 60] = 1; }
            CHECK(!bad && seen[0] && seen[1] && seen[2] && seen[3], "random direction: %d %d %d %d", seen[0], seen[1], seen[2], seen[3]);
        }
    }
    /* transposition and pattern change at the wrap */
    seq_fresh(240);
    S->seq.pat[0].len = 2; S->seq.pat[0].rate = SR_32; S->seq.pat[0].trans = 24 + 12; on(0, 60, 0);
    wt_seq_clear(&S->seq.pat[1]); S->seq.pat[1].len = 2; S->seq.pat[1].rate = SR_32;
    S->seq.pat[1].step[0].note[0] = 50; S->seq.pat[1].step[0].flags = SF_ON;
    wt_seq_play(S, 1); seq_run(1378 * 2 + 200);
    CHECK(find(1, 72, 0) >= 0, "transpose +12 did not move C4 to C5");
    wt_seq_queue(S, 1);
    CHECK(S->seq.cur == 0 && S->seq.next == 1, "a queued pattern must wait for the wrap");
    seq_run(1378 * 4);
    CHECK(S->seq.cur == 1 && find(1, 50, 0) >= 0, "the queued pattern did not take over (%d)", S->seq.cur);
    /* stop: everything the sequencer holds is released */
    seq_fresh(60);
    on(0, 60, 0); stp(0)->gate = 100; stp(1)->flags = SF_ON | SF_TIE; stp(1)->gate = 100;
    wt_seq_play(S, 1); seq_run(600);
    CHECK(wt_synth_active_voices(S) >= 1, "nothing sounds");
    wt_seq_play(S, 0); seq_run(44100);
    CHECK(wt_synth_active_voices(S) == 0 && !S->seq.run && S->seq.pos == -1, "stop left %d voices sounding", wt_synth_active_voices(S));
    wt_seq_log = 0;
}

static uint32_t golden_hash(void)
{
    static int16_t buf[2 * 4096];
    uint32_t h = 2166136261u;
    int i, b;
    fresh();
    patch_apply("pad", setp, S);
    wt_synth_note_on(S, 52, 90); wt_synth_note_on(S, 59, 90); wt_synth_note_on(S, 64, 90);
    for (b = 0; b < 8; b++) {
        wt_synth_render(S, buf, 4096);
        for (i = 0; i < 8192; i++) h = (h ^ (uint16_t)buf[i]) * 16777619u;
        if (b == 3) { wt_synth_note_off(S, 59); wt_synth_note_on(S, 67, 100); }
    }
    patch_apply("bass", setp, S);
    wt_synth_note_on(S, 36, 110);
    for (b = 0; b < 4; b++) {
        wt_synth_render(S, buf, 4096);
        for (i = 0; i < 8192; i++) h = (h ^ (uint16_t)buf[i]) * 16777619u;
    }
    return h;
}

static void test_golden(const char *file, int update)
{
    uint32_t h1 = golden_hash(), h2 = golden_hash(), want = 0;
    FILE *f;
    printf("golden render\n");
    CHECK(h1 == h2, "two renders of the same scene differ (%08x vs %08x)", h1, h2);
    f = file ? fopen(file, "r") : NULL;
    if (f && !update) {
        if (fscanf(f, "%x", &want) == 1) CHECK(h1 == want, "golden hash %08x, now %08x (run with --update if intended)", want, h1);
        fclose(f);
    } else if (file) {
        if (f) fclose(f);
        f = fopen(file, "w");
        if (f) { fprintf(f, "%08x\n", h1); fclose(f); printf("  wrote golden %08x\n", h1); }
    }
}

static void bench(void)
{
    static int16_t buf[2 * 4096];
    struct timespec a, b;
    int i, blocks = 200, v;
    double ns;
    printf("benchmark (host, not representative of the device)\n");
    fresh();
    P("A_ON", 1); P("A_UNI", 3); P("B_ON", 1); P("B_UNI", 3); P("B_WTPOS", 250); P("A_WARPMODE", 1); P("A_WARP", 300);
    P("F_TYPE", 2); P("F_RESO", 400); P("SUB_ON", 1); P("AMP_DEC", 1000); P("AMP_REL", 1000);
    for (v = 0; v < 8; v++) wt_synth_note_on(S, 40 + v * 4, 100);
    wt_synth_render(S, buf, 4096);
    S->cpu_samples = 0;
    clock_gettime(CLOCK_MONOTONIC, &a);
    for (i = 0; i < blocks; i++) wt_synth_render(S, buf, 4096);
    clock_gettime(CLOCK_MONOTONIC, &b);
    ns = (b.tv_sec - a.tv_sec) * 1e9 + (b.tv_nsec - a.tv_nsec);
    printf("  8 voices x (2 osc x 3 unison, LP24, sub, bend warp): %.0f ns per voice-sample, %.1fx realtime for 8 voices\n",
           ns / S->cpu_samples, (double)blocks * 4096 / WT_FS / (ns / 1e9));
    {   /* the same with everything in the effects on: distortion, compressor, tape delay with ping-pong, reverb */
        double ns2;
        P("FX_DIST", 3); P("FX_COMP", 80); P("RV_SEND", 60); P("RV_LEVEL", 100); P("DL_SEND", 60); P("DL_TYPE", 1); P("DL_PING", 1);
        wt_synth_render(S, buf, 4096);
        clock_gettime(CLOCK_MONOTONIC, &a);
        for (i = 0; i < blocks; i++) wt_synth_render(S, buf, 4096);
        clock_gettime(CLOCK_MONOTONIC, &b);
        ns2 = (b.tv_sec - a.tv_sec) * 1e9 + (b.tv_nsec - a.tv_nsec);
        printf("  with all the effects on: %.1f%% more time (%.0f ns per output frame for the effects)\n", 100 * (ns2 - ns) / ns,
               (ns2 - ns) / ((double)blocks * 4096));
    }
}

int main(int argc, char **argv)
{
    int i, matrix = 0, update = 0;
    const char *golden = NULL;
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--interp") && i + 1 < argc) wt_cfg.interp = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--bwdiv") && i + 1 < argc) wt_cfg.bwdiv = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--taper") && i + 1 < argc) wt_cfg.taper = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--morph") && i + 1 < argc) wt_cfg.morph = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--matrix")) matrix = 1;
        else if (!strcmp(argv[i], "-v")) verbose = 1;
        else if (!strcmp(argv[i], "--master") && i + 1 < argc) master_override = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--golden") && i + 1 < argc) golden = argv[++i];
        else if (!strcmp(argv[i], "--update")) update = 1;
    }
    S = (wt_synth_t *)calloc(1, sizeof *S);
    wt_global_init();
    wt_gen_basic_shapes(&tb[0]); wt_gen_fold_sine(&tb[1]); wt_gen_odd_even(&tb[2]); wt_gen_samples(smp);
    printf("WT_N=%d interp=%d bwdiv=%d taper=%d morph=%d\n", WT_N, wt_cfg.interp, wt_cfg.bwdiv, wt_cfg.taper, wt_cfg.morph);
    if (matrix) { test_alias(); return 0; }
    test_tables();
    test_voices();
    test_filter();
    test_unison_warp();
    test_spectral_morph();
    test_table_morph();
    test_fm();
    test_sub();
    test_noise();
    test_sample_layer();
    test_routing();
    test_fx();
    test_seq();
    test_alias();
    test_golden(golden, update);
    bench();
    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
