/* SPDX-License-Identifier: GPL-3.0-only */
/* WT-1 effects (X0X's send FX and master, shared here by the synth and later the drums). A port of 9W9's
 * (GPL-3.0) send reverb, tempo-synced delay and master stage:
 *
 *   out = volume * glue( dist( dry + reverb(rev) + delay(dly) ) )
 *
 *  - Reverb: four combs + two allpasses, the comb loop quantised to 12 bits
 *    (the early-rack grain), damping in the loop, HPF on the send.
 *  - Delay: a note division of the tempo, slewed so time changes warp instead of
 *    clicking, feedback through a darkening one-pole, 12-bit write, HPF on the
 *    send. TYPE = DIGI is 9W9's delay exactly. TYPE = TAPE (an X0X addition, for
 *    the 303) reads the same line with wow and flutter, saturates inside the
 *    loop, darkens the loop further and lets feedback run slightly past unity
 *    into self-oscillation that the saturation holds; WEAR scales all of it.
 *  - Master: 9W9's seven distortion characters (or Off) and its one-knob glue
 *    compressor (hard bypass at 0), then volume.
 *
 * The delay line is int16 (9W9 writes it at 12 bits, so nothing is lost; values
 * past +-16 full scale saturate) and is passed in by the caller (.pool). The
 * reverb lines are int16 combs (also 12-bit in 9W9) and float allpasses, inside
 * the struct. Both effects go idle (no per-sample work) once their input and
 * their lines are silent. */
#pragma once
#include <stdint.h>
#include "fx_dsp.h"

#define FX_RV_QUIET_LEN (1356 + 8192)   /* = the reverb's FX_RV_QUIET (wt_fx.c) */
#define FX_RV_COMB_TOTAL (1116 + 1188 + 1277 + 1356)
#define FX_RV_AP_TOTAL (556 + 441)
#define FX_RV_APR_TOTAL (579 + 457)   /* X0X: the right side's allpasses */

enum { FX_VOLUME, FX_DIST, FX_DRIVE, FX_COMP,
       FX_RV_DECAY, FX_RV_TONE, FX_RV_HPF, FX_RV_LEVEL,
       FX_DL_TIME, FX_DL_FDBK, FX_DL_TONE, FX_DL_LEVEL,
       FX_DL_HPF, FX_DL_TYPE, FX_DL_WEAR, FX_DL_PING, FX_NPARAMS };

struct fxbus {
    /* reverb */
    float rv_decay, rv_tone, rv_level;
    d9_biquad_t rv_hp;
#if WT_NODRUMS && defined(WT_DEVICE)
    /* WT-2: these lines live in the pool (wt_fx.c), not in wt_synth_t. The 96 KB RAM is the tight region;
     * the drum machine used to occupy the pool space this takes. Pointers so f->comb[i] is unchanged. */
    int16_t *comb;
    float *ap, *apr;
#else
    int16_t comb[FX_RV_COMB_TOTAL];
    float ap[FX_RV_AP_TOTAL];
    float apr[FX_RV_APR_TOTAL];       /* X0X stereo: the right side reads the combs with alternating
                                       * signs through its own allpasses (the left is 9W9's) */
#endif
    int32_t cpos[4], apos[2];
    float cdmp[4];
    int32_t rv_quiet;
    int32_t aposr[2];                 /* the right side's allpass positions */
    /* delay */
    float dl_time_ms, dl_fdbk, dl_tone, dl_level, bpm, wear;
    int32_t dl_div, dl_type;
    d9_biquad_t dl_hp;
    int16_t *dbuf;
    int32_t dlen, w, dl_quiet;
    float dcur, lp, tape_hp;
    uint32_t wow_ph, flut_ph;
    int32_t dl_ping;                  /* X0X: ping-pong, a second tap at twice the time */
    /* master */
    float drive, volume, comp;
    int32_t dist_mode;
    d9_shape_t shape;
    float crush_st[2], crush_str[2];
    float comp_env_db, comp_det, c_thr, c_slope, c_makeup, c_drel, c_atk, c_rel;
    uint8_t pots[FX_NPARAMS];
    int active;                       /* WT-1: set by the synth while the effects run */
};
typedef struct fxbus fxbus_t;

typedef struct {                    /* a pot's descriptor (X0X's x0x_param_t) */
    const char *name;
    uint8_t max;                    /* 127 = continuous; else a switch with max + 1 positions */
    uint8_t def;
    const char *const *names;       /* switch: max + 1 labels, else 0 */
} wt_fxp_t;
extern const char *const wt_fx_dist_names[8], *const wt_fx_div_names[13], *const wt_fx_type_names[2];
void fxbus_init(fxbus_t *f, int16_t *dly_buf, int dly_len);
void fxbus_set_bpm(fxbus_t *f, float bpm);
void fxbus_process(fxbus_t *f, const float *dry, const float *rev, const float *dly, float *out, int n);
/* X0X stereo: dry in L / R, the sends mono; with everything centred and no reverb, out_l and out_r
 * are fxbus_process's out exactly */
void fxbus_process_st(fxbus_t *f, const float *dry_l, const float *dry_r, const float *rev, const float *dly,
                      float *out_l, float *out_r, int n);
int fxbus_nparams(void);
const wt_fxp_t *fxbus_param(int i);
void fxbus_set(fxbus_t *f, int i, int value);
int fxbus_get(const fxbus_t *f, int i);
