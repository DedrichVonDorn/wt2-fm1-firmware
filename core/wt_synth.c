/* SPDX-License-Identifier: GPL-3.0-only
 * WT-1 core: voices, wavetable oscillators (unison, warp), filter, envelopes, LFOs, mod matrix.
 * Integer only. Control values are recomputed every WT_CTL samples. */
#include "wt_int.h"
#if !WT_NODRUMS
#include "wt_drums.h"
#endif
#ifdef WT_DEVICE                    /* firmware/src/libc.c has exactly these two */
void *memset(void *d, int c, unsigned n);
void *memcpy(void *d, const void *s, unsigned n);
#else
#include <string.h>
#endif

/* ---------------------------------------------------------------- parameters */
const char *const wt_src_names[S_COUNT] = {"NONE", "LFO1", "LFO2", "ENV2", "ENV3", "VEL", "NOTE", "MAC1", "MAC2", "MAC3",
                                           "MAC4", "RAND", "MOD", "ACCENT"};
const char *const wt_dst_names[D_COUNT] = {"NONE", "A_WTPOS", "B_WTPOS", "A_WARP", "B_WARP", "A_DETUNE", "B_DETUNE",
                                           "A_LEVEL", "B_LEVEL", "A_PAN", "B_PAN", "A_PITCH", "B_PITCH", "A_FM", "B_FM",
                                           "A_MORPH", "B_MORPH", "CUTOFF", "RESO", "DRIVE", "SUB_PW"
#if WT_NODRUMS
                                           , "SUB_LVL", "NOI_LVL", "AMP_ATK", "AMP_DEC", "AMP_REL", "LFO1_RATE", "LFO2_RATE",
                                           "CH_MIX", "DL_SEND", "RV_SEND", "MASTER"
#endif
                                           };

wt_param_desc_t wt_param_desc[P_COUNT];
static char pname[P_COUNT][20];
static int pdesc_ready;

/* name = a + decimal n (omitted when n < 0) + b, without libc */
static void mkname(char *d, const char *a, int n, const char *b)
{
    while (*a) *d++ = *a++;
    if (n >= 0) { if (n >= 10) *d++ = (char)('0' + n / 10); *d++ = (char)('0' + n % 10); }
    while (*b) *d++ = *b++;
    *d = 0;
}
static void pd(int id, const char *name, int mn, int mx, int def)
{
    mkname(pname[id], name, -1, "");
    wt_param_desc[id].name = pname[id];
    wt_param_desc[id].min = (int16_t)mn; wt_param_desc[id].max = (int16_t)mx; wt_param_desc[id].def = (int16_t)def;
}

static void params_init(void)
{
    int o, i, f;
    char nm[24];
    static const char *const osc_pre[2] = {"A_", "B_"};
    static const char *of[O_SIZE] = {"ON", "TABLE", "WTPOS", "WARPMODE", "WARP", "OCT", "SEMI", "FINE", "PHASE", "RAND",
                                     "UNI", "DETUNE", "BLEND", "SPREAD", "LEVEL", "PAN", "FM", "PD", "TABLE2", "MORPH", "FLT"};
    static const int omin[O_SIZE] = {0, 0, 0, 0, -1000, -3, -12, -100, 0, 0, 1, 0, 0, 0, 0, -1000, 0, 0, 0, 0, 0};
    static const int omax[O_SIZE] = {1, WT_NTABLES - 1, 1000, 3, 1000, 3, 12, 100, 360, 100, WT_MAXUNI, 1000, 1000, 1000,
                                     1000, 1000, 1000, 1000, WT_NTABLES - 1, 1000, 1};
    static const int odef[O_SIZE] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 250, 750, 500, 700, 0, 0, 0, 0, 0, 1};
    static const char *ef[E_SIZE] = {"DLY", "ATK", "HLD", "DEC", "SUS", "REL"};
    if (pdesc_ready) return;
    pdesc_ready = 1;
    pd(P_MASTER, "MASTER", 0, 1000, 800); pd(P_TUNE, "TUNE", -100, 100, 0); pd(P_VOICEMODE, "VOICEMODE", 0, 8, 0);
    pd(P_GLIDE, "GLIDE", 0, 1000, 0); pd(P_BEND, "BEND", 0, 24, 2); pd(P_VELAMP, "VELAMP", 0, 1000, 500);
    for (o = 0; o < 2; o++)
        for (f = 0; f < O_SIZE; f++) {
            mkname(nm, osc_pre[o], -1, of[f]);
#if WT_NODRUMS
            pd(P_OSC(o, f), nm, omin[f], f == O_WARPMODE ? 6 : omax[f], f == O_ON ? (o == 0) : odef[f]);
#else
            pd(P_OSC(o, f), nm, omin[f], omax[f], f == O_ON ? (o == 0) : odef[f]);
#endif
        }
    pd(P_SUB_ON, "SUB_ON", 0, 1, 0); pd(P_SUB_WAVE, "SUB_WAVE", 0, 3, 0); pd(P_SUB_OCT, "SUB_OCT", -3, 3, -1);
    pd(P_SUB_CRS, "SUB_CRS", -1200, 1200, 0); pd(P_SUB_PW, "SUB_PW", 0, 1000, 500); pd(P_SUB_PHASE, "SUB_PHASE", 0, 360, 0);
    pd(P_SUB_CONT, "SUB_CONT", 0, 1, 0); pd(P_SUB_LEVEL, "SUB_LEVEL", 0, 1000, 500); pd(P_SUB_PAN, "SUB_PAN", -1000, 1000, 0);
    pd(P_SUB_FLT, "SUB_FLT", 0, 1, 1);
    pd(P_NOI_ON, "NOI_ON", 0, 1, 0); pd(P_NOI_TYPE, "NOI_TYPE", 0, 4, 0); pd(P_NOI_STEREO, "NOI_STEREO", 0, 1000, 0);
    pd(P_NOI_FILTER, "NOI_FILTER", -1000, 1000, 0); pd(P_NOI_LEVEL, "NOI_LEVEL", 0, 1000, 200);
    pd(P_NOI_PAN, "NOI_PAN", -1000, 1000, 0); pd(P_NOI_FLT, "NOI_FLT", 0, 1, 1);
    pd(P_NOI_SMP, "NOI_SMP", 0, WT_NSAMPLES, 0); pd(P_NOI_SMP_LEVEL, "NOI_SMP_LEVEL", 0, 1000, 500);
    pd(P_NOI_SMP_PITCH, "NOI_SMP_PITCH", -24, 24, 0); pd(P_NOI_SMP_FINE, "NOI_SMP_FINE", -100, 100, 0);
    pd(P_NOI_SMP_KEY, "NOI_SMP_KEY", 0, 1, 0); pd(P_NOI_SMP_START, "NOI_SMP_START", 0, 1000, 0);
    pd(P_NOI_SMP_RAND, "NOI_SMP_RAND", 0, 1000, 0); pd(P_NOI_SMP_LOOP, "NOI_SMP_LOOP", 0, 1, 1);
    pd(P_F_TYPE, "F_TYPE", 0, 6, 0); pd(P_F_CUTOFF, "F_CUTOFF", 0, 1350, 1000); pd(P_F_RESO, "F_RESO", 0, 1000, 200);
    pd(P_F_DRIVE, "F_DRIVE", 0, 1000, 0); pd(P_F_KEYTRK, "F_KEYTRK", 0, 1000, 0); pd(P_F_MIX, "F_MIX", 0, 1000, 1000);
    for (i = 0; i < 3; i++)
        for (f = 0; f < E_SIZE; f++) {
            mkname(nm, i == 0 ? "AMP_" : i == 1 ? "ENV2_" : "ENV3_", -1, ef[f]);
            pd(P_ENV(i, f), nm, 0, 1000, f == E_DEC ? 500 : f == E_SUS ? (i == 0 ? 1000 : 0) : f == E_REL ? 300 : 0);
        }
    for (i = 0; i < 2; i++) {
        mkname(nm, "LFO", i + 1, "_SHAPE"); pd(P_LFO(i, 0), nm, 0, WT_LFOSHAPES ? 6 + WT_NUSHAPE : 6, 0);
        mkname(nm, "LFO", i + 1, "_RATE"); pd(P_LFO(i, 1), nm, 0, 1000, 400);
    }
    for (i = 0; i < 4; i++) { mkname(nm, "MACRO", i + 1, ""); pd(P_MAC0 + i, nm, 0, 1000, 0); }
    {
        static const char *const fxn[FX_NPARAMS] = {"FX_VOL", "FX_DIST", "FX_DRIVE", "FX_COMP", "RV_DECAY", "RV_TONE", "RV_HPF",
                                                    "RV_LEVEL", "DL_TIME", "DL_FDBK", "DL_TONE", "DL_LEVEL", "DL_HPF", "DL_TYPE",
                                                    "DL_WEAR", "DL_PING"};
        for (f = 0; f < FX_NPARAMS; f++) {
            const wt_fxp_t *q = fxbus_param(f);
            pd(P_FX0 + f, fxn[f], 0, q->max, q->def);
        }
        pd(P_FX_RVSEND, "RV_SEND", 0, 127, 0); pd(P_FX_DLSEND, "DL_SEND", 0, 127, 0); pd(P_BPM, "BPM", 40, 240, 120);
        pd(P_ACCENT, "ACCENT", 0, 1000, 600); pd(P_SLIDE, "SLIDE", 0, 1000, 300); pd(P_CPUMODE, "CPU_MODE", 0, 2, 1);
        pd(P_ARP_ON, "ARP_ON", 0, 1, 0); pd(P_ARP_MODE, "ARP_MODE", 0, AM_N - 1, 0); pd(P_ARP_RATE, "ARP_RATE", 0, 5, 3); pd(P_ARP_OCT, "ARP_OCT", 1, 4, 1);
        pd(P_ARP_GATE, "ARP_GATE", 5, 100, 60); pd(P_ARP_SWING, "ARP_SWING", 50, 75, 50); pd(P_ARP_LATCH, "ARP_LATCH", 0, 1, 0);
        pd(P_EQ_ON, "EQ_ON", 0, 1, 1); pd(P_EQ_LOW, "EQ_LOW", -120, 120, 0); pd(P_EQ_LOWF, "EQ_LOWF", 40, 400, 120);
        pd(P_EQ_MID, "EQ_MID", -120, 120, 0); pd(P_EQ_MIDF, "EQ_MIDF", 200, 8000, 1000); pd(P_EQ_Q, "EQ_Q", 3, 40, 10);
        pd(P_EQ_HIGH, "EQ_HIGH", -120, 120, 0); pd(P_EQ_HIGHF, "EQ_HIGHF", 2000, 12000, 6000);
#if WT_NODRUMS
        pd(P_DR_KIT, "UNI_TOP", 0, 1, 0);
#else
        pd(P_DR_KIT, "DR_KIT", 0, 1, 0);
#endif
        pd(P_DR_LEVEL, WT_LFOSHAPES ? "CH_DEPTH" : "DR_LEVEL", 0, 1000, WT_LFOSHAPES ? 500 : 800); pd(P_DR_SEQ, "DR_SEQ", 0, 1, 1); pd(P_DR_ACCENT, "DR_ACCENT", -127, 255, 255); pd(P_DR_SOFT, "DR_SOFT", -127, 255, 25); pd(P_CORE2, "CORE2", 0, 1, 1);
        pd(P_SYN_DIST, "SYN_DIST", 0, 7, 0); pd(P_SYN_DRIVE, "SYN_DRIVE", 0, 127, 40);
#if WT_NODRUMS
        pd(P_DR_DIST, "HYPER", 0, 7, 0);
#else
        pd(P_DR_DIST, "DR_DIST", 0, 7, 0);
#endif
        pd(P_DR_DRIVE, "DR_DRIVE", 0, 127, 40);
        pd(P_DR_RVSEND, "DR_RVSEND", 0, 127, 127); pd(P_DR_DLSEND, "DR_DLSEND", 0, 127, 127); pd(P_SYN_LEVEL, "SYN_LEVEL", 0, 1000, 1000);
        pd(P_SC_AMT, WT_LFOSHAPES ? "CH_MIX" : "SC_AMT", 0, 1000, 0); pd(P_SC_REL, WT_LFOSHAPES ? "CH_RATE" : "SC_REL", 0, 1000, WT_LFOSHAPES ? 350 : 450);
        pd(P_SCALE, "SCALE", 0, WT_NSCALES - 1, 0); pd(P_ROOT, "ROOT", 0, 11, 0); pd(P_SCALE_SEQ, "SCALE_SEQ", 0, 1, 0);
        pd(P_SYN_MUTE, "SYN_MUTE", 0, 2, 0); pd(P_DR_MUTE, "DR_MUTE", 0, 2, 0);
        pd(P_MIDI_CH, "MIDI_CH", 0, 16, 0); pd(P_MIDI_DRCH, "MIDI_DRCH", 0, 16, 10); pd(P_MIDI_OUTCH, "MIDI_OUTCH", 0, 16, 0); pd(P_USB_AUDIO, "USB_AUDIO", 0, 1, 1);
        pd(P_CLK_SRC, "CLK_SRC", 0, 2, 0); pd(P_CLK_SS, "CLK_SS", 0, 1, 1);
        pd(P_LFO1_SYNC, "LFO1_SYNC", 0, 15, 0); pd(P_LFO2_SYNC, "LFO2_SYNC", 0, 15, 0);
        pd(P_LFO1_KEY, "LFO1_KEY", 0, 2, 0); pd(P_LFO2_KEY, "LFO2_KEY", 0, 2, 0);
        pd(P_LFO1_POL, "LFO1_POL", 0, 2, 0); pd(P_LFO2_POL, "LFO2_POL", 0, 2, 0);
        {   /* the P37's fader banks: MIX, AMP ENV, MOD, OSC */
            static const int DEF[16] = {P_MASTER, P_SYN_LEVEL, P_DR_LEVEL, P_FX_RVSEND,
                                        P_ENV(0, E_ATK), P_ENV(0, E_DEC), P_ENV(0, E_SUS), P_ENV(0, E_REL),
                                        P_LFO(0, 1), P_LFO(1, 1), P_MAC0, P_MAC0 + 1,
                                        P_OSC(0, O_WTPOS), P_OSC(1, O_WTPOS), P_OSC(0, O_WARP), P_OSC(1, O_WARP)};
            int f;
            for (f = 0; f < 8; f++) {
                char nm[8] = "FDR0";
                nm[3] = (char)('0' + f);
                pd(P_FDR0 + f, nm, -32768, 32767, (int)(int16_t)(uint16_t)((DEF[2 * f] + 1) | ((DEF[2 * f + 1] + 1) << 8)));
            }
        }
    }
    for (i = 0; i < 8; i++) {
        mkname(nm, "MX", i + 1, "_SRC"); pd(P_MX(i, 0), nm, 0, S_COUNT - 1, 0);
        mkname(nm, "MX", i + 1, "_DST"); pd(P_MX(i, 1), nm, 0, D_COUNT - 1, 0);
        mkname(nm, "MX", i + 1, "_AMT"); pd(P_MX(i, 2), nm, -1000, 1000, 0);
    }
}

