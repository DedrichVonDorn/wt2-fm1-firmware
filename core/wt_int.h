/* SPDX-License-Identifier: GPL-3.0-only
 * WT-1 core internals: shared lookup tables (built once by wt_global_init). */
#pragma once
#include "wt_core.h"

#ifdef WT_DEVICE
#define WT_TAB const                   /* the device's tables are generated const data (tools/gen_wt_data) */
#else
#define WT_TAB
#endif
extern WT_TAB int16_t wt_sin1024[1024];       /* Q15 sine, one cycle */
extern WT_TAB uint32_t wt_inc_tbl[193];       /* phase increment of MIDI note 0 raised by i/192 of an octave... see wt_pitch_inc */
extern WT_TAB uint32_t wt_g_tbl[1351];        /* SVF g = tan(pi fc / fs), Q24, index = cutoff in 0.1 semitone */
extern WT_TAB uint32_t wt_atk_step[1001];     /* env level step per sample (level is Q30) */
extern WT_TAB uint32_t wt_coef[1001];         /* exponential coefficient per sample, Q24 */
extern WT_TAB uint32_t wt_samples[1001];      /* delay / hold length in samples */
extern WT_TAB uint32_t wt_lfo_inc[1001];      /* LFO phase increment per sample */
extern WT_TAB int16_t wt_pan_l[129], wt_pan_r[129];   /* Q15 equal-power pan, index 0 (left) .. 128 (right) */

uint32_t wt_isqrt(uint64_t x);
uint32_t wt_pitch_inc(int32_t semi256);        /* phase increment (cycles/sample * 2^32) for a pitch in 1/256 semitone */

void wt_analyse(const int16_t *x, int32_t *a, int32_t *b, int kmax);
void wt_synth_cycle(int16_t *out, const int32_t *a, const int32_t *b, int H, int M, int taper_pct);
