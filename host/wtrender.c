/* SPDX-License-Identifier: GPL-3.0-only
 * WT-1 Mac harness: renders a script to a 16-bit stereo 44.1 kHz WAV and prints level statistics.
 *
 *   wtrender script.wts out.wav
 *
 * Script (one command per line, # starts a comment):
 *   patch NAME            apply a built-in patch (init pad bass pluck lead sweep) at the cursor time
 *   set PARAM VALUE       set a parameter at the cursor time; VALUE is a number or, for MXn_SRC / MXn_DST, a name
 *   frame A|B N           set the oscillator's WT position to frame N of its table
 *   at SECONDS            move the cursor
 *   note MIDI VEL DUR     note on at the cursor, note off DUR seconds later
 *   on MIDI VEL | off MIDI | bend V | mod V
 *   play | stop           start / stop the sequencer (pattern 1 is a bass line); pattern N queues pattern N
 *   len SECONDS           total length (default: last event + 2 s)
 */
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "../core/wt_core.h"
#include "../core/wt_patch.h"

typedef struct { long t; int kind, a, b; } ev_t;
enum { EV_SET, EV_ON, EV_OFF, EV_BEND, EV_MOD, EV_PLAY, EV_STOP, EV_QUEUE };
static ev_t *evs; static int nev, capev;

static void add(long t, int kind, int a, int b)
{
    if (nev == capev) { capev = capev ? capev * 2 : 256; evs = (ev_t *)realloc(evs, (size_t)capev * sizeof *evs); }
    evs[nev].t = t; evs[nev].kind = kind; evs[nev].a = a; evs[nev].b = b; nev++;
}

static int lookup(const char *const *tbl, int n, const char *s)
{
    int i;
    for (i = 0; i < n; i++) if (!strcasecmp(tbl[i], s)) return i;
    return -1;
}

static long cursor;
static int tab_frames[WT_NTABLES];

static int set_param(void *ctx, const char *name, const char *val)
{
    int id = wt_param_find(name), v;
    (void)ctx;
    if (id < 0) { fprintf(stderr, "unknown parameter %s\n", name); return -1; }
    if (id >= P_MX0 && (id - P_MX0) % 3 == 0 && (v = lookup(wt_src_names, S_COUNT, val)) >= 0) ;
    else if (id >= P_MX0 && (id - P_MX0) % 3 == 1 && (v = lookup(wt_dst_names, D_COUNT, val)) >= 0) ;
    else v = atoi(val);
    add(cursor, EV_SET, id, v);
    return 0;
}

