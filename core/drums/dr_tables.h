/* The 909 / 808 engines' pot curves (9W9 er99_pots.h), generated: value = min * (max/min)^(pot/127). */
#pragma once
#define DR_EXP_BD_DECAY 0   /* 100 .. 4000 */
#define DR_EXP_BD_PITCH 1   /* 0.43 .. 4.7 */
#define DR_EXP_DRIVE 2   /* 0.85 .. 12 */
#define DR_EXP_SD_TUNE 3   /* 130 .. 320 */
#define DR_EXP_SD_TONE 4   /* 300 .. 4000 */
#define DR_EXP_LT_TUNE 5   /* 50 .. 90 */
#define DR_EXP_LT_DECAY 6   /* 80 .. 2600 */
#define DR_EXP_MT_TUNE 7   /* 80 .. 125 */
#define DR_EXP_MT_DECAY 8   /* 70 .. 2200 */
#define DR_EXP_HT_TUNE 9   /* 110 .. 170 */
#define DR_EXP_HT_DECAY 10   /* 60 .. 2000 */
#define DR_EXP_RS_TUNE 11   /* 150 .. 300 */
#define DR_EXP_CP_TUNE 12   /* 650 .. 1400 */
#define DR_EXP_CP_TAIL 13   /* 120 .. 1000 */
#define DR_EXP_OH_DECAY 14   /* 20 .. 1200 */
#define DR_EXP_CH_DECAY 15   /* 15 .. 300 */
#define DR_EXP_CY_DECAY 16   /* 100 .. 3000 */
#define DR_EXP_SMP_PITCH 17   /* 0.25 .. 4 */
#define DR_EXP_FX_HPF 18   /* 30 .. 800 */
#define DR_EXP_COUNT 19
extern const float dr_pot_exp[19][128];
