/* SPDX-License-Identifier: GPL-3.0-only */
#include "wt_core.h"
#include "wt_patch.h"

/* BASIC SHAPES frame -> WT POS (9 frames: triangle, saw, square, sine, pulse 37 / 25 / 17 / 10 / 5 %): pos = frame * 1000 / 8 */
#define TRI "0"
#define SAW "125"
#define SQUARE "250"
#define SINE "375"
#define PULSE25 "625"

typedef struct { const char *n, *v; } kv;
static int same(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return !*a && !*b;
}

static const kv init_p[] = {
    {"A_ON", "1"}, {"A_TABLE", "0"}, {"A_WTPOS", SAW}, {"A_UNI", "1"}, {"A_LEVEL", "700"}, {"B_ON", "0"},
    {"SUB_ON", "0"}, {"NOI_ON", "0"}, {"F_TYPE", "0"}, {"VOICEMODE", "0"}, {"MASTER", "800"},
    {"AMP_ATK", "0"}, {"AMP_DEC", "500"}, {"AMP_SUS", "1000"}, {"AMP_REL", "300"}, {0, 0}};

static const kv pad_p[] = {
    {"VOICEMODE", "4"},
    {"A_ON", "1"}, {"A_TABLE", "0"}, {"A_WTPOS", SAW}, {"A_UNI", "5"}, {"A_DETUNE", "350"}, {"A_SPREAD", "800"},
    {"A_BLEND", "800"}, {"A_RAND", "100"}, {"A_LEVEL", "600"},
    {"B_ON", "1"}, {"B_TABLE", "0"}, {"B_WTPOS", SQUARE}, {"B_OCT", "-1"}, {"B_UNI", "3"}, {"B_DETUNE", "200"},
    {"B_RAND", "100"}, {"B_LEVEL", "400"},
    {"F_TYPE", "1"}, {"F_CUTOFF", "820"}, {"F_RESO", "250"}, {"F_KEYTRK", "500"},
    {"AMP_ATK", "620"}, {"AMP_REL", "640"}, {"AMP_DEC", "500"}, {"AMP_SUS", "1000"},
    {"LFO1_SHAPE", "0"}, {"LFO1_RATE", "180"},
    {"MX1_SRC", "LFO1"}, {"MX1_DST", "A_WTPOS"}, {"MX1_AMT", "250"},
    {"MX2_SRC", "LFO1"}, {"MX2_DST", "CUTOFF"}, {"MX2_AMT", "150"}, {0, 0}};

static const kv bass_p[] = {
    {"VOICEMODE", "2"}, {"GLIDE", "400"}, {"A_ON", "1"}, {"A_TABLE", "0"}, {"A_WTPOS", SAW}, {"A_OCT", "-1"},
    {"A_UNI", "2"}, {"A_DETUNE", "100"}, {"A_LEVEL", "650"}, {"SUB_ON", "1"}, {"SUB_WAVE", "0"}, {"SUB_OCT", "-1"},
    {"SUB_LEVEL", "600"}, {"F_TYPE", "2"}, {"F_CUTOFF", "520"}, {"F_RESO", "450"}, {"F_DRIVE", "300"},
    {"AMP_ATK", "0"}, {"AMP_REL", "200"}, {"ENV2_ATK", "0"}, {"ENV2_DEC", "380"}, {"ENV2_SUS", "0"},
    {"MX1_SRC", "ENV2"}, {"MX1_DST", "CUTOFF"}, {"MX1_AMT", "500"}, {0, 0}};

static const kv pluck_p[] = {
    {"A_ON", "1"}, {"A_TABLE", "0"}, {"A_WTPOS", PULSE25}, {"A_UNI", "3"}, {"A_DETUNE", "150"}, {"A_LEVEL", "700"},
    {"F_TYPE", "2"}, {"F_CUTOFF", "600"}, {"F_RESO", "350"}, {"AMP_DEC", "600"}, {"AMP_SUS", "0"}, {"AMP_REL", "250"},
    {"ENV2_DEC", "420"}, {"ENV2_SUS", "0"}, {"MX1_SRC", "ENV2"}, {"MX1_DST", "CUTOFF"}, {"MX1_AMT", "700"},
    {"ENV3_DEC", "350"}, {"ENV3_SUS", "0"}, {"MX2_SRC", "ENV3"}, {"MX2_DST", "A_WTPOS"}, {"MX2_AMT", "-300"}, {0, 0}};

