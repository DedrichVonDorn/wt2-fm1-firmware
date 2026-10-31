/* SPDX-License-Identifier: GPL-3.0-only */
/* WT-2 only: chord headroom, the gain rider, unison on the top note, and the CPU governor.
 * Also prints host CPU (fraction of realtime) and the synth object size. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "wt_core.h"

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("  FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

static wt_table_t gtab;
static wt_synth_t synth;

static int iabs(int x) { return x < 0 ? -x : x; }

static void base(wt_synth_t *s)
{
    wt_synth_init(s);
    wt_synth_set_table(s, 0, &gtab);
    wt_synth_set_param(s, P_OSC(0, O_TABLE), 0);
    wt_synth_set_param(s, P_OSC(0, O_ON), 1);
    wt_synth_set_param(s, P_OSC(0, O_RAND), 0);
    wt_synth_set_param(s, P_OSC(0, O_DETUNE), 0);
    wt_synth_set_param(s, P_OSC(0, O_UNI), 1);
    wt_synth_set_param(s, P_OSC(1, O_ON), 0);
    wt_synth_set_param(s, P_F_TYPE, 0);
    wt_synth_set_param(s, P_SYN_DIST, 0);
    wt_synth_set_param(s, P_DR_DIST, 0);
    wt_synth_set_param(s, P_DR_KIT, 0);
    wt_synth_set_param(s, P_CH_MIX, 0);
    wt_synth_set_param(s, wt_param_find("FX_COMP"), 0);
    wt_synth_set_param(s, P_CPUMODE, 0);
}

/* peak and hard-clip count over `frames` samples, ignoring the first `skip` frames */
static void render_notes(wt_synth_t *s, const int *notes, int nn, int frames, int skip, int *peak, int *clips)
{
    int16_t buf[512];                                          /* stereo: two samples per frame */
    int i, done = 0;
    *peak = 0; *clips = 0;
    for (i = 0; i < nn; i++) wt_synth_note_on(s, notes[i], 110);
    while (done + 16 <= frames) {
        int m = frames - done > 256 ? 256 : frames - done;
        m -= m % 16;
        wt_synth_render(s, buf, m);
        if (done + m > skip) {
            int from = done >= skip ? 0 : (skip - done) * 2;
            for (i = from; i < m * 2; i++) {
                int a = iabs(buf[i]);
                if (a > *peak) *peak = a;
                if (a >= 32767) (*clips)++;
            }
        }
        done += m;
    }
}

static int voice_of(const wt_synth_t *s, int note)
{
    int i;
    for (i = 0; i < WT_NVOICES; i++) if (s->v[i].active && s->v[i].note == note) return i;
    return -1;
}

/* seconds of CPU per second of audio */
static double cpu_load(void (*setup)(wt_synth_t *), int audio_frames)
{
    int16_t buf[512];
    int done = 0;
    clock_t t0, t1;
    setup(&synth);
    t0 = clock();
    while (done + 256 <= audio_frames) { wt_synth_render(&synth, buf, 256); done += 256; }
    t1 = clock();
    return ((double)(t1 - t0) / (double)CLOCKS_PER_SEC) / ((double)done / 44100.0);
}

static void setup_one(wt_synth_t *s) { int n = 60; base(s); wt_synth_note_on(s, n, 110); }
static void setup_four(wt_synth_t *s)
{
    int i, n[4] = {48, 55, 60, 64};
    base(s);
    wt_synth_set_param(s, P_OSC(0, O_UNI), 5);
    wt_synth_set_param(s, P_OSC(1, O_ON), 1);
    wt_synth_set_param(s, P_OSC(1, O_UNI), 5);
    wt_synth_set_param(s, P_OSC(1, O_RAND), 0);
    wt_synth_set_param(s, P_F_TYPE, 1);
    wt_synth_set_param(s, P_F_DRIVE, 700);
    for (i = 0; i < 4; i++) wt_synth_note_on(s, n[i], 110);
}
static void setup_chorus(wt_synth_t *s) { setup_four(s); wt_synth_set_param(s, P_CH_MIX, 800); wt_synth_set_param(s, P_CH_DEPTH, 800); }
static void setup_hyper(wt_synth_t *s) { setup_four(s); wt_synth_set_param(s, P_CH_MIX, 0); wt_synth_set_param(s, P_DR_DIST, 7); }
static void setup_dist(wt_synth_t *s) { setup_four(s); wt_synth_set_param(s, P_SYN_DIST, 2); wt_synth_set_param(s, P_SYN_DRIVE, 90); }
static void setup_comp(wt_synth_t *s) { setup_four(s); wt_synth_set_param(s, wt_param_find("FX_COMP"), 90); }
static void setup_pwm(wt_synth_t *s) { setup_four(s); wt_synth_set_param(s, P_OSC(0, O_WARPMODE), 6); wt_synth_set_param(s, P_OSC(0, O_WARP), 700); }

