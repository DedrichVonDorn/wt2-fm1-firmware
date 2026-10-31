/* SPDX-License-Identifier: GPL-3.0-only
 * WT-1 core: the step sequencer. Integer only, no libc. The model is X0X's (seq/sequencer.c, GPL-3.0): a pattern of up to 64
 * steps with its own length, rate, swing, direction and transposition; each step has its note (here up to four, a chord), a
 * velocity, a gate length, accent, slide and tie flags and a chance. It runs once per control block (16 samples) inside the
 * synth's render, so a step lands within one block of its exact time and never drifts.
 *
 *   NOTE   starts the step's notes; they are released after the gate (a % of the step).
 *   TIE    the notes of the step before are held through this one (no new attack), released after this step's gate.
 *   SLIDE  the notes are held until the next step; if that is a note, the pitch glides into it (no new attack).
 *   chance the step is skipped (a rest) when the dice say so.                                                         */
#include "wt_core.h"
#if !WT_NODRUMS
#include "wt_drums.h"
#endif

void (*wt_seq_log)(int kind, int note, int vel, int accent, uint32_t t);

static const uint8_t RATE_PER_BEAT[SR_N] = {4, 6, 8, 3};      /* steps per beat: 16th, 16th triplet, 32nd, 8th triplet */

void wt_seq_clear(wt_pat_t *p)
{
    int i;
    for (i = 0; i < WT_NSTEP; i++) {
        wt_step_t *st = &p->step[i];
        st->note[0] = st->note[1] = st->note[2] = st->note[3] = 0;
        st->vel = 100; st->gate = 60; st->flags = 0; st->prob = 100;
    }
    p->len = 16; p->rate = SR_16; p->swing = 50; p->dir = SD_FWD; p->trans = 24; p->rsv[0] = p->rsv[1] = p->rsv[2] = 0;
}

void wt_seq_init(wt_seq_t *q)
{
    static const uint8_t NOTE[16] = {48, 0, 51, 0, 55, 60, 0, 55, 48, 0, 46, 0, 48, 51, 55, 0};
    static const uint8_t FLAG[16] = {SF_ON | SF_ACCENT, 0, SF_ON, 0, SF_ON | SF_SLIDE, SF_ON, 0, SF_ON, SF_ON | SF_ACCENT, 0, SF_ON, 0,
                                     SF_ON, SF_ON | SF_SLIDE, SF_ON, 0};
    int i;
    for (i = 0; i < (int)sizeof *q; i++) ((uint8_t *)q)[i] = 0;
    for (i = 0; i < WT_NPAT; i++) wt_seq_clear(&q->pat[i]);
    for (i = 0; i < 16; i++) {                  /* pattern 1: a bass line to hear the sequencer with */
        q->pat[0].step[i].note[0] = NOTE[i];
        q->pat[0].step[i].flags = NOTE[i] ? FLAG[i] : 0;
        q->pat[0].step[i].vel = (uint8_t)(NOTE[i] ? (FLAG[i] & SF_ACCENT ? 110 : 85) : 100);
    }
    q->next = 255; q->pos = -1; q->pp = 1; q->slide_from = -1; q->rng = 0x2545F491u;
}

static uint32_t seq_rand(wt_seq_t *q)
{
    uint32_t x = q->rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return q->rng = x;
}

/* length of the next step, samples << 8: the beat, split by the rate, with swing on every second step */
static uint32_t step_len_q8(const wt_synth_t *s, const wt_pat_t *p, uint32_t n)
{
    uint32_t beat_q8 = (uint32_t)(((uint64_t)2646000u << 12) / wt_bpm_x16(s));            /* 60 s * 44100 / bpm, << 8 */
    uint32_t base = beat_q8 / RATE_PER_BEAT[p->rate % SR_N];
    uint32_t sw = p->swing < 50 ? 50 : p->swing > 75 ? 75 : p->swing;
    return (n & 1u) ? (uint32_t)(((uint64_t)base * 2u * (100u - sw)) / 100u) : (uint32_t)(((uint64_t)base * 2u * sw) / 100u);
}

static void seq_note_off_all(wt_synth_t *s)
{
    wt_seq_t *q = &s->seq;
    int i;
    for (i = 0; i < 8; i++)
        if (q->held[i].used) {
            wt_synth_note_off(s, q->held[i].note);
            if (wt_seq_log) wt_seq_log(0, q->held[i].note, 0, 0, q->t);
            q->held[i].used = 0;
        }
}

/* only the notes held "until the next step" (a slide's, a tie's): the timed ones end on their own, so a long note rings on while later steps play (poly mode) */
static void seq_off_open(wt_synth_t *s)
{
    wt_seq_t *q = &s->seq;
    int i;
    for (i = 0; i < 8; i++)
        if (q->held[i].used && q->held[i].off_in < 0) {
            wt_synth_note_off(s, q->held[i].note);
            if (wt_seq_log) wt_seq_log(0, q->held[i].note, 0, 0, q->t);
            q->held[i].used = 0;
        }
}

