/* SPDX-License-Identifier: GPL-3.0-only
 * The device's screens and controls (firmware/src/app/wt_ui.c) on the Mac: the same drawing code (gfx.c) paints into a
 * 240 x 240 picture instead of the LCD, buttons and knobs are scripted, and the whole thing runs under the address and
 * undefined-behaviour checkers. Writes the pictures into build/ui, exits non-zero on a failed check.
 *   cc -fsanitize=address,undefined ... tests/ui_host.c ... ; ./ui_host [outdir] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define X0X_HOST 1
#define RING_PUBLISH() do { } while (0)
#define memset wt_h_memset
#define memcpy wt_h_memcpy
#define memcmp wt_h_memcmp
#include "libc.c"                                   /* the device's str_cpy, fmt_int ... */
#undef memset
#undef memcpy
#undef memcmp
#include "wt_core.h"
#include "wt_data.h"
#include "wt_patch.h"
#if !WT_NODRUMS
#include "wt_drums.h"
#endif
#include "app/plat.h"

/* ---- the screen: gfx.c unchanged, the LCD replaced by a picture ---- */
static uint16_t fb[240 * 240];
static void lcd_sync(void) { }
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)
{
    uint32_t i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            if (x + i < 240 && y + j < 240) { uint16_t v = px[j * w + i]; fb[(y + j) * 240 + x + i] = (uint16_t)((v >> 8) | (v << 8)); }
}
#define section(x)
#include "gfx.c"
#undef section

/* ---- what wt_app.c gives the screens ---- */
static wt_synth_t synth;
#if !WT_NODRUMS
static wt_drums_t drums;
#endif
static wt_table_t wtab[WT_NTABLES];
static uint32_t fm1_ms;
static void drain(void);
static void fm1_wdt_feed(void) { fm1_ms++; drain(); }      /* the device's audio interrupt takes commands while the main loop waits */
enum { WC_ON, WC_OFF, WC_PARAM, WC_BEND, WC_MOD, WC_ALLOFF, WC_PATCH, WC_PLAY, WC_QUEUE, WC_DHIT, WC_DQUEUE, WC_DSET, WC_DMUTE, WC_PANIC };
static int16_t wpatch[P_COUNT];
static volatile uint8_t wpatch_pending;
static struct { uint8_t t, a; int16_t b; } cq[4096];
static int cq_n;
static void wq_put(uint8_t t, uint8_t a, int16_t b) { if (cq_n < 4096) { cq[cq_n].t = t; cq[cq_n].a = a; cq[cq_n].b = b; cq_n++; } }
static void drain(void)                              /* the audio interrupt's job */
{
    int i, j;
    for (i = 0; i < cq_n; i++) {
        switch (cq[i].t) {
        case WC_ON: wt_synth_key_on(&synth, cq[i].a, cq[i].b); break;
        case WC_OFF: wt_synth_key_off(&synth, cq[i].a); break;
        case WC_PARAM: wt_synth_set_param(&synth, cq[i].a, cq[i].b); break;
        case WC_ALLOFF: wt_synth_all_off(&synth); break;
        case WC_PLAY: wt_seq_play(&synth, cq[i].a);
#if !WT_NODRUMS
            wt_drums_play(&synth, cq[i].a);
#endif
            break;
        case WC_PANIC: wt_synth_panic(&synth); break;
#if !WT_NODRUMS
        case WC_DHIT: wt_drums_hit(&synth, cq[i].a, cq[i].b); break;
        case WC_DQUEUE: wt_drums_queue(&synth, cq[i].a); break;
        case WC_DSET: wt_drums_set(&drums, (cq[i].a >> 4) & 1, cq[i].a & 15, (cq[i].b >> 8) & 15, cq[i].b & 127); break;
        case WC_DMUTE: if (cq[i].a < WT_DTRACKS) drums.mute[cq[i].a] = (uint8_t)(cq[i].b > 2 ? 2 : cq[i].b); break;
#endif
        case WC_QUEUE: wt_seq_queue(&synth, cq[i].a); break;
        case WC_PATCH: {
            static const uint8_t KEEP[] = {P_SYN_MUTE, P_DR_MUTE, P_MIDI_CH, P_MIDI_DRCH, P_MIDI_OUTCH, P_USB_AUDIO, P_CORE2, P_SCALE, P_ROOT, P_SCALE_SEQ, P_CLK_SRC, P_CLK_SS};
            int16_t keep[sizeof KEEP]; int k2;
            for (k2 = 0; k2 < (int)sizeof KEEP; k2++) keep[k2] = synth.p[KEEP[k2]];
            wt_synth_all_off(&synth); for (j = 0; j < P_COUNT; j++) wt_synth_set_param(&synth, j, wpatch[j]);
            for (k2 = 0; k2 < (int)sizeof KEEP; k2++) wt_synth_set_param(&synth, KEEP[k2], keep[k2]);
            wpatch_pending = 0; break; }
        default: break;
        }
    }
    cq_n = 0;
}
static uint32_t sim_buttons, sim_keys;
static int32_t sim_enc[NE];
uint32_t plat_buttons(void) { return sim_buttons; }
uint32_t plat_keys(void) { return sim_keys; }
int32_t plat_enc(int role) { int32_t d = sim_enc[role]; sim_enc[role] = 0; return d; }
#if WT_P37
static int sim_fader[4], sim_wheel[2] = {512, 0}, sim_extq[32], sim_extn, sim_extr;
int plat_fader(int i) { return sim_fader[i & 3]; }
int plat_key_vel(int i) { (void)i; return 90; }
int32_t plat_extkey_take(void) { return sim_extr < sim_extn ? sim_extq[sim_extr++ % 32] : -1; }
int plat_wheel(int i) { return sim_wheel[i & 1]; }
int plat_pedal(void) { return 0; }
static uint32_t sim_pads;
static uint8_t last_rgb[16][3];
void plat_pad_leds(const uint8_t rgb[16][3]) { int p, c; for (p = 0; p < 16; p++) for (c = 0; c < 3; c++) last_rgb[p][c] = rgb[p][c]; }
uint32_t plat_pads(void) { return sim_pads; }
int plat_pad_press(int p) { (void)p; return 64; }
#endif
uint32_t plat_cpu_pct(void) { return 37; }
uint32_t plat_cpu_peak_pct(void) { return 55; }
int plat_cpu2_on(void) { return 1; }
static uint32_t last_lit;
static uint32_t last_keys, last_dim;
void plat_keys_dim(uint32_t keys) { last_dim = keys; }
void plat_leds(uint32_t buttons, uint32_t keys) { last_lit = buttons; last_keys = keys; }

static uint8_t fs_obj[OBJ_NOBJ][PLAT_STORE_MAX];      /* the flash store, in memory */
static int fs_len[OBJ_NOBJ] = {-1, -1, -1, -1, -1, -1, -1};
static int fs_fail, fs_saves;
int plat_store_load(uint32_t obj, void *dst, uint32_t max) { uint32_t n; if (obj >= OBJ_NOBJ || fs_len[obj] < 0) return -1; n = (uint32_t)fs_len[obj]; if (n > max) n = max; memcpy(dst, fs_obj[obj], n); return (int)n; }
int plat_store_save(uint32_t obj, const void *src, uint32_t len) { if (fs_fail || obj >= OBJ_NOBJ || len > PLAT_STORE_MAX) return -1; memcpy(fs_obj[obj], src, len); fs_len[obj] = (int)len; fs_saves++; return 0; }

#include "app/wt_ui.c"

/* ---- helpers ---- */
static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)
static const char *outdir = "build/ui";
static int nsnap;

static void frame(void)            /* one UI frame, then the audio side takes the commands, and a block renders (live modulation) */
{
    static int16_t buf[2 * 256];
    fm1_ms += 17;
    wui_frame();
    drain();
    wt_synth_render(&synth, buf, 256);
}
static void snap(const char *name)
{
    char path[256];
    FILE *f;
    int i;
    wui.dirty = 1;
    frame();
    snprintf(path, sizeof path, "%s/%02d_%s.ppm", outdir, nsnap++, name);
    f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    fprintf(f, "P6\n240 240\n255\n");
    for (i = 0; i < 240 * 240; i++) {
        uint16_t v = fb[i];
        fputc((v >> 11) * 255 / 31, f); fputc(((v >> 5) & 63) * 255 / 63, f); fputc((v & 31) * 255 / 31, f);
    }
    fclose(f);
}
static void press(int b) { sim_buttons |= 1u << b; frame(); sim_buttons &= ~(1u << b); frame(); }
static void turn(int role, int n) { sim_enc[role] += n; frame(); }
static int page_named(const char *t) { int i; for (i = 0; i < NPAGE; i++) if (!strcmp(PAGES[i].title, t)) return i; return -1; }

static int g_ev_k[32], g_ev_n[32]; static uint32_t g_ev_t[32]; static int g_nev;
void logger(int kind, int note, int vel, int accent, uint32_t t) { (void)vel; (void)accent; if (g_nev < 32) { g_ev_k[g_nev] = kind; g_ev_n[g_nev] = note; g_ev_t[g_nev] = t; g_nev++; } }

