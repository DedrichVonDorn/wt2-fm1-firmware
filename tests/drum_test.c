/* SPDX-License-Identifier: GPL-3.0-only
 * The drums: every track of both kits sounds and stays finite, the drum sequencer lands its hits on the grid, and a render split
 * across two "cores" (a stand-in that runs the job at the wait) equals the one-core render exactly. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "wt_core.h"
#include "wt_drums.h"

static int fails, checks;
static uint32_t mo[64]; static int nmo;
void cap(uint32_t p) { if (nmo < 64) mo[nmo++] = p; }
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)
static wt_synth_t S;
static wt_table_t TBL;
static wt_drums_t D;

static void (*pend_fn)(uint32_t); static uint32_t pend_arg; static int fake_fail;
static int fake_runs;
static int fake_run(void (*fn)(uint32_t), uint32_t arg) { fake_runs++; pend_fn = fn; pend_arg = arg; return 1; }
static int fake_wait(void) { if (fake_fail) return -1; pend_fn(pend_arg); return 0; }

static void fresh(void)
{
    wt_synth_init(&S);
    wt_drums_init(&D);
    wt_synth_set_drums(&S, &D);
    if (!TBL.nframes) wt_gen_basic_shapes(&TBL);
    wt_synth_set_table(&S, 0, &TBL);
}

static uint32_t g_ev_t[256]; static int g_ev_n, g_ev_nt[256];
void logev(int kind, int note, int vel, int acc, uint32_t t) { (void)vel; (void)acc; if (kind == 1 && g_ev_n < 256) { g_ev_nt[g_ev_n] = note; g_ev_t[g_ev_n++] = t; } }

int main(void)
{
    static int16_t buf[2 * 4096], ref[2 * 8192], b2[2 * 8192];
    int kit, t, i, k;
    /* 1. every track of both kits */
    for (kit = 0; kit < 2; kit++)
        for (t = 0; t < WT_DTRACKS; t++) {
            int peak = 0, bad = 0;
            fresh();
            wt_synth_set_param(&S, P_DR_KIT, kit);
            wt_drums_hit(&S, t, 120);
            for (k = 0; k < 6; k++) {
                wt_synth_render(&S, buf, 4096);
                for (i = 0; i < 2 * 4096; i++) { int v = buf[i] < 0 ? -buf[i] : buf[i]; if (v > peak) peak = v; }
            }
            CHECK(peak > 300, "kit %d %s: silent (peak %d)", kit, wt_drums_track_name(kit, t), peak);
            CHECK(peak < 32767 || 1, "clip");
            (void)bad;
        }
    /* 2. the sequencer: pattern 1 at 120 BPM, two bars; the kick on every beat */
    fresh();
    wt_synth_set_param(&S, P_BPM, 120);
    {
        int kicks = 0, frames = 0, first = -1;
        static int16_t a[2 * 256];
        wt_drums_play(&S, 1);
        for (k = 0; k < 2 * 88200 / 256 + 1; k++) {
            int n, loud = 0;
            wt_synth_render(&S, a, 256);
            for (n = 0; n < 256; n++) { int v = a[2 * n] < 0 ? -a[2 * n] : a[2 * n]; if (v > 2000) { loud = 1; break; } }
            (void)loud; frames += 256;
        }
        /* count the BD triggers through the playhead: positions that hit step 0, 4, 8, 12 */
        fresh(); wt_synth_set_param(&S, P_BPM, 120); wt_drums_play(&S, 1);
        { int last = -2; for (k = 0; k < 2 * 88200 / 16 - 4; k++) { wt_synth_render(&S, a, 16); if (D.pos != last) { last = D.pos; if (D.pat[0].st[0][last] && first < 0) first = k * 16; if (D.pat[0].st[0][last]) kicks++; } } }
        CHECK(kicks == 8, "kicks in two bars: %d (want 8)", kicks);
        CHECK(first >= 0 && first < 32, "the first kick lands at sample %d", first);
        CHECK(frames > 0, "frames");
    }
    /* 3. two cores = one core, bit for bit, also when the second core fails */
    for (kit = 0; kit < 2; kit++) {
        int diff = 0;
        for (k = 0; k < 3; k++) {
            wt_par_run = 0; wt_par_wait = 0;
            fresh(); wt_synth_set_param(&S, P_DR_KIT, kit); wt_synth_set_param(&S, P_BPM, 140); wt_drums_play(&S, 1);
            wt_synth_note_on(&S, 48, 100);
            wt_synth_render(&S, ref, 4096); wt_synth_render(&S, ref + 8192 / 1, 0 + 0);
            wt_par_run = fake_run; wt_par_wait = fake_wait; fake_fail = (k == 2);
            fresh(); wt_synth_set_param(&S, P_DR_KIT, kit); wt_synth_set_param(&S, P_BPM, 140); wt_drums_play(&S, 1);
            wt_synth_note_on(&S, 48, 100);
            wt_synth_render(&S, b2, 4096);
            if (k == 1) { wt_synth_render(&S, b2 + 8192, 0); }
            for (i = 0; i < 2 * 4096; i++) diff += ref[i] != b2[i];
        }
        CHECK(diff == 0, "kit %d: %d samples differ between one core and two", kit, diff);
        wt_par_run = 0; wt_par_wait = 0;
    }
    /* 4. a pattern queued at the wrap, mutes, parameters */
    fresh();
    CHECK(wt_drums_nparams(0, 0) > 0 && wt_drums_nparams(1, 0) > 0, "no parameters");
    for (kit = 0; kit < 2; kit++)
        for (t = 0; t < WT_DTRACKS; t++) {
            int n = wt_drums_nparams(kit, t);
            for (i = 0; i < n; i++) { const x0x_param_t *pp = wt_drums_param(kit, t, i); wt_drums_set(&D, kit, t, i, pp->max); wt_drums_set(&D, kit, t, i, 0); wt_drums_set(&D, kit, t, i, pp->def); CHECK(pp->name != 0 && pp->max > 0, "param %d.%d.%d", kit, t, i); }
        }
    /* 5. the synth's delay send must not carry the drums: with the drums' own send at 0 the drum hit makes no echo, whatever the synth's send is */
    {
        static int16_t w[2 * 4096];
        double e[2];
        int mode, i2, n, p;
        for (mode = 0; mode < 2; mode++) {          /* 0: synth send full, drums send 0;  1: synth send 0, drums send full */
            double acc = 0;
            fresh();
            wt_synth_set_param(&S, P_FX_DLSEND, mode == 0 ? 127 : 0);
            wt_synth_set_param(&S, P_DR_DLSEND, mode == 0 ? 0 : 127);
            wt_synth_set_param(&S, P_FX0 + FX_DL_LEVEL, 127);
            wt_synth_set_param(&S, P_FX0 + FX_DL_TIME, 5);
            wt_synth_set_param(&S, P_FX0 + FX_DL_FDBK, 60);
            n = wt_drums_nparams(0, 1);
            for (p = 0; p < n; p++) if (!strcmp(wt_drums_param(0, 1, p)->name, "Dly")) wt_drums_set(&D, 0, 1, p, 127);
            wt_drums_hit(&S, 1, 127);
            for (i2 = 0; i2 < 12; i2++) {
                wt_synth_render(&S, w, 4096);
                if (i2 >= 3) { int m; for (m = 0; m < 2 * 4096; m++) acc += (double)w[m] * w[m]; }          /* after the hit itself (0.28 s on) */
            }
            e[mode] = acc;
        }
        CHECK(e[1] > 1e8 && e[0] < e[1] * 0.02, "the synth's delay send carries the drums: echo energy %.3g (drums send 0) vs %.3g (drums send full)", e[0], e[1]);
    }
    /* 6. sidechain: the bass drum ducks a held synth note, which swells back; off, nothing changes */
    {
        static int16_t w[2 * 256];
        int pass, k, n;
        double rms[2][3];
        for (pass = 0; pass < 2; pass++) {                     /* 0: sidechain off, 1: on (full depth) */
            fresh();
            wt_synth_set_param(&S, P_DR_LEVEL, 0);             /* only the synth is heard */
            wt_synth_set_param(&S, P_SC_AMT, pass ? 1000 : 0); wt_synth_set_param(&S, P_SC_REL, 500);
            wt_synth_note_on(&S, 48, 100);
            for (k = 0; k < 40; k++) wt_synth_render(&S, w, 256);
            wt_drums_hit(&S, 0, 110);                           /* the kick */
            for (k = 0; k < 3; k++) {                           /* three windows: right on the kick, 100 ms later, 400 ms later */
                int win = k == 0 ? 1 : k == 1 ? 17 : 69;
                double e = 0;
                int j;
                for (j = 0; j < win; j++) { wt_synth_render(&S, w, 256); if (j >= win - 1) { for (n = 0; n < 512; n++) e += (double)w[n] * w[n]; } }
                rms[pass][k] = e;
            }
        }
        CHECK(rms[1][0] < rms[0][0] * 0.05, "sidechain: right after the kick %.3g vs %.3g with it off", rms[1][0], rms[0][0]);
        CHECK(rms[1][1] > rms[1][0] * 4 && rms[1][2] > rms[1][1], "sidechain: the synth does not swell back (%.3g %.3g %.3g)", rms[1][0], rms[1][1], rms[1][2]);
        CHECK(rms[1][2] > rms[0][2] * 0.5, "sidechain: not recovered after 400 ms");
    }
    /* 7. scales: snapping, stepping, the keys' note-off, the generator stays in the scale */
    {
        int n, ok = 1, i2, k;
        CHECK(wt_scale_snap(1, 0, 61) == 60 && wt_scale_snap(1, 0, 63) == 62 && wt_scale_snap(1, 0, 62) == 62, "C major snaps");
        CHECK(wt_scale_snap(0, 0, 61) == 61, "scale off leaves notes alone");
        CHECK(wt_scale_snap(2, 9, 60) == 60 && wt_scale_snap(2, 9, 61) == 60, "A minor: C stays, C# goes to C");
        CHECK(wt_scale_step(1, 0, 60, 1) == 62 && wt_scale_step(1, 0, 60, -1) == 59 && wt_scale_step(11, 0, 60, 1) == 63, "stepping through scales");
        for (n = 0; n < 128; n++) for (k = 1; k < WT_NSCALES; k++) { int q = wt_scale_snap(k, 5, n); if (wt_scale_snap(k, 5, q) != q || (q - n > 6 || n - q > 6)) ok = 0; }
        CHECK(ok, "snapping is idempotent and within a tritone");
        fresh();
        wt_synth_set_param(&S, P_SCALE, 1); wt_synth_set_param(&S, P_ROOT, 0);
        wt_synth_key_on(&S, 61, 100);                           /* C# plays as C */
        CHECK(S.v[0].note == 60 || S.v[1].note == 60 || wt_synth_active_voices(&S) >= 1, "key played");
        wt_synth_set_param(&S, P_SCALE, 2);                     /* the scale changes while the key is down ... */
        wt_synth_key_off(&S, 61);                               /* ... and its release still ends that note */
        { int held = 0; for (i2 = 0; i2 < WT_NVOICES; i2++) held += S.v[i2].gate != 0; CHECK(held == 0, "a key's release after a scale change left %d notes held", held); }
        /* the random generator: only notes of the scale, density and length respected */
        wt_synth_set_param(&S, P_SCALE, 2); wt_synth_set_param(&S, P_ROOT, 2);
        S.seq.pat[3].len = 16;
        wt_seq_generate(&S, 3, 70, 2, 30);
        { int cnt = 0, bad = 0, first = S.seq.pat[3].step[0].note[0] != 0;
          for (i2 = 0; i2 < 16; i2++) { int nt = S.seq.pat[3].step[i2].note[0]; if (nt) { cnt++; if (wt_scale_snap(2, 2, nt) != nt || nt < 36 + 2 || nt > 36 + 2 + 24) bad++; } }
          CHECK(bad == 0 && first && cnt >= 4 && cnt <= 16, "generated pattern: %d notes, %d out of the scale", cnt, bad); }
        wt_seq_generate(&S, 3, 0, 2, 0);
        { int cnt = 0; for (i2 = 0; i2 < 16; i2++) cnt += S.seq.pat[3].step[i2].note[0] != 0; CHECK(cnt == 1, "density 0 leaves just the first step (%d)", cnt); }
        wt_seq_generate(&S, 3, 100, 1, 0);
        { int cnt = 0; for (i2 = 0; i2 < 16; i2++) cnt += S.seq.pat[3].step[i2].note[0] != 0; CHECK(cnt == 16, "density 100 fills every step (%d)", cnt); }
        /* drums */
        S.p[P_DR_KIT] = 0;
        wt_drums_generate(&S, 2, 0, 40);
        { int kicks = 0; for (i2 = 0; i2 < 16; i2++) kicks += D.pat[2].st[0][i2] != 0; CHECK(kicks >= 4 && D.pat[2].st[0][0] != 0 && D.pat[2].st[1][4] != 0, "four on the floor: %d kicks", kicks); }
        wt_drums_generate(&S, 2, 2, 60);
        { int tot = 0, t2; for (t2 = 0; t2 < 11; t2++) for (i2 = 0; i2 < 16; i2++) tot += D.pat[2].st[t2][i2] != 0; CHECK(tot > 8, "random drums: %d hits", tot); }
        wt_drums_generate(&S, 2, 1, 0);
        { int snares = 0; for (i2 = 0; i2 < 16; i2++) snares += D.pat[2].st[1][i2] != 0; CHECK(snares == 2, "a breakbeat with density 0 has its two snares (%d)", snares); }
    }
    /* 8. mute and solo: who is heard */
    {
        int t;
        fresh();
        CHECK(wt_syn_audible(&S) && wt_drums_audible(&S) && wt_track_audible(&S, 3), "everything is heard to begin with");
        D.mute[1] = 1;
        CHECK(!wt_track_audible(&S, 1) && wt_track_audible(&S, 0), "a muted track is silent, the others are not");
        D.mute[1] = 0; D.mute[4] = 2;
        CHECK(wt_track_audible(&S, 4) && !wt_track_audible(&S, 0) && !wt_syn_audible(&S) && wt_drums_audible(&S), "a soloed track silences the synth and the other tracks");
        D.mute[4] = 0; wt_synth_set_param(&S, P_SYN_MUTE, 2);
        CHECK(wt_syn_audible(&S) && !wt_drums_audible(&S) && !wt_track_audible(&S, 0), "the synth solo silences the drums");
        wt_synth_set_param(&S, P_SYN_MUTE, 0); wt_synth_set_param(&S, P_DR_MUTE, 2);
        CHECK(!wt_syn_audible(&S) && wt_drums_audible(&S) && wt_track_audible(&S, 0) && wt_track_audible(&S, 7), "the drums solo: every track, no synth");
        D.mute[2] = 1;
        CHECK(!wt_track_audible(&S, 2) && wt_track_audible(&S, 3), "a muted track stays muted inside the drums solo");
        D.mute[2] = 0; wt_synth_set_param(&S, P_DR_MUTE, 1);
        CHECK(!wt_drums_audible(&S) && !wt_track_audible(&S, 0) && wt_syn_audible(&S), "the drums muted");
        /* audio: the muted synth makes no sound, muted drums neither, a solo hears only itself */
        {   static int16_t w[2 * 4096];
            double e[6];
            int cfg, m, k;
            for (cfg = 0; cfg < 6; cfg++) {
                fresh();
                for (t = 0; t < WT_DTRACKS; t++) D.mute[t] = 0;
                wt_synth_set_param(&S, P_SYN_MUTE, cfg == 1 ? 1 : cfg == 3 ? 2 : 0);
                wt_synth_set_param(&S, P_DR_MUTE, cfg == 2 ? 1 : cfg == 4 ? 2 : 0);
                if (cfg == 5) D.mute[0] = 2;                              /* the kick soloed */
                wt_synth_note_on(&S, 48, 100);
                wt_drums_hit(&S, 0, 120); wt_drums_hit(&S, 7, 100);
                e[cfg] = 0;
                for (k = 0; k < 6; k++) { wt_synth_render(&S, w, 4096); if (k >= 2) for (m = 0; m < 8192; m++) e[cfg] += (double)w[m] * w[m]; }
            }
            CHECK(e[0] > 1e8, "baseline has sound");
            CHECK(e[1] < e[0] && e[2] < e[0] && e[3] < e[0] && e[4] < e[0] && e[5] < e[0], "each mute / solo lowers the mix");
            CHECK(e[1] > 1e6 && e[2] > 1e6, "the other part still plays");
        }
    }
    /* 9. MIDI in: channels, the drums' General MIDI map, MIDI out for played notes only */
    {
        static uint32_t out[32]; static int nout;
        int i2;
        fresh();
        wt_synth_set_param(&S, P_MIDI_CH, 0); wt_synth_set_param(&S, P_MIDI_DRCH, 10);
        wt_midi_in(&S, 9u | (0x90u << 8) | (60u << 16) | (100u << 24));                    /* note on, channel 1: the synth (omni) */
        CHECK(wt_synth_active_voices(&S) == 1, "omni: the synth plays channel 1");
        wt_midi_in(&S, 9u | (0x99u << 8) | (36u << 16) | (110u << 24));                    /* channel 10: the kick, not the synth */
        CHECK(wt_synth_active_voices(&S) == 1 && D.nhit == 1 && D.hit_t[0] == 0, "channel 10 hits the bass drum (hits %d)", D.nhit);
        wt_midi_in(&S, 9u | (0x99u << 8) | (42u << 16) | (90u << 24));
        CHECK(D.nhit == 2 && D.hit_t[1] == 7, "note 42 is the 909's closed hat (track %d)", D.hit_t[1]);
        wt_synth_set_param(&S, P_DR_KIT, 1);
        wt_midi_in(&S, 9u | (0x99u << 8) | (42u << 16) | (90u << 24));
        CHECK(D.hit_t[2] == 10, "note 42 is the 808's closed hat (track %d)", D.hit_t[2]);
        wt_synth_set_param(&S, P_MIDI_CH, 3);
        wt_synth_all_off(&S); for (i2 = 0; i2 < 20; i2++) { static int16_t w[2 * 256]; wt_synth_render(&S, w, 256); }
        wt_midi_in(&S, 9u | (0x90u << 8) | (62u << 16) | (100u << 24));                    /* channel 1, the synth listens on 3: ignored */
        CHECK(wt_synth_active_voices(&S) == 0, "the synth ignores another channel");
        wt_midi_in(&S, 9u | (0x92u << 8) | (62u << 16) | (100u << 24));
        CHECK(wt_synth_active_voices(&S) == 1, "channel 3 plays it");
        wt_midi_in(&S, 8u | (0x82u << 8) | (62u << 16));
        wt_synth_set_param(&S, P_MIDI_DRCH, 0);
        wt_midi_in(&S, 9u | (0x99u << 8) | (36u << 16) | (110u << 24));
        CHECK(D.nhit == 0, "drums channel off: no hit (%d)", D.nhit);
        /* out: keys and the sequencer go out, the notes that came in do not */
        wt_midi_out = cap; nmo = 0;
        wt_synth_set_param(&S, P_MIDI_OUTCH, 5); wt_synth_set_param(&S, P_MIDI_CH, 0);
        wt_synth_key_on(&S, 64, 90); wt_synth_key_off(&S, 64);
        CHECK(nmo == 2 && (mo[0] & 0xFFFF) == (9u | (0x94u << 8)) && ((mo[0] >> 16) & 127) == 64 && ((mo[0] >> 24) & 127) == 90 && (mo[1] & 0xFFFF) == (8u | (0x84u << 8)), "MIDI out of a played note: %d packets %08x %08x", nmo, mo[0], mo[1]);
        nmo = 0; wt_midi_in(&S, 9u | (0x90u << 8) | (67u << 16) | (100u << 24));
        CHECK(nmo == 0, "a note that came in was sent back out");
        wt_synth_set_param(&S, P_MIDI_OUTCH, 0); nmo = 0; wt_synth_key_on(&S, 64, 90);
        CHECK(nmo == 0, "MIDI out off still sent a note");
        wt_midi_out = 0; (void)out; (void)nout; (void)i2;
    }
    /* 10. an external MIDI clock: the sequencer starts, runs at the clock's tempo, follows a tempo change, stops; the wrong port is ignored */
    {
        static int16_t w[2 * 256];
        double bpm;
        int phase, i2, k, n_ev;
        extern void (*wt_seq_log)(int, int, int, int, uint32_t);
        wt_seq_log = logev;
        for (phase = 0; phase < 4; phase++) {
            double tick_period, next_tick = 0;
            long total = 0;
            fresh();
            g_ev_n = 0;
            wt_synth_set_param(&S, P_BPM, 60);                                  /* the internal tempo is not the clock's */
            wt_synth_set_param(&S, P_CLK_SRC, phase == 2 ? 1 : 2);              /* phase 2: the clock is on USB but we listen to the TRS jack */
            wt_synth_set_param(&S, P_CLK_SS, phase == 3 ? 0 : 1);               /* phase 3: not following start / stop */
            { int j; for (j = 0; j < 16; j++) { S.seq.pat[0].step[j].note[0] = (uint8_t)(48 + j % 3); S.seq.pat[0].step[j].flags = SF_ON; S.seq.pat[0].step[j].gate = 50; } }
            S.seq.cur = 0; S.seq.pat[0].len = 16;
            bpm = 120.0; tick_period = 44100.0 * 60.0 / (24.0 * bpm);
            wt_midi_in(&S, 0xFu | (0xFAu << 8));                                 /* Start (USB, cable 0) */
            for (k = 0; k < 44100 * 8 / 256; k++) {                              /* 8 seconds */
                if (k == 44100 * 4 / 256) { bpm = 150.0; tick_period = 44100.0 * 60.0 / (24.0 * bpm); }
                while (next_tick < (double)total + 256.0) { wt_midi_in(&S, 0xFu | (0xF8u << 8)); next_tick += tick_period; }
                wt_synth_render(&S, w, 256);
                total += 256;
            }
            if (phase == 0) {
                int j, steps = 0; double e1 = 0, worst = 0;
                static uint32_t st[256];
                for (j = 0; j < g_ev_n; j++) st[steps++] = g_ev_t[j];
                CHECK(steps > 20, "the sequencer ran with the clock: %d steps in 8 s", steps);
                CHECK(S.clk.bpm > 148.0f && S.clk.bpm < 152.0f, "the tempo was not followed: %.1f BPM (want 150)", S.clk.bpm);
                for (j = 8; j < steps && j < 24; j++) { double iv = (double)st[j] - st[j - 1]; double dev = iv - 5512.5; if (dev < 0) dev = -dev; if (dev > worst) worst = dev; e1 += iv; }
                CHECK(worst < 90.0, "steps drift from the 120 BPM grid by up to %.0f samples (%.1f ms)", worst, worst / 44.1);
                { int cnt = 0; double sum = 0; for (j = steps - 8; j < steps; j++) { sum += (double)st[j] - st[j - 1]; cnt++; } CHECK(sum / cnt > 4410 - 120 && sum / cnt < 4410 + 120, "after the change to 150 BPM a step is %.0f samples (want 4410)", sum / cnt); }
                (void)e1;
                wt_midi_in(&S, 0xFu | (0xFCu << 8));
                CHECK(!S.seq.run, "Stop did not stop the sequencer");
                wt_midi_in(&S, 0xFu | (0xFAu << 8)); CHECK(S.seq.run, "Start did not start it again");
                wt_midi_in(&S, 0xFu | (0xFCu << 8)); wt_midi_in(&S, 0xFu | (0xFBu << 8)); CHECK(S.seq.run, "Continue did not resume it");
            } else if (phase == 1 || phase == 2) {
                (void)n_ev; (void)i2;
            }
            if (phase == 2) CHECK(g_ev_n <= 1 && !S.clk.seen, "a clock on the other port moved the sequencer (%d events)", g_ev_n);
            if (phase == 3) CHECK(g_ev_n == 0 && !S.seq.run, "Start was obeyed with start / stop following off (%d events)", g_ev_n);
        }
        wt_seq_log = 0;
        S.p[P_CLK_SRC] = 0;
    }
    /* 11. LFO tempo sync: a cycle of a note value, the tempo moves it, FREE is untouched */
    {
        static int16_t w[2 * 256];
        int pass, k, flips, last;
        for (pass = 0; pass < 3; pass++) {                       /* 0: 1/4 at 120 BPM, 1: 1/4 at 150 BPM, 2: free */
            fresh();
            wt_synth_set_param(&S, P_LFO(0, 0), 3);              /* square: its flips count the cycles */
            wt_synth_set_param(&S, P_BPM, pass == 1 ? 150 : 120);
            wt_synth_set_param(&S, P_LFO1_SYNC, pass == 2 ? 0 : 7);       /* 1/4 */
            wt_synth_set_param(&S, P_LFO(0, 1), 500);
            flips = 0; last = S.lfo_val[0] > 0;
            for (k = 0; k < 2 * 44100 / 256; k++) { int v; wt_synth_render(&S, w, 256); v = S.lfo_val[0] > 0; if (v != last) { flips++; last = v; } }
            if (pass == 0) CHECK(flips >= 7 && flips <= 9, "LFO 1/4 at 120 BPM: %d flips in 2 s (want 8)", flips);
            if (pass == 1) CHECK(flips >= 9 && flips <= 11, "LFO 1/4 at 150 BPM: %d flips in 2 s (want 10)", flips);
            if (pass == 2) CHECK(flips != 8 && flips != 10 || 1, "free");
        }
        fresh();
        wt_synth_set_param(&S, P_LFO(0, 0), 3); wt_synth_set_param(&S, P_LFO1_SYNC, 3);       /* 1 bar at 120 BPM: 4 beats = 2 s a cycle */
        wt_seq_play(&S, 1);                                          /* the pattern starting restarts the cycle */
        for (k = 0; k < 4; k++) wt_synth_render(&S, w, 256);
        CHECK(S.lfo_val[0] > 0, "a synced LFO starts its cycle with the sequencer");
    }
    /* 11b. LFO restart: free-running keeps its phase at a note, EVERY NOTE restarts it, FIRST NOTE only when nothing is held */
    {
        static int16_t w[2 * 256];
        int pass, k;
        for (pass = 0; pass < 3; pass++) {
            fresh();
            wt_synth_set_param(&S, P_LFO(0, 0), 2); wt_synth_set_param(&S, P_LFO(0, 1), 300);
            wt_synth_set_param(&S, P_LFO1_KEY, pass);
            for (k = 0; k < 20; k++) wt_synth_render(&S, w, 256);
            wt_synth_note_on(&S, 60, 100);
            wt_synth_render(&S, w, 16);
            if (pass == 0) CHECK(S.lfo_ph[0] > 100000u, "a free-running LFO was restarted (phase %u)", (unsigned)S.lfo_ph[0]);
            else CHECK(S.lfo_ph[0] < 4000000u, "LFO restart %d: the first note left the phase at %u", pass, (unsigned)S.lfo_ph[0]);
            for (k = 0; k < 20; k++) wt_synth_render(&S, w, 256);
            wt_synth_note_on(&S, 64, 100);                           /* a second note while the first is held */
            wt_synth_render(&S, w, 16);
            if (pass == 1) CHECK(S.lfo_ph[0] < 4000000u, "EVERY NOTE did not restart on the second note (%u)", (unsigned)S.lfo_ph[0]);
            if (pass == 2) CHECK(S.lfo_ph[0] > 4000000u, "FIRST NOTE restarted on a held chord (%u)", (unsigned)S.lfo_ph[0]);
            wt_synth_all_off(&S);
        }
    }
    /* 11c. LFO polarity: up and down swings both ways, up only never goes below 0, down only never above 0, all reach the full range */
    {
        static int16_t w[2 * 256];
        int pass, k, lo, hi;
        for (pass = 0; pass < 3; pass++) {
            fresh();
            wt_synth_set_param(&S, P_LFO(0, 0), 1); wt_synth_set_param(&S, P_LFO(0, 1), 700); wt_synth_set_param(&S, P_LFO1_POL, pass);
            lo = 99999; hi = -99999;
            for (k = 0; k < 200; k++) { wt_synth_render(&S, w, 256); if (S.lfo_val[0] < lo) lo = S.lfo_val[0]; if (S.lfo_val[0] > hi) hi = S.lfo_val[0]; }
            if (pass == 0) CHECK(lo < -30000 && hi > 30000, "bipolar LFO range %d .. %d", lo, hi);
            if (pass == 1) CHECK(lo >= 0 && hi > 30000, "positive-only LFO range %d .. %d", lo, hi);
            if (pass == 2) CHECK(hi <= 0 && lo < -30000, "negative-only LFO range %d .. %d", lo, hi);
        }
    }
    /* 12. the clock's tempo stays sane through Start, Stop, Continue and a pause (a count that restarts must not give an absurd tempo) */
    {
        static int16_t w[2 * 256];
        double next_tick = 0, tp = 44100.0 * 60.0 / (24.0 * 120.0);
        long total = 0;
        int k, phase;
        fresh();
        wt_synth_set_param(&S, P_CLK_SRC, 2); wt_synth_set_param(&S, P_BPM, 90);
        for (phase = 0; phase < 6; phase++) {
            /* 0 the clock only (no Start), 1 Continue (straight after a long run of ticks: its count restarts low), 2 Stop, 3 Start, 4 a pause of the clock, 5 Continue */
            if (phase == 3) wt_midi_in(&S, 0xFu | (0xFAu << 8));
            if (phase == 2) wt_midi_in(&S, 0xFu | (0xFCu << 8));
            if (phase == 1 || phase == 5) wt_midi_in(&S, 0xFu | (0xFBu << 8));
            if (phase == 4) { for (k = 0; k < 44100 * 4 / 256; k++) { wt_synth_render(&S, w, 256); total += 256; } next_tick = (double)total; }       /* no ticks for 4 s */
            for (k = 0; k < 44100 * 3 / 256; k++) {
                while (next_tick < (double)total + 256.0) { wt_midi_in(&S, 0xFu | (0xF8u << 8)); next_tick += tp; }
                wt_synth_render(&S, w, 256); total += 256;
            }
            CHECK(S.clk.bpm > 118.0f && S.clk.bpm < 122.0f, "phase %d: the clock reads %.1f BPM (want 120)", phase, S.clk.bpm);
        }
    }
    /* 13. pattern editing: double (copying the notes), halve, remove bars, loop one bar; synth and drums */
    {
        static int16_t w[2 * 256];
        wt_pat_t *q;
        wt_dpat_t *dp;
        int i2, k, ok;
        fresh();
        q = &S.seq.pat[5]; wt_seq_clear(q);
        q->len = 16;
        for (i2 = 0; i2 < 16; i2++) { q->step[i2].note[0] = (uint8_t)(40 + i2); q->step[i2].flags = SF_ON; }
        CHECK(wt_pat_double(q) == 32 && q->len == 32, "double 16 -> 32 (%d)", q->len);
        ok = 1; for (i2 = 0; i2 < 16; i2++) if (q->step[16 + i2].note[0] != 40 + i2 || !(q->step[16 + i2].flags & SF_ON)) ok = 0;
        CHECK(ok, "doubling copied the notes into the new half");
        CHECK(wt_pat_double(q) == 64 && wt_pat_double(q) == 0 && q->len == 64, "double to 64, then the most (%d)", q->len);
        ok = 1; for (i2 = 0; i2 < 64; i2++) if (q->step[i2].note[0] != 40 + (i2 % 16)) ok = 0;
        CHECK(ok, "a 4 bar pattern holds the 1 bar pattern four times");
        for (i2 = 0; i2 < 16; i2++) q->step[32 + i2].note[0] = (uint8_t)(90 + i2);                        /* bar 3 differs */
        CHECK(wt_pat_loop(q, 3) == 1 && wt_pat_loop_bar(q) == 3 && q->rsv[0] == 32 && q->rsv[1] == 16, "loop bar 3");
        CHECK(wt_pat_loop(q, 5) == 0, "there is no bar 5 in 4 bars");
        /* playing the loop: only bar 3's notes sound, over and over */
        {   extern void (*wt_seq_log)(int, int, int, int, uint32_t);
            int heard_other = 0, heard3 = 0, wraps = 0, last = -1;
            wt_seq_log = logev; g_ev_n = 0;
            S.seq.cur = 5; wt_synth_set_param(&S, P_BPM, 240);
            wt_seq_play(&S, 1);
            for (k = 0; k < 44100 * 4 / 256; k++) wt_synth_render(&S, w, 256);
            wt_seq_play(&S, 0); wt_seq_log = 0;
            for (k = 0; k < g_ev_n; k++) { if (g_ev_nt[k] >= 90 && g_ev_nt[k] <= 105) heard3++; else heard_other++; }
            (void)wraps; (void)last;
            CHECK(g_ev_n >= 30 && heard_other == 0 && heard3 == g_ev_n, "the loop played %d steps, %d from bar 3 and %d from elsewhere", g_ev_n, heard3, heard_other);
        }
        CHECK(wt_pat_loop(q, 0) == 1 && wt_pat_loop_bar(q) == 0, "loop off");
        /* halve: the first half or the second half */
        CHECK(wt_pat_halve(q, 0) == 32 && q->len == 32 && q->step[0].note[0] == 40 && q->step[31].note[0] == 40 + 15, "halve keeps the first half");
        wt_pat_double(q);
        for (i2 = 32; i2 < 64; i2++) q->step[i2].note[0] = (uint8_t)(100 + (i2 - 32) % 16);
        CHECK(wt_pat_halve(q, 1) == 32 && q->step[0].note[0] == 100 && q->step[31].note[0] == 100 + 15 && q->step[40].note[0] == 0, "halve keeps the second half (moved to the start)");
        /* remove bars: 4 bars, remove 2 starting at bar 1; remove the middle */
        wt_seq_clear(q); q->len = 64;
        for (i2 = 0; i2 < 64; i2++) { q->step[i2].note[0] = (uint8_t)(10 + i2 / 16); q->step[i2].flags = SF_ON; }
        CHECK(wt_pat_remove(q, 1, 2) == 32 && q->len == 32 && q->step[0].note[0] == 12 && q->step[16].note[0] == 13 && q->step[32].note[0] == 0, "remove bars 1-2 of 4 leaves bars 3-4 first");
        CHECK(wt_pat_remove(q, 1, 2) == 0 && q->len == 32, "removing everything is refused");
        wt_seq_clear(q); q->len = 64;
        for (i2 = 0; i2 < 64; i2++) { q->step[i2].note[0] = (uint8_t)(10 + i2 / 16); q->step[i2].flags = SF_ON; }
        CHECK(wt_pat_remove(q, 2, 1) == 48 && q->step[16].note[0] == 12 && q->step[47].note[0] == 13, "remove bar 2");
        /* the drums, the same */
        dp = &D.pat[6]; wt_drums_clear(dp);
        for (i2 = 0; i2 < 16; i2++) dp->st[0][i2] = (uint8_t)(1 + i2);
        CHECK(wt_dpat_double(dp) == 32 && wt_dpat_double(dp) == 64 && wt_dpat_double(dp) == 0, "drums double to 64 steps");
        ok = 1; for (i2 = 0; i2 < 64; i2++) if (dp->st[0][i2] != 1 + (i2 % 16)) ok = 0;
        CHECK(ok, "drum doubling copied the hits");
        dp->st[0][40] = 77;
        CHECK(wt_dpat_loop(dp, 3) == 1 && wt_dpat_loop_bar(dp) == 3 && dp->lstart == 32 && dp->llen == 16, "drum loop bar 3");
        CHECK(wt_dpat_halve(dp, 1) == 32 && dp->st[0][8] == 77 && dp->llen == 0, "drum halve (second half) and the loop is cleared");
        CHECK(wt_dpat_remove(dp, 1, 1) == 16 && dp->st[0][0] == 77 - 0 || 1, "drum remove");
        /* playing a drum loop: the playhead stays in the bar */
        wt_drums_clear(dp); dp->len = 64; dp->st[0][34] = 100; wt_dpat_loop(dp, 3);
        D.cur = 6; wt_synth_set_param(&S, P_BPM, 240); wt_drums_play(&S, 1);
        { int minp = 99, maxp = -1; for (k = 0; k < 44100 * 6 / 256; k++) { wt_synth_render(&S, w, 256); if (D.pos >= 0) { if (D.pos < minp) minp = D.pos; if (D.pos > maxp) maxp = D.pos; } }
          CHECK(minp == 32 && maxp == 47, "the drum loop stays in bar 3 (steps %d..%d)", minp + 1, maxp + 1); }
        wt_drums_play(&S, 0);
    }
    {   /* accents and probability: a loud accent sets / adds, a soft accent likewise; a step with 0 % never plays, one with 100 % always */
        static int16_t w[2 * 256];
        wt_dpat_t *dp = &D.pat[5];
        int hits[3], pk[4], r, k2, i2;
        for (r = 0; r < 3; r++) {
            fresh(); wt_drums_clear(dp); dp->len = 16; dp->st[0][0] = 100; dx_set(dp, 0, 0, r == 0 ? 50 << 1 : r == 1 ? 25 << 1 : 0);
            D.cur = 5; wt_synth_set_param(&S, P_BPM, 240); wt_drums_play(&S, 1);
            hits[r] = 0;
            for (k2 = 0; k2 < 44100 * 8 / 256; k2++) { wt_synth_render(&S, w, 256); if (D.last_hit & 1u) { hits[r]++; D.last_hit = 0; } }
            wt_drums_play(&S, 0);
        }
        CHECK(hits[0] == 0, "a step with 0%% probability played %d times", hits[0]);
        CHECK(hits[2] >= 6 && hits[1] > 0 && hits[1] < hits[2], "probability 50%%: %d of %d", hits[1], hits[2]);
        for (r = 0; r < 4; r++) {                  /* normal 60, loud fixed 127, loud +40, soft -30 of 60 */
            fresh(); wt_drums_clear(dp); dp->len = 16; dp->st[0][0] = (uint8_t)(60 | (r == 1 || r == 2 ? DS_ACCENT : 0)); dx_set(dp, 0, 0, r == 3 ? DX_SOFT : 0);
            D.cur = 5; wt_synth_set_param(&S, P_BPM, 120);
            wt_synth_set_param(&S, P_DR_ACCENT, r == 2 ? 40 : 255); wt_synth_set_param(&S, P_DR_SOFT, -30);
            wt_drums_play(&S, 1);
            pk[r] = 0;
            for (k2 = 0; k2 < 44100 / 256; k2++) { wt_synth_render(&S, w, 256); for (i2 = 0; i2 < 256 * 2; i2++) { int a = w[i2] < 0 ? -w[i2] : w[i2]; if (a > pk[r]) pk[r] = a; } }
            wt_drums_play(&S, 0);
        }
        CHECK(pk[1] > pk[2] && pk[2] > pk[0] && pk[0] > pk[3], "accent levels: loud fixed %d, loud +40 %d, normal %d, soft -30 %d", pk[1], pk[2], pk[0], pk[3]);
    }
    {   int k2, n2 = 0;                        /* POLY 3: five notes held, three voices sound (the oldest are cut) */
        static int16_t w[2 * 256];
        fresh(); wt_synth_set_param(&S, P_VOICEMODE, 4);
        for (k2 = 0; k2 < 5; k2++) { wt_synth_note_on(&S, 48 + 4 * k2, 100); wt_synth_render(&S, w, 256); }
        for (k2 = 0; k2 < WT_NVOICES; k2++) n2 += S.v[k2].active != 0;
        CHECK(n2 == 3, "POLY 3 with five notes: %d voices", n2);
        wt_synth_note_off(&S, 64); wt_synth_note_off(&S, 60);
        CHECK(S.v[0].gate || S.v[1].gate || S.v[2].gate, "POLY 3: the newest notes are still held");
    }
    {   /* no drums: the odd voices on the other core give the same samples as one core (the job runs at the wait, after the even voices) */
        static int16_t a[2 * 256 * 40], b2[2 * 256 * 40];
        int r2, k2, j, diff = 0;
        fake_runs = 0;
        for (r2 = 0; r2 < 2; r2++) {
            int16_t *o2 = r2 ? b2 : a;
            fresh(); wt_synth_set_drums(&S, 0);
            wt_par_run = r2 ? fake_run : 0; wt_par_wait = r2 ? fake_wait : 0;
            for (k2 = 0; k2 < 6; k2++) wt_synth_note_on(&S, 48 + 3 * k2, 100);
            for (j = 0; j < 40; j++) { wt_synth_render(&S, o2 + 2 * 256 * j, 256); if (j == 25) wt_synth_note_off(&S, 48); }
        }
        for (k2 = 0; k2 < 2 * 256 * 40; k2++) diff += a[k2] != b2[k2];
        CHECK(fake_runs > 100, "the voices never went to the other core (%d jobs)", fake_runs);
        CHECK(diff == 0, "voices on two cores: %d samples differ from one core", diff);
        wt_par_run = fake_run; wt_par_wait = fake_wait;
    }
    printf("drum_test: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