static void held_add(wt_synth_t *s, int note, int32_t off_in)
{
    wt_seq_t *q = &s->seq;
    int i, oldest = 0;
    for (i = 0; i < 8; i++)
        if (!q->held[i].used) { q->held[i].used = 1; q->held[i].note = (uint8_t)note; q->held[i].off_in = off_in; return; }
    for (i = 1; i < 8; i++) if (q->held[i].off_in >= 0 && (q->held[oldest].off_in < 0 || q->held[i].off_in < q->held[oldest].off_in)) oldest = i;
    wt_synth_note_off(s, q->held[oldest].note);                     /* all eight slots busy: the one that would end first ends now (never a note nobody will release) */
    q->held[oldest].note = (uint8_t)note; q->held[oldest].off_in = off_in;
}

static int held_count(const wt_seq_t *q)
{
    int i, n = 0;
    for (i = 0; i < 8; i++) n += q->held[i].used;
    return n;
}

/* the step that follows pos is a tie (the note before it is then held through the boundary, as on a 303); only forward and reverse know their next step */
/* the part of the pattern that plays: all of it, or the one bar set to loop (rsv[0] = its first step, rsv[1] = its length; 0 = all) */
static void loop_range(const wt_pat_t *p, int len, int *lo, int *n)
{
    int ls = p->rsv[0], ll = p->rsv[1];
    if (ll == 0 || ls >= len) { *lo = 0; *n = len; return; }
    *lo = ls; *n = ll > len - ls ? len - ls : ll;
}

static int next_is_tie(const wt_pat_t *p, int pos, int lo, int hi)
{
    int n;
    if (p->dir == SD_FWD) n = pos + 1 >= hi ? lo : pos + 1;
    else if (p->dir == SD_REV) n = pos - 1 < lo ? hi - 1 : pos - 1;
    else return 0;
    return (p->step[n % WT_NSTEP].flags & (SF_ON | SF_TIE)) == (SF_ON | SF_TIE);
}

static void fire_step(wt_synth_t *s)
{
    wt_seq_t *q = &s->seq;
    const wt_pat_t *p;
    const wt_step_t *st;
    int len, lo, nn, hi, wrapped = 0, i, played = 0, nnotes = 0, trans, hold_on;
    int32_t step_samples, gate_samples, dur_samples;
    /* 1. the next step */
    p = &q->pat[q->cur % WT_NPAT];
    len = p->len < 1 ? 1 : p->len > WT_NSTEP ? WT_NSTEP : p->len;
    loop_range(p, len, &lo, &nn);
    hi = lo + nn;
    if (q->pos < 0) {
        q->pos = (int16_t)(p->dir == SD_REV ? hi - 1 : lo);
        q->pp = p->dir == SD_REV ? -1 : 1;
    } else {
        int pos = q->pos;
        switch (p->dir) {
        case SD_REV: pos--; if (pos < lo || pos >= hi) { pos = hi - 1; wrapped = 1; } break;
        case SD_PING:
            pos += q->pp;
            if (pos >= hi) { pos = nn > 1 ? hi - 2 : lo; q->pp = -1; wrapped = 1; }
            else if (pos < lo) { pos = nn > 1 ? lo + 1 : lo; q->pp = 1; wrapped = 1; }
            break;
        case SD_RAND: pos = lo + (int)(seq_rand(q) % (uint32_t)nn); wrapped = (q->n % (uint32_t)nn) == 0; break;
        default: pos++; if (pos >= hi || pos < lo) { pos = lo; wrapped = 1; }
        }
        q->pos = (int16_t)pos;
    }
    if (wrapped && q->next != 255) {
        q->cur = q->next; q->next = 255; p = &q->pat[q->cur % WT_NPAT];
        len = p->len < 1 ? 1 : p->len > WT_NSTEP ? WT_NSTEP : p->len;
        loop_range(p, len, &lo, &nn); hi = lo + nn;
        q->pos = (int16_t)(p->dir == SD_REV ? hi - 1 : lo);                    /* the queued pattern starts at the start of its loop */
    }
    st = &p->step[q->pos % WT_NSTEP];
    q->len_q8 = step_len_q8(s, p, q->n);                           /* how long until the next step fires: this step's length */
    step_samples = (int32_t)(q->len_q8 >> 8);
    gate_samples = step_samples * (st->gate < 5 ? 5 : st->gate > 100 ? 100 : st->gate) / 100;
    {   int L = ((st->flags >> 4) & 15) + 1, k;                        /* a note's length in steps (flags bits 4..7): the steps in between, then the gate of the last one */
        int64_t sum = 0;
        dur_samples = gate_samples;
        if (L > 1) {
            for (k = 0; k < L - 1; k++) sum += step_len_q8(s, p, q->n + (uint32_t)k) >> 8;
            dur_samples = (int32_t)(sum + (int64_t)(step_len_q8(s, p, q->n + (uint32_t)(L - 1)) >> 8) * (st->gate < 5 ? 5 : st->gate > 100 ? 100 : st->gate) / 100);
        }
    }
    trans = (int)p->trans - 24;
    hold_on = (st->flags & SF_SLIDE) || next_is_tie(p, q->pos, lo, hi);          /* held until the next step starts */
    /* 2. what this step does */
    for (i = 0; i < 4; i++) nnotes += st->note[i] != 0;
    if ((st->flags & SF_ON) && st->prob < 100 && (int)(seq_rand(q) % 100u) >= (int)st->prob) { nnotes = 0; }   /* the dice: a rest */
    if ((st->flags & SF_ON) && (st->flags & SF_TIE) && held_count(q)) {
        for (i = 0; i < 8; i++)                                   /* a tie: the notes go on, to the end of this step's gate */
            if (q->held[i].used) q->held[i].off_in = hold_on ? -1 : gate_samples;
        q->slide_from = (st->flags & SF_SLIDE) ? q->slide_from : -1;
        q->n++;
        return;
    }
    if ((st->flags & SF_ON) && nnotes) {
        int slide = q->slide_from >= 0 && nnotes == 1 && held_count(q) == 1;
        int sf = q->slide_from, note0 = st->note[0] + trans;
        if (!slide) seq_off_open(s);                              /* a slid / tied note before this one ends here (timed ones run on) */
        for (i = 0; i < 4; i++) {
            int note = st->note[i] ? st->note[i] + trans : 0;
            if (!note) continue;
            note = note < 0 ? 0 : note > 127 ? 127 : note;
            if (s->p[P_SCALE] && s->p[P_SCALE_SEQ]) note = wt_scale_snap(s->p[P_SCALE], s->p[P_ROOT], note);
            if (slide) {
                wt_synth_slide(s, sf, note, st->vel, (st->flags & SF_ACCENT) != 0);
                if (wt_seq_log) wt_seq_log(2, note, st->vel, (st->flags & SF_ACCENT) != 0, q->t);
                {   int k;                                          /* the held entry of the note that slid follows the glide (not the other notes ringing) */
                    for (k = 0; k < 8; k++) if (q->held[k].used && q->held[k].note == sf) { q->held[k].note = (uint8_t)note; q->held[k].off_in = hold_on ? -1 : dur_samples; break; }
                }
            } else {
                wt_synth_note_on_ex(s, note, st->vel, (st->flags & SF_ACCENT) != 0);
                if (wt_seq_log) wt_seq_log(1, note, st->vel, (st->flags & SF_ACCENT) != 0, q->t);
                held_add(s, note, hold_on ? -1 : dur_samples);
            }
            played = 1;
        }
        (void)note0;
        q->slide_from = -1;
        if (played && (st->flags & SF_SLIDE) && nnotes == 1) {
            int n0 = st->note[0] + trans;
            n0 = n0 < 0 ? 0 : n0 > 127 ? 127 : n0;
            if (s->p[P_SCALE] && s->p[P_SCALE_SEQ]) n0 = wt_scale_snap(s->p[P_SCALE], s->p[P_ROOT], n0);
            q->slide_from = (int16_t)n0;
        }
    } else {                                                        /* a rest: a note held "until the next step" ends now, timed ones run on */
        seq_off_open(s);
        q->slide_from = -1;
    }
    q->n++;
}

