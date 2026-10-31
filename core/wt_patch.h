/* SPDX-License-Identifier: GPL-3.0-only
 * Built-in test and demo patches, written as parameter settings through a callback (no libc). */
#pragma once
#include <stdint.h>
#define WT_NPATCHES 11
extern const char *const wt_patch_names[WT_NPATCHES];
typedef int (*patch_set_fn)(void *ctx, const char *name, const char *value);
/* returns 0 if the patch exists */
int patch_apply(const char *name, patch_set_fn set, void *ctx);
/* the patch as parameters (P_COUNT values), without touching a synth; returns 0 if the patch exists */
int wt_patch_values(const char *name, int16_t *out);