static int up(int c) { return c >= 'a' && c <= 'z' ? c - 32 : c; }

int wt_param_find(const char *name)
{
    int i;
    params_init();
    for (i = 0; i < P_COUNT; i++) {
        const char *a = wt_param_desc[i].name, *b = name;
        while (*a && *b && up(*a) == up(*b)) { a++; b++; }
        if (!*a && !*b) return i;
    }
    return -1;
}

/* ---------------------------------------------------------------- helpers */
static inline int32_t clampi(int32_t x, int32_t lo, int32_t hi) { return x < lo ? lo : x > hi ? hi : x; }
static inline uint32_t rnd32(wt_synth_t *s)
{
    uint32_t x = s->rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return s->rng = x;
}

/* ---------------------------------------------------------------- envelopes (level is Q30) */
enum { ES_IDLE, ES_DLY, ES_ATK, ES_HLD, ES_DEC, ES_SUS, ES_REL };
#define ONE 0x40000000

static void env_on(wt_env_t *e, const int16_t *p)
{
    e->st = p[E_DLY] ? ES_DLY : ES_ATK;
    e->cnt = wt_samples[p[E_DLY]];
}
static void env_off(wt_env_t *e) { if (e->st != ES_IDLE) e->st = ES_REL; }

static inline int32_t env_tick(wt_env_t *e, const int16_t *p)
{
    switch (e->st) {
    case ES_DLY: if (e->cnt) e->cnt--; else e->st = ES_ATK; break;
    case ES_ATK:
        e->lvl += (int32_t)wt_atk_step[p[E_ATK]];
        if (e->lvl >= ONE || e->lvl < 0) { e->lvl = ONE; e->st = ES_HLD; e->cnt = wt_samples[p[E_HLD]]; }
        break;
    case ES_HLD: if (e->cnt) e->cnt--; else e->st = ES_DEC; break;
    case ES_DEC: {
        int32_t sus = p[E_SUS] >= 1000 ? ONE : p[E_SUS] * 1073741;
        int32_t d = (int32_t)(((int64_t)(sus - e->lvl) * wt_coef[p[E_DEC]]) >> 24);
        if (d == 0) d = sus > e->lvl ? 1 : -1;
        e->lvl += d;
        if ((e->lvl > sus ? e->lvl - sus : sus - e->lvl) < (ONE >> 12)) { e->lvl = sus; e->st = ES_SUS; }
        break; }
    case ES_SUS: e->lvl = p[E_SUS] >= 1000 ? ONE : p[E_SUS] * 1073741; break;
    case ES_REL: {
        int32_t d = (int32_t)(((int64_t)e->lvl * wt_coef[p[E_REL]]) >> 24);
        e->lvl -= d < 1 ? 1 : d;
        if (e->lvl < (ONE >> 14)) { e->lvl = 0; e->st = ES_IDLE; }
        break; }
    default: break;
    }
    return e->lvl >> 15;
}

/* ---------------------------------------------------------------- oscillator read */
static inline int32_t rd(const int16_t *p, int sh, uint32_t ph, int cubic)
{
    uint32_t mask = (1u << sh) - 1u, i = ph >> (32 - sh), fr = (ph << sh) >> 16;
    int32_t y0 = p[i], y1 = p[(i + 1) & mask];
    if (!cubic) return y0 + (((y1 - y0) * (int32_t)(fr >> 1)) >> 15);
    {   /* Catmull-Rom in 32 bits (the fraction in Q10: every product stays under 2^30): the 64-bit version cost three 64-bit multiplies per read */
        int32_t ym = p[(i - 1) & mask], y2 = p[(i + 2) & mask], x = (int32_t)(fr >> 6), t;
        t = ((3 * (y0 - y1) + y2 - ym) * x) >> 10;
        t += 2 * ym - 5 * y0 + 4 * y1 - y2;
        t = (t * x) >> 10;
        t += y1 - ym;
        t = (t * x) >> 10;
        return y0 + (t >> 1);
    }
}

static inline uint32_t warp_phase(uint32_t p, int mode, int32_t amt)
{
    switch (mode) {
    case 1: {   /* BEND: x + a x (1 - x) */
        int64_t x = p >> 16, a = amt;
        int64_t y = x + ((((a * x * (65536 - x)) >> 16) * 1049) >> 20);     /* about / 1000 */
        return (uint32_t)clampi((int32_t)y, 0, 65535) << 16; }
    case 2: {   /* SYNC: ratio 1 .. 8 */
        uint32_t r = 256u + (uint32_t)(clampi(amt, 0, 1000) * 7 * 256 / 1000);
        return (uint32_t)(((uint64_t)p * r) >> 8); }
    case 3: {   /* MIRROR: crossfade to a folded phase */
        int32_t x = (int32_t)(p >> 16), tri = x < 32768 ? x * 2 : (65535 - x) * 2;
        return (uint32_t)(x + ((tri - x) * clampi(amt, 0, 1000)) / 1000) << 16; }
    case 4: {   /* ASYM: the bend sits on the first half of the cycle; the second half stays linear */
        int32_t x = (int32_t)(p >> 16), a = amt, y;
        if (x < 32768) {
            int64_t u = (int64_t)x * 2;
            if (u > 65535) u = 65535;
            y = (int32_t)(u + ((((a * u * (65536 - u)) >> 16) * 1049) >> 20));
            y = clampi(y, 0, 65535) >> 1;
        } else y = x;
        return (uint32_t)y << 16; }
    case 5: {   /* QUANTIZE: the phase snaps to 2..16 steps across the cycle */
        int steps = 2 + clampi(amt, 0, 1000) * 14 / 1000;
        int32_t x = (int32_t)(p >> 16);
        int32_t q = (int32_t)(((int64_t)x * steps) >> 16);
        if (q >= steps) q = steps - 1;
        return (uint32_t)((q * 65536) / steps) << 16; }
    case 6: {   /* PWM: a window that narrows as the amount rises, then stretches to fill the cycle */
        int32_t x = (int32_t)(p >> 16);
        int32_t pw = 32768 - clampi(amt, 0, 1000) * (32768 - 4096) / 1000;
        int32_t y = x < pw ? (int32_t)((int64_t)x * 32768 / pw) : (int32_t)(32768 + (int64_t)(x - pw) * 32768 / (65536 - pw));
        return (uint32_t)clampi(y, 0, 65535) << 16; }
    default: return p;
    }
}

/* ---------------------------------------------------------------- filter */
static inline int32_t svf(const wt_voice_t *v, int32_t *st, int32_t x, int type)
{
    int64_t v3 = (int64_t)x - st[1];
    int32_t v1 = (int32_t)(((int64_t)v->fa1 * st[0] + (int64_t)v->fa2 * v3) >> 24);
    int32_t v2 = st[1] + (int32_t)(((int64_t)v->fa2 * st[0] + (int64_t)v->fa3 * v3) >> 24);
    int32_t kv1 = (int32_t)(((int64_t)v->fk * v1) >> 24);
    st[0] = clampi(2 * v1 - st[0], -3000000, 3000000);
    st[1] = clampi(2 * v2 - st[1], -3000000, 3000000);
    switch (type) {
    case 3: return x - kv1 - v2;                    /* HP */
    case 4: return kv1;                             /* BP, unity at the peak */
    case 5: return x - kv1;                         /* notch */
    default: return v2;                             /* LP */
    }
}

static inline int32_t svf_b(const wt_voice_t *v, int32_t *st, int32_t x)     /* the same filter with the second formant's coefficients: its band-pass */
{
    int64_t v3 = (int64_t)x - st[1];
    int32_t v1 = (int32_t)(((int64_t)v->fb1 * st[0] + (int64_t)v->fb2 * v3) >> 24);
    int32_t v2 = st[1] + (int32_t)(((int64_t)v->fb2 * st[0] + (int64_t)v->fb3 * v3) >> 24);
    st[0] = clampi(2 * v1 - st[0], -3000000, 3000000);
    st[1] = clampi(2 * v2 - st[1], -3000000, 3000000);
    return (int32_t)(((int64_t)v->fkb * v1) >> 24);
}

/* VOWEL: two band-pass formants that glide A - E - I - O - U as the CUTOFF goes up; RESONANCE narrows them. Formant centres in 0.1 semitone
 * from 8.18 Hz (the cutoff table's index): F1 730 530 270 570 300 Hz, F2 1090 1840 2290 840 870 Hz. */
static const int16_t VOW_F1[5] = {778, 722, 605, 735, 624}, VOW_F2[5] = {847, 938, 976, 802, 808};
static void set_coef(int64_t g, int64_t k, int32_t *a1, int32_t *a2, int32_t *a3)
{
    int64_t den = (1 << 24) + ((g * (g + k)) >> 24);
    *a1 = (int32_t)((1ll << 48) / den);
    *a2 = (int32_t)((g * *a1) >> 24);
    *a3 = (int32_t)((g * *a2) >> 24);
}
static inline int32_t softclip(int32_t x)           /* x - x^3 / 6.75, input clamped to +-1.5 (Q15) */
{
    int32_t t;
    x = clampi(x, -49151, 49151);
    t = (int32_t)(((int64_t)x * x) >> 15);          /* 49151^2 does not fit 32 bits */
    t = (int32_t)(((int64_t)t * x) >> 15);
    return x - (int32_t)(((int64_t)t * 4854) >> 15);
}

/* ---------------------------------------------------------------- sub oscillator (analytic, anti-aliased) */
#define SUBAMP 29490
static wt_table_t g_saw;                          /* one band-limited saw cycle, shared */
#ifdef WT_DEVICE
extern const int16_t wt_saw_frame[WT_N];          /* generated (tools/gen_wt_data) */
#if WT_NODRUMS
static int16_t g_saw_mem[1152] WT_POOL;           /* the sub's saw, in the pool on WT-2 (the 96 KB RAM is the tight one) */
#else
static int16_t g_saw_mem[1152];                   /* 7 levels of one frame: 512 + 256 + 128 + 4 x 64 samples */
#endif
static void saw_init(void)
{
    if (!g_saw.mem) wt_table_build_levels(&g_saw, "SAW", wt_saw_frame, 1, 1, 1, 0, g_saw_mem, 1152);
}
#else
static void saw_init(void) { if (!g_saw.mem) wt_gen_saw(&g_saw); }
#endif

static inline int32_t sub_sample(uint32_t ph, int wave)
{
    switch (wave) {
    case 0: {   /* sine */
        uint32_t i = ph >> 22, fr = (ph >> 6) & 0xFFFF;
        int32_t a = wt_sin1024[i], b = wt_sin1024[(i + 1) & 1023];
        return (int32_t)(((int64_t)(a + (((b - a) * (int32_t)(fr >> 1)) >> 15)) * SUBAMP) >> 15); }
    case 1: {   /* triangle, zero at the start of the cycle and rising */
        int32_t t = (int32_t)(((ph >> 16) + 16384u) & 0xFFFF);
        int32_t y = t < 32768 ? t * 2 - 32768 : 98303 - t * 2;
        return (int32_t)(((int64_t)y * SUBAMP) >> 15); }
    default: return 0;      /* square and pulse are read from the band-limited saw (voice_render) */
    }
}