void wt_seq_play(wt_synth_t *s, int on)
{
    wt_seq_t *q = &s->seq;
    if (on) {
        if (q->run) return;
        seq_note_off_all(s);
        q->pos = -1; q->n = 0; q->t = 0; q->slide_from = -1;
        if (s->p[P_LFO1_SYNC]) s->lfo_ph[0] = 0;                    /* a synced LFO starts its cycle with the pattern */
        if (s->p[P_LFO2_SYNC]) s->lfo_ph[1] = 0;
        q->len_q8 = step_len_q8(s, &q->pat[q->cur % WT_NPAT], 0);
        q->acc_q8 = q->len_q8;                                      /* the first step fires in the first block */
        q->run = 1;
    } else {
        q->run = 0;
        seq_note_off_all(s);
        q->pos = -1; q->slide_from = -1;
    }
}

void wt_seq_queue(wt_synth_t *s, int pat)
{
    wt_seq_t *q = &s->seq;
    pat = pat < 0 ? 0 : pat >= WT_NPAT ? WT_NPAT - 1 : pat;
    if (!q->run) { q->cur = (uint8_t)pat; q->next = 255; } else q->next = (uint8_t)pat;
}

void wt_seq_block(wt_synth_t *s)
{
    wt_seq_t *q = &s->seq;
    int i;
    if (!q->run) return;
    q->t += WT_CTL;
    for (i = 0; i < 8; i++)
        if (q->held[i].used && q->held[i].off_in >= 0) {
            q->held[i].off_in -= WT_CTL;
            if (q->held[i].off_in <= 0) {
                wt_synth_note_off(s, q->held[i].note);
                if (wt_seq_log) wt_seq_log(0, q->held[i].note, 0, 0, q->t);
                q->held[i].used = 0;
            }
        }
    q->acc_q8 += s->blk_inc[s->cur_blk & 15];
    {   int guard = 4;                                              /* a very fast step could fire twice in a block */
        while (q->acc_q8 >= q->len_q8 && q->len_q8 && guard--) {
            q->acc_q8 -= q->len_q8;
            fire_step(s);
        }
    }
    q->phase8 = (uint8_t)(q->len_q8 ? ((uint64_t)q->acc_q8 * 255u) / q->len_q8 : 0);
}

/* ============================================================ the arpeggiator */
static const uint8_t ARP_PER_BEAT[6] = {1, 2, 3, 4, 6, 8};      /* 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32 */