int main(void)
{
    int one_pk, four_pk, loud_pk, clips, i;
    const int same = 60, chord[4] = {60, 60, 60, 60};
    wt_global_init();
    wt_gen_saw(&gtab);
    printf("sizeof wt_synth_t %zu  wt_voice_t %zu  (host, WT-2)\n", sizeof(wt_synth_t), sizeof(wt_voice_t));
    base(&synth);
    CHECK(P_COUNT == 206, "param count %d", P_COUNT);
    CHECK(wt_param_desc[P_OSC(0, O_WARPMODE)].max == 6, "warp max %d", wt_param_desc[P_OSC(0, O_WARPMODE)].max);
    CHECK(wt_param_find("UNI_TOP") == P_DR_KIT && wt_param_find("HYPER") == P_DR_DIST, "renamed slots");
    CHECK(!strcmp(wt_dst_names[D_CHMIX], "CH_MIX") && D_MASTER + 1 == D_COUNT, "destinations");
    wt_synth_set_param(&synth, P_MASTER, 350);
    wt_synth_set_param(&synth, P_OSC(0, O_LEVEL), 1000);
    render_notes(&synth, &same, 1, 12000, 1000, &one_pk, &clips);
    base(&synth);
    wt_synth_set_param(&synth, P_MASTER, 350);
    wt_synth_set_param(&synth, P_OSC(0, O_LEVEL), 1000);
    render_notes(&synth, chord, 4, 12000, 1000, &four_pk, &clips);
    printf("headroom: 1 note peak %d, 4 coherent notes peak %d, hard clips %d\n", one_pk, four_pk, clips);
    CHECK(one_pk > 400 && four_pk <= 32767 && clips == 0, "4-note peak %d clips %d", four_pk, clips);
    CHECK(four_pk * 10 < one_pk * 26 && four_pk * 10 > one_pk * 12, "4-note / 1-note = %d/%d (want about 2)", four_pk, one_pk);

    base(&synth);
    wt_synth_set_param(&synth, P_MASTER, 1000);
    wt_synth_set_param(&synth, P_OSC(0, O_LEVEL), 1000);
    wt_synth_set_param(&synth, P_OSC(0, O_UNI), 5);
    wt_synth_set_param(&synth, P_OSC(1, O_ON), 1);
    wt_synth_set_param(&synth, P_OSC(1, O_LEVEL), 1000);
    wt_synth_set_param(&synth, P_OSC(1, O_UNI), 5);
    render_notes(&synth, chord, 4, 20000, 6000, &loud_pk, &clips);
    printf("rider: sustained peak %d, samples on the rail %d, gain %d\n", loud_pk, clips, synth.lim_g);
    CHECK(loud_pk <= 32767 && clips < 8 && loud_pk < 30000 && loud_pk > 16000, "rider peak %d clips %d gain %d", loud_pk, clips, synth.lim_g);

    /* extra warp modes play and stay inside the converter */
    for (i = 4; i <= 6; i++) {
        int pk, cl;
        base(&synth);
        wt_synth_set_param(&synth, P_OSC(0, O_WARPMODE), i);
        wt_synth_set_param(&synth, P_OSC(0, O_WARP), 800);
        render_notes(&synth, &same, 1, 4000, 500, &pk, &cl);
        CHECK(pk > 200 && pk <= 32767 && cl == 0, "warp mode %d peak %d clips %d", i, pk, cl);
    }
    {   int dry, wet, cl;
        base(&synth);
        render_notes(&synth, &same, 1, 8000, 2000, &dry, &cl);
        base(&synth);
        wt_synth_set_param(&synth, P_DR_DIST, 7);
        render_notes(&synth, &same, 1, 8000, 2000, &wet, &cl);
        printf("hyper: dry %d wet %d\n", dry, wet);
        CHECK(wet > 200 && wet <= 32767 && wet != dry, "hyper peak %d (dry %d)", wet, dry);
    }

    /* UNI_TOP: only the highest note keeps the full unison */
    base(&synth);
    wt_synth_set_param(&synth, P_DR_KIT, 1);
    wt_synth_set_param(&synth, P_OSC(0, O_UNI), 5);
    {   int notes[2] = {48, 72}, lo, hi, pk, cl;
        render_notes(&synth, notes, 2, 64, 0, &pk, &cl);
        lo = voice_of(&synth, 48); hi = voice_of(&synth, 72);
        printf("uni_top: low lanes %d, high lanes %d\n", lo >= 0 ? wt_voice_lanes(&synth, lo) : -1, hi >= 0 ? wt_voice_lanes(&synth, hi) : -1);
        CHECK(lo >= 0 && hi >= 0 && wt_voice_lanes(&synth, hi) > wt_voice_lanes(&synth, lo), "UNI_TOP lanes low %d high %d", lo >= 0 ? wt_voice_lanes(&synth, lo) : -1, hi >= 0 ? wt_voice_lanes(&synth, hi) : -1);
        CHECK(wt_voice_lanes(&synth, hi) == 5 && wt_voice_lanes(&synth, lo) == 1, "UNI_TOP spread");
    }

    /* overload: shed unison on the newest notes; every note still sounds */
    base(&synth);
    wt_synth_set_param(&synth, P_CPUMODE, 2);
    wt_synth_set_param(&synth, P_OSC(0, O_UNI), 5);
    wt_synth_set_param(&synth, P_OSC(1, O_ON), 1);
    wt_synth_set_param(&synth, P_OSC(1, O_UNI), 5);
    {   int notes[4] = {48, 52, 55, 60}, pk, cl, nact = 0, oldest = -1, newest = -1;
        uint32_t oage = 0xFFFFFFFFu, nage = 0;
        for (i = 0; i < 4; i++) wt_synth_note_on(&synth, notes[i], 100);
        for (i = 0; i < 6; i++) wt_synth_load(&synth, 90);
        render_notes(&synth, notes, 0, 64, 0, &pk, &cl);   /* notes already started */
        for (i = 0; i < WT_NVOICES; i++) if (synth.v[i].active) {
            nact++;
            if (synth.v[i].age < oage) { oage = synth.v[i].age; oldest = i; }
            if (synth.v[i].age >= nage) { nage = synth.v[i].age; newest = i; }
        }
        printf("governor: budget %d, oldest lanes %d, newest lanes %d, voices %d\n",
               synth.lane_budget, oldest >= 0 ? wt_voice_lanes(&synth, oldest) : -1, newest >= 0 ? wt_voice_lanes(&synth, newest) : -1, nact);
        CHECK(nact == 4 && oldest >= 0 && newest >= 0, "voices kept %d", nact);
        CHECK(wt_voice_lanes(&synth, oldest) >= wt_voice_lanes(&synth, newest), "oldest keeps lanes");
        CHECK(wt_voice_lanes(&synth, newest) >= 1 && wt_voice_lanes(&synth, newest) < 10, "newest shed to %d", newest >= 0 ? wt_voice_lanes(&synth, newest) : -1);
    }

    {   /* host CPU, about 4 seconds of audio each. Printed for the report; not a pass/fail (the machine varies). */
        const int N = 44100 * 4;
        struct { const char *name; void (*fn)(wt_synth_t *); } c[] = {
            {"1 note, features off", setup_one},
            {"4-note heavy (uni 5+5, filter drive)", setup_four},
            {"heavy + chorus", setup_chorus},
            {"heavy + hyper", setup_hyper},
            {"heavy + synth dist", setup_dist},
            {"heavy + compressor", setup_comp},
            {"heavy + PWM warp", setup_pwm},
        };
        for (i = 0; i < 7; i++) printf("cpu %-40s %6.2f%% of realtime\n", c[i].name, 100.0 * cpu_load(c[i].fn, N));
    }
    printf("chord_test: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
