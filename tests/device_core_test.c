/* SPDX-License-Identifier: GPL-3.0-only
 * The device's code path on the Mac: WT_DEVICE core (const generated tables, integer builder, no libc tables) must render
 * the pad scene exactly like the normal build. Usage: device_core_test reference.wav [patch] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wt_core.h"
#include "wt_data.h"
#include "wt_patch.h"

static wt_synth_t synth;
static wt_table_t tab[3];
static int16_t mem[3][WT_FAC_NINT * 1152];
static wt_sample_t smp[3];

int main(int argc, char **argv)
{
    const int16_t *fr[3] = {wt_fac_frames_0, wt_fac_frames_1, wt_fac_frames_2}, *lp[3] = {wt_loop_0, wt_loop_1, wt_loop_2};
    static int16_t patch[P_COUNT], out[2 * 128], ref[2 * 44096], got[2 * 44096];
    int i, f, bad = 0, maxd = 0;
    FILE *w;
    if (argc < 2) return 2;
    for (i = 0; i < 3; i++)
        if (wt_table_build_levels(&tab[i], "T", fr[i], wt_fac_nint[i], wt_fac_nsrc[i], wt_fac_expand[i], 1, mem[i], WT_FAC_NINT * 1152)) { puts("table build failed"); return 1; }
    wt_synth_init(&synth);
    for (i = 0; i < 3; i++) {
        wt_synth_set_table(&synth, i, &tab[i]);
        smp[i].d = lp[i]; smp[i].n = WT_LOOP_N;
        wt_synth_set_sample(&synth, i, &smp[i]);
    }
    if (wt_patch_values(argc > 2 ? argv[2] : "pad", patch)) { puts("no pad patch"); return 1; }
    for (i = 0; i < P_COUNT; i++) wt_synth_set_param(&synth, i, patch[i]);
    if (argc > 3 && !strcmp(argv[3], "seq")) wt_seq_play(&synth, 1);        /* the sequencer plays pattern 1 */
    else { wt_synth_note_on(&synth, 52, 90); wt_synth_note_on(&synth, 59, 90); wt_synth_note_on(&synth, 64, 90); }
    for (f = 0; f < 44096; f += 128) { wt_synth_render(&synth, out, 128); memcpy(got + 2 * f, out, sizeof out); }
    w = fopen(argv[1], "rb");
    if (!w) { perror(argv[1]); return 1; }
    fseek(w, 44, SEEK_SET);
    if (fread(ref, 2, 2 * 44096, w) != 2 * 44096) { puts("short reference"); return 1; }
    fclose(w);
    for (i = 0; i < 2 * 44096; i++) { int d = abs(got[i] - ref[i]); if (d) bad++; if (d > maxd) maxd = d; }
    printf("device core vs normal build: %d of %d samples differ, max %d LSB\n", bad, 2 * 44096, maxd);
    return bad ? 1 : 0;
}