static void arp_release(wt_synth_t *s)
{
    wt_arp_t *a = &s->arp;
    int i;
    for (i = 0; i < a->nsnd; i++) wt_synth_note_off(s, a->snd[i]);
    a->nsnd = 0; a->off_in = -1;
}

void wt_arp_stop(wt_synth_t *s)
{
    wt_arp_t *a = &s->arp;
    arp_release(s);
    a->nheld = 0; a->run = 0; a->pos = 0; a->pp = 1;
}

static uint32_t arp_rand(wt_arp_t *a)
{
    uint32_t x = a->rng ? a->rng : 0x1234567u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return a->rng = x;
}

static uint32_t arp_len_q8(const wt_synth_t *s, uint32_t n)
{
    uint32_t base = (uint32_t)(((uint64_t)2646000u << 12) / wt_bpm_x16(s)) / ARP_PER_BEAT[s->p[P_ARP_RATE] % 6];
    uint32_t sw = (uint32_t)(s->p[P_ARP_SWING] < 50 ? 50 : s->p[P_ARP_SWING] > 75 ? 75 : s->p[P_ARP_SWING]);
    return (n & 1u) ? (uint32_t)(((uint64_t)base * 2u * (100u - sw)) / 100u) : (uint32_t)(((uint64_t)base * 2u * sw) / 100u);
}

void wt_synth_key_on(wt_synth_t *s, int note, int vel)
{
    wt_arp_t *a = &s->arp;
    int i, any_down = 0;
    note = note < 0 ? 0 : note > 127 ? 127 : note;
    {   int out = s->p[P_SCALE] ? wt_scale_snap(s->p[P_SCALE], s->p[P_ROOT], note) : note;        /* scale mode: the key plays the nearest note of the scale */
        s->keymap[note] = (uint8_t)out; note = out; }
    if (!s->p[P_ARP_ON]) { wt_synth_note_on(s, note, vel); return; }
    note = note < 0 ? 0 : note > 127 ? 127 : note;
    for (i = 0; i < a->nheld; i++) any_down |= a->h[i].down;
    if (s->p[P_ARP_LATCH] && !any_down) a->nheld = 0;               /* latch: a new chord replaces the held one */
    for (i = 0; i < a->nheld; i++)
        if (a->h[i].note == note) { a->h[i].down = 1; a->h[i].vel = (uint8_t)vel; return; }
    if (a->nheld < 16) { a->h[a->nheld].note = (uint8_t)note; a->h[a->nheld].vel = (uint8_t)vel; a->h[a->nheld].down = 1; a->nheld++; }
    if (!a->run) {                                                    /* the first key starts it, on the next block */
        a->run = 1; a->pos = 0; a->pp = 1; a->n = 0;
        a->len_q8 = arp_len_q8(s, 0); a->acc_q8 = a->len_q8;
    }
}

void wt_synth_key_off(wt_synth_t *s, int note)
{
    wt_arp_t *a = &s->arp;
    int i, j;
    note = note < 0 ? 0 : note > 127 ? 127 : note;
    note = s->keymap[note] ? s->keymap[note] : note;               /* the note this key was played as */
    if (!s->p[P_ARP_ON] && !a->nheld) { wt_synth_note_off(s, note); return; }
    if (!s->p[P_ARP_ON]) { wt_synth_note_off(s, note); return; }
    for (i = 0; i < a->nheld; i++)
        if (a->h[i].note == note) {
            if (s->p[P_ARP_LATCH]) { a->h[i].down = 0; return; }
            for (j = i; j + 1 < a->nheld; j++) a->h[j] = a->h[j + 1];
            a->nheld--;
            return;
        }
    wt_synth_note_off(s, note);                                       /* a note played before the arpeggiator was switched on */
}

static void arp_step(wt_synth_t *s)
{
    wt_arp_t *a = &s->arp;
    uint8_t list[16 * 4], vl[16 * 4];
    int n = 0, i, o, oct = s->p[P_ARP_OCT], mode = s->p[P_ARP_MODE], idx, len;
    int32_t step_samples, gate;
    uint8_t srt[16], sv[16];
    for (i = 0; i < a->nheld; i++) { srt[i] = a->h[i].note; sv[i] = a->h[i].vel; }
    if (mode != AM_PLAYED) {                                          /* by pitch (insertion sort) */
        int j;
        for (i = 1; i < a->nheld; i++) {
            uint8_t k = srt[i], v = sv[i];
            for (j = i - 1; j >= 0 && srt[j] > k; j--) { srt[j + 1] = srt[j]; sv[j + 1] = sv[j]; }
            srt[j + 1] = k; sv[j + 1] = v;
        }
    }
    for (o = 0; o < oct; o++)
        for (i = 0; i < a->nheld; i++) {
            int nt = srt[i] + 12 * o;
            list[n] = (uint8_t)(nt > 127 ? 127 : nt); vl[n] = sv[i]; n++;
        }
    arp_release(s);
    a->len_q8 = arp_len_q8(s, a->n);
    step_samples = (int32_t)(a->len_q8 >> 8);
    gate = step_samples * (s->p[P_ARP_GATE] < 5 ? 5 : s->p[P_ARP_GATE]) / 100;
    if (!n) return;
    switch (mode) {
    case AM_DOWN: idx = n - 1 - (a->pos % n); a->pos = (int16_t)((a->pos + 1) % n); break;
    case AM_UPDOWN: case AM_DOWNUP: {
        int cyc = n > 1 ? 2 * n - 2 : 1, p = a->pos % cyc;
        idx = p < n ? p : cyc - p;
        if (mode == AM_DOWNUP) idx = n - 1 - idx;
        a->pos = (int16_t)((a->pos + 1) % cyc);
        break; }
    case AM_RANDOM: idx = (int)(arp_rand(a) % (uint32_t)n); break;
    case AM_CHORD: idx = -1; break;
    default: idx = a->pos % n; a->pos = (int16_t)((a->pos + 1) % n); break;
    }
    len = 0;
    if (idx < 0) {                                                    /* the whole chord (one octave of it), lowest voices first */
        for (i = 0; i < a->nheld && len < 16; i++) { wt_synth_note_on(s, list[i], vl[i]); a->snd[len++] = list[i]; }
    } else { wt_synth_note_on(s, list[idx], vl[idx]); a->snd[len++] = list[idx]; }
    a->nsnd = (uint8_t)len;
    a->off_in = gate < WT_CTL ? WT_CTL : gate;
    a->step++;
}