int main(int argc, char **argv)
{
    char line[256];
    FILE *f, *w;
    long total = 0, len = -1, maxt = 0;
    wt_synth_t *s;
    wt_table_t tb[3];
    wt_sample_t sm[3];
    int16_t *buf;
    int ei = 0, i;
    double peak = 0, sumsq = 0;
    long nn = 0;
    int maxv = 0;
    uint32_t hash = 2166136261u;
    uint8_t hdr[44];
    while (argc > 3 && argv[1][0] == '-') {      /* options before the file names */
        if (!strcmp(argv[1], "--interp")) wt_cfg.interp = atoi(argv[2]);
        else if (!strcmp(argv[1], "--bwdiv")) wt_cfg.bwdiv = atoi(argv[2]);
        else if (!strcmp(argv[1], "--taper")) wt_cfg.taper = atoi(argv[2]);
        else if (!strcmp(argv[1], "--morph")) wt_cfg.morph = atoi(argv[2]);
        else break;
        argv += 2; argc -= 2;
    }
    if (argc < 3) { fprintf(stderr, "usage: wtrender [--interp 1|3] [--bwdiv 2|4|8] [--taper PCT] [--morph 0|1] script.wts out.wav\n"); return 2; }
    f = fopen(argv[1], "r");
    if (!f) { perror(argv[1]); return 1; }
    s = (wt_synth_t *)calloc(1, sizeof *s);
    wt_synth_init(s);
    wt_gen_basic_shapes(&tb[0]); wt_gen_fold_sine(&tb[1]); wt_gen_odd_even(&tb[2]);
    for (i = 0; i < 3; i++) { wt_synth_set_table(s, i, &tb[i]); tab_frames[i] = tb[i].nsrc; }
    wt_gen_samples(sm);
    for (i = 0; i < 3; i++) wt_synth_set_sample(s, i, &sm[i]);

    while (fgets(line, sizeof line, f)) {
        char c[16], a[64], b[64], d[64];
        char *h = strchr(line, '#');
        int n;
        if (h) *h = 0;
        n = sscanf(line, "%15s %63s %63s %63s", c, a, b, d);
        if (n < 1) continue;
        if (!strcmp(c, "patch")) { if (patch_apply(a, set_param, NULL)) fprintf(stderr, "unknown patch %s\n", a); }
        else if (!strcmp(c, "set") && n >= 3) set_param(NULL, a, b);
        else if (!strcmp(c, "frame") && n >= 3) {
            int o = toupper(a[0]) == 'B', k = atoi(b), id = P_OSC(o, O_TABLE);
            int tbl = 0;                      /* the table in effect at the cursor: replay the events so far */
            for (i = 0; i < nev; i++) if (evs[i].kind == EV_SET && evs[i].a == id) tbl = evs[i].b;
            {
                int nf = tab_frames[tbl];
                char val[16];
                snprintf(val, sizeof val, "%d", nf > 1 ? (k * 1000 + (nf - 1) / 2) / (nf - 1) : 0);
                set_param(NULL, o ? "B_WTPOS" : "A_WTPOS", val);
            }
        }
        else if (!strcmp(c, "at")) cursor = (long)(atof(a) * WT_FS);
        else if (!strcmp(c, "note") && n >= 4) {
            long dur = (long)(atof(d) * WT_FS);
            add(cursor, EV_ON, atoi(a), atoi(b)); add(cursor + dur, EV_OFF, atoi(a), 0);
        }
        else if (!strcmp(c, "on") && n >= 3) add(cursor, EV_ON, atoi(a), atoi(b));
        else if (!strcmp(c, "off")) add(cursor, EV_OFF, atoi(a), 0);
        else if (!strcmp(c, "bend")) add(cursor, EV_BEND, atoi(a), 0);
        else if (!strcmp(c, "mod")) add(cursor, EV_MOD, atoi(a), 0);
        else if (!strcmp(c, "play")) add(cursor, EV_PLAY, 0, 0);
        else if (!strcmp(c, "stop")) add(cursor, EV_STOP, 0, 0);
        else if (!strcmp(c, "pattern")) add(cursor, EV_QUEUE, atoi(a) - 1, 0);
        else if (!strcmp(c, "len")) len = (long)(atof(a) * WT_FS);
        else fprintf(stderr, "ignored: %s", line);
    }
    fclose(f);
    for (i = 0; i < nev; i++) if (evs[i].t > maxt) maxt = evs[i].t;
    total = len >= 0 ? len : maxt + 2 * WT_FS;
    total = (total + WT_CTL - 1) / WT_CTL * WT_CTL;
    /* stable sort by time (insertion; scripts are small) */
    for (i = 1; i < nev; i++) {
        ev_t e = evs[i];
        int j = i - 1;
        while (j >= 0 && evs[j].t > e.t) { evs[j + 1] = evs[j]; j--; }
        evs[j + 1] = e;
    }
    buf = (int16_t *)malloc((size_t)total * 2 * sizeof(int16_t));
    for (long pos = 0; pos < total; pos += WT_CTL) {
        while (ei < nev && evs[ei].t < pos + WT_CTL) {
            ev_t *e = &evs[ei++];
            switch (e->kind) {
            case EV_SET: wt_synth_set_param(s, e->a, e->b); break;
            case EV_ON: wt_synth_key_on(s, e->a, e->b); break;
            case EV_OFF: wt_synth_key_off(s, e->a); break;
            case EV_PLAY: wt_seq_play(s, 1); break;
            case EV_STOP: wt_seq_play(s, 0); break;
            case EV_QUEUE: wt_seq_queue(s, e->a); break;
            case EV_BEND: wt_synth_bend(s, e->a); break;
            default: wt_synth_modwheel(s, e->a); break;
            }
        }
        wt_synth_render(s, buf + pos * 2, WT_CTL);
        if (wt_synth_active_voices(s) > maxv) maxv = wt_synth_active_voices(s);
    }
    for (long k = 0; k < total * 2; k++) {
        double x = buf[k] / 32768.0;
        if (fabs(x) > peak) peak = fabs(x);
        sumsq += x * x; nn++;
        hash = (hash ^ (uint16_t)buf[k]) * 16777619u;
    }
    w = fopen(argv[2], "wb");
    if (!w) { perror(argv[2]); return 1; }
    memcpy(hdr, "RIFF\0\0\0\0WAVEfmt \x10\0\0\0\x01\0\x02\0\x44\xAC\0\0\x10\xB1\x02\0\x04\0\x10\0data\0\0\0\0", 44);
    {
        uint32_t db = (uint32_t)(total * 4), rb = db + 36;
        memcpy(hdr + 4, &rb, 4); memcpy(hdr + 40, &db, 4);
    }
    fwrite(hdr, 1, 44, w); fwrite(buf, 2, (size_t)total * 2, w); fclose(w);
    printf("%s: %.2f s, peak %.1f dBFS, rms %.1f dBFS, max voices %d, hash %08x, voice-samples %d\n", argv[2],
           (double)total / WT_FS, 20 * log10(peak + 1e-12), 10 * log10(sumsq / (nn ? nn : 1) + 1e-18), maxv, hash,
           s->cpu_samples);
    return 0;
}