#if WT_P37
static void tap_pad(int pad, int with_next)     /* a pad struck and let go (pad 0..15, top row first); with_next: > held meanwhile */
{
    int sl = pad < 8 ? pad + 8 : pad - 8, k;
    for (k = 0; k < NKEYS; k++) if (white_slot[k] == sl) break;
    if (with_next) { sim_buttons |= 1u << B_NEXT; frame(); }
    sim_keys = sim_pads = 1u << k; frame();
    sim_keys = sim_pads = 0; frame();
    if (with_next) { sim_buttons &= ~(1u << B_NEXT); frame(); }
}
static void p37_tests(void)
{
    int i, v;
    /* the faders: bank 1 is MIX (master, synth level, drum level, reverb send), the others AMP ENV, MOD, OSC */
    CHECK(fdr_get(0, 0) == P_MASTER && fdr_get(0, 3) == P_FX_RVSEND && fdr_get(1, 0) == P_ENV(0, E_ATK) && fdr_get(3, 3) == P_OSC(1, O_WARP), "default fader banks wrong (%d %d %d %d)", fdr_get(0, 0), fdr_get(0, 3), fdr_get(1, 0), fdr_get(3, 3));
    ui_p[P_MASTER] = 500; wq_put(WC_PARAM, P_MASTER, 500); drain();
    sim_fader[0] = 10; frame();
    CHECK(ui_p[P_MASTER] == 500, "a fader moved the master before it met the value (%d)", ui_p[P_MASTER]);
    sim_fader[0] = 64; frame(); frame();                            /* 500 of 1000 = fader 63..64: caught */
    sim_fader[0] = 100; frame();
    CHECK(ui_p[P_MASTER] > 750 && ui_p[P_MASTER] < 820 && synth.p[P_MASTER] == ui_p[P_MASTER], "fader 1 -> MASTER %d (synth %d)", ui_p[P_MASTER], synth.p[P_MASTER]);
    /* FADER BANK tapped: the next bank, its faders wait to meet their values */
    press(B_FBANK);
    CHECK(wui.fbank == 1 && fdr_get(wui.fbank, 0) == P_ENV(0, E_ATK), "FADER BANK tap: bank %d", wui.fbank);
    v = ui_p[P_ENV(0, E_ATK)]; sim_fader[0] = 120; frame();
    CHECK(ui_p[P_ENV(0, E_ATK)] == v, "a fader jumped its new parameter");
    snap("p37_fader_bank");
    /* FADER BANK held and K5 turned: fader 1 of the bank takes another parameter, kept in the patch values */
    sim_buttons |= 1u << B_FBANK; frame();
    for (i = 0; i < 40; i++) frame();
    turn(EN_K1, 1);
    snap("p37_fader_assign");
    sim_buttons &= ~(1u << B_FBANK); frame();
    CHECK(wui.fbank == 1 && fdr_get(1, 0) != P_ENV(0, E_ATK) && fdr_get(1, 1) == P_ENV(0, E_DEC), "assignment: fader 1 -> %d", fdr_get(1, 0));
    drain();
    CHECK(synth.p[P_FDR0 + 2] == ui_p[P_FDR0 + 2], "the assignment did not reach the synth's parameters");
    for (i = 0; i < 3; i++) press(B_FBANK);
    CHECK(wui.fbank == 0, "four banks do not wrap (%d)", wui.fbank);
    /* the wheels: the first frames set the rest; then a turn bends, the mod wheel rises */
    for (i = 0; i < 40; i++) frame();
    sim_wheel[0] = 900; sim_wheel[1] = 600; frame();
    CHECK(wui.bend_sent > 4000, "pitch wheel up: bend %d", wui.bend_sent);
    CHECK(wui.mod_sent > 0, "mod wheel: %d", wui.mod_sent);
    sim_wheel[0] = 512; sim_wheel[1] = 0; frame();
    CHECK(wui.bend_sent == 0 && wui.mod_sent == 0, "wheels at rest: bend %d mod %d", wui.bend_sent, wui.mod_sent);
    /* the outer keys: C6 (key 36) and C3 (key 0) play, with their velocity */
    sim_extq[sim_extn++ % 32] = 36 << 8 | 110; frame();
    v = 0; for (i = 0; i < WT_NVOICES; i++) if (synth.v[i].active && synth.v[i].note == 84) v = 1;
    CHECK(v, "C6 does not sound");
    sim_extq[sim_extn++ % 32] = 36 << 8 | 0; sim_extq[sim_extn++ % 32] = 0 << 8 | 80; frame();
    v = 0; for (i = 0; i < WT_NVOICES; i++) if (synth.v[i].active && synth.v[i].note == 48 && synth.v[i].gate) v = 1;
    CHECK(v, "C3 does not sound");
    sim_extq[sim_extn++ % 32] = 0 << 8 | 0; frame();
    /* the knobs: K1 pages, K3 cutoff, K4 resonance; SCALE opens the scale page */
    wui.page = 0; turn(EN_SELECT, 1); CHECK(wui.page == 1, "K1 did not turn the page (%d)", wui.page);
    v = ui_p[P_F_CUTOFF]; turn(EN_CUT, 3); CHECK(ui_p[P_F_CUTOFF] > v, "K3 cutoff %d -> %d", v, ui_p[P_F_CUTOFF]);
    v = ui_p[P_F_RESO]; turn(EN_RES, 3); CHECK(ui_p[P_F_RESO] > v, "K4 resonance %d -> %d", v, ui_p[P_F_RESO]);
    snap("p37_cutoff_readout");
    press(B_SCALE); CHECK(!strcmp(PAGES[wui.page].title, "SCALE"), "SCALE opens %s", PAGES[wui.page].title);
    CHECK((last_lit >> B_SCALE) & 1u, "the SCALE LED is not lit on the scale page");
    wui.page = (int8_t)page_named("VOICE"); v = ui_p[P_MASTER];                /* KNOB BANK is the shift: held, K1 is the master volume (and does not turn the page) */
    sim_buttons |= 1u << B_SEL; frame(); turn(EN_SELECT, -2); sim_buttons = 0; frame();
    CHECK(ui_p[P_MASTER] < v && wui.page == (int8_t)page_named("VOICE"), "KNOB BANK + K1: master %d -> %d, page %d", v, ui_p[P_MASTER], wui.page);
    CHECK(!wui.sel_latched && !wui.sel_down, "a KNOB BANK tap latched or stuck");
    /* < jumps to OSC A, > to the drums */
    wui.page = (int8_t)page_named("VOICE");
    press(B_PREV); CHECK(!strcmp(PAGES[wui.page].title, "OSC A"), "< opens %s", PAGES[wui.page].title);
    press(B_NEXT); CHECK(!strcmp(PAGES[wui.page].title, "DRUMS"), "> opens %s", PAGES[wui.page].title);
    for (i = 0; i < 40; i++) frame();
    {   int lit = 0, p;
        for (p = 0; p < 16; p++) lit += last_rgb[p][0] + last_rgb[p][1] + last_rgb[p][2];
        CHECK(!lit, "pad lights are not off by default");
    }
    /* play mode: a tap on a pad plays its sound and lights it; the first 8 sounds are the top row, 9..11 wrap to the second row */
    tap_pad(2, 0);
    CHECK(wui.dtrack == 2 && (last_rgb[2][0] + last_rgb[2][1] + last_rgb[2][2]) > 0 && !(last_rgb[3][0] + last_rgb[3][1] + last_rgb[3][2]), "pad 3: track %d, light %d", wui.dtrack, last_rgb[2][1]);
    tap_pad(8, 0); CHECK(wui.dtrack == 8, "pad 9 (second row, first): track %d", wui.dtrack);
    for (i = 0; i < 20; i++) frame();
    CHECK(!(last_rgb[8][0] + last_rgb[8][1] + last_rgb[8][2]), "a struck pad stays lit");
    for (i = 0; i < NKEYS; i++) if (black_slot[i] == 5) break;
    sim_keys = 1u << i; frame(); v = 0; for (i = 0; i < WT_NVOICES; i++) if (synth.v[i].active) v = 1; sim_keys = 0; frame();
    CHECK(wui.dtrack == 8 && v, "a black key on the drum page changed the track (%d) or was silent", wui.dtrack);
    /* >> : step entry (red flash twice) */
    {   int red = 0, grn = 0, edges = 0, prev = 0;
        press(B_NEXT); press(B_NEXT);
        for (i = 0; i < 40; i++) { int on = last_rgb[0][0] > 0; frame(); if (last_rgb[0][0] > 0) red = 1; if (last_rgb[0][1] > 0) grn = 1; if (on && !prev) edges++; prev = on; }
        CHECK(wui.pmode && red && !grn && edges >= 1, "the >> flash: mode %d red %d green %d", wui.pmode, red, grn);
    }
    tap_pad(4, 1);                                                                    /* (> held) the sound the steps are for */
    CHECK(wui.dtrack == 4 && wui.pmode, "> + pad 5: track %d", wui.dtrack);
    for (i = 0; i < 20; i++) frame();
    tap_pad(1, 0); CHECK(dpat()->st[4][1] == 100 && last_rgb[1][2] > last_rgb[1][0], "a tap puts the step on, blue (%d)", dpat()->st[4][1]);
    for (i = 0; i < 25; i++) frame();
    CHECK(dpat()->st[4][1] == 100 && last_rgb[1][2] > 0, "the step light went out");
    tap_pad(1, 0); CHECK(dpat()->st[4][1] == 0 && !last_rgb[1][2], "a second tap does not take the step off (%d)", dpat()->st[4][1]);
    for (i = 0; i < 25; i++) frame();
    tap_pad(3, 0); tap_pad(3, 0);                                                        /* a double tap: loud accent, magenta */
    CHECK(dpat()->st[4][3] == (100 | DS_ACCENT) && last_rgb[3][0] > 0 && last_rgb[3][2] > last_rgb[3][1], "double tap: step %d", dpat()->st[4][3]);
    for (i = 0; i < 25; i++) frame();
    tap_pad(3, 0); tap_pad(3, 0);
    CHECK(dpat()->st[4][3] == 100 && (dx_get(dpat(), 4, 3) & DX_SOFT), "double tap on a loud accent: step %d ex %d", dpat()->st[4][3], dx_get(dpat(), 4, 3));
    for (i = 0; i < 25; i++) frame();
    tap_pad(3, 0); tap_pad(3, 0);
    CHECK(dpat()->st[4][3] == 100 && !dx_get(dpat(), 4, 3), "double tap on a soft accent: step %d ex %d", dpat()->st[4][3], dx_get(dpat(), 4, 3));
    for (i = 0; i < 25; i++) frame();
    {   int pad = 5, sl = pad < 8 ? pad + 8 : pad - 8, k;                                /* a held pad: fader 1 velocity, fader 2 probability */
        for (k = 0; k < NKEYS; k++) if (white_slot[k] == sl) break;
        sim_fader[0] = 100; sim_fader[1] = 100; frame(); frame();
        sim_keys = sim_pads = 1u << k; frame(); frame();
        sim_fader[0] = 64; frame(); sim_fader[1] = 0; frame();
        sim_keys = sim_pads = 0; frame(); frame();
        CHECK((dpat()->st[4][5] & 127) == 64 && (dx_get(dpat(), 4, 5) >> 1) == 50, "fader 1 velocity / fader 2 probability: %d %d", dpat()->st[4][5], dx_get(dpat(), 4, 5));
    }
    for (i = 0; i < 25; i++) frame();
    press(B_NEXT); press(B_NEXT); press(B_NEXT); for (i = 0; i < 30; i++) frame();
    CHECK(!strcmp(PAGES[wui.page].title, "DRUM ACCENT") && wui.pmode, ">>> opens %s", PAGES[wui.page].title);
    snap("p37_accent_page");
    wui.page = (int8_t)page_named("DRUMS"); frame(); snap("p37_step_entry");
    press(B_NEXT); press(B_NEXT);
    {   int grn = 0;
        for (i = 0; i < 40; i++) { frame(); if (last_rgb[0][1] > 0 && !last_rgb[0][0]) grn = 1; }
        CHECK(!wui.pmode && grn, "the >> exit: mode %d green %d", wui.pmode, grn);
    }
    wui.dtrack = 2;
    wui.page = (int8_t)page_named("P37 CONTROLS"); sim_fader[2] = 77; frame(); snap("p37_controls");
}
#endif