void wt_arp_block(wt_synth_t *s)
{
    wt_arp_t *a = &s->arp;
    if (!s->p[P_ARP_ON]) { if (a->run || a->nsnd || a->nheld) wt_arp_stop(s); return; }
    if (!s->p[P_ARP_LATCH]) {                                         /* latch switched off: what was only held by it goes */
        int i, j;
        for (i = 0; i < a->nheld; i++)
            if (!a->h[i].down) { for (j = i; j + 1 < a->nheld; j++) a->h[j] = a->h[j + 1]; a->nheld--; i--; }
    }
    if (!a->run) return;
    if (a->nsnd && a->off_in >= 0) {
        a->off_in -= WT_CTL;
        if (a->off_in <= 0) arp_release(s);
    }
    if (!a->nheld) { arp_release(s); a->run = 0; return; }           /* everything let go: it stops */
    a->acc_q8 += s->blk_inc[s->cur_blk & 15];
    {   int guard = 4;
        while (a->acc_q8 >= a->len_q8 && a->len_q8 && guard--) { a->acc_q8 -= a->len_q8; arp_step(s); a->n++; }
    }
}

/* the panic: everything stops and every voice is cut at once (no release tail) */
void wt_synth_panic(wt_synth_t *s)
{
    int i, e;
    wt_seq_play(s, 0);
    wt_arp_stop(s);
#if !WT_NODRUMS
    wt_drums_panic(s);
#endif
    if (wt_midi_out && s->p[P_MIDI_OUTCH]) wt_midi_out(0xBu | ((uint32_t)(0xB0 | (s->p[P_MIDI_OUTCH] - 1)) << 8) | (123u << 16));      /* all notes off, out */
    s->nstack = 0;
    for (i = 0; i < WT_NVOICES; i++) {
        wt_voice_t *v = &s->v[i];
        v->active = 0; v->gate = 0;
        for (e = 0; e < 3; e++) { v->env[e].st = 0; v->env[e].lvl = 0; }
    }
}

/* ============================================================ scales and the random generators */
const char *const wt_scale_names[WT_NSCALES] = {"OFF", "MAJOR", "MINOR", "DORIAN", "PHRYGIAN", "LYDIAN", "MIXOLYDIAN", "LOCRIAN", "HARM MINOR", "MEL MINOR",
                                                 "MAJ PENT", "MIN PENT", "BLUES", "WHOLE TONE", "PHRYG DOM", "IN (JAPAN)"};
static const uint16_t SCALE_MASK[WT_NSCALES] = {
    0xFFF,
    (1 << 0) | (1 << 2) | (1 << 4) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 11),            /* major */
    (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 10),            /* natural minor */
    (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 10),            /* dorian */
    (1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 10),            /* phrygian */
    (1 << 0) | (1 << 2) | (1 << 4) | (1 << 6) | (1 << 7) | (1 << 9) | (1 << 11),            /* lydian */
    (1 << 0) | (1 << 2) | (1 << 4) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 10),            /* mixolydian */
    (1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (1 << 6) | (1 << 8) | (1 << 10),            /* locrian */
    (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 11),            /* harmonic minor */
    (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 11),            /* melodic minor */
    (1 << 0) | (1 << 2) | (1 << 4) | (1 << 7) | (1 << 9),                                   /* major pentatonic */
    (1 << 0) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 10),                                  /* minor pentatonic */
    (1 << 0) | (1 << 3) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 10),                       /* blues */
    (1 << 0) | (1 << 2) | (1 << 4) | (1 << 6) | (1 << 8) | (1 << 10),                       /* whole tone */
    (1 << 0) | (1 << 1) | (1 << 4) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 10),            /* phrygian dominant */
    (1 << 0) | (1 << 1) | (1 << 5) | (1 << 7) | (1 << 8)};                                  /* in (japanese) */

static int in_scale(int scale, int root, int note)
{
    int pc = ((note - root) % 12 + 12) % 12;
    return (SCALE_MASK[scale < 0 || scale >= WT_NSCALES ? 0 : scale] >> pc) & 1;
}