/* ---------------------------------------------------------------- noise colours */
static inline int32_t noise_color(wt_synth_t *s, wt_voice_t *v, int ch, int type)
{
    int32_t w = (int32_t)(rnd32(s) >> 16) - 32768;
    switch (type) {
    case 0: return w;
    case 1: {   /* pink: Paul Kellet's three-pole filter */
        int32_t *b = v->nz_pk[ch];
        b[0] = (int32_t)(((int64_t)b[0] * 32687) >> 15) + ((w * 3245) >> 15);
        b[1] = (int32_t)(((int64_t)b[1] * 31554) >> 15) + ((w * 9716) >> 15);
        b[2] = (int32_t)(((int64_t)b[2] * 18678) >> 15) + ((w * 34495) >> 15);
        return ((b[0] + b[1] + b[2] + ((w * 6056) >> 15)) * 6554) >> 15; }
    case 2:     /* brown: a leaky integrator of white */
        v->nz_brown[ch] = clampi(((v->nz_brown[ch] * 32702) >> 15) + (w >> 5), -32767, 32767);
        return v->nz_brown[ch];
    default:    /* geiger: random bursts of noise that die away */
        if ((rnd32(s) >> 20) < 3u) v->nz_geig[ch] = 20000 + (int32_t)((rnd32(s) >> 18) & 0x1FFF);
        v->nz_geig[ch] = (v->nz_geig[ch] * 31000) >> 15;
        return (int32_t)(((int64_t)v->nz_geig[ch] * w) >> 14);
    }
}

/* ---------------------------------------------------------------- effects */
static int16_t g_dly[WT_DLY_LEN] WT_POOL;          /* the delay line (one synth per program) */
/* the second core's hooks live with the synth: WT-2 does not link the drum unit that used to define them */
int (*wt_par_run)(void (*fn)(uint32_t), uint32_t arg);
int (*wt_par_wait)(void);

static void fx_push(wt_synth_t *s, int id)          /* a parameter changed: tell the effects */
{
    if (id >= P_FX0 && id < P_FX0 + FX_NPARAMS) fxbus_set(&s->fx, id - P_FX0, s->p[id]);
    else if (id == P_BPM) fxbus_set_bpm(&s->fx, (float)s->p[P_BPM]);
}

static void fx_reset(wt_synth_t *s)
{
    int i;
    fxbus_init(&s->fx, g_dly, WT_DLY_LEN);
    for (i = 0; i < FX_NPARAMS; i++) fxbus_set(&s->fx, i, s->p[P_FX0 + i]);
    fxbus_set_bpm(&s->fx, (float)s->p[P_BPM]);
}

/* Runs the effects on one block of the voices' sum: dry in, X0X's chain (reverb and delay returns, distortion, glue compressor) out.
 * Off (no distortion, no compressor, no sends) and silent: not touched, so a dry sound is exactly what it was. */
/* X0X's effects are made for a mix that peaks near full scale; the voices' sum is 12 dB lower, so the chain gets it 4 x hotter and gives it back 4 x quieter */
#define FX_IN 4.0f
#define FX_OUT 0.25f

/* smono: the synth alone, mono, in the effects' units, when the drums are in the mix too (the synth's sends must not carry the drums); null = the mix itself */
static void fx_block(wt_synth_t *s, int32_t *accL, int32_t *accR, const float *drev, const float *ddly, const float *smono)
{
    const int16_t *P = s->p;
    int rsv = P[P_FX_RVSEND], dsv = P[P_FX_DLSEND];
    int on;
    float dl[WT_CTL], dr[WT_CTL], rv[WT_CTL], dy[WT_CTL], ol[WT_CTL], orr[WT_CTL], rs, ds;
    int i;
#if WT_NODRUMS
    rsv = clampi(rsv + s->hold_gd[4], 0, 127);
    dsv = clampi(dsv + s->hold_gd[3], 0, 127);
#endif
    on = P[P_FX0 + FX_DIST] || P[P_FX0 + FX_COMP] || rsv || dsv || drev;
    if (!on && s->fx.rv_quiet >= FX_RV_QUIET_LEN && s->fx.dl_quiet >= s->fx.dlen) { s->fx_ran = 0; return; }
    rs = (float)rsv * (1.0f / 127.0f);
    ds = (float)dsv * (1.0f / 127.0f);
    for (i = 0; i < WT_CTL; i++) {
        float m;
        dl[i] = (float)accL[i] * (FX_IN / 32768.0f);
        dr[i] = (float)accR[i] * (FX_IN / 32768.0f);
        m = smono ? smono[i] : (dl[i] + dr[i]) * 0.5f;
        rv[i] = m * rs;
        dy[i] = m * ds;
        if (drev) { rv[i] += drev[i]; dy[i] += ddly[i]; }
    }
    fxbus_process_st(&s->fx, dl, dr, rv, dy, ol, orr, WT_CTL);
    for (i = 0; i < WT_CTL; i++) {
        float l = ol[i] * (32768.0f * FX_OUT), r = orr[i] * (32768.0f * FX_OUT);
        accL[i] = (int32_t)(l > 1.0e8f ? 1.0e8f : l < -1.0e8f ? -1.0e8f : l);
        accR[i] = (int32_t)(r > 1.0e8f ? 1.0e8f : r < -1.0e8f ? -1.0e8f : r);
    }
    s->fx_ran = 1;
}

/* ---------------------------------------------------------------- EQ */
/* Low shelf, peak, high shelf: RBJ's cookbook biquads (shelf slope 1), float, transposed direct form II, stereo. Bypassed (no work)
 * while every gain is 0 dB. Coefficients are remade when a parameter changes. */
static void eq_coefs(wt_eq_t *e, const int16_t *P)
{
    static const int8_t KIND[3] = {0, 1, 2};      /* low shelf, peak, high shelf */
    int b;
    for (b = 0; b < 3; b++) {
        float db;
        float f, q = 0.7071f;
        int gi = b == 0 ? P_EQ_LOW : b == 1 ? P_EQ_MID : P_EQ_HIGH, fi = b == 0 ? P_EQ_LOWF : b == 1 ? P_EQ_MIDF : P_EQ_HIGHF;
        float A, w0, cs, sn, al, sa, a0;
        eq_coef_t *c = &e->c[b];
        db = (float)P[gi] * 0.1f;
        f = (float)P[fi];
        if (f > 20000.0f) f = 20000.0f;
        if (b == 1) q = (float)P[P_EQ_Q] * 0.1f;
        A = fm_db2lin(db * 0.5f);
        w0 = 6.2831853f * f * (1.0f / 44100.0f);
        cs = fm_cosf(w0); sn = fm_sinf(w0);
        if (KIND[b] == 1) {
            al = sn / (2.0f * q);
            c->b0 = 1.0f + al * A; c->b1 = -2.0f * cs; c->b2 = 1.0f - al * A;
            a0 = 1.0f + al / A; c->a1 = -2.0f * cs; c->a2 = 1.0f - al / A;
        } else {
            sa = 2.0f * fm_sqrtf(A) * (sn * 0.70710678f);                 /* 2 sqrt(A) alpha, slope 1: alpha = sin(w0) / sqrt(2) */
            if (KIND[b] == 0) {
                c->b0 = A * ((A + 1.0f) - (A - 1.0f) * cs + sa);
                c->b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cs);
                c->b2 = A * ((A + 1.0f) - (A - 1.0f) * cs - sa);
                a0 = (A + 1.0f) + (A - 1.0f) * cs + sa;
                c->a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cs);
                c->a2 = (A + 1.0f) + (A - 1.0f) * cs - sa;
            } else {
                c->b0 = A * ((A + 1.0f) + (A - 1.0f) * cs + sa);
                c->b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cs);
                c->b2 = A * ((A + 1.0f) + (A - 1.0f) * cs - sa);
                a0 = (A + 1.0f) - (A - 1.0f) * cs + sa;
                c->a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cs);
                c->a2 = (A + 1.0f) - (A - 1.0f) * cs - sa;
            }
        }
        a0 = 1.0f / a0;
        c->b0 *= a0; c->b1 *= a0; c->b2 *= a0; c->a1 *= a0; c->a2 *= a0;
    }
}

static void eq_block(wt_synth_t *s, int32_t *accL, int32_t *accR)
{
    const int16_t *P = s->p;
    wt_eq_t *e = &s->eq;
    int16_t key[8] = {P[P_EQ_LOW], P[P_EQ_LOWF], P[P_EQ_MID], P[P_EQ_MIDF], P[P_EQ_Q], P[P_EQ_HIGH], P[P_EQ_HIGHF], 0};
    int i, b, ch, flat = !P[P_EQ_ON] || (!P[P_EQ_LOW] && !P[P_EQ_MID] && !P[P_EQ_HIGH]);
    if (flat) { e->on = 0; return; }
    for (i = 0; i < 7; i++)
        if (key[i] != e->key[i] || !e->on) { eq_coefs(e, P); for (b = 0; b < 8; b++) e->key[b] = key[b]; break; }
    if (!e->on) { for (ch = 0; ch < 2; ch++) for (b = 0; b < 3; b++) e->z[ch][b][0] = e->z[ch][b][1] = 0.0f; e->on = 1; }
    for (ch = 0; ch < 2; ch++) {
        int32_t *acc = ch ? accR : accL;
        for (i = 0; i < WT_CTL; i++) {
            float x = (float)acc[i];
            for (b = 0; b < 3; b++) {
                const eq_coef_t *c = &e->c[b];
                float *z = e->z[ch][b], y = c->b0 * x + z[0];
                z[0] = fm_flush(c->b1 * x - c->a1 * y + z[1]);
                z[1] = fm_flush(c->b2 * x - c->a2 * y);
                x = y;
            }
            acc[i] = (int32_t)(x > 1.0e8f ? 1.0e8f : x < -1.0e8f ? -1.0e8f : x);
        }
    }
}

/* ---------------------------------------------------------------- synth */
#if !WT_NODRUMS
/* Master limiter: untouched below 0.6 of full scale, then a smooth curve (slope 1 at the knee, 0 at the end) that
 * reaches full scale at an input of 1.4. Transparent at ordinary levels, unlike a cubic through the origin.
 * WT-2 uses a gain rider instead (wt2_ride). */
static inline int32_t limiter(int32_t x)
{
    int32_t ax = x < 0 ? -x : x, y;
    const int32_t T = 19661, D = 26214;
    if (ax <= T) return x;
    ax -= T;
    if (ax > D) ax = D;
    y = T + ax - (ax * ax) / (2 * D);
    if (y > 32767) y = 32767;
    return x < 0 ? -y : y;
}
#endif

void wt_synth_defaults(wt_synth_t *s)
{
    int i;
    params_init();
    for (i = 0; i < P_COUNT; i++) s->p[i] = wt_param_desc[i].def;
    fx_reset(s);
}

void wt_synth_init(wt_synth_t *s)
{
#ifndef WT_DEVICE
    wt_global_init();
#endif
    params_init();
    memset(s, 0, sizeof *s);
    s->rng = 0x12345679u;
    s->lane_budget = WT_NVOICES * 2 * WT_MAXUNI;
    s->mix_syn_g = 1.0f;
#if WT_NODRUMS
    s->vc_g = 32768; s->lim_g = 32768;
#else
    s->sc_gain = 1.0f; s->mix_dr_g = 1.0f;
#endif
    s->lfo_rnd[0][0] = rnd32(s); s->lfo_rnd[0][1] = rnd32(s);
    s->lfo_rnd[1][0] = rnd32(s); s->lfo_rnd[1][1] = rnd32(s);
    saw_init();
    wt_synth_defaults(s);
    wt_seq_init(&s->seq);
}

void wt_synth_set_table(wt_synth_t *s, int slot, const wt_table_t *t)
{
    if (slot >= 0 && slot < WT_NTABLES) s->table[slot] = t;
}

void wt_synth_set_sample(wt_synth_t *s, int slot, const wt_sample_t *smp)
{
    if (slot < 0 || slot >= WT_NSAMPLES) return;
    if (smp) s->smp[slot] = *smp; else memset(&s->smp[slot], 0, sizeof s->smp[slot]);
    s->smp[slot].owned = 0;
}

void wt_synth_set_param(wt_synth_t *s, int id, int value)
{
    if (id < 0 || id >= P_COUNT) return;
    s->p[id] = (int16_t)clampi(value, wt_param_desc[id].min, wt_param_desc[id].max);
    fx_push(s, id);
}