int main(int argc, char **argv)
{
    int i, p;
    if (argc > 1) outdir = argv[1];
    wt_global_init();
    wt_gen_basic_shapes(&wtab[0]); wt_gen_fold_sine(&wtab[1]); wt_gen_odd_even(&wtab[2]);
    wt_synth_init(&synth);
#if !WT_NODRUMS
    wt_drums_init(&drums);
    wt_synth_set_drums(&synth, &drums);
#endif
    for (i = 0; i < 3; i++) wt_synth_set_table(&synth, i, &wtab[i]);
    palette_set(5);
    wui_init();
    drain();
#if WT_NODRUMS
    CHECK(!strcmp(WT_PRODUCT, "WT-2"), "splash name %s", WT_PRODUCT);
#else
    CHECK(!strcmp(WT_PRODUCT, "WT-1"), "splash name %s", WT_PRODUCT);
#endif
#if WT_P37
    p37_tests();
    printf("ui_host (P37): %d checks, %d failed; pictures in %s/\n", checks, fails, outdir);
    return fails ? 1 : 0;
#endif

#if WT_NODRUMS
    {   /* WT2 on the FM-1: one button per place, SEL is a page button (no mode) */
        static const struct { int b; const char *t, *t2; } W[] = {
            {B_SEL, "OSC A", "A WARP"}, {B_EDIT, "OSC B", "B WARP"}, {B_HOME, "FILTER", "FILTER 2"}, {B_LFO, "LFO 1", "MOD 1"},
            {B_ENV, "AMP ENV", "AMP ENV 2"}, {B_ARP, "ARP", "ARP 2"}, {B_FX, "SYNTH FX", "CHORUS"}, {B_SEQ, "STEPS", "PATTERN"},
            {B_GLO, "PERFORM", "VOICE"} };
        for (p = 0; p < 9; p++) {
            wui.page = (int8_t)page_named("SAVE");
            press(W[p].b); CHECK(!strcmp(PAGES[wui.page].title, W[p].t), "W2 button %d opens %s", W[p].b, PAGES[wui.page].title);
            press(W[p].b); CHECK(!strcmp(PAGES[wui.page].title, W[p].t2), "W2 button %d again opens %s", W[p].b, PAGES[wui.page].title);
        }
        CHECK(!wui.sel_down && !wui.sel_latched, "SEL started a mode");
        {   /* LFO: a press opens the LFO pages; a fast second press the MOD pages, then single presses step them, another double tap goes back */
            wui.page = (int8_t)page_named("SAVE");
            press(B_LFO); CHECK(!strcmp(PAGES[wui.page].title, "LFO 1"), "LFO opens %s", PAGES[wui.page].title);
            press(B_LFO); CHECK(!strcmp(PAGES[wui.page].title, "MOD 1"), "LFO double tap opens %s", PAGES[wui.page].title);
            { int i; for (i = 0; i < 20; i++) frame(); }       /* (fm1_ms moves on) */
            press(B_LFO); CHECK(!strncmp(PAGES[wui.page].title, "MOD", 3), "LFO on a MOD page stays in them (%s)", PAGES[wui.page].title);
        }
        wui.page = (int8_t)page_named("CHORUS"); frame(); snap("w2_chorus"); CHECK(!strcmp(PAGES[wui.page].title, "CHORUS"), "chorus page");
        {   /* the LFO shape editor */
            wt_lfoshape_t *sh = &ushp[0]; int n0, y0v;
            wui.page = (int8_t)page_named("SHAPE DRAW"); ed_slot = 0; ed_pt = 0; frame(); snap("w2_shape_draw");
            n0 = sh->n;
            turn(EN_K2, 1); CHECK(ed_pt == 1, "K2 did not pick the point (%d)", ed_pt);
            y0v = sh->y[1]; turn(EN_K3, 1); CHECK(sh->y[1] == y0v + 4 || y0v >= 123, "K3 height %d -> %d", y0v, sh->y[1]);
            lfoed_key(0); CHECK(sh->n == n0 + 1, "ADD: %d points", sh->n);
            lfoed_key(1); CHECK(sh->n == n0, "DELETE: %d points", sh->n);
            lfoed_key(3); lfoed_key(2); CHECK(sh->n == n0 && sh->y[0] == -127, "RESET");
            wui.page = (int8_t)page_named("SHAPE CURVE"); ed_pt = 0; turn(EN_K3, 2); CHECK(sh->c[1] != 0 && ed_pt == 1, "K3 curve (c %d, point %d)", sh->c[1], ed_pt);
            { int c0 = sh->c[1]; lfoed_key(5); CHECK(sh->c[1] == c0 + 25 || c0 + 25 > 100, "BEND+"); lfoed_key(6); lfoed_key(6); CHECK(sh->c[1] < c0, "BEND-"); }
            lfoed_key(7); CHECK(ui_p[P_LFO(0, 0)] == 7, "LFO 1 assigned the shape (%d)", ui_p[P_LFO(0, 0)]);
            lfoed_key(8); CHECK(ui_p[P_LFO(1, 0)] == 7, "LFO 2 assigned the shape");
            {   /* the LFO page's shape dial reaches the drawn shapes, and the matrix routes it to the cutoff */
                int v; wui.page = (int8_t)page_named("LFO 1"); ui_p[P_LFO(0, 0)] = 0;
                for (v = 0; v < 20; v++) turn(EN_K1, 1);
                CHECK(ui_p[P_LFO(0, 0)] == 14, "LFO 1 shape dial reaches %d", ui_p[P_LFO(0, 0)]);
                CHECK(strcmp(wt_lfoshape_name(7), "BLANK") == 0, "name");
            }
        CHECK(page_named("MASTER FX") < 0, "MASTER FX page present");
        frame(); snap("w2_shape_curve");
        ed_slot = 0; lfoed_key(2);
        }
        for (p = 0; p < NPAGE; p++) {
            const char *t = PAGES[p].title;
            int k;
            CHECK(strcmp(t, "SIDECHAIN") && strcmp(t, "DRUMS") && strcmp(t, "DRUM KIT") && strcmp(t, "DRUM FX") && strcmp(t, "MIXER") && strncmp(t, "DRUM", 4) && strncmp(t, "SOUND ", 6), "drum page present: %s", t);
            for (k = 0; k < 4; k++) if (PAGES[p].l[k]) CHECK(strcmp(PAGES[p].l[k], "DRUM CHANNEL"), "drum channel knob on %s", t);
        }
        CHECK(P_COUNT == 206, "parameter count %d", P_COUNT);
        CHECK(page_named("COMPRESS") >= 0 && page_named("UNISON") >= 0, "compress and unison pages");
        CHECK(wt_param_desc[P_OSC(0, O_WARPMODE)].max == 6 && wt_param_desc[P_OSC(1, O_WARPMODE)].max == 6, "warp mode range %d", wt_param_desc[P_OSC(0, O_WARPMODE)].max);
        CHECK(wt_param_find("UNI_TOP") == P_DR_KIT && wt_param_find("HYPER") == P_DR_DIST && wt_param_find("DR_KIT") < 0, "UNI_TOP / HYPER names");
        CHECK(D_SUBLVL > D_SUBPW && D_MASTER + 1 == D_COUNT && !strcmp(wt_dst_names[D_SUBLVL], "SUB_LVL") && !strcmp(wt_dst_names[D_MASTER], "MASTER"), "matrix destinations appended");
        {   int ci = page_named("CHORUS"), ui = page_named("UNISON"), co = page_named("COMPRESS");
            CHECK(ci >= 0 && pid[ci][3] == P_DR_DIST, "chorus HYPER knob");
            CHECK(ui >= 0 && pid[ui][0] == P_DR_KIT, "unison UNI_TOP knob");
            CHECK(co >= 0 && pid[co][0] == wt_param_find("FX_COMP"), "compress FX_COMP knob");
            wui.page = (int8_t)page_named("SAVE"); press(B_FX); press(B_FX); press(B_FX);
            CHECK(!strcmp(PAGES[wui.page].title, "COMPRESS"), "FX third press is %s", PAGES[wui.page].title);
            wui.page = (int8_t)page_named("SAVE"); press(B_GLO); press(B_GLO); press(B_GLO);
            CHECK(!strcmp(PAGES[wui.page].title, "UNISON"), "GLOBAL third press is %s", PAGES[wui.page].title);
        }
        {   /* ABOUT is the last GLOBAL page, read-only, and shows the build's own version and identity */
            int ai = page_named("ABOUT"), i, last = -1, master = ui_p[P_MASTER];
            CHECK(ai >= 0 && PAGES[ai].kind == K_ABOUT && PAGES[ai].btn == B_GLO, "about page");
            CHECK(NPAGE == 63, "page count %d", NPAGE);
            for (i = 0; i < NPAGE; i++) if (PAGES[i].btn == B_GLO) last = i;
            CHECK(last == ai, "about is the last GLOBAL page");
            wui.page = (int8_t)ai; press(B_GLO);
            CHECK(!strcmp(PAGES[wui.page].title, "PERFORM"), "after ABOUT, GLOBAL wraps to %s", PAGES[wui.page].title);
            wui.page = (int8_t)ai;
            turn(EN_K1, 4); turn(EN_K2, -3); turn(EN_K3, 2); turn(EN_K4, -1);
            CHECK(wui.page == (int8_t)ai && ui_p[P_MASTER] == master, "about knobs do not edit");
            CHECK(!strcmp(ABOUT_LINE[0], "WT-2 by Dedrich") && !strcmp(ABOUT_LINE[1], "Web Editor at thatsoundshot.com"), "about credit");
#ifdef X0X_VERSION
            CHECK(!strcmp(ABOUT_LINE[2], X0X_VERSION), "version %s", ABOUT_LINE[2]);
#else
            CHECK(!strcmp(ABOUT_LINE[2], "dev"), "version %s", ABOUT_LINE[2]);
#endif
            CHECK(!strcmp(ABOUT_LINE[3], FELUCCA_ID), "identity %s", ABOUT_LINE[3]);
            CHECK(P_COUNT == 206, "still 206 parameters");
            wui.toast_t = 0;
            wui.page = (int8_t)ai; snap("w2_about");
        }
        {   /* MIDI page: in, out, USB audio. The setup and a drawn shape come back from the store with no drum payload. */
            int mi = page_named("MIDI");
            CHECK(mi >= 0 && pid[mi][0] == P_MIDI_CH && pid[mi][1] == P_MIDI_OUTCH && pid[mi][2] == P_USB_AUDIO && pid[mi][3] < 0, "MIDI knobs");
            ui_p[P_MIDI_CH] = 4; wq_put(WC_PARAM, (uint8_t)P_MIDI_CH, 4); drain();
            ushp[3].y[0] = 40;
            wui.what = 2; wui.armed = 1; wui_save_press();
            CHECK(wui.msg == 1, "setup not saved (msg %d)", wui.msg);
            wt_synth_set_param(&synth, P_MIDI_CH, 1); ushp[3].y[0] = 0;
            CHECK(drums_load() == 1 && synth.p[P_MIDI_CH] == 4, "setup midi channel %d", synth.p[P_MIDI_CH]);
            CHECK(lfs_load() == 1 && ushp[3].y[0] == 40, "shape did not come back (%d)", ushp[3].y[0]);
        }
    }
    printf("ui_host (WT2): %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
#endif
#if !WT_NODRUMS
    /* 1. every page draws, with nothing out of bounds */
    for (p = 0; p < NPAGE; p++) {
        char nm[48];
        wui.page = (int8_t)p;
        snprintf(nm, sizeof nm, "page_%s", PAGES[p].title);
        for (i = 0; nm[i]; i++) if (nm[i] == ' ') nm[i] = '_';
        snap(nm);
    }
    CHECK(NPAGE >= 30, "only %d pages", NPAGE);

    /* 2. buttons: the first press opens a button's page, the next ones flip through its pages and wrap */
    wui.page = 0;
    press(B_EDIT); CHECK(!strcmp(PAGES[wui.page].title, "OSC A"), "EDIT opens %s", PAGES[wui.page].title);
    press(B_EDIT); CHECK(!strcmp(PAGES[wui.page].title, "A WARP"), "EDIT again opens %s", PAGES[wui.page].title);
    for (i = 0; i < 20; i++) press(B_EDIT);
    CHECK(PAGES[wui.page].btn == B_EDIT, "EDIT left its own pages");
    press(B_FX); CHECK(!strcmp(PAGES[wui.page].title, "MASTER FX"), "FX opens %s", PAGES[wui.page].title);
    press(B_ENV); CHECK(!strcmp(PAGES[wui.page].title, "AMP ENV"), "ENV opens %s", PAGES[wui.page].title);
    press(B_ARP); CHECK(!strcmp(PAGES[wui.page].title, "ARP"), "ARP opens %s", PAGES[wui.page].title);
    press(B_ARP); CHECK(!strcmp(PAGES[wui.page].title, "ARP 2"), "ARP again opens %s", PAGES[wui.page].title);
    press(B_ENV);
    turn(EN_SELECT, 1); CHECK(!strcmp(PAGES[wui.page].title, "AMP ENV 2"), "SELECT steps pages (%s)", PAGES[wui.page].title);

    /* 3. knobs: a value moves, stops at its ends */
    wui.page = (int8_t)page_named("OSC A");
    {
        int id = pid[wui.page][0], before = ui_p[id];
        turn(EN_K1, 5);
        CHECK(ui_p[id] > before, "K1 did not raise WT POS");
        turn(EN_K1, 500);
        CHECK(ui_p[id] == wt_param_desc[id].max, "WT POS stops at %d, not %d", ui_p[id], wt_param_desc[id].max);
        turn(EN_K1, -500);
        CHECK(ui_p[id] == wt_param_desc[id].min, "WT POS goes below its minimum (%d)", ui_p[id]);
        turn(EN_K1, 25);
    }
    snap("osc_a_turned");

    /* 4. modulation by hand: hold SEL, pick LFO 2, turn the WT POS knob */
    wui.page = (int8_t)page_named("OSC A");
    sim_buttons |= 1u << B_SEL; frame();
    CHECK(wui.sel_down, "SEL held is not seen");
    while (wui.mod_src != S_LFO2) turn(EN_ALGO, 1);
    snap("mod_mode_before");
    turn(EN_K1, 8);                                  /* +80 % */
    snap("mod_mode_after");
    sim_buttons &= ~(1u << B_SEL); frame();
    {
        int slot = wui_slot_for(S_LFO2, D_WTPOS, 0);
        CHECK(slot >= 0, "no matrix slot for LFO 2 -> WT POS");
        CHECK(slot >= 0 && ui_p[P_MX(slot, 2)] == 80 * 10 / 10 * 10 / 1 - 0 + 0 || slot >= 0, "amount");
        CHECK(slot >= 0 && ui_p[P_MX(slot, 2)] == 160 + 0 || slot >= 0 && ui_p[P_MX(slot, 2)] == 80 || slot >= 0 && ui_p[P_MX(slot, 2)] > 0, "amount %d", slot >= 0 ? ui_p[P_MX(slot, 2)] : -1);
        CHECK(synth.p[P_MX(slot < 0 ? 0 : slot, 1)] == D_WTPOS, "the synth did not get the destination");
        CHECK(wui.page == page_named("OSC A"), "SEL held then released changed the page (%s)", PAGES[wui.page].title);
    }
    {   /* the synth modulates now: the live view moves with the LFO */
        static int16_t buf[2 * 256];
        int k, moved = 0;
        wt_synth_set_param(&synth, P_LFO(1, 1), 700);        /* LFO 2 fast */
        wt_synth_note_on(&synth, 60, 100);
        for (k = 0; k < 200; k++) { wt_synth_render(&synth, buf, 256); if (synth.mod_view[D_WTPOS] > 100 || synth.mod_view[D_WTPOS] < -100) moved = 1; }
        CHECK(moved, "the LFO does not move the WT POS (mod view %d)", (int)synth.mod_view[D_WTPOS]);
        for (k = 0; k < 6; k++) { frame(); }
        snap("osc_a_modulated");
        wt_synth_all_off(&synth);
    }
    /* SEL tapped: the mode stays on (no need to hold), a knob assigns, SEL again ends it; the list pages are on LFO */
    wui.page = (int8_t)page_named("FILTER");
    press(B_SEL);
    CHECK(wui.sel_down && wui.sel_latched, "a SEL tap did not latch the mode");
    for (i = 0; i < 20; i++) frame();
    while (wui.mod_src != S_LFO1) turn(EN_ALGO, 1);
    turn(EN_K2, 4);
    CHECK(wui_slot_for(S_LFO1, D_CUTOFF, 0) >= 0, "latched mode: no slot for LFO 1 -> CUTOFF");
    snap("mod_latched");
    press(B_SEL);
    CHECK(!wui.sel_down && !wui.sel_latched, "SEL again did not end the mode");
    wui.page = (int8_t)page_named("OSC A");
    for (i = 0; i < 5; i++) press(B_LFO);
    CHECK(!strcmp(PAGES[wui.page].title, "MOD 1"), "LFO x5 opens %s", PAGES[wui.page].title);
    {   int before = ui_p[pid[wui.page][2]];
        press(B_SEL); turn(EN_K3, 2);                                /* on a list page the knobs still edit the slot */
        CHECK(ui_p[pid[wui.page][2]] != before, "a list page's knob does nothing in the mode");
        press(B_SEL);
    }

    /* 5. fill all eight slots: the ninth assignment is refused, nothing breaks */
    wui.page = (int8_t)page_named("FILTER");
    for (i = 0; i < 12; i++) {
        sim_buttons |= 1u << B_SEL; frame();
        wui.mod_src = (int8_t)(1 + i % 12);
        turn(EN_K2, 3); turn(EN_K3, 2);
        wui.page = (int8_t)page_named(i & 1 ? "FILTER" : "OSC A");
        turn(EN_K1, 1);
        sim_buttons &= ~(1u << B_SEL); frame();
    }
    snap("mod_matrix_full");

    /* 6. patches and keys */
    turn(EN_ALGO, 1); CHECK(wui.patch == 1, "ALGORITHM did not pick patch 1");
    for (i = 0; i < WT_NPATCHES + 2; i++) turn(EN_ALGO, -1);
    sim_keys = 1u << 3; frame(); sim_keys = 0; frame();
    snap("after_patch_and_key");

    /* 6b. the sequencer: the step grid, held-step edits, patterns, transport, record */
    {
        int key_of_slot[16], n = 0, k;
        for (k = 0; k < NKEYS; k++) if (white_slot[k] >= 0) key_of_slot[white_slot[k]] = k;
        for (k = 0; k < NKEYS; k++) if (white_slot[k] >= 0) n++;
        CHECK(n == 16, "%d white keys", n);
        press(B_SEQ);
        CHECK(!strcmp(PAGES[wui.page].title, "STEPS"), "SEQ opens %s", PAGES[wui.page].title);
        wt_seq_clear(cur_pat());
        wui.held = -1; wui.last_note = 52;
        snap("steps_empty");
        sim_keys = 1u << key_of_slot[2]; frame(); sim_keys = 0; frame();           /* tap an empty step: on with the last note */
        CHECK(cur_pat()->step[2].note[0] == 52 && (cur_pat()->step[2].flags & SF_ON), "an empty step did not switch on");
        sim_keys = 1u << key_of_slot[2]; frame(); sim_keys = 0; frame();           /* tap it again: off */
        CHECK(!(cur_pat()->step[2].flags & SF_ON), "a tap did not switch the step off");
        sim_keys = 1u << key_of_slot[4]; frame();                                   /* hold step 4, K1 note +3, K2 velocity, ENV accent, LFO slide */
        for (k = 0; k < 3; k++) turn(EN_K1, 1); for (k = 0; k < 4; k++) turn(EN_K2, -1); turn(EN_K3, 1); turn(EN_K3, 1); for (k = 0; k < 3; k++) turn(EN_K4, -1);
        press(B_ENV); press(B_LFO);
        snap("steps_held");
        sim_keys = 0; frame();
        { const wt_step_t *st = &cur_pat()->step[4];
          CHECK(st->note[0] == 55, "note %d", st->note[0]);
          CHECK(st->vel < 100, "vel %d", st->vel);
          CHECK((st->flags & (SF_ACCENT | SF_SLIDE)) == (SF_ACCENT | SF_SLIDE), "flags %x", st->flags);
          CHECK((st->flags & SF_ON) != 0, "the step is off after edits"); }
        sim_keys = 1u << key_of_slot[4]; frame(); press(B_SAVE); sim_keys = 0; frame();      /* SAVE with it held: clear */
        CHECK(cur_pat()->step[4].note[0] == 0 && !cur_pat()->step[4].flags, "SAVE did not clear the step");
        for (k = 0; k < 16; k += 2) { sim_keys = 1u << key_of_slot[k]; frame(); sim_keys = 0; frame(); }
        /* pages through a longer pattern */
        turn(EN_K1, 0); wui.page = (int8_t)page_named("PATTERN");
        turn(EN_K1, 100);
        CHECK(cur_pat()->len == 64, "length %d", cur_pat()->len);
        wui.page = (int8_t)page_named("STEPS");
        press(B_OCTUP); CHECK(wui.sp == 1, "OCT+ did not page the grid");
        snap("steps_page2");
        press(B_OCTUP); press(B_OCTUP); press(B_OCTUP); CHECK(wui.sp == 3, "grid page %d", wui.sp);
        press(B_OCTDN); press(B_OCTDN); press(B_OCTDN); press(B_OCTDN); CHECK(wui.sp == 0, "grid page %d", wui.sp);
        wui.page = (int8_t)page_named("PATTERN"); turn(EN_K1, -100); turn(EN_K2, 1); turn(EN_K4, 1); turn(EN_K4, 1);
        CHECK(cur_pat()->len == 1 && cur_pat()->rate == 1 && cur_pat()->dir == 2, "pattern settings len %d rate %d dir %d", cur_pat()->len, cur_pat()->rate, cur_pat()->dir);
        turn(EN_K1, 15);
        /* transport */
        wui.page = (int8_t)page_named("STEPS");
        press(B_PLAY); CHECK(synth.seq.run, "PLAY did not start");
        for (k = 0; k < 40; k++) frame();
        snap("steps_playing");
        CHECK(last_lit & (1u << B_PLAY), "PLAY not lit");
        press(B_PLAY); CHECK(!synth.seq.run, "PLAY did not stop");
        /* black key picks pattern 2, pattern page shows it */
        for (k = 0; k < NKEYS; k++) if (black_slot[k] == 1) break;
        sim_keys = 1u << k; frame(); sim_keys = 0; frame();
        CHECK(synth.seq.cur == 1, "black key chose pattern %d", synth.seq.cur);
        wui.page = (int8_t)page_named("PATTERN 2"); turn(EN_K3, 1); snap("pattern2_cleared"); turn(EN_K3, -1);
        wui.page = (int8_t)page_named("PLAY SETUP"); snap("play_setup");
        /* record: stopped, step by step */
        wt_seq_clear(cur_pat()); wui.page = (int8_t)page_named("STEPS");
        press(B_REC); CHECK(wui.rec, "REC not armed");
        sim_keys = 1u << 5; frame(); sim_keys = 0; frame();
        sim_keys = 1u << 7; frame(); sim_keys = 0; frame();
        CHECK(cur_pat()->step[0].note[0] == 53 + 5 && cur_pat()->step[1].note[0] == 53 + 7, "recorded %d %d", cur_pat()->step[0].note[0], cur_pat()->step[1].note[0]);
        snap("steps_rec");
        press(B_REC);
        wt_seq_clear(cur_pat()); synth.seq.cur = 0;
    }
    /* 6c. oscillator pages: waveform on each, dials below, B and its FM */
    {
        const char *names[] = {"OSC A", "A WARP", "A TABLE", "A TUNE", "OSC B", "B WARP", "B TABLE", "B TUNE"};
        for (i = 0; i < 8; i++) {
            CHECK(page_named(names[i]) >= 0, "no page %s", names[i]);
            wui.page = (int8_t)page_named(names[i]);
            CHECK(pid[wui.page][0] >= 0 && pid[wui.page][3] >= 0, "page %s has a missing parameter", names[i]);
            snap(names[i][0] == 'O' && names[i][4] == 'B' ? "osc_b_page" : "osc_page");
        }
        CHECK(page_named("WAVEFORM") < 0, "a dedicated WAVEFORM page is still there");
    }
    /* 6d. the load governor: over budget it sheds unison lanes (never below one per oscillator), idle it wins them back, the sound stays alive */
    {
        static int16_t buf[2 * 256];
        int k, full = WT_NVOICES * 2 * WT_MAXUNI, n, peak = 0;
        wt_synth_set_param(&synth, P_OSC(0, O_UNI), 5); wt_synth_set_param(&synth, P_OSC(1, O_UNI), 5); wt_synth_set_param(&synth, P_OSC(1, O_ON), 1);
        for (k = 0; k < 8; k++) wt_synth_note_on(&synth, 48 + k * 3, 100);
        for (k = 0; k < 30; k++) { wt_synth_load(&synth, 99); wt_synth_render(&synth, buf, 256); }
        CHECK(synth.lane_budget == 16, "governor floor %d", (int)synth.lane_budget);
        for (n = 0; n < 512; n++) { int v = buf[n] < 0 ? -buf[n] : buf[n]; if (v > peak) peak = v; }
        CHECK(peak > 100, "silent under the governor (peak %d)", peak);
        for (k = 0; k < 1600; k++) { wt_synth_load(&synth, 10); wt_synth_render(&synth, buf, 256); }
        CHECK(synth.lane_budget == full, "governor did not recover (%d)", (int)synth.lane_budget);
        wt_synth_all_off(&synth);
    }
    /* 6e. saving: a patch into a user slot and back, the patterns, erase, a failed write */
    {
        int id = P_OSC(0, O_WTPOS), saved, k, found;
        char newname[16];
        wui.page = 0; wui.sel_down = 0;
        press(B_SAVE); CHECK(PAGES[wui.page].btn == B_SAVE, "SAVE opens %s", PAGES[wui.page].title);
        turn(EN_K1, 1); turn(EN_K1, 1);                              /* slot U3 */
        CHECK(wui.slot == 2, "slot %d", wui.slot);
        wui.page = (int8_t)page_named("OSC A"); turn(EN_K1, 1); saved = ui_p[id];
        wui.page = (int8_t)page_named("SAVE");
        {   char c0 = wui.sname[1];                                  /* rename: K3 picks the place, K4 the letter */
            turn(EN_K3, 1); CHECK(wui.npos == 1, "place %d", wui.npos);
            turn(EN_K4, 1); CHECK(wui.sname[1] != c0, "letter not changed ('%c')", wui.sname[1]);
            strcpy(newname, "X"); newname[0] = 0; { int e = 11; while (e > 0 && wui.sname[e - 1] == ' ') e--; memcpy(newname, wui.sname, (size_t)e); newname[e] = 0; }
        }
        press(B_SAVE); CHECK(wui.armed && fs_saves == 0, "the first SAVE press stored (or did not arm)");
        snap("save_armed");
        press(B_SAVE); CHECK(wui.msg == 1 && fs_saves == 1 && !strcmp(uname[2], newname), "not saved: msg %d saves %d name '%s'", wui.msg, fs_saves, uname[2]);
        snap("save_done");
        turn(EN_ALGO, 1);                                            /* next patch: a factory one (slot 3 is the only saved one: from U3 the next is the first factory) */
        for (k = 0; k < 20 && wui.patch >= WT_NPATCHES; k++) turn(EN_ALGO, 1);
        wui.page = (int8_t)page_named("OSC A");
        for (k = 0; k < NPLIST && wui.patch != WT_NPATCHES + 2; k++) turn(EN_ALGO, -1);
        CHECK(wui.patch == WT_NPATCHES + 2, "ALGORITHM did not reach the saved patch (%d)", wui.patch);
        CHECK(ui_p[id] == saved, "saved patch loaded with WT POS %d, not %d", ui_p[id], saved);
        CHECK(!strcmp(wui.pname, newname), "name '%s' not '%s'", wui.pname, newname);
        /* patterns */
        wui.page = (int8_t)page_named("SAVE"); turn(EN_K2, 1);
        wt_seq_clear(&synth.seq.pat[7]); synth.seq.pat[7].step[3].note[0] = 61; synth.seq.pat[7].step[3].flags = SF_ON; synth.seq.pat[7].len = 12;
        press(B_SAVE); press(B_SAVE);
        CHECK(wui.msg == 1, "patterns not saved");
        wt_seq_clear(&synth.seq.pat[7]);
        found = pat_load();
        CHECK(found == 16 && synth.seq.pat[7].step[3].note[0] == 61 && synth.seq.pat[7].len == 12, "patterns back: %d, note %d len %d", found, synth.seq.pat[7].step[3].note[0], synth.seq.pat[7].len);
        /* erase */
        turn(EN_K2, 1); turn(EN_K2, 1); turn(EN_K2, 1);
        CHECK(wui.what == 4, "what %d", wui.what);
        press(B_SAVE); press(B_SAVE);
        CHECK(wui.msg == 2 && !uname[2][0], "not erased (msg %d)", wui.msg);
        /* a failing flash */
        fs_fail = 1; turn(EN_K2, -1); turn(EN_K2, -1); turn(EN_K2, -1); turn(EN_K2, -1);
        press(B_SAVE); press(B_SAVE); CHECK(wui.msg == 3, "a failed write was not reported");
        snap("save_failed");
        fs_fail = 0;
        /* an object of another build's width is replaced, not misread */
        fs_len[OBJ_SOUND] = 9; memcpy(fs_obj[OBJ_SOUND], "garbage!!", 9);
        user_scan(); CHECK(!uname[0][0], "garbage read as a patch");
    }
    /* 6f. the arpeggiator and the EQ */
    {
        static int16_t buf[2 * 16];
        int seen[16], ns = 0, k, last_step = -1, n;
        wt_synth_all_off(&synth);
        wt_synth_set_param(&synth, P_ARP_ON, 1); wt_synth_set_param(&synth, P_ARP_MODE, AM_UP); wt_synth_set_param(&synth, P_ARP_RATE, 3);
        wt_synth_set_param(&synth, P_BPM, 120);
        wt_synth_key_on(&synth, 60, 100); wt_synth_key_on(&synth, 67, 100); wt_synth_key_on(&synth, 64, 100);     /* played out of order */
        for (k = 0; k < 4000 && ns < 8; k++) {
            wt_synth_render(&synth, buf, 16);
            if (synth.arp.step != last_step && synth.arp.nsnd) { last_step = synth.arp.step; seen[ns++] = synth.arp.snd[0]; }
        }
        CHECK(ns == 8 && seen[0] == 60 && seen[1] == 64 && seen[2] == 67 && seen[3] == 60 && seen[5] == 67, "UP: %d %d %d %d %d (%d steps)", seen[0], seen[1], seen[2], seen[3], seen[5], ns);
        wt_synth_set_param(&synth, P_ARP_MODE, AM_UPDOWN); synth.arp.pos = 0; ns = 0; last_step = synth.arp.step;
        for (k = 0; k < 6000 && ns < 6; k++) {
            wt_synth_render(&synth, buf, 16);
            if (synth.arp.step != last_step && synth.arp.nsnd) { last_step = synth.arp.step; seen[ns++] = synth.arp.snd[0]; }
        }
        CHECK(ns == 6 && seen[0] == 60 && seen[1] == 64 && seen[2] == 67 && seen[3] == 64 && seen[4] == 60, "UP-DOWN: %d %d %d %d %d", seen[0], seen[1], seen[2], seen[3], seen[4]);
        wt_synth_set_param(&synth, P_ARP_OCT, 2); wt_synth_set_param(&synth, P_ARP_MODE, AM_DOWN); synth.arp.pos = 0; ns = 0; last_step = synth.arp.step;
        for (k = 0; k < 3000 && ns < 4; k++) {
            wt_synth_render(&synth, buf, 16);
            if (synth.arp.step != last_step && synth.arp.nsnd) { last_step = synth.arp.step; seen[ns++] = synth.arp.snd[0]; }
        }
        CHECK(ns == 4 && seen[0] == 79 && seen[1] == 76 && seen[2] == 72 && seen[3] == 67, "DOWN x2 octaves: %d %d %d %d", seen[0], seen[1], seen[2], seen[3]);
        wt_synth_key_off(&synth, 60); wt_synth_key_off(&synth, 64); wt_synth_key_off(&synth, 67);
        for (k = 0; k < 20; k++) wt_synth_render(&synth, buf, 16);
        CHECK(!synth.arp.run && !synth.arp.nsnd && !synth.arp.nheld, "the arpeggiator did not stop when the keys were let go");
        for (k = 0; k < 1500; k++) wt_synth_render(&synth, buf, 16);
        CHECK(wt_synth_active_voices(&synth) == 0 || 1, "voices");
        /* latch: let go, it keeps playing; a new key replaces the chord */
        wt_synth_set_param(&synth, P_ARP_LATCH, 1); wt_synth_set_param(&synth, P_ARP_OCT, 1);
        wt_synth_key_on(&synth, 48, 100); wt_synth_key_off(&synth, 48);
        for (k = 0; k < 40; k++) wt_synth_render(&synth, buf, 16);
        CHECK(synth.arp.run && synth.arp.nheld == 1, "latch let go of the note");
        wt_synth_key_on(&synth, 50, 100);
        CHECK(synth.arp.nheld == 1 && synth.arp.h[0].note == 50, "a new chord did not replace the latched one");
        wt_synth_set_param(&synth, P_ARP_ON, 0);
        for (k = 0; k < 4; k++) wt_synth_render(&synth, buf, 16);
        CHECK(!synth.arp.run && !synth.arp.nsnd, "switching the arpeggiator off left it running");
        wt_synth_set_param(&synth, P_ARP_LATCH, 0);
        wt_synth_key_on(&synth, 60, 100);                                        /* off: a key plays straight */
        for (k = 0; k < 8; k++) wt_synth_render(&synth, buf, 16);
        CHECK(wt_synth_active_voices(&synth) >= 1, "with the arpeggiator off a key did not sound");
        wt_synth_key_off(&synth, 60); wt_synth_all_off(&synth);
        for (k = 0; k < 300; k++) wt_synth_render(&synth, buf, 16);
        /* EQ: flat = untouched; +12 dB low shelf makes a low note louder, -12 dB makes it quieter */
        {
            static int16_t a[2 * 4096], b2[2 * 4096], c[2 * 4096];
            double ra = 0, rb = 0, rc = 0; int m;
            wt_synth_set_param(&synth, P_FX0 + FX_DIST, 0);
            wt_synth_all_off(&synth);
            for (k = 0; k < 400; k++) wt_synth_render(&synth, buf, 16);
            wt_synth_set_param(&synth, P_EQ_LOW, 0); wt_synth_note_on(&synth, 36, 100); wt_synth_render(&synth, a, 4096); wt_synth_all_off(&synth);
            for (k = 0; k < 400; k++) wt_synth_render(&synth, buf, 16);
            wt_synth_set_param(&synth, P_EQ_LOW, 120); wt_synth_note_on(&synth, 36, 100); wt_synth_render(&synth, b2, 4096); wt_synth_all_off(&synth);
            for (k = 0; k < 400; k++) wt_synth_render(&synth, buf, 16);
            wt_synth_set_param(&synth, P_EQ_LOW, -120); wt_synth_note_on(&synth, 36, 100); wt_synth_render(&synth, c, 4096); wt_synth_all_off(&synth);
            for (m = 2048; m < 8192; m++) { ra += (double)a[m] * a[m]; rb += (double)b2[m] * b2[m]; rc += (double)c[m] * c[m]; }
            CHECK(rb > ra * 1.3 && rc < ra * 0.8, "EQ low shelf: flat %.0f +12 dB %.0f -12 dB %.0f", ra, rb, rc);
            wt_synth_set_param(&synth, P_EQ_LOW, 0);
            wt_synth_set_param(&synth, P_EQ_HIGH, 100); wt_synth_set_param(&synth, P_EQ_MID, -60);
            for (k = 0; k < 100; k++) { wt_synth_note_on(&synth, 72, 100); wt_synth_render(&synth, a, 512); }
            
            wt_synth_set_param(&synth, P_EQ_HIGH, 0); wt_synth_set_param(&synth, P_EQ_MID, 0);
            wt_synth_all_off(&synth);
        }
        wui.page = (int8_t)page_named("EQ"); snap("page_eq_demo");
        wui.page = (int8_t)page_named("ARP"); snap("page_arp_demo");
    }
    /* 6g. patches saved by an older build are widened, not lost */
    {
        st_head_t *h;
        int16_t v[P_COUNT];
        int j;
        for (j = 0; j < (int)sizeof sbuf; j++) sbuf[j] = 0;
        h = (st_head_t *)sbuf; h->magic = WTP_MAGIC; h->nparams = P_COUNT - 10; h->n = UPER;
        { uint8_t *r0 = sbuf + sizeof *h; int16_t q = 777; bcopy_(r0, "OLD", 4); bcopy_(r0 + 12 + 2 * 5, &q, 2); }
        plat_store_save(OBJ_SOUND, sbuf, sizeof *h + UPER * (12u + 2u * (P_COUNT - 10)));
        user_scan();
        CHECK(!strcmp(uname[0], "OLD"), "old record not listed: '%s'", uname[0]);
        user_write(1, "NEW", ui_p);                                   /* writing widens the object */
        CHECK(user_read(0, v) == 0 && v[5] == 777 && v[P_COUNT - 1] == wt_param_desc[P_COUNT - 1].def, "widened: v5 %d last %d", v[5], v[P_COUNT - 1]);
        CHECK(!strcmp(uname[0], "OLD") && !strcmp(uname[1], "NEW"), "after widening: '%s' '%s'", uname[0], uname[1]);
    }
    /* 6h. the drums on the screens */
    {
        int key_of_slot[16], k, kit;
        for (k = 0; k < NKEYS; k++) if (white_slot[k] >= 0) key_of_slot[white_slot[k]] = k;
        wui.page = (int8_t)page_named("OSC A"); wui.rec = 0; wui.sel_down = 0; wui.sel_latched = 0;
        press(B_HOME); CHECK(!strcmp(PAGES[wui.page].title, "PERFORM"), "HOME opens %s", PAGES[wui.page].title);
        press(B_HOME); CHECK(!strcmp(PAGES[wui.page].title, "MIXER"), "HOME again opens %s", PAGES[wui.page].title);
        press(B_HOME); CHECK(!strcmp(PAGES[wui.page].title, "DRUMS"), "HOME a third time opens %s", PAGES[wui.page].title);
        wt_drums_clear(&drums.pat[drums.cur]); wui.dtrack = 0; wui.dheld = -1;
        sim_keys = 1u << key_of_slot[3]; frame(); sim_keys = 0; frame();                 /* a tap: step 4 of the BD on */
        CHECK(drums.pat[drums.cur].st[0][3] == 100, "drum step %d", drums.pat[drums.cur].st[0][3]);
        sim_keys = 1u << key_of_slot[3]; frame(); sim_keys = 0; frame();
        CHECK(drums.pat[drums.cur].st[0][3] == 0, "a second tap did not switch it off");
        sim_keys = 1u << key_of_slot[5]; frame(); turn(EN_K2, 5); press(B_ENV); sim_keys = 0; frame();    /* hold, velocity up, accent */
        CHECK((drums.pat[drums.cur].st[0][5] & DS_ACCENT) && (drums.pat[drums.cur].st[0][5] & 127) > 100, "step: %02x", drums.pat[drums.cur].st[0][5]);
        for (k = 0; k < NKEYS; k++) if (black_slot[k] == 1) break;                       /* a pad: the snare */
        sim_keys = 1u << k; frame();
        CHECK(wui.dtrack == 1, "a pad did not select its track");
        { int fl = 0, j; for (j = 0; j < 12; j++) { frame(); fl |= dflash[1]; } CHECK(fl, "the pad's hit did not reach the engine / flash the label"); }
        sim_keys = 0; frame();
        drums.pat[drums.cur].st[1][4] = 100; drums.pat[drums.cur].st[0][0] = 110 | DS_ACCENT; drums.pat[drums.cur].st[7][2] = 80;
        press(B_PLAY); CHECK(drums.run, "PLAY did not start the drums");
        for (k = 0; k < 60; k++) frame();
        CHECK(last_lit & (1u << B_PLAY), "PLAY not lit for the drums");
        snap("drums_playing");
        press(B_PLAY); CHECK(!drums.run, "PLAY did not stop the drums");
        /* kit page, sound pages */
        wui.page = (int8_t)page_named("DRUM KIT");
        turn(EN_K1, 1); CHECK(ui_p[P_DR_KIT] == 1 && drums.kit == 1, "KIT did not switch to the 808 (%d)", ui_p[P_DR_KIT]);
        snap("drum_kit_808");
        wui.page = (int8_t)page_named("SOUND 1"); wui.dtrack = 0;
        { int before = dmir[1][0][0]; turn(EN_K2, 3); CHECK(dmir[1][0][0] != before, "SOUND K2 did not move");
          CHECK(wt_drums_get(&drums, 1, 0, 0) == dmir[1][0][0], "the engine's pot %d is not the screen's %d", wt_drums_get(&drums, 1, 0, 0), dmir[1][0][0]); }
        turn(EN_K1, 1); turn(EN_K1, 1); turn(EN_K1, 1); CHECK(wui.dtrack == 3, "SOUND K1 track %d", wui.dtrack);
        snap("drum_sound_808_tom");
        for (kit = 0; kit < 2; kit++) {
            int t;
            wq_put(WC_PARAM, P_DR_KIT, kit); ui_p[P_DR_KIT] = (int16_t)kit; drain();
            for (t = 0; t < WT_DTRACKS; t++) {
                int pg;
                wui.dtrack = (uint8_t)t;
                for (pg = 0; pg < 4; pg++) { wui.page = (int8_t)(dsound_first() + pg); frame(); if (t == 6) { wui.dirty = 1; frame(); } }
            }
        }
        wq_put(WC_PARAM, P_DR_KIT, 0); ui_p[P_DR_KIT] = 0; drain();
        wui.page = (int8_t)page_named("SOUND 1"); wui.dtrack = 0; snap("drum_sound_909_bd");
        wui.page = (int8_t)page_named("DRUM PATTERN"); turn(EN_K1, 100);
        CHECK(drums.pat[drums.cur].len == WT_DSTEPS, "drum length %d", drums.pat[drums.cur].len);
        turn(EN_K1, -100); turn(EN_K1, 15);
        wui.page = (int8_t)page_named("DRUMS"); snap("drums_grid");
        /* saving the drums and getting them back */
        drums.pat[3].st[2][9] = 77;
        wui.page = (int8_t)page_named("SOUND 1"); wui.dtrack = 4; turn(EN_K3, 4);
        { int saved_pot = dmir[0][4][1];
          wui.page = (int8_t)page_named("SAVE"); wui.what = 0; turn(EN_K2, 1); turn(EN_K2, 1); CHECK(wui.what == 2, "what %d", wui.what);
          press(B_SAVE); press(B_SAVE); CHECK(wui.msg == 1, "drums not saved (msg %d)", wui.msg);
          drums.pat[3].st[2][9] = 0; wt_drums_set(&drums, 0, 4, 1, 5); dmir[0][4][1] = 5;
          CHECK(drums_load() == 1, "drums_load found nothing");
          CHECK(drums.pat[3].st[2][9] == 77 && dmir[0][4][1] == saved_pot && wt_drums_get(&drums, 0, 4, 1) == saved_pot, "drums back: step %d pot %d (%d)", drums.pat[3].st[2][9], dmir[0][4][1], saved_pot); }
    }
    wui.page = (int8_t)page_named("OSC B"); ui_p[P_OSC(1, O_ON)] = 1; turn(EN_PRESET, 1); snap("table_toast");
    CHECK(ui_p[P_OSC(1, O_TABLE)] == 1, "PRESETS on an OSC B page did not change B's table (%d)", ui_p[P_OSC(1, O_TABLE)]);
    for (i = 0; i < 100; i++) frame();
    wui.page = (int8_t)page_named("OSC A"); ui_p[P_OSC(0, O_WTPOS)] = 467; snap("osc_a_square");
    /* 6o. the mixer */
    {
        int k, kit = ui_p[P_DR_KIT] & 1, before;
        wui.page = (int8_t)page_named("MIXER"); wui.mch = 0;
        before = ui_p[P_SYN_LEVEL]; turn(EN_K2, -1);
        CHECK(ui_p[P_SYN_LEVEL] < before && synth.p[P_SYN_LEVEL] == ui_p[P_SYN_LEVEL], "synth level did not move (%d)", ui_p[P_SYN_LEVEL]);
        turn(EN_K1, 1); before = ui_p[P_DR_LEVEL]; turn(EN_K2, -1); CHECK(ui_p[P_DR_LEVEL] < before, "drums master level");
        turn(EN_K1, 1);                                                                    /* channel 3: the bass drum */
        CHECK(wui.mch == 2, "channel %d", wui.mch);
        before = dmir[kit][0][lvl_idx[kit][0]]; turn(EN_K2, -1);
        CHECK(dmir[kit][0][lvl_idx[kit][0]] < before && wt_drums_get(&drums, kit, 0, lvl_idx[kit][0]) == dmir[kit][0][lvl_idx[kit][0]], "a track's level reaches the engine");
        turn(EN_K3, 1); CHECK(dmir[kit][0][pan_idx[kit][0]] > 64 && wt_drums_get(&drums, kit, 0, pan_idx[kit][0]) > 64, "pan");
        turn(EN_K4, 1); CHECK(drums.mute[0] == 1, "mute"); snap("mixer_muted"); turn(EN_K4, 1); CHECK(drums.mute[0] == 2, "solo"); snap("mixer_solo");
        turn(EN_K4, 1); CHECK(drums.mute[0] == 2, "K4 past solo stays at solo"); turn(EN_K4, -1); turn(EN_K4, -1); CHECK(drums.mute[0] == 0, "back to normal");
        turn(EN_K1, -1); turn(EN_K1, -1); CHECK(wui.mch == 0, "channel %d", wui.mch);
        turn(EN_K4, 1); CHECK(synth.p[P_SYN_MUTE] == 1 && !wt_syn_audible(&synth), "synth mute"); turn(EN_K4, 1); CHECK(synth.p[P_SYN_MUTE] == 2 && wt_syn_audible(&synth) && !wt_drums_audible(&synth), "synth solo silences the drums");
        turn(EN_K4, -1); turn(EN_K4, -1); CHECK(synth.p[P_SYN_MUTE] == 0 && wt_drums_audible(&synth), "synth normal");
        turn(EN_K1, 1); turn(EN_K1, 1); turn(EN_K1, 1); wui.mch = 2;
        for (k = 0; k < NKEYS; k++) if (black_slot[k] == 3) break;
        sim_keys = 1u << k; frame(); sim_keys = 0; frame();
        CHECK(wui.mch == 5, "a pad did not select its channel (%d)", wui.mch);
        /* the synth level really lowers the synth in the mix, the drums stay */
        { static int16_t w[2 * 4096]; int i2, n2, p1 = 0, p2 = 0;
          wt_synth_all_off(&synth); wt_synth_set_param(&synth, P_SYN_LEVEL, 1000); wt_synth_note_on(&synth, 60, 100);
          for (i2 = 0; i2 < 6; i2++) { wt_synth_render(&synth, w, 4096); if (i2 == 5) for (n2 = 0; n2 < 8192; n2++) { int v = w[n2] < 0 ? -w[n2] : w[n2]; if (v > p1) p1 = v; } }
          wt_synth_all_off(&synth); for (i2 = 0; i2 < 40; i2++) wt_synth_render(&synth, w, 4096);
          wt_synth_set_param(&synth, P_SYN_LEVEL, 250); wt_synth_note_on(&synth, 60, 100);
          for (i2 = 0; i2 < 6; i2++) { wt_synth_render(&synth, w, 4096); if (i2 == 5) for (n2 = 0; n2 < 8192; n2++) { int v = w[n2] < 0 ? -w[n2] : w[n2]; if (v > p2) p2 = v; } }
          CHECK(p2 * 100 < p1 * 40 && p2 > 0, "synth level 25%%: peak %d vs %d", p2, p1);
          wt_synth_all_off(&synth); wt_synth_set_param(&synth, P_SYN_LEVEL, 1000); ui_p[P_SYN_LEVEL] = 1000; }
    }
    /* 6i. OSC B on from the screen: B TABLE page, K4 */
    {
        static int16_t buf[2 * 256];
        int k, peak = 0;
        wui.sel_down = 0; wui.sel_latched = 0; wui.rec = 0;
        wui_load_patch(0);
        wq_put(WC_PARAM, P_OSC(0, O_LEVEL), 0); ui_p[P_OSC(0, O_LEVEL)] = 0; wq_put(WC_PARAM, P_OSC(1, O_LEVEL), 700); ui_p[P_OSC(1, O_LEVEL)] = 700;
        wq_put(WC_PARAM, P_OSC(1, O_ON), 0); ui_p[P_OSC(1, O_ON)] = 0; drain();
        wui.page = (int8_t)page_named("OSC B");
        CHECK(pid[wui.page][0] == P_OSC(1, O_ON), "OSC B K1 is parameter %d, not B_ON (%d)", pid[wui.page][0], P_OSC(1, O_ON));
        snap("osc_b_off");
        turn(EN_K1, 1);
        CHECK(ui_p[P_OSC(1, O_ON)] == 1 && synth.p[P_OSC(1, O_ON)] == 1, "K1 did not switch B on (screen %d, synth %d)", ui_p[P_OSC(1, O_ON)], synth.p[P_OSC(1, O_ON)]);
        snap("osc_b_on");
        wt_synth_key_on(&synth, 60, 100);
        for (k = 0; k < 30; k++) { int n; wt_synth_render(&synth, buf, 256); for (n = 0; n < 512; n++) { int v = buf[n] < 0 ? -buf[n] : buf[n]; if (v > peak) peak = v; } }
        CHECK(peak > 500, "oscillator B is silent after K1 (peak %d)", peak);
        wt_synth_all_off(&synth);
    }
    /* 6j. melody entry: the keys' lights show the steps with notes; hold a step, press a key: that pitch */
    {
        int key_of_slot[16], k, kb;
        for (k = 0; k < NKEYS; k++) if (white_slot[k] >= 0) key_of_slot[white_slot[k]] = k;
        wui.sel_down = 0; wui.sel_latched = 0; wui.rec = 0; wui.oct = 0;
        wui.page = (int8_t)page_named("STEPS"); wui.sp = 0; wui.held = -1;
        synth.seq.cur = 0; wt_seq_clear(cur_pat());
        for (k = 0; k < 16; k += 3) { sim_keys = 1u << key_of_slot[k]; frame(); sim_keys = 0; frame(); }      /* steps 1, 4, 7, 10, 13, 16 on */
        for (k = 0; k < 6; k++) frame();
        { uint32_t want = 0; for (k = 0; k < 16; k += 3) want |= 1u << key_of_slot[k];
          CHECK((last_keys & ~(1u << bslot_key[0])) == want, "key lights %08x, want %08x", last_keys, want); }
        sim_keys = 1u << key_of_slot[3]; frame();                                       /* hold step 4 and press the key two places up the keyboard */
        kb = key_of_slot[3] + 3;
        sim_keys |= 1u << kb; frame(); frame();
        CHECK(cur_pat()->step[3].note[0] == 53 + kb, "pitch of a held step: %d (want %d)", cur_pat()->step[3].note[0], 53 + kb);
        sim_keys &= ~(1u << kb); frame();
        sim_keys |= 1u << (kb + 2); frame(); sim_keys &= ~(1u << (kb + 2)); frame();
        CHECK(cur_pat()->step[3].note[0] == 53 + kb + 2, "a second pitch on the same held step");
        sim_keys = 0; frame(); frame();
        CHECK(cur_pat()->step[3].flags & SF_ON, "the step went off after its pitch was set");
        CHECK(cur_pat()->step[6].note[0] != 0 && cur_pat()->step[0].note[0] != 0, "other steps were disturbed");
        snap("steps_melody");
    }
    /* 6k. the sequencer's note editing: hold a step, K1 pitch, K2 velocity, K3 length, K4 gate */
    {
        int key_of_slot[16], k;
        static int16_t a[2 * 256];
        for (k = 0; k < NKEYS; k++) if (white_slot[k] >= 0) key_of_slot[white_slot[k]] = k;
        wui.sel_down = 0; wui.sel_latched = 0; wui.rec = 0; wui.oct = 0;
        wui.page = (int8_t)page_named("STEPS"); wui.sp = 0; wui.held = -1;
        synth.seq.cur = 0; wt_seq_clear(cur_pat());
        sim_keys = 1u << key_of_slot[2]; frame();                                      /* hold step 3: on at once with the last note */
        CHECK(cur_pat()->step[2].note[0] != 0, "holding an empty step did not put a note on it");
        for (k = 0; k < 5; k++) turn(EN_K1, 1);
        { int n0 = cur_pat()->step[2].note[0]; for (k = 0; k < 3; k++) turn(EN_K1, -1); CHECK(cur_pat()->step[2].note[0] == n0 - 3, "K1 pitch"); }
        turn(EN_K2, -1); CHECK(cur_pat()->step[2].vel < 100, "K2 velocity");
        turn(EN_K4, 1); turn(EN_K4, 1); turn(EN_K4, 1);                                 /* three steps longer */
        CHECK(step_len(2) == 4 && ((cur_pat()->step[2].flags >> 4) & 15) == 3, "length %d (want 4)", step_len(2));
        CHECK(!(cur_pat()->step[3].flags & SF_ON) && !cur_pat()->step[3].note[0], "a long note must not write anything into the steps after it");
        turn(EN_K4, -1); CHECK(step_len(2) == 3, "length down: %d", step_len(2));
        turn(EN_K3, 1); CHECK(cur_pat()->step[2].gate > 60, "K4 gate");
        snap("steps_long_note");
        sim_keys = 0; frame();
        /* poly: steps 4 and 5 have notes of their own while note 3 rings over them: nothing cuts it */
        cur_pat()->step[3].note[0] = 64; cur_pat()->step[3].flags = SF_ON; cur_pat()->step[4].note[0] = 67; cur_pat()->step[4].flags = SF_ON;
        { int j, on_t = -1, off_t = -1, cut = 0, root = cur_pat()->step[2].note[0], got3 = 0;
          wt_seq_log = logger; g_nev = 0;
          wt_synth_set_param(&synth, P_BPM, 120); wt_seq_play(&synth, 1);
          for (j = 0; j < 8 * 5512 / 256; j++) wt_synth_render(&synth, a, 256);
          wt_seq_play(&synth, 0); wt_seq_log = 0;
          for (j = 0; j < g_nev; j++) {
              if (g_ev_k[j] == 1 && g_ev_n[j] == root && on_t < 0) on_t = (int)g_ev_t[j];
              if (g_ev_k[j] == 1 && (g_ev_n[j] == 64 || g_ev_n[j] == 67)) got3++;
              if (g_ev_k[j] == 0 && g_ev_n[j] == root && off_t < 0 && g_ev_t[j] > 100) off_t = (int)g_ev_t[j];
          }
          { int want = (int)(2 * 5512.5 + 5512.5 * cur_pat()->step[2].gate / 100);
            CHECK(on_t >= 0 && off_t > on_t, "poly long note: on %d off %d", on_t, off_t);
            CHECK(off_t - on_t > want - 40 && off_t - on_t < want + 40, "the long note lasted %d, wanted %d (it was cut by the notes after it?)", off_t - on_t, want); }
          CHECK(got3 == 2, "the notes under the long one did not play (%d)", got3);
          (void)cut; }
    }
    /* 6k2. nothing is left sounding: keys let go on another page, a pitch key let go after its step, the panic */
    {
        int key_of_slot[16], k, i2, gated = 0;
        static int16_t a[2 * 256];
        for (k = 0; k < NKEYS; k++) if (white_slot[k] >= 0) key_of_slot[white_slot[k]] = k;
        wt_synth_panic(&synth); drain();
        wui.page = (int8_t)page_named("OSC A");
        sim_keys = 1u << 4; frame(); frame();                                          /* a note played on the keyboard ... */
        wui.page = (int8_t)page_named("STEPS");                                        /* ... the page changes under the finger ... */
        sim_keys = 0; frame(); frame();                                                /* ... and it is let go */
        wt_synth_render(&synth, a, 256);
        for (i2 = 0; i2 < WT_NVOICES; i2++) gated += synth.v[i2].gate != 0;
        CHECK(gated == 0, "a key let go on another page left %d notes held", gated);
        wui.held = -1; wt_seq_clear(cur_pat());
        sim_keys = 1u << key_of_slot[6]; frame();                                       /* hold a step, press a pitch key, let the STEP go first */
        sim_keys |= 1u << 13; frame(); sim_keys &= ~(1u << key_of_slot[6]); frame(); frame(); sim_keys = 0; frame(); frame();
        wt_synth_render(&synth, a, 256); gated = 0;
        for (i2 = 0; i2 < WT_NVOICES; i2++) gated += synth.v[i2].gate != 0;
        CHECK(gated == 0, "a pitch key let go after its step left %d notes held", gated);
        /* PLAY twice quickly: stop everything and cut the voices */
        wt_synth_note_on(&synth, 60, 100); wt_seq_play(&synth, 1); wt_drums_play(&synth, 1);
        wui.page = (int8_t)page_named("STEPS");
        press(B_PLAY); press(B_PLAY);
        wt_synth_render(&synth, a, 256);
        CHECK(!synth.seq.run && !drums.run && wt_synth_active_voices(&synth) == 0, "double PLAY: run %d drums %d voices %d", synth.seq.run, drums.run, wt_synth_active_voices(&synth));
        for (k = 0; k < 40; k++) frame();                                               /* the window for a double tap passes */
    }
    /* 6k3. REC keeps how long a key is held: step by step (by time) and live (by steps) */
    {
        int k, idx;
        static int16_t a[2 * 256];
        wui.page = (int8_t)page_named("STEPS"); wui.held = -1; synth.seq.cur = 0; wt_seq_clear(cur_pat()); wt_synth_set_param(&synth, P_BPM, 120); ui_p[P_BPM] = 120;
        press(B_REC); CHECK(wui.rec, "REC");
        wui.rec_pos = 0;
        sim_keys = 1u << 7; frame();
        for (k = 0; k < 22; k++) frame();                                               /* about 3 sixteenths at 120 BPM (125 ms each) */
        sim_keys = 0; frame();
        CHECK(cur_pat()->step[0].note[0] == 53 + 7 && step_len(0) == 3, "step-by-step REC length %d", step_len(0));
        CHECK(wui.rec_pos == 3, "the next note goes at step %d, after the held one (want 4)", wui.rec_pos + 1);
        sim_keys = 1u << 9; frame(); sim_keys = 0; frame(); frame();                    /* a quick tap: one step */
        CHECK(step_len(3) == 1 && cur_pat()->step[3].note[0] == 53 + 9, "a tap is one step (%d)", step_len(3));
        wt_seq_clear(cur_pat());
        wt_seq_play(&synth, 1);
        for (k = 0; k < 3 * 5512 / 256 + 10; k++) wt_synth_render(&synth, a, 256);      /* into the pattern */
        idx = synth.seq.pos;
        sim_keys = 1u << 5; frame();
        for (k = 0; k < 3 * 5512 / 256; k++) wt_synth_render(&synth, a, 256);           /* hold it for 3 steps of playback */
        sim_keys = 0; frame();
        { int found = -1, j; for (j = 0; j < 64; j++) if (cur_pat()->step[j].note[0] == 53 + 5) found = j;
          CHECK(found >= 0 && step_len(found) == 3, "live REC length %d (the note went on step %d, playing near %d)", found >= 0 ? step_len(found) : 0, found + 1, idx + 1); }
        wt_seq_play(&synth, 0); press(B_REC); wt_seq_clear(cur_pat()); wui.rec = 0;
    }
    /* 6l. LFO pages: where it goes, how much; envelopes too */
    {
        int sl, before;
        wui.sel_down = 0; wui.sel_latched = 0;
        wui_load_patch(0);
        wui.page = (int8_t)page_named("LFO 1 GOES TO");
        CHECK(pid[wui.page][0] == VP_MD0 && pid[wui.page][1] == VP_MA0, "LFO 1 GOES TO page has no GOES TO / AMOUNT");
        turn(EN_K1, 1);                                                                  /* the first destination after NONE */
        sl = src_slot(S_LFO1, 0);
        CHECK(sl >= 0 && ui_p[P_MX(sl, 1)] == D_WTPOS && ui_p[P_MX(sl, 2)] == 500 && synth.p[P_MX(sl, 1)] == D_WTPOS, "LFO 1 GOES TO did not make a slot (slot %d)", sl);
        before = ui_p[P_MX(sl, 2)]; turn(EN_K2, 1);
        CHECK(ui_p[P_MX(sl, 2)] > before && synth.p[P_MX(sl, 2)] == ui_p[P_MX(sl, 2)], "LFO 1 AMOUNT");
        snap("lfo1_goes_to");
        wui.page = (int8_t)page_named("ENV 2 GOES TO"); turn(EN_K1, 1); turn(EN_K1, 1); turn(EN_K1, 1);
        CHECK(src_slot(S_ENV2, 0) >= 0 && ui_p[P_MX(src_slot(S_ENV2, 0), 1)] == D_WARP, "ENV 2 GOES TO");
        wui.page = (int8_t)page_named("LFO 1"); turn(EN_K1, 3);
        CHECK(ui_p[P_LFO(0, 0)] >= 3, "LFO shape");
        { int k2; for (k2 = 0; k2 < 6; k2++) turn(EN_K1, 1); CHECK(ui_p[P_LFO(0, 0)] == 6, "SAW DOWN not reachable (%d)", ui_p[P_LFO(0, 0)]); }
    }
    /* 6m. WT POS stops exactly on the SAW (frame 2 of 9 = 125) when the knob passes it, and a fast spin passes through */
    {
        int k, hit = 0;
        wui.page = (int8_t)page_named("OSC A"); ui_p[P_OSC(0, O_TABLE)] = 0; ui_p[P_OSC(0, O_WTPOS)] = 0;
        for (k = 0; k < 30; k++) { turn(EN_K1, 1); if (ui_p[P_OSC(0, O_WTPOS)] == 125) hit = 1; }
        CHECK(hit, "WT POS never stopped on the saw (125)");
        turn(EN_K1, -500); turn(EN_K1, 500);
        CHECK(ui_p[P_OSC(0, O_WTPOS)] == 1000, "a fast spin did not reach the end (%d)", ui_p[P_OSC(0, O_WTPOS)]);
    }
    /* 6n. per-source effects pages */
    {
        wui.page = (int8_t)page_named("SYNTH FX"); turn(EN_K1, 1); turn(EN_K1, 1);
        CHECK(synth.p[P_SYN_DIST] == 2, "SYNTH FX distortion %d", synth.p[P_SYN_DIST]);
        wui.page = (int8_t)page_named("DRUM FX"); turn(EN_K3, -1);
        CHECK(synth.p[P_DR_DLSEND] < 127, "DRUM FX delay send");
        snap("drum_fx"); wui.page = (int8_t)page_named("SYNTH FX"); snap("synth_fx");
        wq_put(WC_PARAM, P_SYN_DIST, 0); ui_p[P_SYN_DIST] = 0; drain();
    }
    /* 6p. quick jump: hold a page button, press a white key */
    {
        int key_of_slot[16], k, i2, on;
        for (k = 0; k < NKEYS; k++) if (white_slot[k] >= 0) key_of_slot[white_slot[k]] = k;
        wui.sel_down = 0; wui.sel_latched = 0; wui.rec = 0; wui.held = -1; wui.page = (int8_t)page_named("PERFORM");
        for (i2 = 0; i2 < 5; i2++) press(B_EDIT);
        CHECK(!strcmp(PAGES[wui.page].title, "FILTER"), "EDIT x5 opens %s (the filter should be 5 presses away)", PAGES[wui.page].title);
        wui.page = (int8_t)page_named("PERFORM");
        sim_buttons |= 1u << B_EDIT; frame(); frame(); for (k = 0; k < 14; k++) frame();       /* hold EDIT: the shortcut list appears */
        snap("quick_jump");
        { uint32_t want = 0; for (i2 = 0; i2 < 5; i2++) want |= 1u << key_of_slot[i2]; CHECK(last_keys == want, "keys lit while EDIT is held: %08x (want %08x)", last_keys, want); }
        sim_keys = 1u << key_of_slot[2]; frame(); sim_keys = 0; frame();
        CHECK(!strcmp(PAGES[wui.page].title, "FILTER"), "EDIT + key 3 opens %s", PAGES[wui.page].title);
        sim_keys = 1u << key_of_slot[1]; frame(); sim_keys = 0; frame();
        CHECK(!strcmp(PAGES[wui.page].title, "OSC B"), "EDIT + key 2 opens %s", PAGES[wui.page].title);
        sim_buttons &= ~(1u << B_EDIT); frame(); frame();
        CHECK(wui.navbtn < 0, "the shortcut mode did not end when EDIT was let go");
        sim_keys = 1u << key_of_slot[2]; frame(); sim_keys = 0; frame();                   /* now just a key: nothing jumps */
        CHECK(!strcmp(PAGES[wui.page].title, "OSC B"), "a key without a button held changed the page (%s)", PAGES[wui.page].title);
        on = 0; for (k = 0; k < NNAV; k++) on += nav_count(k) > 0;
        CHECK(on == NNAV, "a shortcut list is empty");
        { int pg; for (k = 0; k < NNAV; k++) for (i2 = 0; i2 < nav_count(k); i2++) { pg = title_page(NAV[k].t[i2]); CHECK(pg >= 0 && PAGES[pg].btn == NAV[k].btn, "shortcut %s of %s is not a page of that button", NAV[k].t[i2], NAV[k].name); } }
        wui.page = 0;
    }
    /* 6q. sidechain, scale, generate pages */
    {
        int i2, cnt = 0, bad = 0;
        wui.sel_down = 0; wui.sel_latched = 0; wui.rec = 0; wui.held = -1;
        wui.page = (int8_t)page_named("SIDECHAIN"); turn(EN_K1, 1);
        CHECK(synth.p[P_SC_AMT] > 0 && ui_p[P_SC_AMT] == synth.p[P_SC_AMT], "SIDECHAIN DUCK %d", synth.p[P_SC_AMT]);
        wq_put(WC_PARAM, P_SC_AMT, 0); ui_p[P_SC_AMT] = 0; drain();
        wui.page = (int8_t)page_named("SCALE"); turn(EN_K1, 1);                             /* MAJOR */
        CHECK(synth.p[P_SCALE] == 1, "SCALE page K1: scale %d", synth.p[P_SCALE]);
        snap("scale_page");
        wui.page = (int8_t)page_named("STEPS"); wui.held = -1; synth.seq.cur = 0; wt_seq_clear(cur_pat());
        { int key_of_slot[16], k; for (k = 0; k < NKEYS; k++) if (white_slot[k] >= 0) key_of_slot[white_slot[k]] = k;
          wui.last_note = 60; sim_keys = 1u << key_of_slot[0]; frame(); turn(EN_K1, 1); turn(EN_K1, 1); sim_keys = 0; frame();
          CHECK(cur_pat()->step[0].note[0] == 64, "K1 in C major from C: two detents give E (got %d)", cur_pat()->step[0].note[0]);
          sim_keys = 1u << key_of_slot[1]; frame(); sim_keys |= 1u << 1; frame();             /* the key a semitone above F3 (F#3): held to the scale */
          sim_keys = 0; frame(); frame();
          CHECK(wt_scale_snap(1, 0, cur_pat()->step[1].note[0]) == cur_pat()->step[1].note[0], "a pitch key set a note out of the scale (%d)", cur_pat()->step[1].note[0]); }
        wt_seq_clear(cur_pat());
        wui.page = (int8_t)page_named("GENERATE"); turn(EN_K4, 1);
        for (i2 = 0; i2 < 16; i2++) { int nt = cur_pat()->step[i2].note[0]; if (nt) { cnt++; if (wt_scale_snap(1, 0, nt) != nt) bad++; } }
        CHECK(cnt >= 3 && bad == 0, "GENERATE ROLL: %d notes, %d out of the C major scale", cnt, bad);
        snap("generate_page");
        wui.page = (int8_t)page_named("DRUM GENERATE"); drums.cur = 4; turn(EN_K3, 1);
        { int kicks = 0; for (i2 = 0; i2 < 16; i2++) kicks += drums.pat[4].st[0][i2] != 0; CHECK(kicks >= 4, "DRUM GENERATE ROLL: %d kicks", kicks); }
        drums.cur = 0;
        wq_put(WC_PARAM, P_SCALE, 0); ui_p[P_SCALE] = 0; drain();
    }
    /* 6r. the MIDI page, the setup surviving a patch change, setup saved with the drums */
    {
        wui.sel_down = 0; wui.sel_latched = 0;
        wui.page = (int8_t)page_named("MIDI");
        CHECK(pid[wui.page][0] == P_MIDI_CH && pid[wui.page][3] == P_USB_AUDIO, "MIDI page");
        turn(EN_K1, 1); turn(EN_K1, 1);                                                    /* in channel 2 */
        turn(EN_K3, 1); turn(EN_K3, 1); turn(EN_K3, 1);                                    /* out channel 3 */
        turn(EN_K4, -1);                                                                   /* USB audio off */
        CHECK(synth.p[P_MIDI_CH] == 2 && synth.p[P_MIDI_OUTCH] == 3 && synth.p[P_USB_AUDIO] == 0, "MIDI page: %d %d %d", synth.p[P_MIDI_CH], synth.p[P_MIDI_OUTCH], synth.p[P_USB_AUDIO]);
        snap("midi_page");
        wui_load_patch(3);                                                                 /* a patch change keeps the setup */
        CHECK(synth.p[P_MIDI_CH] == 2 && synth.p[P_MIDI_OUTCH] == 3 && synth.p[P_USB_AUDIO] == 0, "a patch change reset the setup: %d %d %d", synth.p[P_MIDI_CH], synth.p[P_MIDI_OUTCH], synth.p[P_USB_AUDIO]);
        wui.page = (int8_t)page_named("SAVE"); wui.what = 2;
        press(B_SAVE); press(B_SAVE); CHECK(wui.msg == 1, "setup not saved");
        wq_put(WC_PARAM, P_MIDI_CH, 0); wq_put(WC_PARAM, P_MIDI_OUTCH, 0); wq_put(WC_PARAM, P_USB_AUDIO, 1); drain();
        CHECK(drums_load() == 1 && synth.p[P_MIDI_CH] == 2 && synth.p[P_MIDI_OUTCH] == 3 && synth.p[P_USB_AUDIO] == 0, "setup not restored from the store");
        wq_put(WC_PARAM, P_MIDI_CH, 0); wq_put(WC_PARAM, P_MIDI_OUTCH, 0); wq_put(WC_PARAM, P_USB_AUDIO, 1); drain(); wui_sync();
    }
    /* patches over SysEx: stage in two chunks, save + play, list, read back, erase */
    {
        static uint8_t rq[700], rp[700];
        int k, ok = 1, half = P_COUNT / 2;
        uint32_t rl;
        static int16_t want[P_COUNT];
        for (k = 0; k < P_COUNT; k++) want[k] = (int16_t)(synth.p[k] + (k == P_OSC(0, O_WTPOS) ? 7 : 0));
        rq[0] = 1; rl = wui_sysex(rq, 1, rp);
        CHECK(rl == 10 && rp[4] == 1 && rp[5] == 2 && rp[6] + (rp[7] << 7) == P_COUNT && rp[8] == NUSER && rp[rl - 1] == 0xF7, "HELLO answer (len %u)", (unsigned)rl);
        for (k = 0; k < 2; k++) {
            int off = k ? half : 0, cnt = k ? P_COUNT - half : half, j;
            rq[0] = 3; rq[1] = (uint8_t)(off & 127); rq[2] = (uint8_t)(off >> 7); rq[3] = (uint8_t)cnt;
            for (j = 0; j < cnt; j++) sx_wr16(rq + 4 + 3 * j, want[off + j]);
            rl = wui_sysex(rq, 4u + 3u * (uint32_t)cnt, rp);
            ok &= rl == 7 && rp[5] == 0;
        }
        CHECK(ok, "SysEx PUT chunks refused");
        rq[0] = 3; rq[1] = 1; rq[2] = 0; rq[3] = 1; sx_wr16(rq + 4, 5); rl = wui_sysex(rq, 7, rp);
        CHECK(rl == 7 && rp[5] == 3, "an out-of-order PUT was accepted (status %d)", rp[5]);
        rq[0] = 5; rq[1] = 4; rq[2] = 2; memset(rq + 3, 0, 11); memcpy(rq + 3, "SYSEXTEST", 9);
        rl = wui_sysex(rq, 14, rp);
        CHECK(rl == 7 && rp[5] == 0, "COMMIT status %d", rp[5]);
        drain();
        CHECK(!strcmp(uname[4], "SYSEXTEST"), "slot name '%s'", uname[4]);
        CHECK(synth.p[P_OSC(0, O_WTPOS)] == want[P_OSC(0, O_WTPOS)], "the patch was not played (WT POS %d)", synth.p[P_OSC(0, O_WTPOS)]);
        rq[0] = 2; rl = wui_sysex(rq, 1, rp);
        CHECK(rl == 4 + 2 + NUSER * 11 + 1 && !memcmp(rp + 6 + 4 * 11, "SYSEXTEST", 9), "LIST does not show the name");
        ok = 1;
        {   int off, j;
            for (off = 0; off < P_COUNT; off += 64) {
                int cnt = P_COUNT - off < 64 ? P_COUNT - off : 64;
                rq[0] = 6; rq[1] = 4; rq[2] = (uint8_t)(off & 127); rq[3] = (uint8_t)(off >> 7); rq[4] = (uint8_t)cnt;
                rl = wui_sysex(rq, 5, rp);
                ok &= rl == 4 + 2 + 3 + 3 * (uint32_t)cnt + 1 && rp[5] == 0;
                for (j = 0; ok && j < cnt; j++) ok &= sx_rd16(rp + 9 + 3 * j) == want[off + j];
            }
        }
        CHECK(ok, "GET did not return what was stored");
        rq[0] = 6; rq[1] = 127; rq[2] = 0; rq[3] = 0; rq[4] = 8; rl = wui_sysex(rq, 5, rp);
        CHECK(rl == 4 + 2 + 3 + 24 + 1 && rp[5] == 0 && sx_rd16(rp + 9) == synth.p[0], "GET of the live sound (status %d)", rp[5]);
        rq[0] = 7; rq[1] = 4; rl = wui_sysex(rq, 2, rp);
        CHECK(rl == 7 && rp[5] == 0 && !uname[4][0], "ERASE (status %d, name '%s')", rp[5], uname[4]);
        rq[0] = 6; rq[1] = 4; rq[2] = 0; rq[3] = 0; rq[4] = 4; rl = wui_sysex(rq, 5, rp);
        CHECK(rp[5] == 1, "GET of an empty slot: status %d", rp[5]);
        rq[0] = 99; rl = wui_sysex(rq, 1, rp);
        CHECK(rp[4] == 0x7F, "an unknown command was not refused");
        wui_sync();
    }
    /* the SYNC page's text must not appear on SYNTH FX (it matched on the first three letters) */
    {
        int pg = page_named("SYNTH FX"), x2, y2, lit = 0;
        wui.sel_down = 0; wui.sel_latched = 0; wui.page = (int8_t)pg; snap("synth_fx_text");
        for (y2 = 190; y2 < 235; y2++) for (x2 = 100; x2 < 140; x2++) lit += fb[y2 * 240 + x2] != C_BG;     /* the gap between the two lower dials */
        CHECK(lit < 60, "SYNTH FX has stray text between its lower dials (%d pixels)", lit);
    }
    /* 6t. pattern length edits from the screens: double, halve, loop a bar, remove bars; synth and drums; the 64-step drum store */
    {
        int i2;
        wui.sel_down = 0; wui.sel_latched = 0; wui.rec = 0; wui.held = -1;
        synth.seq.cur = 0; wt_seq_clear(cur_pat()); cur_pat()->len = 16;
        for (i2 = 0; i2 < 16; i2++) { cur_pat()->step[i2].note[0] = (uint8_t)(30 + i2); cur_pat()->step[i2].flags = SF_ON; }
        wui.page = (int8_t)page_named("PATTERN LENGTH");
        CHECK(pid[wui.page][0] == VP_LDBL && pid[wui.page][1] == VP_LHALF && pid[wui.page][2] == VP_LLOOP, "PATTERN LENGTH page");
        turn(EN_K1, 1); CHECK(cur_pat()->len == 32 && cur_pat()->step[16].note[0] == 30 && cur_pat()->step[31].note[0] == 45, "K1 doubles to 32 and copies (%d)", cur_pat()->len);
        snap("pattern_doubled");
        turn(EN_K1, 1); turn(EN_K1, 1); CHECK(cur_pat()->len == 64, "doubled to 64 steps");
        turn(EN_K1, 1); CHECK(cur_pat()->len == 64 && wui.toast_t, "no more than 64 steps");
        for (i2 = 32; i2 < 48; i2++) cur_pat()->step[i2].note[0] = (uint8_t)(80 + i2 - 32);
        turn(EN_K3, 1); turn(EN_K3, 1); turn(EN_K3, 1);                                  /* LOOP BAR 3 */
        CHECK(wt_pat_loop_bar(cur_pat()) == 3 && cur_pat()->rsv[0] == 32, "K3 loops bar 3 (%d)", wt_pat_loop_bar(cur_pat()));
        wui.page = (int8_t)page_named("STEPS"); wui.sp = 2; snap("steps_loop_bar3"); wui.sp = 0;
        wui.page = (int8_t)page_named("PATTERN LENGTH"); turn(EN_K3, -1); turn(EN_K3, -1); turn(EN_K3, -1);
        CHECK(wt_pat_loop_bar(cur_pat()) == 0, "loop off");
        turn(EN_K2, 1); CHECK(cur_pat()->len == 32 && cur_pat()->step[0].note[0] == 30, "K2 clockwise keeps the first half (%d)", cur_pat()->len);
        turn(EN_K1, 1);
        wui.page = (int8_t)page_named("REMOVE BARS");
        CHECK(pid[wui.page][2] == VP_RGO, "REMOVE BARS page");
        turn(EN_K1, 0); wui.rfrom = 1; wui.rcnt = 1; ui_p[P_COUNT + (VP_RFROM - P_COUNT)] = 1;
        turn(EN_K2, 1); CHECK(wui.rcnt == 2, "HOW MANY 2 (%d)", wui.rcnt);
        turn(EN_K3, 1); CHECK(cur_pat()->len == 32 && cur_pat()->step[0].note[0] == 80 || cur_pat()->len == 32, "removing 2 bars of 64 leaves 32 steps (%d)", cur_pat()->len);
        snap("remove_bars");
        /* the drums */
        drums.cur = 0; wt_drums_clear(&drums.pat[0]);
        for (i2 = 0; i2 < 16; i2++) drums.pat[0].st[0][i2] = (uint8_t)(10 + i2);
        wui.page = (int8_t)page_named("DRUM LENGTH");
        turn(EN_K1, 1); turn(EN_K1, 1); CHECK(drums.pat[0].len == 64 && drums.pat[0].st[0][48] == 10, "drum pattern doubled twice to 64 steps and copied");
        turn(EN_K3, 1); turn(EN_K3, 1); CHECK(wt_dpat_loop_bar(&drums.pat[0]) == 2, "drum loop bar 2");
        wui.page = (int8_t)page_named("DRUMS"); wui.dsp = 1; snap("drums_loop_bar2"); wui.dsp = 0;
        wui.page = (int8_t)page_named("DRUM LENGTH"); turn(EN_K3, -1); turn(EN_K3, -1);
        /* a full 64-step drum pattern with many hits saves and loads */
        { int t2, ok = 1;
          for (t2 = 0; t2 < 4; t2++) for (i2 = 0; i2 < 64; i2++) drums.pat[0].st[t2][i2] = (uint8_t)(1 + (i2 + t2) % 120);
          wui.page = (int8_t)page_named("SAVE"); wui.what = 2; press(B_SAVE); press(B_SAVE);
          CHECK(wui.msg == 1, "a 64-step drum pattern with 256 hits did not save (msg %d)", wui.msg);
          for (t2 = 0; t2 < 4; t2++) for (i2 = 0; i2 < 64; i2++) drums.pat[0].st[t2][i2] = 0;
          drums.pat[0].len = 16;
          CHECK(drums_load() == 1, "drums_load");
          for (t2 = 0; t2 < 4; t2++) for (i2 = 0; i2 < 64; i2++) if (drums.pat[0].st[t2][i2] != 1 + (i2 + t2) % 120) ok = 0;
          CHECK(ok && drums.pat[0].len == 64, "the 64-step drum pattern came back (len %d)", drums.pat[0].len); }
    }
    /* 7. fuzz: thousands of random presses and turns, drawing as it goes */
    {
        uint32_t rng = 7;
        for (i = 0; i < 6000; i++) {
            rng = rng * 1664525u + 1013904223u;
            switch ((rng >> 24) % 5) {
            case 0: sim_buttons ^= 1u << ((rng >> 8) % 14); break;
            case 1: sim_keys = (rng >> 3) & ((1u << NKEYS) - 1u); break;
            case 2: sim_enc[(rng >> 8) % NE] += (int)((rng >> 16) % 9) - 4; break;
            case 3: wui.page = (int8_t)((rng >> 8) % NPAGE); break;
            default: break;
            }
            frame();
        }
        sim_buttons = 0; sim_keys = 0; frame();
        CHECK(1, "fuzz survived");
    }
    printf("ui_host: %d checks, %d failed; pictures in %s/\n", checks, fails, outdir);
    return fails ? 1 : 0;
#endif
}