int wt_scale_snap(int scale, int root, int note)
{
    int d;
    note = note < 0 ? 0 : note > 127 ? 127 : note;
    if (scale <= 0 || scale >= WT_NSCALES || in_scale(scale, root, note)) return note;
    for (d = 1; d <= 6; d++) {
        if (note - d >= 0 && in_scale(scale, root, note - d)) return note - d;
        if (note + d <= 127 && in_scale(scale, root, note + d)) return note + d;
    }
    return note;
}

int wt_scale_step(int scale, int root, int note, int dir)
{
    int n = note, guard = 13;
    if (scale <= 0 || scale >= WT_NSCALES) return note + (dir > 0 ? 1 : -1);
    while (guard--) {
        n += dir > 0 ? 1 : -1;
        if (n < 0 || n > 127) return note;
        if (in_scale(scale, root, n)) return n;
    }
    return note;
}

void wt_seq_generate(wt_synth_t *s, int pat, int density, int range, int spice)
{
    wt_seq_t *q = &s->seq;
    wt_pat_t *p;
    uint8_t pool[40];
    int np = 0, n, i, idx, lo, scale = s->p[P_SCALE] ? s->p[P_SCALE] : 11, root = s->p[P_ROOT];
    if (pat < 0 || pat >= WT_NPAT) return;
    p = &q->pat[pat];
    range = range < 1 ? 1 : range > 3 ? 3 : range;
    density = density < 0 ? 0 : density > 100 ? 100 : density;
    spice = spice < 0 ? 0 : spice > 100 ? 100 : spice;
    lo = 36 + root;                                                   /* the root of the scale around C2, up `range` octaves */
    for (n = lo; n <= lo + 12 * range && np < 40; n++) if (in_scale(scale, root, n)) pool[np++] = (uint8_t)n;
    for (i = 0; i < WT_NSTEP; i++) { int k; for (k = 0; k < 4; k++) p->step[i].note[k] = 0; p->step[i].flags = 0; p->step[i].vel = 100; p->step[i].gate = 60; p->step[i].prob = 100; }
    idx = 0;
    for (i = 0; i < p->len && i < WT_NSTEP; i++) {
        wt_step_t *st = &p->step[i];
        int r = (int)(seq_rand(q) % 100u);
        if (i != 0 && r >= density) continue;                         /* a rest */
        {   int jump = (int)(seq_rand(q) % 100u), d;
            if (jump < 62) d = (int)(seq_rand(q) % 3u) - 1;           /* mostly stays or steps */
            else if (jump < 90) d = (int)(seq_rand(q) % 5u) - 2;
            else d = (int)(seq_rand(q) % 9u) - 4;                     /* now and then a leap */
            idx += d;
            if (idx < 0) idx = (int)(seq_rand(q) % 3u); if (idx >= np) idx = np - 1 - (int)(seq_rand(q) % 3u);
            if (idx < 0) idx = 0;
        }
        st->note[0] = pool[idx];
        st->flags = SF_ON;
        st->vel = (uint8_t)(78 + seq_rand(q) % 40u);
        st->gate = (uint8_t)(45 + seq_rand(q) % 46u);
        if ((int)(seq_rand(q) % 100u) < 6 + spice / 5) { st->flags |= SF_ACCENT; st->vel = (uint8_t)(112 + seq_rand(q) % 16u); }
        if ((int)(seq_rand(q) % 100u) < spice / 7 && i + 1 < p->len) st->flags |= SF_SLIDE;
        if ((int)(seq_rand(q) % 100u) < spice / 3) { int L = 2 + (int)(seq_rand(q) % 3u); st->flags = (uint8_t)((st->flags & 15) | ((L - 1) << 4)); }     /* now and then a long note */
        if (spice > 50 && (int)(seq_rand(q) % 100u) < (spice - 50) / 3) st->prob = (uint8_t)(50 + seq_rand(q) % 40u);
    }
}

/* ============================================================ USB MIDI in */
#if !WT_NODRUMS
/* General MIDI drum notes onto the tracks of the kit: 909 BD SD LT MT HT RS CP CH OH CR RD, 808 BD SD LT MT HT RS CP CB CY OH CH */
static int gm_track(int kit, int note)
{
    switch (note) {
    case 35: case 36: return 0;
    case 38: case 40: return 1;
    case 41: case 43: return 2;
    case 45: case 47: return 3;
    case 48: case 50: return 4;
    case 37: return 5;
    case 39: return 6;
    case 42: case 44: return kit ? 10 : 7;
    case 46: return kit ? 9 : 8;
    case 49: case 52: case 55: case 57: return kit ? 8 : 9;
    case 51: case 53: case 59: return kit ? 8 : 10;
    case 56: return kit ? 7 : -1;
    default: return -1;
    }
}
#endif

static void clk_msg(wt_synth_t *s, uint32_t st);

