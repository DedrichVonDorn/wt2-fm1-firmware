/* SPDX-License-Identifier: GPL-3.0-only */
/* WT-1 app: the synth, the command queue between the main loop and the audio interrupt, a first screen.
 * Part of the unity build (wt1.c), after the HAL, usb.c, panel.c and plat_fm1.c.
 *
 * One context touches the synth: the audio ISR. The main loop (keys, knobs, screen) and USB MIDI hand it commands
 * through a small ring; the screen only reads the parameter mirror and the (read only) table data. */
#include "wt_core.h"
#include "wt_data.h"
#include "wt_patch.h"
#if !WT_NODRUMS
#include "wt_drums.h"
#endif

extern void (*wt_idle)(void);          /* core/wt_build.c: called between frames while a table builds */

static wt_synth_t synth;
#if !WT_NODRUMS
static wt_drums_t drums __attribute__((section(".pool")));      /* both kits and the drum patterns */
#endif
static wt_table_t wtab[WT_NTABLES];
/* 31 internal frames x 1152 samples of mip levels each: 71 KB per table, in the pool */
#define WT_TAB_SAMPLES (WT_FAC_NINT * 1152)
static int16_t wtab_mem[WT_NTABLES][WT_TAB_SAMPLES] __attribute__((section(".pool")));
static wt_sample_t wsmp[3];

/* parameters a patch change leaves alone: the setup and the song-level settings */
static const uint8_t WT_KEEP[] = {P_SYN_MUTE, P_DR_MUTE, P_MIDI_CH, P_MIDI_DRCH, P_MIDI_OUTCH, P_USB_AUDIO, P_CORE2, P_SCALE, P_ROOT, P_SCALE_SEQ, P_CLK_SRC, P_CLK_SS};
#define WT_NKEEP ((uint32_t)sizeof WT_KEEP)

/* ---- commands: main loop and USB MIDI -> audio ISR ---- */
enum { WC_ON, WC_OFF, WC_PARAM, WC_BEND, WC_MOD, WC_ALLOFF, WC_PATCH, WC_PLAY, WC_QUEUE, WC_DHIT, WC_DQUEUE, WC_DSET, WC_DMUTE, WC_PANIC };
#define WCQ 64u
static volatile uint8_t wq_t[WCQ], wq_a[WCQ];
static volatile int16_t wq_b[WCQ];
static volatile uint32_t wq_w, wq_r;
static int16_t wpatch[P_COUNT];                 /* a patch being loaded: written by the main loop, taken by the ISR */
static volatile uint8_t wpatch_pending;
static volatile uint32_t wq_dropped;

static void wq_put(uint8_t t, uint8_t a, int16_t b)
{
    uint32_t w = wq_w;
    if (w - wq_r >= WCQ) { wq_dropped++; return; }
    wq_t[w % WCQ] = t; wq_a[w % WCQ] = a; wq_b[w % WCQ] = b;
    RING_PUBLISH();
    wq_w = w + 1u;
}

static void wt_apply(uint8_t t, uint8_t a, int16_t b)
{
    switch (t) {
    case WC_ON: wt_synth_key_on(&synth, a, b); break;
    case WC_OFF: wt_synth_key_off(&synth, a); break;
    case WC_PARAM: wt_synth_set_param(&synth, a, b); break;
    case WC_BEND: wt_synth_bend(&synth, b); break;
    case WC_MOD: wt_synth_modwheel(&synth, b); break;
    case WC_ALLOFF: wt_synth_all_off(&synth); break;
    case WC_PLAY: wt_seq_play(&synth, a);
#if !WT_NODRUMS
        wt_drums_play(&synth, a);
#endif
        break;
    case WC_PANIC: wt_synth_panic(&synth); break;
#if !WT_NODRUMS
    case WC_DHIT: wt_drums_hit(&synth, a, b); break;
    case WC_DQUEUE: wt_drums_queue(&synth, a); break;
    case WC_DSET: wt_drums_set(&drums, (a >> 4) & 1, a & 15, (b >> 8) & 15, b & 127); break;      /* kit << 4 | track; i << 8 | value */
    case WC_DMUTE: if ((a & 255) < WT_DTRACKS) drums.mute[a] = (uint8_t)(b > 2 ? 2 : b); break;
#endif
    case WC_QUEUE: wt_seq_queue(&synth, a); break;
    default: {      /* WC_PATCH */
        uint32_t i;
        int16_t keep[WT_NKEEP];
        for (i = 0; i < WT_NKEEP; i++) keep[i] = synth.p[WT_KEEP[i]];
        wt_synth_all_off(&synth);
        for (i = 0; i < P_COUNT; i++) wt_synth_set_param(&synth, (int)i, wpatch[i]);
        for (i = 0; i < WT_NKEEP; i++) wt_synth_set_param(&synth, WT_KEEP[i], keep[i]);       /* the setup (MIDI, USB audio), the mixer's mute / solo and the scale outlive a patch change */
        wpatch_pending = 0;
        break; }
    }
}