static void voice_start(wt_synth_t *s, wt_voice_t *v, int note, int vel, int retrig, int glide_from_current, int accent)
{
    const int16_t *P = s->p;
    int o, u, was = v->active;
    v->target = note * 256;
    if (!(glide_from_current && was && P[P_GLIDE])) v->pitch = v->target;
    v->note = (uint8_t)note;
    v->accent = (uint8_t)(accent != 0);
    v->slide = 0;
    v->vel = vel * 32767 / 127;
    v->rnd = (int32_t)(rnd32(s) >> 17) - 16384;
    v->rnd *= 2;
    v->age = ++s->age;
    v->active = 1;
    v->gate = 1;
    if (!retrig) return;
    for (o = 0; o < 2; o++) {
        uint32_t base = (uint32_t)(((uint64_t)P[P_OSC(o, O_PHASE)] << 32) / 360u);
        for (u = 0; u < WT_MAXUNI; u++)
            v->ph[o][u] = base + (uint32_t)(((uint64_t)rnd32(s) * (uint32_t)P[P_OSC(o, O_RAND)]) / 100u);
    }
    v->phsub = P[P_SUB_CONT] ? s->last_subph : (uint32_t)(((uint64_t)P[P_SUB_PHASE] << 32) / 360u);
    {   /* sample layer: START plus a random share of the loop (Serum's START and RAND) */
        int idx = P[P_NOI_SMP];
        const wt_sample_t *sm = idx > 0 ? &s->smp[idx - 1] : NULL;
        v->sd = sm ? sm->d : NULL; v->sn = sm && sm->d ? sm->n : 0; v->sdone = 0;
        if (v->sn) {
            uint64_t off = (uint64_t)P[P_NOI_SMP_START] * (uint64_t)v->sn / 1000u +
                           (((uint64_t)rnd32(s) * ((uint64_t)P[P_NOI_SMP_RAND] * (uint64_t)v->sn / 1000u)) >> 32);
            v->spos = ((uint32_t)off % (uint32_t)v->sn) << 16;          /* 32-bit modulo: the device has no 64-bit divide */
        }
    }
    for (o = 0; o < 3; o++) env_on(&v->env[o], P + P_ENV(o, 0));
}

static void stack_drop(wt_synth_t *s, int note)
{
    int i, j;
    for (i = 0; i < s->nstack; i++)
        if (s->stack[i] == note) { for (j = i; j + 1 < s->nstack; j++) s->stack[j] = s->stack[j + 1]; s->nstack--; return; }
}

void (*wt_midi_out)(uint32_t pkt);
static void midi_note(wt_synth_t *s, int on, int note, int vel)
{
    int ch = s->p[P_MIDI_OUTCH];
    if (!wt_midi_out || !ch || s->no_midi_out) return;
    wt_midi_out((uint32_t)(on ? 9u : 8u) | ((uint32_t)((on ? 0x90 : 0x80) | (ch - 1)) << 8) | ((uint32_t)(note & 127) << 16) | ((uint32_t)(vel & 127) << 24));
}

int wt_syn_audible(const wt_synth_t *s)
{
    /* P_DR_MUTE stays in every patch. Solo on the drum machine (2) still silences the synth, so a stored patch sounds as it did. */
    int any = s->p[P_SYN_MUTE] == 2 || s->p[P_DR_MUTE] == 2;
#if !WT_NODRUMS
    int t;
    if (s->dr) for (t = 0; t < WT_DTRACKS; t++) any |= s->dr->mute[t] == 2;
#endif
    return s->p[P_SYN_MUTE] != 1 && (!any || s->p[P_SYN_MUTE] == 2);
}
#if !WT_NODRUMS
int wt_drums_audible(const wt_synth_t *s)
{
    int any = s->p[P_SYN_MUTE] == 2 || s->p[P_DR_MUTE] == 2, trk = 0, t;
    if (s->dr) for (t = 0; t < WT_DTRACKS; t++) trk |= s->dr->mute[t] == 2;
    return s->p[P_DR_MUTE] != 1 && (!(any || trk) || s->p[P_DR_MUTE] == 2 || trk);
}
int wt_track_audible(const wt_synth_t *s, int t)
{
    int any, tt, trk = 0;
    if (!s->dr || t < 0 || t >= WT_DTRACKS) return 1;
    any = s->p[P_SYN_MUTE] == 2 || s->p[P_DR_MUTE] == 2;
    for (tt = 0; tt < WT_DTRACKS; tt++) trk |= s->dr->mute[tt] == 2;
    if (s->dr->mute[t] == 1 || s->p[P_DR_MUTE] == 1) return 0;
    return !(any || trk) || s->dr->mute[t] == 2 || s->p[P_DR_MUTE] == 2;
}
#endif

void wt_synth_note_on_ex(wt_synth_t *s, int note, int vel, int accent)
{
    int i, mode = s->p[P_VOICEMODE], pick = 0, cap = WT_NVOICES, nact = 0;      /* mode 3..8: polyphonic, but at most mode - 1 notes sounding (the oldest is cut): heavy sounds keep the CPU in time */
    uint32_t best = 0xFFFFFFFFu;
    if (mode >= 3) { cap = mode - 1; mode = 0; }
    note = clampi(note, 0, 127);
    vel = clampi(vel, 1, 127);
    {   int held = 0, l;                                         /* an LFO set to restart: at every note, or only when no note is sounding */
        for (l = 0; l < WT_NVOICES; l++) if (s->v[l].active && s->v[l].gate) held = 1;
        for (l = 0; l < 2; l++) { int k = s->p[P_LFO1_KEY + l]; if (k == 1 || (k == 2 && !held)) s->lfo_ph[l] = 0; }
    }
    if (accent) vel += (127 - vel) * s->p[P_ACCENT] / 1000;      /* an accent lifts the velocity towards the top */
    if (mode != 0) {
        wt_voice_t *v = &s->v[0];
        int legato = mode == 2 && v->active && v->gate;
        stack_drop(s, note);
        if (s->nstack < 16) s->stack[s->nstack++] = (uint8_t)note;
        voice_start(s, v, note, vel, !legato, 1, accent);
        midi_note(s, 1, note, vel);
        return;
    }
    for (i = 0; i < WT_NVOICES; i++) nact += s->v[i].active != 0;
    if (nact < cap)
        for (i = 0; i < WT_NVOICES; i++)
            if (!s->v[i].active) { pick = i; best = 0; break; }
    if (best) {
        for (i = 0; i < WT_NVOICES; i++) {
            uint32_t sc;
            if (!s->v[i].active) continue;
            sc = s->v[i].age + (s->v[i].gate ? 0x40000000u : 0u);
            if (sc < best) { best = sc; pick = i; }
        }
    }
    voice_start(s, &s->v[pick], note, vel, 1, 0, accent);
    midi_note(s, 1, note, vel);
}

void wt_synth_note_on(wt_synth_t *s, int note, int vel) { wt_synth_note_on_ex(s, note, vel, 0); }

void wt_synth_slide(wt_synth_t *s, int from, int to, int vel, int accent)
{
    int i;
    for (i = 0; i < WT_NVOICES; i++) {
        wt_voice_t *v = &s->v[i];
        if (v->active && v->gate && v->note == from) {      /* the same voice carries on: no new attack, the pitch glides */
            v->target = clampi(to, 0, 127) * 256;
            v->note = (uint8_t)clampi(to, 0, 127);
            v->slide = 1;
            v->accent = (uint8_t)(accent != 0);
            return;
        }
    }
    wt_synth_note_on_ex(s, to, vel, accent);
}

void wt_synth_note_off(wt_synth_t *s, int note)
{
    int i;
    midi_note(s, 0, note, 0);
    if (s->p[P_VOICEMODE] == 1 || s->p[P_VOICEMODE] == 2) {
        wt_voice_t *v = &s->v[0];
        stack_drop(s, note);
        if (!v->active) return;
        if (s->nstack) { v->target = s->stack[s->nstack - 1] * 256; v->note = s->stack[s->nstack - 1]; if (!s->p[P_GLIDE]) v->pitch = v->target; }
        else if (v->gate) { v->gate = 0; for (i = 0; i < 3; i++) env_off(&v->env[i]); }
        return;
    }
    for (i = 0; i < WT_NVOICES; i++) {
        wt_voice_t *v = &s->v[i];
        if (v->active && v->gate && v->note == note) {
            int e;
            v->gate = 0;
            for (e = 0; e < 3; e++) env_off(&v->env[e]);
        }
    }
}

void wt_synth_all_off(wt_synth_t *s)
{
    int i, e;
    s->nstack = 0;
    s->arp.nheld = 0; s->arp.nsnd = 0; s->arp.run = 0; s->arp.off_in = -1;
    for (i = 0; i < WT_NVOICES; i++)
        if (s->v[i].active) { s->v[i].gate = 0; for (e = 0; e < 3; e++) env_off(&s->v[i].env[e]); }
}
void wt_synth_bend(wt_synth_t *s, int v) { s->bend = clampi(v, -8192, 8191); }
void wt_synth_modwheel(wt_synth_t *s, int v) { s->modwheel = clampi(v, 0, 127); }
int wt_synth_active_voices(const wt_synth_t *s)
{
    int i, n = 0;
    for (i = 0; i < WT_NVOICES; i++) n += s->v[i].active;
    return n;
}

/* ---------------------------------------------------------------- per-block control */
static const int16_t uni_off[WT_MAXUNI + 1][WT_MAXUNI] = {
    {0}, {0}, {-1024, 1024}, {-1024, 0, 1024}, {-1024, -341, 341, 1024}, {-1024, -512, 0, 512, 1024}};

/* an LFO cycle in beats, for the sync values 1..15: 4 bars down to a 1/32, plain, triplet and dotted */
const char *const wt_lfo_div_names[16] = {"FREE", "4 BARS", "2 BARS", "1 BAR", "1/2.", "1/2", "1/4.", "1/4", "1/4T", "1/8.", "1/8", "1/8T", "1/16.", "1/16", "1/16T", "1/32"};
/* ---- drawn LFO shapes ---- */
static const char *const SHAPE_NM[WT_NUSHAPE] = {"SHARK FIN", "FALL", "STAIRS", "PULSE 25", "SKEW TRI", "TWIN HUMP", "BOUNCE", "BLANK"};
const char *wt_lfoshape_name(int k) { return SHAPE_NM[k & (WT_NUSHAPE - 1)]; }
void wt_lfoshape_default(int k, wt_lfoshape_t *o)
{
    static const struct { uint8_t n; uint8_t x[8]; int8_t y[8], c[8]; } D[WT_NUSHAPE] = {
        {3, {0, 225, 255}, {-127, 127, -127}, {0, -80, 0}},                                       /* SHARK FIN: a curved climb, then the drop */
        {2, {0, 255}, {127, -127}, {0, 80}},                                                       /* FALL: a plucked decay */
        {8, {0, 63, 64, 127, 128, 191, 192, 255}, {-127, -127, -42, -42, 42, 42, 127, 127}, {0, 0, 0, 0, 0, 0, 0, 0}},
        {4, {0, 63, 64, 255}, {127, 127, -127, -127}, {0, 0, 0, 0}},
        {3, {0, 64, 255}, {-127, 127, -127}, {0, 0, 0}},
        {5, {0, 64, 128, 160, 255}, {-127, 127, -127, 60, -127}, {0, 40, 40, 40, 40}},
        {5, {0, 40, 90, 150, 255}, {127, -127, 70, -127, -127}, {0, 60, -60, 60, 0}},
        {2, {0, 255}, {0, 0}, {0, 0}} };
    int i;
    k &= WT_NUSHAPE - 1;
    for (i = 0; i < WT_SHP_PTS; i++) { o->x[i] = 255; o->y[i] = 0; o->c[i] = 0; }
    o->n = D[k].n;
    for (i = 0; i < D[k].n; i++) { o->x[i] = D[k].x[i]; o->y[i] = D[k].y[i]; o->c[i] = D[k].c[i]; }
}
static int32_t shp_bend(int32_t t, int c)                       /* t Q15 -> Q15, bent by c (-100..100): t + a t (1-t), three times for a strong curve */
{
    int pass;
    for (pass = 0; pass < 3; pass++) t += (int32_t)c * ((t * (32768 - t)) >> 15) / 100;
    return t < 0 ? 0 : t > 32768 ? 32768 : t;
}
void wt_lfoshape_table(const wt_lfoshape_t *sh, int16_t out[WT_LTAB])
{
    int i, j, n = sh->n < 2 ? 2 : sh->n > WT_SHP_PTS ? WT_SHP_PTS : sh->n;
    for (i = 0; i < WT_LTAB; i++) {
        int32_t xq = i * 510;                                    /* 0..255 in 1/256 */
        int32_t y;
        j = 1;
        while (j < n && ((int32_t)sh->x[j] << 8) <= xq) j++;
        if (xq < ((int32_t)sh->x[0] << 8)) y = sh->y[0];
        else if (j >= n) y = sh->y[n - 1];
        else {
            int32_t x0 = (int32_t)sh->x[j - 1] << 8, dx = ((int32_t)sh->x[j] << 8) - x0, y0 = sh->y[j - 1], y1 = sh->y[j];
            int32_t t = dx > 0 ? ((xq - x0) << 15) / dx : 32768;
            t = shp_bend(t, sh->c[j]);
            y = y0 + (int32_t)(((int64_t)(y1 - y0) * t) >> 15);
        }
        out[i] = (int16_t)(y * 258);
    }
}
void wt_synth_set_shape(wt_synth_t *s, int k, const wt_lfoshape_t *sh)
{
#if WT_LFOSHAPES
    wt_lfoshape_table(sh, s->lfo_tab[k & (WT_NUSHAPE - 1)]);
#else
    (void)s; (void)k; (void)sh;
#endif
}

static const float LFO_BEATS[16] = {0.0f, 16.0f, 8.0f, 4.0f, 3.0f, 2.0f, 1.5f, 1.0f, 2.0f / 3.0f, 0.75f, 0.5f, 1.0f / 3.0f, 0.375f, 0.25f, 1.0f / 6.0f, 0.125f};