void wt_midi_in(wt_synth_t *s, uint32_t pkt)
{
    uint32_t st = (pkt >> 8) & 0xF0u, ch = ((pkt >> 8) & 0x0Fu) + 1u, d1 = (pkt >> 16) & 0x7Fu, d2 = (pkt >> 24) & 0x7Fu;
    if ((pkt & 0xFu) == 0xFu) {                                       /* a real-time byte (cable 1: the TRS jack, cable 0: USB) */
        uint32_t rt = (pkt >> 8) & 0xFFu;
        int src = ((pkt >> 4) & 0xFu) ? 1 : 2;
        if (rt >= 0xF8u && s->p[P_CLK_SRC] == src) clk_msg(s, rt);
        return;
    }
#if WT_NODRUMS
    /* no drum channel: every channel the synth listens on plays the synth (channel 10 included) */
    int sch = s->p[P_MIDI_CH];
    int to_synth = sch == 0 || (int)ch == sch;
#else
    int drch = s->p[P_MIDI_DRCH], sch = s->p[P_MIDI_CH];
    int to_drums = drch != 0 && (int)ch == drch, to_synth = !to_drums && (sch == 0 || (int)ch == sch);
    if (to_drums) {
        if (st == 0x90u && d2) { int t = gm_track(s->p[P_DR_KIT] ? 1 : 0, (int)d1); if (t >= 0) wt_drums_hit(s, t, (int)d2); }
        return;
    }
#endif
    if (!to_synth) return;
    s->no_midi_out = 1;                                              /* what comes in is not sent back out */
    if (st == 0x90u && d2) wt_synth_key_on(s, (int)d1, (int)d2);
    else if (st == 0x80u || st == 0x90u) wt_synth_key_off(s, (int)d1);
    else if (st == 0xE0u) wt_synth_bend(s, (int)(((d2 << 7) | d1)) - 8192);
    else if (st == 0xB0u && d1 == 1u) wt_synth_modwheel(s, (int)d2);
    else if (st == 0xB0u && d1 == 123u) wt_synth_all_off(s);
    s->no_midi_out = 0;
}

/* ============================================================ external MIDI clock */
uint32_t wt_bpm_x16(const wt_synth_t *s)
{
    if (s->p[P_CLK_SRC] && s->clk.seen && s->clk.bpm >= 20.0f) {
        float b = s->clk.bpm > 300.0f ? 300.0f : s->clk.bpm;
        return (uint32_t)(b * 16.0f + 0.5f);
    }
    return (uint32_t)(s->p[P_BPM] < 20 ? 20 : s->p[P_BPM]) * 16u;
}

void wt_clk_plan(wt_synth_t *s, int nblk)
{
    wt_clk_t *c = &s->clk;
    int b;
    if (nblk > 16) nblk = 16;
    if (!s->p[P_CLK_SRC]) {                                         /* internal: 16 samples a block, as always */
        for (b = 0; b < nblk; b++) s->blk_inc[b] = (uint32_t)WT_CTL << 8;
        c->seen = 0; c->pending = 0; c->smp += (uint32_t)nblk * WT_CTL;
        return;
    }
    {
        uint32_t n = c->pending;
        float corr = 0.0f, rint = (float)s->p[P_BPM] * (24.0f / (60.0f * 44100.0f));        /* the internal tempo as ticks per sample, until the clock tells better */
        c->pending = 0;
        if (c->r <= 0.0f) c->r = rint;
        if (n) {
            uint32_t dt;
            c->ticks += n; c->last_rx = c->smp; c->seen = 1;
            if ((int32_t)(c->ticks - c->mark_ticks) < 0 || (c->smp - c->mark_smp) > 44100u * 3u) {     /* the count was reset (Start, Continue) or the clock paused: start a new window */
                c->mark_ticks = c->ticks; c->mark_smp = c->smp;
            } else if (c->ticks - c->mark_ticks >= 24u) {              /* a beat of ticks: the tempo */
                dt = c->smp - c->mark_smp;
                if (dt > 0u) {
                    float rn = (float)(c->ticks - c->mark_ticks) / (float)dt;
                    float lo = 30.0f * (24.0f / (60.0f * 44100.0f)), hi = 300.0f * (24.0f / (60.0f * 44100.0f));       /* only 30..300 BPM is a tempo */
                    if (rn >= lo && rn <= hi) c->r = c->bpm_ok ? 0.7f * c->r + 0.3f * rn : rn;
                    if (rn >= lo && rn <= hi) c->bpm_ok = 1;
                }
                c->mark_ticks = c->ticks; c->mark_smp = c->smp;
            }
            if (c->started) {                                        /* phase: where the ticks say we are, against where we are (the ticks were queued about half a chunk ago) */
                float where = (float)(c->ticks - 1u) + 0.5f * (float)(c->smp - c->last_plan) * c->r;
                corr = 0.12f * (where - c->pos);
            }
        }
        c->last_plan = c->smp;
        if (c->r < 30.0f * (24.0f / (60.0f * 44100.0f)) || c->r > 300.0f * (24.0f / (60.0f * 44100.0f))) c->r = rint;     /* never an absurd tempo */
        c->period = c->r > 0.0f ? 1.0f / c->r : 1.0f;
        c->bpm = c->r * (60.0f * 44100.0f / 24.0f);
        {   int alive = c->seen && (c->smp - c->last_rx) < 44100u;       /* no tick for a second: the clock has stopped, hold */
            for (b = 0; b < nblk; b++) {
                float dp = alive ? c->r * (float)WT_CTL + corr / (float)nblk : 0.0f;
                if (dp < 0.0f) dp = 0.0f;
                c->pos += dp;
                s->blk_inc[b] = (uint32_t)(dp * c->period * 256.0f);
            }
        }
        c->smp += (uint32_t)nblk * WT_CTL;
    }
}

