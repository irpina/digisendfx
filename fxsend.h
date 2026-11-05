/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SEND FX: the Digitone mk1's send effects and master drive, taken over or moved (core-dn1 5.0's ev_render_fx
 * and 3.3's ev_render_params). The render side (dsp.c, interrupt level) and the UI side (ui.c, the UI task and
 * the MIDI task) share the controls below: bytes, each written whole by the UI and read by the render. */
#ifndef FXSEND_H
#define FXSEND_H

#include <stdint.h>
#include "digitone-mk1/core3.h"

typedef int32_t s32;
typedef uint32_t u32;
typedef int16_t s16;
typedef uint16_t u16;
typedef uint8_t u8;

/* The controls: four pages of eight knobs (A-D the top row, E-H the bottom). A knob is 0-127; an option is
 * 0 .. its count - 1. */
#define FS_N        32
enum {
    /* CHORUS: the stock chorus or the mod's own (its DPTH, SPD, HPF and WDTH stay the kit's, on the stock FX page) */
    C_ENG, C_VOICES, C_TONE, C_DRIFT, C_VIBE, C_VRATE, C_FDBK, C_SAT,
    /* DELAY and DRIVE */
    D_MODE, D_TIME, D_NONE1, D_NONE2, V_MODE, V_BOOST, V_TONE, V_LEVEL,
    /* LFO 1 and LFO 2: eight each, the same layout */
    L1_RATE, L1_SYNC, L1_WAVE, L1_DEST, L1_DEPTH, L1_PHASE, L1_RESET, L1_NONE,
    L2_RATE, L2_SYNC, L2_WAVE, L2_DEST, L2_DEPTH, L2_PHASE, L2_RESET, L2_NONE,
};
#define L_RATE      0                   /* an LFO's controls from its first */
#define L_SYNC      1
#define L_WAVE      2
#define L_DEST      3
#define L_DEPTH     4
#define L_PHASE     5
#define L_RESET     6

enum { ENG_STOCK, ENG_LUSH };
enum { DT_STOCK, DT_SHORT };
enum { DRV_STOCK, DRV_HARD, DRV_FUZZ, DRV_FOLD, DRV_N };
enum { W_TRI, W_SINE, W_SQR, W_SAW, W_RAMP, W_SH, W_RND, W_N };
#define SYNC_N      13                  /* FREE, then 16 bars .. 1/32 */
#define DEST_N      23                  /* OFF, then the FX record's slots (fs_dest_slot) */
#define DEST_DTIME  8                   /* the delay's TIME: in SHORT mode, the mod's own TIME */

extern volatile u8 fs_val[FS_N];
extern volatile u8 fs_reset;            /* bit i: LFO i restarts at its PHASE (PLAY, from the transport) */
extern volatile s16 fs_lfo_now[2];      /* each LFO's last value, Q15, for the screen */
extern volatile u32 fs_blocks;          /* the render's blocks */
extern volatile u32 fs_taken;           /* bit s: the mod ran stage s in the last block it saw */
extern volatile u16 fs_load, fs_mine;   /* the whole render's share of a block, and the mod's, per mille (DMA
                                           timer 0: the unit only; the emulator has none) */
extern const u8 fs_dest_slot[DEST_N];   /* the FX record's slot each destination moves (0: none) */

#endif