static void lfo_block(wt_synth_t *s)
{
    int i;
    for (i = 0; i < 2; i++) {
        uint32_t old = s->lfo_ph[i], ph;
        int shape = s->p[P_LFO(i, 0)];
        int32_t t, out;
        int sy = s->p[P_LFO1_SYNC + i];
        if (sy > 0 && sy < 16) {                                    /* tempo sync: cycles of a note value; the tempo, and the external clock's corrections, move it */
            float spc = LFO_BEATS[sy] * (44100.0f * 60.0f * 16.0f / (float)wt_bpm_x16(s));          /* samples per cycle */
            float adv = (float)s->blk_inc[s->cur_blk & 15] * (1.0f / 256.0f);                      /* samples this block is worth */
            s->lfo_ph[i] = ph = old + (uint32_t)(adv * (4294967296.0f / spc));
        } else {
            int rate = s->p[P_LFO(i, 1)];
#if WT_NODRUMS
            rate = clampi(rate + s->hold_gd[i], 0, 1000);   /* previous block's rate mod: this block's voices have not run yet */
#endif
            s->lfo_ph[i] = ph = old + wt_lfo_inc[rate] * WT_CTL;
        }
        if (ph < old) { s->lfo_rnd[i][0] = s->lfo_rnd[i][1]; s->lfo_rnd[i][1] = rnd32(s); }
        t = (int32_t)(ph >> 16);
#if WT_LFOSHAPES
        if (shape >= 7) {                                           /* a drawn shape: its table, read with a linear interpolation */
            const int16_t *tb = s->lfo_tab[(shape - 7) & (WT_NUSHAPE - 1)];
            int ix = (int)(ph >> 25);
            int32_t a = tb[ix], b = tb[(ix + 1) & (WT_LTAB - 1)], fr = (int32_t)((ph >> 9) & 0xFFFF);
            out = a + (int32_t)(((int64_t)(b - a) * fr) >> 16);
        } else
#endif
        switch (shape) {
        case 0: out = wt_sin1024[ph >> 22]; break;
        case 1: out = t < 32768 ? t * 2 - 32768 : 98303 - t * 2; break;
        case 2: out = t - 32768; break;
        case 3: out = t < 32768 ? 32767 : -32767; break;
        case 6: out = 32767 - t; break;                 /* SAW DOWN (case 2, SAW UP, keeps its number: patches use it) */
        case 4: out = (int32_t)(s->lfo_rnd[i][1] >> 17) - 16384; out *= 2; break;
        default: {
            int32_t a = (int32_t)(s->lfo_rnd[i][0] >> 17) * 2 - 32768, b = (int32_t)(s->lfo_rnd[i][1] >> 17) * 2 - 32768;
            out = a + (int32_t)(((int64_t)(b - a) * t) >> 16); }
        }
        { int pol = s->p[P_LFO1_POL + i];                       /* positive only: the shape from 0 up to the top; negative only: from the bottom up to 0 */
          if (pol == 1) out = (out + 32767) >> 1; else if (pol == 2) out = (out - 32767) >> 1; }
        s->lfo_val[i] = out;
    }
}

static void pan_gains(int pan, int32_t *l, int32_t *r)
{
    int idx = (clampi(pan, -1000, 1000) + 1000) * 128 / 2000;
    *l = wt_pan_l[idx]; *r = wt_pan_r[idx];
}
/* equal-power pan with unity at the centre: gain g (Q15) -> left and right (Q15) */
static void pan_lr(int32_t g, int pan, int32_t *l, int32_t *r)
{
    int32_t pl, pr;
    pan_gains(pan, &pl, &pr);
    *l = (int32_t)((((int64_t)g * pl) >> 15) * 46341 >> 15);
    *r = (int32_t)((((int64_t)g * pr) >> 15) * 46341 >> 15);
}