void wt_seq_resume(wt_synth_t *s) { if (!s->seq.run) s->seq.run = 1; }

/* a real-time byte from the clock source: Clock, Start, Continue, Stop */
static void clk_msg(wt_synth_t *s, uint32_t st)
{
    wt_clk_t *c = &s->clk;
    if (st == 0xF8u) { c->pending++; return; }
    if (!s->p[P_CLK_SS]) return;                                    /* not following start / stop: the transport stays the player's */
    if (st == 0xFAu) {                                              /* Start: from the first step, the first tick after this is the downbeat */
        c->ticks = 0; c->pos = 0.0f; c->pending = 0; c->mark_ticks = 0; c->mark_smp = c->smp; c->started = 1; c->corr = 0.0f;
        if (c->r <= 0.0f) c->r = (float)s->p[P_BPM] * (24.0f / (60.0f * 44100.0f));
        wt_seq_play(s, 0); wt_seq_play(s, 1);
#if !WT_NODRUMS
        if (s->dr) { wt_drums_play(s, 0); wt_drums_play(s, 1); }
#endif
    } else if (st == 0xFBu) {                                       /* Continue */
        c->ticks = (uint32_t)(c->pos + 0.5f) + 1u; c->pending = 0; c->started = 1; c->mark_ticks = c->ticks; c->mark_smp = c->smp;
        wt_seq_resume(s);
#if !WT_NODRUMS
        if (s->dr) wt_drums_resume(s);
#endif
    } else if (st == 0xFCu) {                                       /* Stop */
        c->started = 0;
        wt_seq_play(s, 0);
#if !WT_NODRUMS
        if (s->dr) wt_drums_play(s, 0);
#endif
    }
}

/* ============================================================ pattern editing: double, halve, remove bars, loop a bar (a bar is 16 steps) */
int wt_pat_double(wt_pat_t *p)                 /* the length doubled (up to 64 steps), the notes copied into the new half; the new length, or 0 when it is already at the most */
{
    int len = p->len < 1 ? 1 : p->len > WT_NSTEP ? WT_NSTEP : p->len, nl, i;
    if (len >= WT_NSTEP) return 0;
    nl = len * 2 > WT_NSTEP ? WT_NSTEP : len * 2;
    for (i = len; i < nl; i++) p->step[i] = p->step[i - len];
    p->len = (uint8_t)nl;
    p->rsv[0] = p->rsv[1] = 0;                                    /* a loop of the old length would no longer fit */
    return nl;
}

int wt_pat_halve(wt_pat_t *p, int keep_second) /* half the length (at least 1 step): the first half stays, or the second half moves to the start; the new length */
{
    int len = p->len < 1 ? 1 : p->len > WT_NSTEP ? WT_NSTEP : p->len, h = len / 2, i;
    if (h < 1) h = 1;
    if (keep_second) for (i = 0; i < h; i++) p->step[i] = p->step[len - h + i];
    for (i = h; i < WT_NSTEP; i++) { wt_step_t *st = &p->step[i]; int k; for (k = 0; k < 4; k++) st->note[k] = 0; st->vel = 100; st->gate = 60; st->flags = 0; st->prob = 100; }
    p->len = (uint8_t)h;
    p->rsv[0] = p->rsv[1] = 0;
    return h;
}

int wt_pat_remove(wt_pat_t *p, int from_bar, int nbars)   /* take nbars bars out starting at from_bar (1 = the first); what follows moves up; the new length, or 0 if it would leave nothing */
{
    int len = p->len < 1 ? 1 : p->len > WT_NSTEP ? WT_NSTEP : p->len, s = (from_bar - 1) * 16, c = nbars * 16, i;
    if (from_bar < 1 || nbars < 1 || s >= len) return 0;
    if (c > len - s) c = len - s;
    if (len - c < 1) return 0;
    for (i = s; i + c < len; i++) p->step[i] = p->step[i + c];
    for (i = len - c; i < len; i++) { wt_step_t *st = &p->step[i]; int k; for (k = 0; k < 4; k++) st->note[k] = 0; st->vel = 100; st->gate = 60; st->flags = 0; st->prob = 100; }
    p->len = (uint8_t)(len - c);
    p->rsv[0] = p->rsv[1] = 0;
    return len - c;
}

int wt_pat_loop(wt_pat_t *p, int bar)          /* loop just this bar (1..4) of the pattern, or all of it (0); 1 if the bar exists */
{
    int len = p->len < 1 ? 1 : p->len > WT_NSTEP ? WT_NSTEP : p->len, s;
    if (bar <= 0) { p->rsv[0] = p->rsv[1] = 0; return 1; }
    s = (bar - 1) * 16;
    if (s >= len) return 0;
    p->rsv[0] = (uint8_t)s; p->rsv[1] = (uint8_t)(len - s < 16 ? len - s : 16);
    return 1;
}
int wt_pat_loop_bar(const wt_pat_t *p)         /* the bar being looped (1..4), 0 = all */
{
    return p->rsv[1] ? p->rsv[0] / 16 + 1 : 0;
}