static const kv lead_p[] = {
    {"VOICEMODE", "2"}, {"GLIDE", "300"}, {"A_ON", "1"}, {"A_TABLE", "0"}, {"A_WTPOS", SAW}, {"A_WARPMODE", "2"},
    {"A_WARP", "250"}, {"A_UNI", "3"}, {"A_DETUNE", "180"}, {"A_LEVEL", "700"},
    {"F_TYPE", "1"}, {"F_CUTOFF", "950"}, {"F_RESO", "200"}, {"AMP_ATK", "40"}, {"AMP_REL", "300"},
    {"LFO1_SHAPE", "0"}, {"LFO1_RATE", "560"}, {"MX1_SRC", "LFO1"}, {"MX1_DST", "A_PITCH"}, {"MX1_AMT", "12"},
    {"MX2_SRC", "MOD"}, {"MX2_DST", "A_WARP"}, {"MX2_AMT", "600"}, {0, 0}};

static const kv sweep_p[] = {
    {"A_ON", "1"}, {"A_TABLE", "0"}, {"A_WTPOS", "0"}, {"A_UNI", "1"}, {"A_LEVEL", "800"}, {"AMP_ATK", "20"},
    {"AMP_REL", "300"}, {"LFO1_SHAPE", "2"}, {"LFO1_RATE", "130"}, {"MX1_SRC", "LFO1"}, {"MX1_DST", "A_WTPOS"},
    {"MX1_AMT", "500"}, {0, 0}};

static const kv fmbell_p[] = {   /* A is the carrier, B a silent modulator an octave and a fifth up: B_FM and A_FM at once */
    {"A_ON", "1"}, {"A_WTPOS", SINE}, {"A_LEVEL", "800"}, {"A_FM", "0"},
    {"B_ON", "1"}, {"B_WTPOS", SINE}, {"B_LEVEL", "0"}, {"B_OCT", "1"}, {"B_SEMI", "7"},
    {"A_FM", "260"}, {"B_FM", "90"},
    {"AMP_DEC", "720"}, {"AMP_SUS", "0"}, {"AMP_REL", "500"},
    {"ENV2_DEC", "640"}, {"ENV2_SUS", "0"}, {"MX1_SRC", "ENV2"}, {"MX1_DST", "A_FM"}, {"MX1_AMT", "600"}, {0, 0}};

static const kv tmorph_p[] = {   /* BASIC SHAPES -> FOLD-SINE: WT POS and MORPH from one slow LFO */
    {"A_ON", "1"}, {"A_TABLE", "0"}, {"A_TABLE2", "1"}, {"A_MORPH", "0"}, {"A_WTPOS", "0"}, {"A_UNI", "3"},
    {"A_DETUNE", "200"}, {"A_LEVEL", "700"}, {"F_TYPE", "1"}, {"F_CUTOFF", "980"}, {"AMP_ATK", "300"}, {"AMP_REL", "500"},
    {"LFO1_SHAPE", "1"}, {"LFO1_RATE", "110"},
    {"MX1_SRC", "LFO1"}, {"MX1_DST", "A_MORPH"}, {"MX1_AMT", "1000"},
    {"MX2_SRC", "LFO1"}, {"MX2_DST", "A_WTPOS"}, {"MX2_AMT", "700"}, {0, 0}};

static const kv subnoise_p[] = {   /* pulse sub with its width swept, pink noise and a looping sample layer */
    {"VOICEMODE", "1"}, {"A_ON", "1"}, {"A_WTPOS", SAW}, {"A_LEVEL", "500"}, {"A_UNI", "2"}, {"A_DETUNE", "120"},
    {"SUB_ON", "1"}, {"SUB_WAVE", "3"}, {"SUB_OCT", "-1"}, {"SUB_PW", "500"}, {"SUB_LEVEL", "700"},
    {"NOI_ON", "1"}, {"NOI_TYPE", "1"}, {"NOI_LEVEL", "120"}, {"NOI_FILTER", "300"}, {"NOI_STEREO", "700"},
    {"NOI_SMP", "1"}, {"NOI_SMP_LEVEL", "350"}, {"NOI_SMP_START", "0"}, {"NOI_SMP_RAND", "1000"},
    {"F_TYPE", "2"}, {"F_CUTOFF", "760"}, {"F_RESO", "300"}, {"NOI_FLT", "0"},
    {"LFO1_SHAPE", "0"}, {"LFO1_RATE", "120"}, {"MX1_SRC", "LFO1"}, {"MX1_DST", "SUB_PW"}, {"MX1_AMT", "700"},
    {"AMP_REL", "400"}, {0, 0}};