#if WT_NODRUMS
/* 1/sqrt(n) in Q15, n = 1..8. One voice is unity, so a single note is not turned down. */
static int wt2_vc_q15(int n)
{
    static const int T[9] = {32768, 32768, 23170, 18918, 16384, 14654, 13377, 12385, 11585};
    if (n < 1) n = 1;
    if (n > 8) n = 8;
    return T[n];
}
/* Oldest notes keep their unison. A new note gets whatever lanes are left, and still plays (one lane) rather than being cut. */
static void wt2_alloc_lanes(wt_synth_t *s)
{
    int ord[WT_NVOICES], n = 0, i, j, top = -1, topn = -1, left, want;
    want = s->p[P_OSC(0, O_UNI)] + (s->p[P_OSC(1, O_ON)] ? s->p[P_OSC(1, O_UNI)] : 0);
    if (want < 1) want = 1;
    for (i = 0; i < WT_NVOICES; i++) {
        s->lane_of[i] = 0;
        if (!s->v[i].active) continue;
        ord[n++] = i;
        if (s->v[i].note >= topn) { topn = s->v[i].note; top = i; }
    }
    for (i = 0; i < n; i++)
        for (j = i + 1; j < n; j++)
            if (s->v[ord[j]].age < s->v[ord[i]].age) { int t = ord[i]; ord[i] = ord[j]; ord[j] = t; }
    left = s->lane_budget;
    for (j = 0; j < n; j++) {
        int id = ord[j], w = want;
        if (s->p[P_DR_KIT] && id != top) w = 1;          /* UNI_TOP: only the highest note spreads */
        if (w > left) w = left;
        if (w < 1) w = 1;
        left -= w;
        if (left < 0) left = 0;
        s->lane_of[id] = (int8_t)w;
    }
}
static void wt2_grab_gd(wt_synth_t *s)
{
    int i, best = -1, k;
    uint32_t ag = 0;
    for (i = 0; i < WT_NVOICES; i++)
        if (s->v[i].active && (best < 0 || s->v[i].age >= ag)) { ag = s->v[i].age; best = i; }
    if (best < 0) return;
    for (k = 0; k < 6; k++) s->hold_gd[k] = s->v[best].gd[k];
}
static void wt2_q15_ramp(int32_t *L, int32_t *R, int n, int32_t g0, int32_t g1)
{
    int i;
    for (i = 0; i < n; i++) {
        int32_t g = g0 + (int32_t)(((int64_t)(g1 - g0) * (i + 1)) / n);
        L[i] = (int32_t)(((int64_t)L[i] * g) >> 15);
        R[i] = (int32_t)(((int64_t)R[i] * g) >> 15);
    }
}
static int32_t wt2_shl(int32_t x)
{
    if (x > 1000000000) return 2000000000;
    if (x < -1000000000) return -2000000000;
    return x << 1;
}
/* Gain rider: fast attack, slow release. Under the threshold it sits at unity, so a quiet patch is not touched. */
static void wt2_ride(wt_synth_t *s, int32_t *L, int32_t *R)
{
    int32_t xL = *L, xR = *R;
    int32_t aL = xL < 0 ? -xL : xL, aR = xR < 0 ? -xR : xR, peak = aL > aR ? aL : aR;
    int32_t want = 32768, g, d;
    const int32_t thr = 26214;                                   /* about -2 dBFS */
    if (peak > thr) {
        want = (int32_t)(((int64_t)thr << 15) / peak);
        if (want < 1024) want = 1024;
    }
    g = s->lim_g > 0 ? s->lim_g : 32768;
    if (want < g) {
        d = (want - g) >> 2;                                     /* attack: a few samples */
        if (d == 0) d = -1;
    } else if (want > g) {
        d = (want - g) >> 11;                                    /* release: about 50 ms */
        if (d == 0) d = 1;                                       /* the shift stalls inside the last 2048 counts */
    } else d = 0;
    g += d;
    if (g > 32768) g = 32768;
    if (g < 1024) g = 1024;
    s->lim_g = g;
    *L = (int32_t)(((int64_t)xL * g) >> 15);
    *R = (int32_t)(((int64_t)xR * g) >> 15);
}
#endif
static void voice_block(wt_synth_t *s, wt_voice_t *v)
{
    const int16_t *P = s->p;
    int32_t src[S_COUNT], mod[D_COUNT];
    int i, o, u;
    if (v->pitch != v->target) {            /* glide: the GLIDE time, or SLIDE's after a sequencer slide */
        int gl = v->slide ? P[P_SLIDE] : P[P_GLIDE];
        if (gl) {
            int64_t c = (int64_t)wt_coef[gl] * WT_CTL;
            if (c > (1 << 24)) c = 1 << 24;
            v->pitch += (int32_t)(((int64_t)(v->target - v->pitch) * c) >> 24);
            if (v->pitch - v->target < 4 && v->target - v->pitch < 4) v->pitch = v->target;
        } else v->pitch = v->target;
    } else v->slide = 0;
    memset(src, 0, sizeof src);
    src[S_LFO1] = s->lfo_val[0]; src[S_LFO2] = s->lfo_val[1];
    src[S_ENV2] = v->env[1].lvl >> 15; src[S_ENV3] = v->env[2].lvl >> 15;
    src[S_VEL] = v->vel; src[S_NOTE] = clampi((v->note - 60) * 32767 / 60, -32767, 32767);
    for (i = 0; i < 4; i++) src[S_MAC1 + i] = P[P_MAC0 + i] * 32767 / 1000;
    src[S_RAND] = v->rnd; src[S_MOD] = s->modwheel * 32767 / 127;
    src[S_ACCENT] = v->accent ? 32767 : 0;
    memset(mod, 0, sizeof mod);
    for (i = 0; i < 8; i++) {
        int sr = P[P_MX(i, 0)], ds = P[P_MX(i, 1)];
        if (sr && ds) mod[ds] += (int32_t)(((int64_t)src[sr] * P[P_MX(i, 2)]) >> 15);
    }
    v->velgain = 32768 - (int32_t)(((int64_t)(32768 - v->vel) * P[P_VELAMP]) / 1000);
    for (i = 0; i < D_COUNT; i++) s->mod_view[i] = mod[i];                     /* for the screen: the last voice's modulation */
#if WT_NODRUMS
    v->amp_atk = (int16_t)clampi(P[P_ENV(0, E_ATK)] + mod[D_AMPATK], 0, 1000);
    v->amp_dec = (int16_t)clampi(P[P_ENV(0, E_DEC)] + mod[D_AMPDEC], 0, 1000);
    v->amp_rel = (int16_t)clampi(P[P_ENV(0, E_REL)] + mod[D_AMPREL], 0, 1000);
    v->gd[0] = (int16_t)clampi(mod[D_LFO1R], -1000, 1000);
    v->gd[1] = (int16_t)clampi(mod[D_LFO2R], -1000, 1000);
    v->gd[2] = (int16_t)clampi(mod[D_CHMIX], -1000, 1000);
    v->gd[3] = (int16_t)clampi(mod[D_DLSEND] * 127 / 1000, -127, 127);
    v->gd[4] = (int16_t)clampi(mod[D_RVSEND] * 127 / 1000, -127, 127);
    v->gd[5] = (int16_t)clampi(mod[D_MASTER], -1000, 1000);
#endif

    for (o = 0; o < 2; o++) {
        const wt_table_t *t = s->table[P[P_OSC(o, O_TABLE)]];
        const wt_table_t *t2 = s->table[P[P_OSC(o, O_TABLE2)]];
        int n = P[P_OSC(o, O_UNI)];
        {   int n0 = P[P_OSC(0, O_UNI)], n1 = P[P_OSC(1, O_ON)] ? P[P_OSC(1, O_UNI)] : 0, tot = n0 + n1;
#if WT_NODRUMS
            /* lanes were given out oldest-first: a new note loses unison before any note is dropped */
            int give = s->lane_of[(int)(v - s->v)];
            if (give < 1) give = 1;
            if (tot < 1) tot = 1;
            n = n * give / tot; if (n < 1) n = 1;
#else
            int32_t cap = s->lane_budget / (s->nact > 0 ? s->nact : 1);          /* the load governor: lanes per sounding voice */
            if (cap < 2) cap = 2;
            if (tot > cap) { n = n * cap / tot; if (n < 1) n = 1; }
#endif
        }
        int32_t wtpos = clampi(P[P_OSC(o, O_WTPOS)] + mod[D_WTPOS + o], 0, 1000);
        int32_t warp = clampi(P[P_OSC(o, O_WARP)] + mod[D_WARP + o], -1000, 1000);
        int32_t det = clampi(P[P_OSC(o, O_DETUNE)] + mod[D_DETUNE + o], 0, 1000);
        int32_t lev = clampi(P[P_OSC(o, O_LEVEL)] + mod[D_LEVEL + o], 0, 1000);
        int32_t morph = clampi(P[P_OSC(o, O_MORPH)] + mod[D_MORPH + o], 0, 1000);
        int32_t fm = clampi(P[P_OSC(o, O_FM)] + mod[D_FM + o], 0, 1000);
        int32_t other_fm = clampi(P[P_OSC(1 - o, O_FM)] + mod[D_FM + 1 - o], 0, 1000);
        int32_t other_pd = P[P_OSC(1 - o, O_PD)];
        int32_t pan = P[P_OSC(o, O_PAN)] + mod[D_PAN + o];
        int32_t pitch = v->pitch + P[P_TUNE] * 256 / 100 + s->bend * P[P_BEND] * 256 / 8192 +
                        (P[P_OSC(o, O_OCT)] * 12 + P[P_OSC(o, O_SEMI)]) * 256 + P[P_OSC(o, O_FINE)] * 256 / 100 +
                        mod[D_PITCH + o] * 24 * 256 / 1000;
        int32_t detmax = det * det * 256 / 1000000;
        int32_t w[WT_MAXUNI], sumsq = 0, nrm, f0, f1, g0 = 0, g1 = 0;
        int64_t fp;
        uint32_t ratio = 256;
        int mode = P[P_OSC(o, O_WARPMODE)], use2;
        v->o[o].on = P[P_OSC(o, O_ON)] && t && (lev > 0 || other_fm > 0 || other_pd > 0);
        if (!v->o[o].on) continue;
        use2 = morph > 0 && t2 && t2 != t && t2->nlev == t->nlev;
        v->o[o].n = n; v->o[o].mode = mode; v->o[o].warp = warp; v->o[o].fm = fm * 32768 / 1000; v->o[o].pd = P[P_OSC(o, O_PD)] * 32768 / 1000;
        v->o[o].t2 = use2; v->o[o].flt = P[P_OSC(o, O_FLT)];
        v->o[o].morph = morph * 32768 / 1000;
        v->o[o].rawnorm = 32768 / n;
        if (mode == 2) ratio = 256u + (uint32_t)(clampi(warp, 0, 1000) * 7 * 256 / 1000);
        fp = (int64_t)wtpos * (t->nframes - 1) * 65536 / 1000;
        f0 = (int32_t)(fp >> 16);
        v->o[o].ff = (int32_t)(fp & 0xFFFF);
        if (f0 >= t->nframes - 1) { f0 = t->nframes - 1; v->o[o].ff = 0; }
        if (v->o[o].ff < 256) v->o[o].ff = 0;                      /* within 0.4 % of a frame: that frame (one read instead of two per lane) */
        else if (v->o[o].ff > 65536 - 256 && f0 < t->nframes - 1) { f0++; v->o[o].ff = 0; }
        f1 = f0 + 1 < t->nframes ? f0 + 1 : f0;
        if (use2) {
            fp = (int64_t)wtpos * (t2->nframes - 1) * 65536 / 1000;
            g0 = (int32_t)(fp >> 16);
            v->o[o].ff2 = (int32_t)(fp & 0xFFFF);
            if (g0 >= t2->nframes - 1) { g0 = t2->nframes - 1; v->o[o].ff2 = 0; }
            if (v->o[o].ff2 < 256) v->o[o].ff2 = 0;
            else if (v->o[o].ff2 > 65536 - 256 && g0 < t2->nframes - 1) { g0++; v->o[o].ff2 = 0; }
            g1 = g0 + 1 < t2->nframes ? g0 + 1 : g0;
        }
        for (u = 0; u < n; u++) {
            int32_t centre = (n >= 3 && (n & 1) && u == n / 2);
            w[u] = (n >= 3 && (n & 1) && !centre) ? P[P_OSC(o, O_BLEND)] * 1024 / 1000 : 1024;
            sumsq += w[u] * w[u] / 16;
        }
        nrm = (int32_t)wt_isqrt((uint64_t)sumsq * 16u);
        for (u = 0; u < n; u++) {
            int32_t off = uni_off[n][u];
            int32_t pu = pitch + (off * detmax >> 10), gain;
            uint32_t inc = wt_pitch_inc(pu), ie = (uint32_t)(((uint64_t)inc * ratio) >> 8);
            uint32_t hmax;
            int L = 0, lg;
            if (fm) ie = (uint32_t)(((uint64_t)ie * (1000u + 2u * (uint32_t)fm)) / 1000u);   /* leave room for the modulation */
            hmax = ie ? 0x80000000u / ie : 0xFFFFu;
            while (L < t->nlev - 1 && (uint32_t)t->H[L] > hmax) L++;
            lg = t->lg[L];
            if (use2 && t2->lg[L] != lg) { use2 = 0; v->o[o].t2 = 0; }
            v->o[o].inc[u] = inc;
            v->o[o].sh[u] = lg;
            v->o[o].p0[u] = t->lev[L] + ((size_t)f0 << lg);
            v->o[o].p1[u] = t->lev[L] + ((size_t)f1 << lg);
            if (use2) { v->o[o].p2[u] = t2->lev[L] + ((size_t)g0 << lg); v->o[o].p3[u] = t2->lev[L] + ((size_t)g1 << lg); }
            gain = (int32_t)(((int64_t)w[u] * 32768) / (nrm ? nrm : 1));
            gain = (int32_t)(((int64_t)gain * lev * 32768 / 1000) >> 15);
            pan_lr(gain, pan + off * P[P_OSC(o, O_SPREAD)] / 1024, &v->o[o].wl[u], &v->o[o].wr[u]);
        }
    }
    /* sub oscillator */
    {
        int32_t pitch = v->pitch + P[P_TUNE] * 256 / 100 + s->bend * P[P_BEND] * 256 / 8192 + P[P_SUB_OCT] * 12 * 256 +
                        P[P_SUB_CRS] * 256 / 100;
        int32_t pw = clampi(P[P_SUB_PW] + mod[D_SUBPW], 0, 1000);
        v->subinc = (int32_t)wt_pitch_inc(pitch);
        v->subw = P[P_SUB_WAVE];
        {   /* square / pulse = saw(x) - saw(x - duty), from the band-limited saw; duty in per mille */
            int32_t duty = v->subw == 3 ? 500 - 460 * pw / 1000 : 500, big = duty > 1000 - duty ? duty : 1000 - duty;
            uint32_t inc = (uint32_t)v->subinc, hmax = inc ? 0x80000000u / inc : 0xFFFFu;
            int L = 0;
            while (L < g_saw.nlev - 1 && (uint32_t)g_saw.H[L] > hmax) L++;
            v->subthr = (uint32_t)((uint64_t)duty * 4294967296ull / 1000u);
            v->subsc = (int32_t)(32768ll * 500 / big);
            v->subp = g_saw.lev[L]; v->subsh = g_saw.lg[L];
        }
        v->subflt = P[P_SUB_FLT];
        {
            int32_t sl = P[P_SUB_LEVEL];
#if WT_NODRUMS
            sl = clampi(sl + mod[D_SUBLVL], 0, 1000);
#endif
            if (P[P_SUB_ON]) pan_lr(sl * 32768 / 1000, P[P_SUB_PAN], &v->sub_wl, &v->sub_wr);
            else v->sub_wl = v->sub_wr = 0;
        }
    }
    /* noise oscillator: colour generator and looping sample layer */
    {
        v->noiflt = P[P_NOI_FLT];
        v->ntype = P[P_NOI_TYPE];
        v->nstereo = P[P_NOI_STEREO] * 32768 / 1000;       /* Q15 */
        v->nfilt = P[P_NOI_FILTER];
        if (v->nfilt) {         /* one-pole: coefficient from 1 down to 1/256 over the knob's travel (an octave per 1/8) */
            int amt = (v->nfilt < 0 ? -v->nfilt : v->nfilt) * 8, sh = amt / 1000, fr = amt % 1000;
            int32_t a = 32767 >> sh, b = a >> 1;
            a = a - (int32_t)(((int64_t)(a - b) * fr) / 1000);
            v->nfa = v->nfilt < 0 ? a : 32767 - a > 0 ? 32767 - a + 1 : 1;     /* low-pass: slower as the knob rises; high-pass: the inner low-pass speeds up */
            if (v->nfilt > 0) v->nfa = clampi(32768 - a, 1, 32767);
        }
        {
            int32_t nl = P[P_NOI_LEVEL];
#if WT_NODRUMS
            nl = clampi(nl + mod[D_NOILVL], 0, 1000);
#endif
            if (P[P_NOI_ON] && v->ntype < 4) pan_lr(nl * 32768 / 1000, P[P_NOI_PAN], &v->noi_wl, &v->noi_wr);
            else v->noi_wl = v->noi_wr = 0;
        }
        if (P[P_NOI_ON] && v->sn && !v->sdone) {
            int32_t semi = P[P_NOI_SMP_PITCH] * 256 + P[P_NOI_SMP_FINE] * 256 / 100;
            uint32_t a, b;
            if (P[P_NOI_SMP_KEY]) semi += v->pitch - 60 * 256;
            semi = clampi(semi, -36 * 256, 36 * 256);
            a = wt_pitch_inc(60 * 256 + semi); b = wt_pitch_inc(60 * 256);
            v->sinc = (uint32_t)(((uint64_t)a << 16) / b);
            pan_lr(P[P_NOI_SMP_LEVEL] * 32768 / 1000, P[P_NOI_PAN], &v->smp_wl, &v->smp_wr);
        } else v->smp_wl = v->smp_wr = 0;
    }
    /* filter: Simper's zero-delay-feedback state variable filter, coefficients in Q24 */
    v->ftype = P[P_F_TYPE];
    if (v->ftype == 6) {
        int32_t pos = clampi(P[P_F_CUTOFF] + mod[D_CUTOFF] * 600 / 1000, 0, 1350) * 1024 / 1350;    /* 0..1024 = A .. U */
        int32_t sg = pos >> 8, fr = pos & 255, f1, f2;
        int32_t res = clampi(P[P_F_RESO] + mod[D_RESO], 0, 1000);
        int64_t k = 16777216 - (int64_t)res * 14000;
        if (sg >= 4) { sg = 3; fr = 256; }
        f1 = VOW_F1[sg] + (VOW_F1[sg + 1] - VOW_F1[sg]) * fr / 256;
        f2 = VOW_F2[sg] + (VOW_F2[sg + 1] - VOW_F2[sg]) * fr / 256;
        set_coef(wt_g_tbl[f1], k, &v->fa1, &v->fa2, &v->fa3); v->fk = (int32_t)k;
        set_coef(wt_g_tbl[f2], k, &v->fb1, &v->fb2, &v->fb3); v->fkb = (int32_t)k;
        v->fmix = P[P_F_MIX] * 32768 / 1000;
        {   int32_t d = clampi(P[P_F_DRIVE] + mod[D_DRIVE], 0, 1000);
#if WT_NODRUMS
            if (s->nact > 1) d = (int32_t)(((int64_t)d * wt2_vc_q15(s->nact)) >> 15);   /* less drive as the chord gets bigger */
#endif
            v->fdrive = 256 + d * 1792 / 1000;
        }
    } else if (v->ftype) {
        int32_t cut = P[P_F_CUTOFF] + (v->pitch / 256 - 60) * 10 * P[P_F_KEYTRK] / 1000 + mod[D_CUTOFF] * 600 / 1000;
        int32_t res = clampi(P[P_F_RESO] + mod[D_RESO], 0, 1000);
        int64_t g, k, den;
        cut = clampi(cut, 0, 1350);
        g = wt_g_tbl[cut];
        k = 33554432 - (int64_t)res * 32884;             /* 2 - 1.96 res */
        den = (1 << 24) + ((g * (g + k)) >> 24);
        v->fa1 = (int32_t)((1ll << 48) / den);
        v->fa2 = (int32_t)((g * v->fa1) >> 24);
        v->fa3 = (int32_t)((g * v->fa2) >> 24);
        v->fk = (int32_t)k;
        v->fmix = P[P_F_MIX] * 32768 / 1000;                  /* Q15 */
        {   int32_t d = clampi(P[P_F_DRIVE] + mod[D_DRIVE], 0, 1000);
#if WT_NODRUMS
            if (s->nact > 1) d = (int32_t)(((int64_t)d * wt2_vc_q15(s->nact)) >> 15);   /* less drive as the chord gets bigger */
#endif
            v->fdrive = 256 + d * 1792 / 1000;
        }
    }
}

