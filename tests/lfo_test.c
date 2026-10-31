/* SPDX-License-Identifier: GPL-3.0-only */
/* The drawn LFO shapes: the tables the breakpoints become, and the LFO reading them. */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "wt_core.h"
static double gz(const int16_t *x, int n, double f)      /* Goertzel magnitude of f Hz (mono, 44.1 kHz) */
{
    double w = 2 * M_PI * f / 44100.0, c = 2 * cos(w), s1 = 0, s2 = 0; int i;
    for (i = 0; i < n; i++) { double s0 = x[i] + c * s1 - s2; s2 = s1; s1 = s0; }
    return sqrt(s1 * s1 + s2 * s2 - c * s1 * s2) / n;
}
static void vowel_run(int cut, double *lo, double *hi, int *pk)
{
    static wt_synth_t v; static int16_t buf[64 * 2], mono[44100]; int b, n = 0;
    static wt_table_t tab; static int have;
    if (!have) { wt_gen_saw(&tab); have = 1; }
    wt_synth_init(&v);
    wt_synth_set_table(&v, 0, &tab);
    wt_synth_set_param(&v, P_OSC(0, O_TABLE), 0); wt_synth_set_param(&v, P_OSC(0, O_WTPOS), 0);
    wt_synth_set_param(&v, P_F_TYPE, 6); wt_synth_set_param(&v, P_F_CUTOFF, cut); wt_synth_set_param(&v, P_F_RESO, 600); wt_synth_set_param(&v, P_F_MIX, 1000);
    wt_synth_note_on(&v, 45, 110);
    *pk = 0;
    for (b = 0; b < 1000 && n + 64 <= 44100; b++) { int i; wt_synth_render(&v, buf, 64); if (b < 100) continue; for (i = 0; i < 64; i++) { mono[n] = buf[2 * i]; if (abs(mono[n]) > *pk) *pk = abs(mono[n]); n++; } }
    *lo = gz(mono, n, 770); *hi = gz(mono, n, 2310);
}
static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("  FAIL: " __VA_ARGS__); printf("\n"); } } while (0)
int main(void)
{
    wt_lfoshape_t sh; int16_t tb[WT_LTAB]; int k, i;
    static wt_synth_t s;
    wt_global_init();
    wt_synth_init(&s);
    for (k = 0; k < WT_NUSHAPE; k++) {
        wt_lfoshape_default(k, &sh); wt_lfoshape_table(&sh, tb);
        CHECK(sh.n >= 2 && sh.n <= WT_SHP_PTS, "shape %d has %d points", k, sh.n);
        for (i = 0; i < WT_LTAB; i++) CHECK(tb[i] >= -32767 && tb[i] <= 32767, "shape %d sample %d = %d", k, i, tb[i]);
    }
    wt_lfoshape_default(0, &sh); wt_lfoshape_table(&sh, tb);       /* SHARK FIN: starts low, climbs, peaks near 88 %, drops */
    CHECK(tb[0] < -32000, "fin start %d", tb[0]);
    for (i = 1; i < 110; i++) CHECK(tb[i] >= tb[i - 1], "fin not climbing at %d", i);
    CHECK(tb[110] > 20000 && tb[127] < tb[110], "fin peak %d end %d", tb[110], tb[127]);
    CHECK(tb[55] < 0, "a negative bend should be slow to start (%d)", tb[55]);   /* c < 0 lags the straight line */
    wt_lfoshape_default(3, &sh); wt_lfoshape_table(&sh, tb);       /* PULSE 25 */
    CHECK(tb[10] > 32000 && tb[100] < -32000, "pulse %d %d", tb[10], tb[100]);
    /* the LFO reads it: LFO1 on shape 7 at its fastest rate moves the value */
    wt_synth_set_shape(&s, 0, &sh);
    wt_synth_set_param(&s, P_LFO(0, 0), 7); wt_synth_set_param(&s, P_LFO(0, 1), 800);
    {   int16_t l[256]; int lo = 32767, hi = -32767, b;
        wt_synth_note_on(&s, 60, 100);
        for (b = 0; b < 400; b++) { wt_synth_render(&s, l, 64); if (s.lfo_val[0] < lo) lo = s.lfo_val[0]; if (s.lfo_val[0] > hi) hi = s.lfo_val[0]; }
        CHECK(hi > 25000 && lo < -25000, "lfo range %d .. %d", lo, hi);
    }
    {   double aL, aH, iL, iH; int pa, pi;                       /* VOWEL: A is strong near 750 Hz, I near 2300 Hz and weak at 750 */
        vowel_run(0, &aL, &aH, &pa); vowel_run(512 * 1350 / 1024, &iL, &iH, &pi);
        CHECK(aL > 3 * iL, "vowel A / I at 750 Hz: %g vs %g", aL, iL);
        CHECK(iH > 1.5 * aH, "vowel I / A at 2300 Hz: %g vs %g", iH, aH);
        CHECK(pa > 200 && pa < 32000 && pi > 200 && pi < 32000, "vowel level %d %d", pa, pi);
    }
    {   /* the chorus: off is the plain sound; on, the output changes, stays in range and the delay line is not read before it was written */
        static wt_synth_t a, c; static wt_table_t tab; static int16_t oa[128], oc[128]; int b, diff = 0, pk = 0, i;
        CHECK(wt_param_find("CH_MIX") == P_CH_MIX && wt_param_find("CH_RATE") == P_CH_RATE && wt_param_find("CH_DEPTH") == P_CH_DEPTH, "chorus parameter names");
        wt_gen_saw(&tab);
        wt_synth_init(&a); wt_synth_init(&c);
        wt_synth_set_table(&a, 0, &tab); wt_synth_set_table(&c, 0, &tab);
        wt_synth_set_param(&c, P_CH_MIX, 1000); wt_synth_set_param(&c, P_CH_DEPTH, 1000); wt_synth_set_param(&c, P_CH_RATE, 700);
        wt_synth_note_on(&a, 57, 110); wt_synth_note_on(&c, 57, 110);
        for (b = 0; b < 400; b++) {
            wt_synth_render(&a, oa, 64); wt_synth_render(&c, oc, 64);
            if (b > 100) for (i = 0; i < 128; i++) { if (oa[i] != oc[i]) diff++; if (abs(oc[i]) > pk) pk = abs(oc[i]); }
        }
        CHECK(diff > 5000, "chorus changed only %d samples", diff);
        CHECK(pk > 500 && pk < 32767, "chorus peak %d", pk);
    }
    printf("lfo_test: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