static void wt_drain(void)                      /* audio ISR, before each render */
{
    uint32_t n = 0, pkt;
    while (wq_r != wq_w && n++ < WCQ) {
        uint32_t r = wq_r;
        wt_apply(wq_t[r % WCQ], wq_a[r % WCQ], wq_b[r % WCQ]);
        wq_r = r + 1u;
    }
    while (plat_midi_in(&pkt)) wt_midi_in(&synth, pkt);          /* USB MIDI: notes, bend, mod wheel on the channels set; the drums' channel plays the drum tracks */
}

/* ---- audio: n = 256 frames, the codec's 24-bit samples in int32 (X0X's default level: 6 dB under full scale) ---- */
static int16_t rbuf[2u * 256u];
static void wt_render(int32_t *o, uint32_t n)
{
    int32_t g = (int32_t)plat_master();         /* the MASTER knob, 0..4096 */
    uint32_t i;
    wt_drain();
    wt_synth_render(&synth, rbuf, (int)n);
    for (i = 0; i < 2u * n; i++)
        o[i] = ((int32_t)rbuf[i] * g) >> 5;
}

/* the second core renders the drums while this one renders the synth (the CORE 2 switch on the LEVELS page); one core when it is off or did not start */
static int wt_par_run_fn(void (*fn)(uint32_t), uint32_t arg) { return synth.p[P_CORE2] && plat_cpu2_run(fn, arg); }

/* ---- boot: build the factory tables from the generated frames, set the synth up ---- */
static void wt_idle_hook(void) { fm1_wdt_feed(); }

static int wt_boot(void)
{
    static const char *const NAME[3] = {"BASIC SHP", "FOLD-SINE", "ODD-EVEN"};
    const int16_t *frames[3] = {wt_fac_frames_0, wt_fac_frames_1, wt_fac_frames_2};
    const int16_t *loops[3] = {wt_loop_0, wt_loop_1, wt_loop_2};
    static const char *const LN[3] = {"CRACKLE", "AIR", "RUMBLE"};
    int i, rc = 0;
    wt_idle = wt_idle_hook;
    for (i = 0; i < WT_NTABLES; i++)
        rc |= wt_table_build_levels(&wtab[i], NAME[i], frames[i], wt_fac_nint[i], wt_fac_nsrc[i], wt_fac_expand[i], 1, wtab_mem[i], WT_TAB_SAMPLES);
    wt_synth_init(&synth);
#if !WT_NODRUMS
    wt_drums_init(&drums);
    wt_synth_set_drums(&synth, &drums);
#endif
    for (i = 0; i < WT_NTABLES; i++) wt_synth_set_table(&synth, i, &wtab[i]);
    for (i = 0; i < 3; i++) {
        str_cpy(wsmp[i].name, LN[i], sizeof wsmp[i].name);
        wsmp[i].d = loops[i]; wsmp[i].n = WT_LOOP_N; wsmp[i].owned = 0;
        wt_synth_set_sample(&synth, i, &wsmp[i]);
    }
    return rc;
}

/* ---- the screens and controls (wt_ui.c: also built on the Mac by tests/ui_host.c) ---- */
#include "wt_ui.c"