static void voice_render(wt_synth_t *s, wt_voice_t *v, int32_t *accL, int32_t *accR)
{
    const int cubic = wt_cfg.interp == 3 && s->p[P_CPUMODE] != 2;
    int i, o, u;
    for (i = 0; i < WT_CTL; i++) {
#if WT_NODRUMS
        int16_t ep[E_SIZE];
        int ei;
        int32_t amp;
        for (ei = 0; ei < E_SIZE; ei++) ep[ei] = s->p[P_ENV(0, ei)];
        ep[E_ATK] = v->amp_atk; ep[E_DEC] = v->amp_dec; ep[E_REL] = v->amp_rel;
        amp = env_tick(&v->env[0], ep);
#else
        int32_t amp = env_tick(&v->env[0], s->p + P_ENV(0, 0));
#endif
        int32_t FL = 0, FR = 0, DL = 0, DR = 0, sig;       /* filtered and direct buses */
        env_tick(&v->env[1], s->p + P_ENV(1, 0));
        env_tick(&v->env[2], s->p + P_ENV(2, 0));
        for (o = 0; o < 2; o++) {
            int32_t oL = 0, oR = 0, raw = 0;
            int n = v->o[o].n, mode = v->o[o].mode, warp = v->o[o].warp, ff = v->o[o].ff, t2 = v->o[o].t2;
            int32_t fmk = 0;
            uint32_t pdoff = 0;
            if (!v->o[o].on) { v->last[o] = 0; continue; }
            if (v->o[o].fm) fmk = (int32_t)(((int64_t)v->last[1 - o] * v->o[o].fm) >> 15) * 4;     /* FM: up to +-4x the frequency (fm is Q15) */
            if (v->o[o].pd) pdoff = (uint32_t)((((int64_t)v->last[1 - o] * v->o[o].pd) >> 15) * 262144);   /* PD: up to +-2 cycles (pd is Q15) */
            for (u = 0; u < n; u++) {
                uint32_t inc = v->o[o].inc[u], ph, wp;
                int32_t y;
                if (fmk) inc += (uint32_t)(((int64_t)(int32_t)inc * fmk) >> 15);      /* thru-zero: the sum may wrap negative */
                ph = (v->ph[o][u] += inc);
                wp = warp_phase(ph + pdoff, mode, warp);
                y = rd(v->o[o].p0[u], v->o[o].sh[u], wp, cubic);
                if (ff) y += ((rd(v->o[o].p1[u], v->o[o].sh[u], wp, cubic) - y) * (ff >> 1)) >> 15;
                if (t2) {
                    int32_t y2 = rd(v->o[o].p2[u], v->o[o].sh[u], wp, cubic);
                    if (v->o[o].ff2) y2 += ((rd(v->o[o].p3[u], v->o[o].sh[u], wp, cubic) - y2) * (v->o[o].ff2 >> 1)) >> 15;
                    y += ((y2 - y) * (v->o[o].morph >> 1)) >> 14;
                }
                raw += y;
                oL += (y * v->o[o].wl[u]) >> 15;
                oR += (y * v->o[o].wr[u]) >> 15;
            }
            v->last[o] = (raw * v->o[o].rawnorm) >> 15;
            if (v->o[o].flt) { FL += oL; FR += oR; } else { DL += oL; DR += oR; }
        }
        if (v->sub_wl | v->sub_wr) {
            int32_t y;
            v->phsub += (uint32_t)v->subinc;
            if (v->subw >= 2) {
                y = rd(v->subp, v->subsh, v->phsub, cubic) - rd(v->subp, v->subsh, v->phsub - v->subthr, cubic);
                y = (int32_t)(((int64_t)y * v->subsc) >> 15);
            } else y = sub_sample(v->phsub, v->subw);
            if (v->subflt) { FL += (y * v->sub_wl) >> 15; FR += (y * v->sub_wr) >> 15; }
            else { DL += (y * v->sub_wl) >> 15; DR += (y * v->sub_wr) >> 15; }
        }
        if (v->noi_wl | v->noi_wr) {
            int32_t nl = noise_color(s, v, 0, v->ntype), nr = nl;
            if (v->nstereo) nr = nl + (int32_t)(((int64_t)(noise_color(s, v, 1, v->ntype) - nl) * v->nstereo) >> 15);
            if (v->nfilt) {
                v->nz_lp[0] += (int32_t)(((int64_t)(nl - v->nz_lp[0]) * v->nfa) >> 15);
                v->nz_lp[1] += (int32_t)(((int64_t)(nr - v->nz_lp[1]) * v->nfa) >> 15);
                if (v->nfilt < 0) { nl = v->nz_lp[0]; nr = v->nz_lp[1]; } else { nl -= v->nz_lp[0]; nr -= v->nz_lp[1]; }
            }
            if (v->noiflt) { FL += (nl * v->noi_wl) >> 15; FR += (nr * v->noi_wr) >> 15; }
            else { DL += (nl * v->noi_wl) >> 15; DR += (nr * v->noi_wr) >> 15; }
        }
        if (v->smp_wl | v->smp_wr) {
            if (v->sdone) { v->smp_wl = v->smp_wr = 0; }
            else {
                uint32_t idx = v->spos >> 16, nx = idx + 1 >= (uint32_t)v->sn ? 0u : idx + 1;
                int32_t a = v->sd[idx], b = v->sd[nx], y = a + (((b - a) * (int32_t)((v->spos & 0xFFFF) >> 1)) >> 15);
                v->spos += v->sinc;
                if ((v->spos >> 16) >= (uint32_t)v->sn) {
                    if (s->p[P_NOI_SMP_LOOP]) v->spos -= (uint32_t)v->sn << 16; else v->sdone = 1;
                }
                if (v->noiflt) { FL += (y * v->smp_wl) >> 15; FR += (y * v->smp_wr) >> 15; }
                else { DL += (y * v->smp_wl) >> 15; DR += (y * v->smp_wr) >> 15; }
            }
        }
        if (v->ftype) {
            int32_t fl, fr;
            if (v->fdrive > 256) {
                FL = softclip((int32_t)(((int64_t)FL * v->fdrive) >> 8));
                FR = softclip((int32_t)(((int64_t)FR * v->fdrive) >> 8));
            }
            if (v->ftype == 6) {                       /* VOWEL: the two formants side by side, the narrow band-passes made up in level */
                fl = (svf(v, v->fs[0][0], FL, 4) + (svf_b(v, v->fs[1][0], FL) * 3 >> 2)) * 3;
                fr = (svf(v, v->fs[0][1], FR, 4) + (svf_b(v, v->fs[1][1], FR) * 3 >> 2)) * 3;
            } else {
            fl = svf(v, v->fs[0][0], FL, v->ftype <= 2 ? 1 : v->ftype);
            fr = svf(v, v->fs[0][1], FR, v->ftype <= 2 ? 1 : v->ftype);
            if (v->ftype == 2) { fl = svf(v, v->fs[1][0], fl, 1); fr = svf(v, v->fs[1][1], fr, 1); }
            }
            FL += (int32_t)(((int64_t)(fl - FL) * v->fmix) >> 15);
            FR += (int32_t)(((int64_t)(fr - FR) * v->fmix) >> 15);
        }
        sig = (int32_t)(((int64_t)(FL + DL) * amp) >> 15); sig = (int32_t)(((int64_t)sig * v->velgain) >> 15);
#if WT_NODRUMS
        if (s->nact > 1) sig = softclip(sig);                    /* a chord: tame each voice before the sum */
#endif
        accL[i] += sig >> 1;
        sig = (int32_t)(((int64_t)(FR + DR) * amp) >> 15); sig = (int32_t)(((int64_t)sig * v->velgain) >> 15);
#if WT_NODRUMS
        if (s->nact > 1) sig = softclip(sig);
#endif
        accR[i] += sig >> 1;
    }
    s->last_subph = v->phsub;
    if (v->env[0].st == ES_IDLE || (v->env[0].st == ES_SUS && v->env[0].lvl == 0)) {
        v->active = 0; v->gate = 0;
        memset(v->fs, 0, sizeof v->fs);
        v->last[0] = v->last[1] = 0;
    }
}

void wt_synth_load(wt_synth_t *s, uint32_t pct)
{
    int32_t full = WT_NVOICES * 2 * WT_MAXUNI;
    if (s->p[P_CPUMODE] == 0) { s->lane_budget = full; return; }          /* FULL: whatever the CPU can do (it may break up) */
    if (s->p[P_CPUMODE] == 2) full = 32;                                  /* SAVE: lean from the start */
    {   /* the budget starts at the most lanes there could be (80), so on a chord it sat far above what is in use and shedding from it took
         * many blocks to bite: pin it to what the sounding voices use first, then shed from there (an eighth, and one, a block) */
        int32_t used = s->nact * (s->p[P_OSC(0, O_UNI)] + (s->p[P_OSC(1, O_ON)] ? s->p[P_OSC(1, O_UNI)] : 0));
        if (used > full) used = full;
        if (pct >= 70u && s->lane_budget > used) s->lane_budget = used;      /* busy: no more lanes than are in use now (a new note then has to share) */
        if (pct >= 80u) { s->lane_budget -= s->lane_budget / 8 + 1; s->gov_hold = 0; }      /* nearly out of time: shed at once */
        else if (pct < 60u) { if (++s->gov_hold >= 16) { s->lane_budget += 1; s->gov_hold = 0; } }      /* room: win a lane back every 16 blocks (no pumping) */
        else s->gov_hold = 0;
    }
    if (s->lane_budget < 16) s->lane_budget = 16;
    if (s->lane_budget > full) s->lane_budget = full;
    if (s->lane_budget < 16) s->lane_budget = 16;
}

/* a source's own distortion: the master's characters and drive curve, applied before the source joins the mix. x is in the effects units (1.0 = the master stage's nominal level) */
static void sdist_prep(wt_sdist_t *d, int type, int pot)
{
    int16_t key = (int16_t)(type * 128 + pot);
    if (key == d->key && d->sh.k > 0.0f) return;
    d->key = key;
    d9_shape_prep(&d->sh, x0x_pot_exp[WT_EXP_DRIVE][pot], type - 1);
}
static inline float sdist_one(const wt_sdist_t *d, float x, float *st) { return d9_shape(&d->sh, x * d->sh.k, st) * 0.7f; }

uint32_t (*wt_clock)(void);
#define TICK() (wt_clock ? wt_clock() : 0u)

/* Without drums the second core is free for the voices: of the sounding voices of each control block, every other one (the
 * first, third, ...) renders there into its own accumulators, the rest here, and the two are added (integer sums: the same
 * samples as on one core, bit for bit). This core also does the sequencer, the effects and the output, so the other
 * core gets the larger share when the count is odd. */
static wt_synth_t *vjob_s;
static int32_t vjob_l[WT_CTL], vjob_r[WT_CTL];
static uint8_t vjob_v[WT_NVOICES], vjob_n;
static void voice_job(uint32_t unused)
{
    wt_synth_t *s = vjob_s;
    int k;
    (void)unused;
    for (k = 0; k < vjob_n; k++) {
        wt_voice_t *v = &s->v[vjob_v[k]];
        voice_block(s, v);
        voice_render(s, v, vjob_l, vjob_r);
    }
}