/* patches with effects: the pad and the lead, plus X0X's reverb, tape delay, distortion and glue compressor */
static const kv space_p[] = {
    {"RV_SEND", "55"}, {"RV_DECAY", "100"}, {"RV_TONE", "60"}, {"RV_LEVEL", "95"},
    {"DL_SEND", "45"}, {"DL_TIME", "5"}, {"DL_FDBK", "60"}, {"DL_TYPE", "1"}, {"DL_LEVEL", "90"}, {"DL_PING", "1"}, {"BPM", "110"}, {0, 0}};
static const kv crunch_p[] = {
    {"FX_DIST", "2"}, {"FX_DRIVE", "70"}, {"FX_COMP", "70"}, {"DL_SEND", "25"}, {"DL_TIME", "8"}, {"DL_FDBK", "45"}, {"DL_LEVEL", "70"},
    {"RV_SEND", "15"}, {"RV_LEVEL", "70"}, {0, 0}};

static void itoa_(int v, char *d)
{
    char t[8];
    int n = 0, neg = v < 0;
    unsigned u = (unsigned)(neg ? -v : v);
    do { t[n++] = (char)('0' + u % 10u); u /= 10u; } while (u);
    if (neg) *d++ = '-';
    while (n) *d++ = t[--n];
    *d = 0;
}

#include "wt_factory_gen.inc"

const char *const wt_patch_names[WT_NPATCHES] = {"init", "pad", "bass", "pluck", "lead", "sweep", "fmbell", "tmorph", "reece1", "space", "crunch"};

static const kv *patch_base(const char *name)    /* the patch a patch builds on (its settings come first) */
{
    return same(name, "space") ? pad_p : same(name, "crunch") ? lead_p : 0;
}

static const kv *patch_table(const char *name)
{
    int i = 0;
    for (; i < WT_NPATCHES; i++) {
        const char *a = wt_patch_names[i], *b = name;
        while (*a && *a == *b) { a++; b++; }
        if (!*a && !*b) break;
    }
    const kv *g = factory_gen(i);
    if (g) return g;
    switch (i) {
    case 0: return init_p;   case 1: return pad_p;    case 2: return bass_p;   case 3: return pluck_p;
    case 4: return lead_p;   case 5: return sweep_p;  case 6: return fmbell_p; case 7: return tmorph_p;
    case 8: return subnoise_p;
    case 9: return space_p;   case 10: return crunch_p;
    default: return 0;
    }
}

int patch_apply(const char *name, patch_set_fn set, void *ctx)
{
    const kv *p = patch_table(name);
    int i;
    if (!p) return 1;
    for (i = 0; i < P_COUNT; i++) {      /* every patch starts from the parameter defaults */
        char v[8];
        itoa_(wt_param_desc[i].def, v);
        set(ctx, wt_param_desc[i].name, v);
    }
    if (p != init_p) for (const kv *q = init_p; q->n; q++) set(ctx, q->n, q->v);
    if (patch_base(name)) for (const kv *q = patch_base(name); q->n; q++) set(ctx, q->n, q->v);
    for (; p->n; p++) set(ctx, p->n, p->v);
    return 0;
}

/* a patch as a plain parameter array: nothing is touched but out[P_COUNT] */
static int vset(void *ctx, const char *name, const char *val)
{
    int16_t *out = (int16_t *)ctx;
    int id = wt_param_find(name), v = 0, i, neg = 0;
    const char *s = val;
    if (id < 0) return -1;
    if (id >= P_MX0 && (id - P_MX0) % 3 == 0) { for (i = 0; i < S_COUNT; i++) if (same(wt_src_names[i], val)) { out[id] = (int16_t)i; return 0; } }
    if (id >= P_MX0 && (id - P_MX0) % 3 == 1) { for (i = 0; i < D_COUNT; i++) if (same(wt_dst_names[i], val)) { out[id] = (int16_t)i; return 0; } }
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
    if (neg) v = -v;
    if (v < wt_param_desc[id].min) v = wt_param_desc[id].min;
    if (v > wt_param_desc[id].max) v = wt_param_desc[id].max;
    out[id] = (int16_t)v;
    return 0;
}

int wt_patch_values(const char *name, int16_t *out) { return patch_apply(name, vset, out); }