static void render_chunk(wt_synth_t *s, int16_t *out, int n, int32_t g)
{
#if !WT_NODRUMS
    wt_drums_t *D = s->dr;
    int started = 0;
#endif
    int b, i, k;
    uint32_t t0, t1;
#if WT_NODRUMS
    int blk_nact[16];
    for (k = 0; k < 16; k++) blk_nact[k] = 1;
#endif
    wt_clk_plan(s, n / WT_CTL);                                  /* the clock for this chunk: internal, or locked to the ticks that came in */
#if !WT_NODRUMS
    if (D) started = wt_drums_chunk_start(s, n);                 /* the drums, on the other core if there is one, while the voices render here */
#endif
    for (b = 0; b + WT_CTL <= n; b += WT_CTL) {
        int32_t *accL = s->chunkL + b, *accR = s->chunkR + b;
        memset(accL, 0, WT_CTL * sizeof(int32_t)); memset(accR, 0, WT_CTL * sizeof(int32_t));
        s->cur_blk = b / WT_CTL;
        t0 = TICK();
        wt_seq_block(s);
        wt_arp_block(s);
        lfo_block(s);
        t1 = TICK(); s->prof[3] += t1 - t0;
        s->nact = 0;
        for (k = 0; k < WT_NVOICES; k++) s->nact += s->v[k].active != 0;
#if WT_NODRUMS
        if (s->cur_blk >= 0 && s->cur_blk < 16) blk_nact[s->cur_blk] = s->nact < 1 ? 1 : s->nact;
        wt2_alloc_lanes(s);                                     /* oldest notes keep unison; a new note is never dropped */
#endif
        {   int sp = 0, m = 0, a = 0;
            uint8_t mine[WT_NVOICES];
            vjob_n = 0;
            for (k = 0; k < WT_NVOICES; k++) {                      /* the sounding voices, alternately to the other core (first) and this one */
                if (!s->v[k].active) continue;
#if WT_NODRUMS
                if (s->nact > 1 && wt_par_run && !(a++ & 1)) vjob_v[vjob_n++] = (uint8_t)k;
#else
                if (!D && s->nact > 1 && wt_par_run && !(a++ & 1)) vjob_v[vjob_n++] = (uint8_t)k;
#endif
                else mine[m++] = (uint8_t)k;
            }
            if (vjob_n) {
                for (k = 0; k < WT_CTL; k++) vjob_l[k] = vjob_r[k] = 0;
                vjob_s = s;
                sp = wt_par_run(voice_job, 0);
                if (!sp) { for (k = 0; k < vjob_n; k++) mine[m++] = vjob_v[k]; vjob_n = 0; }    /* no second core: all here */
            }
            s->cpu_samples += WT_CTL * (m + (sp ? vjob_n : 0));
            for (k = 0; k < m; k++) {
                wt_voice_t *v = &s->v[mine[k]];
                voice_block(s, v);
                voice_render(s, v, accL, accR);
            }
            if (sp) {
#if WT_NODRUMS
                uint32_t tw0 = TICK(), waited;
#endif
                if (wt_par_wait && wt_par_wait()) voice_job(0);     /* the other core failed: here */
#if WT_NODRUMS
                waited = TICK() - tw0;
                s->prof[1] += waited;                               /* WT-2: the other core is rendering voices */
#endif
                for (k = 0; k < WT_CTL; k++) { accL[k] += vjob_l[k]; accR[k] += vjob_r[k]; }
                vjob_n = 0;
#if WT_NODRUMS
                s->prof[0] += (TICK() - t1) - waited;               /* this block's voices, without the wait just recorded */
                t1 = TICK();
#endif
            }
        }
#if WT_NODRUMS
        wt2_grab_gd(s);                                          /* after both cores have finished this block */
#endif
        s->prof[0] += TICK() - t1;
    }
#if !WT_NODRUMS
    t0 = TICK();
    if (D) wt_drums_chunk_finish(s, started);
    t1 = TICK(); s->prof[1] += t1 - t0;
#endif
    if (s->p[P_CLK_SRC] && s->clk.seen && s->clk.bpm > 20.0f && s->clk.bpm < 300.0f) fxbus_set_bpm(&s->fx, s->clk.bpm);        /* the delay time follows the external tempo */
    for (b = 0; b + WT_CTL <= n; b += WT_CTL) {
        int32_t *accL = s->chunkL + b, *accR = s->chunkR + b;
#if !WT_NODRUMS
        float dr[WT_CTL], dd[WT_CTL], sm[WT_CTL];
        int dact = D && D->active;
#endif
        {   float tg = wt_syn_audible(s) ? 1.0f : 0.0f;           /* mute / solo: the synth, with a short ramp */
            if (tg != s->mix_syn_g || tg != 1.0f) {
                float g0 = s->mix_syn_g, dg = (tg - g0) * (1.0f / WT_CTL);
                if (g0 == 0.0f && tg == 0.0f) { for (i = 0; i < WT_CTL; i++) accL[i] = accR[i] = 0; }
                else for (i = 0; i < WT_CTL; i++) { float gg = g0 + dg * (float)(i + 1); accL[i] = (int32_t)((float)accL[i] * gg); accR[i] = (int32_t)((float)accR[i] * gg); }
                s->mix_syn_g = tg;
            }
        }
#if WT_NODRUMS
        {   /* 1/sqrt(voices), then 6 dB of headroom before the chorus, the distortion and the effects. Made back up after the EQ. */
            int nv = blk_nact[(b / WT_CTL) & 15];
            int32_t tgt = wt2_vc_q15(nv), g0 = s->vc_g > 0 ? s->vc_g : 32768;
            wt2_q15_ramp(accL, accR, WT_CTL, g0, tgt);
            s->vc_g = tgt;
            for (i = 0; i < WT_CTL; i++) { accL[i] >>= 1; accR[i] >>= 1; }
        }
#endif
#if !WT_NODRUMS
#if WT_LFOSHAPES
        s->sc_env = 0.0f; s->sc_gain = 1.0f;
        if (0)
#else
        if (s->p[P_SC_AMT])
#endif
        {                                                        /* sidechain: the bass drum ducks the synth (its sends too), then it swells back */
            float amt = (float)s->p[P_SC_AMT] * 0.001f, g1;
            int blk = b / WT_CTL;
            if (D && ((D->kickmask >> blk) & 1u)) s->sc_env = 1.0f;
            g1 = 1.0f - amt * s->sc_env;
            if (g1 != 1.0f || s->sc_gain != 1.0f) {
                float g0 = s->sc_gain, dg = (g1 - g0) * (1.0f / WT_CTL);
                for (i = 0; i < WT_CTL; i++) { float gg = g0 + dg * (float)(i + 1); accL[i] = (int32_t)((float)accL[i] * gg); accR[i] = (int32_t)((float)accR[i] * gg); }
            }
            s->sc_gain = g1;
            {   float ms = 30.0f * fm_expf((float)s->p[P_SC_REL] * (3.28f / 1000.0f));       /* 30 ms .. 800 ms */
                s->sc_env *= fm_expf(-0.3628f / ms);
                if (s->sc_env < 0.0005f) s->sc_env = 0.0f;
            }
        } else { s->sc_env = 0.0f; s->sc_gain = 1.0f; }
#endif /* sidechain: not in WT-2 (no drums, and these parameters are the chorus) */
#if WT_LFOSHAPES
        {   /* the chorus: a delay of about 6 ms swept by a triangle, the right side a quarter cycle behind, mixed in */
            int32_t mix, dep = s->p[P_CH_DEPTH];
            int ch_wrote = 0;
#if WT_NODRUMS
            mix = clampi((int32_t)s->p[P_CH_MIX] + s->hold_gd[2], 0, 1000);
#else
            mix = s->p[P_CH_MIX];
#endif
            if (mix) {
                uint32_t inc = 9739u + (uint32_t)s->p[P_CH_RATE] * 380u;          /* 0.1 .. 4 Hz */
                ch_wrote = 1;
                for (i = 0; i < WT_CTL; i++) {
                    int ch;
                    int32_t *acc[2];
                    acc[0] = accL + i; acc[1] = accR + i;
                    s->ch_ph += inc;
                    for (ch = 0; ch < 2; ch++) {
                        uint32_t ph = s->ch_ph + (ch ? 0x40000000u : 0u);
                        int32_t p16 = (int32_t)(ph >> 16), tri = (p16 < 32768 ? p16 : 65535 - p16) * 2 - 32768;
                        int32_t dq = 265 * 256 + (int32_t)(((int64_t)tri * dep * 56320) / 32768000);
                        int32_t di = dq >> 8, fr = dq & 255, x = *acc[ch], a, b2, wet;
                        int rp = (int)s->ch_w - di;
                        s->ch_buf[ch][s->ch_w] = (int16_t)clampi(x >> 1, -32768, 32767);
                        a = s->ch_buf[ch][rp & 511]; b2 = s->ch_buf[ch][(rp - 1) & 511];
                        wet = (a + (((b2 - a) * fr) >> 8)) * 2;
                        *acc[ch] = x + (int32_t)(((int64_t)wet * mix) / 1000);
                    }
                    s->ch_w = (uint16_t)((s->ch_w + 1) & 511);
                }
            }
#if WT_NODRUMS
            /* HYPER: a short cross-mix through the chorus lines. Cheaper than another unison stack. */
            if (s->p[P_DR_DIST]) {
                int32_t hm = (int32_t)s->p[P_DR_DIST] * 1000 / 7;
                int w = ch_wrote ? (int)((s->ch_w - (uint16_t)WT_CTL) & 511) : (int)s->ch_w;
                for (i = 0; i < WT_CTL; i++) {
                    int32_t xL = accL[i], xR = accR[i];
                    int drift, rp1, rp2, aL, aR, bL, bR;
                    if (!ch_wrote) {
                        s->ch_buf[0][w] = (int16_t)clampi(xL >> 1, -32768, 32767);
                        s->ch_buf[1][w] = (int16_t)clampi(xR >> 1, -32768, 32767);
                        s->ch_ph += 700u;                        /* the taps wander a few samples; the chorus owns ch_ph when it is on */
                    }
                    drift = (int)((s->ch_ph >> 24) & 31);
                    rp1 = w - 180 - drift;
                    rp2 = w - 320 - (drift >> 1);
                    aL = s->ch_buf[1][rp1 & 511]; bL = s->ch_buf[1][rp2 & 511];
                    aR = s->ch_buf[0][rp1 & 511]; bR = s->ch_buf[0][rp2 & 511];
                    accL[i] = xL + (int32_t)(((int64_t)(aL + bL) * hm) / 1000);
                    accR[i] = xR + (int32_t)(((int64_t)(aR + bR) * hm) / 1000);
                    w = (w + 1) & 511;
                }
                if (!ch_wrote) s->ch_w = (uint16_t)w;
            }
#else
            (void)ch_wrote;
#endif
        }
#endif
        if (s->p[P_SYN_LEVEL] != 1000) {                         /* the mixer: the synth's level (the drums' levels are their own pots) */
            int32_t lv = s->p[P_SYN_LEVEL];
            for (i = 0; i < WT_CTL; i++) { accL[i] = (int32_t)(((int64_t)accL[i] * lv) / 1000); accR[i] = (int32_t)(((int64_t)accR[i] * lv) / 1000); }
        }
        if (s->p[P_SYN_DIST]) {                                  /* the synth's own distortion, before the drums join */
            wt_sdist_t *sd = &s->sdist[0];
            int drv = s->p[P_SYN_DRIVE];
#if WT_NODRUMS
            {   int nv = blk_nact[(b / WT_CTL) & 15];
                if (nv > 1) drv = (int)(((int64_t)drv * wt2_vc_q15(nv)) >> 15);   /* less drive as the chord gets bigger */
            }
#endif
            sdist_prep(sd, s->p[P_SYN_DIST], drv);
            for (i = 0; i < WT_CTL; i++) {
                float l = sdist_one(sd, (float)accL[i] * (1.0f / 8192.0f), sd->st[0]) * 8192.0f, r = sdist_one(sd, (float)accR[i] * (1.0f / 8192.0f), sd->st[1]) * 8192.0f;
                accL[i] = (int32_t)(l > 1.0e8f ? 1.0e8f : l < -1.0e8f ? -1.0e8f : l);
                accR[i] = (int32_t)(r > 1.0e8f ? 1.0e8f : r < -1.0e8f ? -1.0e8f : r);
            }
        }
#if !WT_NODRUMS
        if (dact)                                                /* the synth alone, for its own sends */
            for (i = 0; i < WT_CTL; i++) sm[i] = ((float)accL[i] * (FX_IN / 32768.0f) + (float)accR[i] * (FX_IN / 32768.0f)) * 0.5f;
        if (dact) {                                              /* the drums into the mix: their level, and their sends in the effects' units */
            float lv = (float)s->p[P_DR_LEVEL] * (16384.0f / 1000.0f), ls = (float)s->p[P_DR_LEVEL] * (2.0f / 1000.0f);
            float rs = ls * (float)s->p[P_DR_RVSEND] * (1.0f / 127.0f), ds = ls * (float)s->p[P_DR_DLSEND] * (1.0f / 127.0f);   /* the drums' own sends */
            wt_sdist_t *sd = &s->sdist[1];
            int dd_on = s->p[P_DR_DIST] != 0;
            float dtg = wt_drums_audible(s) ? 1.0f : 0.0f, dg0 = s->mix_dr_g, ddg = (dtg - dg0) * (1.0f / WT_CTL);
            s->mix_dr_g = dtg;
            if (dd_on) sdist_prep(sd, s->p[P_DR_DIST], s->p[P_DR_DRIVE]);
            for (i = 0; i < WT_CTL; i++) {
                float l = D->L[b + i], r = D->R[b + i], mg = (dg0 == 1.0f && dtg == 1.0f) ? 1.0f : dg0 + ddg * (float)(i + 1);
                if (dd_on) { l = sdist_one(sd, l, sd->st[0]); r = sdist_one(sd, r, sd->st[1]); }     /* the drums' own distortion */
                accL[i] += (int32_t)(l * lv * mg);
                accR[i] += (int32_t)(r * lv * mg);
                dr[i] = D->rev[b + i] * rs * mg; dd[i] = D->dly[b + i] * ds * mg;
            }
        }
#endif
        t0 = TICK();
#if WT_NODRUMS
        fx_block(s, accL, accR, 0, 0, 0);
#else
        fx_block(s, accL, accR, dact ? dr : 0, dact ? dd : 0, dact ? sm : 0);
#endif
        eq_block(s, accL, accR);
#if WT_NODRUMS
        {   int32_t mg = g;
            if (s->hold_gd[5]) {
                int32_t mv = clampi((int)s->p[P_MASTER] + s->hold_gd[5], 0, 1000);
                mg = (int32_t)(((int64_t)mv * mv / 1000) * 32768 / 1000);
            }
            for (i = 0; i < WT_CTL; i++) {
                int32_t L = wt2_shl(accL[i]), R = wt2_shl(accR[i]);   /* the 6 dB back, after the effects */
                L = (int32_t)(((int64_t)L * mg) >> 15);
                R = (int32_t)(((int64_t)R * mg) >> 15);
                wt2_ride(s, &L, &R);
                if (L > 32767) L = 32767; if (L < -32768) L = -32768;
                if (R > 32767) R = 32767; if (R < -32768) R = -32768;
                out[2 * (b + i)] = (int16_t)L;
                out[2 * (b + i) + 1] = (int16_t)R;
            }
        }
#else
        for (i = 0; i < WT_CTL; i++) {
            out[2 * (b + i)] = (int16_t)limiter((int32_t)(((int64_t)accL[i] * g) >> 15));
            out[2 * (b + i) + 1] = (int16_t)limiter((int32_t)(((int64_t)accR[i] * g) >> 15));
        }
#endif
        s->prof[2] += TICK() - t0;
    }
}

int wt_voice_lanes(const wt_synth_t *s, int i)
{
#if WT_NODRUMS
    if (!s || i < 0 || i >= WT_NVOICES || !s->v[i].active) return 0;
    return s->lane_of[i];
#else
    (void)s; (void)i;
    return 0;
#endif
}

void wt_synth_render(wt_synth_t *s, int16_t *out, int frames)
{
    int b = 0;
    int32_t g = (int32_t)(((int64_t)s->p[P_MASTER] * s->p[P_MASTER] / 1000) * 32768 / 1000);
    while (b + WT_CTL <= frames) {                              /* chunks of up to 256 frames: the drums render a chunk at a time */
        int n = frames - b < 256 ? frames - b : 256;
        n -= n % WT_CTL;
        render_chunk(s, out + 2 * b, n, g);
        b += n;
    }
}
