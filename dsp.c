/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SEND FX's render (interrupt level, 1500 blocks a second, 32 frames each): what core-dn1 gives it, measured in
 * digikit's emulator (README.md, "What the stages get"):
 *
 *   ev_render_params(words)   each block, before the controllers: the FX record's words at words[632 + slot],
 *                             8.8, slots 17-23 chorus, 24-31 delay, 32-38 reverb, 50 the master drive. What a
 *                             handler writes is this block's value (the smoother writes them again next block),
 *                             and the stages see it: the LFOs move them here, and SHORT writes the delay's TIME.
 *   ev_render_fx(stage, out, in, params)
 *                             stage 0-2: chorus, delay, reverb; in the stage's send bus, out its return, 32
 *                             frames L,R in Q1.31; params the render's copy of the stage's words (chorus 7,
 *                             delay 8, reverb 7). Stage 3, the master drive: in the whole master, out what goes
 *                             on to PVOL and the codec, params the drive's value itself (MOVD, 8.8). A nonzero
 *                             return means the handler wrote out and the stock stage does not run.
 *
 * The LUSH chorus is the mod's own: a mono bucket-brigade-style line, read by two to four voices whose delays a
 * slow triangle sweeps (the stock SPD and DPTH), a faster sine adds shimmer (VIBE) and a slow random walk
 * detunes them (DRIFT); they are spread across the stereo field (the stock WDTH), darkened by the line's
 * filters (TONE), and may saturate (SAT) and feed back (FDBK). The drive's HARD, FUZZ and FOLD replace the
 * stock master drive when it (MOVD) or BOOST is up. Samples are Q15 in an int32 inside (the top 16 bits of
 * the stage's Q1.31); every multiply is 16 x 16 or fits 32 bits. */
#include "fxsend.h"
#include "tables.h"

volatile u8 fs_val[FS_N];
volatile u8 fs_reset;
volatile s16 fs_lfo_now[2];
volatile u32 fs_blocks;
volatile u32 fs_taken;
volatile u16 fs_load, fs_mine;          /* the whole render's share of a block, and the mod's own, per mille */

/* ---- the CPU meter: DMA timer 0 (the bus clock), as the pedals measure; the emulator has none (reads 0) ---- */
#define DTCN0       (*(volatile u32 *)0xFC07000Cu)
static u32 r_in, r_prev, r_mine;

void fs_rin(void)
{
    r_prev = r_in;
    r_in = DTCN0;
    r_mine = 0;
}

void fs_rout(void)
{
    u32 per = r_in - r_prev, busy = DTCN0 - r_in;
    if (per < 1000 || per > 400000)
        return;
    fs_load = (u16)(busy * 1000u / per);
    fs_mine = (u16)(r_mine * 1000u / per);
}

/* OFF, then CHORUS DPTH SPD HPF WDTH >DEL >REV VOL, DELAY TIME WID FDBK HPF LPF >REV VOL, REVERB PRE DEC FREQ
 * GAIN HPF LPF VOL, the master DRIVE (MOVD) */
const u8 fs_dest_slot[DEST_N] = {
    0, 17, 18, 19, 20, 21, 22, 23, 24, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 50,
};

#define FX_REC      632                 /* 79 x 8: the FX record in the words */
#define P_MAX       32512               /* 127.0 */

static inline s32 sat16(s32 v) { return v > 32767 ? 32767 : v < -32768 ? -32768 : v; }
static inline s32 clampi(s32 v, s32 lo, s32 hi) { return v < lo ? lo : v > hi ? hi : v; }

static u32 seed = 0x6c078965u;
static inline u32 rnd(void) { seed = seed * 1664525u + 1013904223u; return seed; }

static inline s32 sinq(u32 ph)          /* Q15, a whole turn in 2^32 */
{
    s32 i = (s32)(ph >> 24), f = (s32)((ph >> 16) & 255);
    return fs_sine[i] + (((fs_sine[i + 1] - fs_sine[i]) * f) >> 8);
}

static inline s32 triq(u32 ph)          /* a triangle, Q15: -32767 at 0, +32767 half way */
{
    s32 t = (s32)(ph >> 16);            /* 0 .. 65535 */
    return t < 32768 ? 2 * t - 32767 : 98303 - 2 * t;
}

/* ---- the LFOs and the parameter words (ev_render_params) -------------------------------------------- */
static u32 lf_ph[2];
static s16 lf_a[2], lf_b[2];            /* S&H's value; RND's from and to */

/* 1/32 notes a cycle for SYNC 1-12: 16 bars .. 1/32 */
static const u16 sync_n[SYNC_N] = { 0, 512, 256, 128, 64, 32, 16, 12, 8, 6, 4, 2, 1 };

static u32 lfo_step(int i)              /* this block's phase step */
{
    const volatile u8 *v = &fs_val[L1_RATE + 8 * i];
    u32 n = v[L_SYNC] < SYNC_N ? sync_n[v[L_SYNC]] : 0;
    if (!n)
        return fs_lfo_rate[v[L_RATE] & 127];
    /* a 1/32 note is 1,350,000 / fw_tempo blocks (fw_tempo: BPM x 120): 2^32 fw_tempo / (1,350,000 n) */
    {
        u32 t = (u32)fw_tempo;
        return (t * 3181u + t * 5556u / 10000u) / n;
    }
}

static s32 lfo_run(int i)               /* advance LFO i by a block: its value, Q15 */
{
    const volatile u8 *v = &fs_val[L1_RATE + 8 * i];
    u32 old, ph;
    s32 y;
    if (fs_reset & (1u << i)) {
        fs_reset &= (u8)~(1u << i);
        lf_ph[i] = 0;
        lf_a[i] = lf_b[i];
    }
    old = lf_ph[i];
    lf_ph[i] = old + lfo_step(i);
    if (lf_ph[i] < old) {               /* a new cycle */
        lf_a[i] = lf_b[i];
        lf_b[i] = (s16)((s32)(rnd() >> 16) - 32768);
    }
    ph = lf_ph[i] + ((u32)(v[L_PHASE] & 127) << 25);
    switch (v[L_WAVE]) {
    default:
    case W_TRI:  y = triq(ph); break;
    case W_SINE: y = sinq(ph); break;
    case W_SQR:  y = ph < 0x80000000u ? 32767 : -32767; break;
    case W_SAW:  y = 32767 - (s32)(ph >> 16); break;
    case W_RAMP: y = (s32)(ph >> 16) - 32768; break;
    case W_SH:   y = lf_b[i]; break;
    case W_RND:  y = lf_a[i] + (((s32)(lf_b[i] - lf_a[i]) * (s32)(lf_ph[i] >> 18)) >> 14); break;
    }
    return clampi(y, -32767, 32767);
}

/* The short delay's TIME word for a knob position (8.8, 0 .. 127.0): the stock delay waits (TIME + 1) / 128 of
 * a whole note, and TIME may go below 0 (in the words), down to 32 frames (one block); below that it is silent.
 * a 1/128 note is 90,000 / BPM frames, so frames = (TIME + 256) / 256 x 10,800,000 / fw_tempo (fw_tempo: BPM x
 * 120), and TIME + 256 = frames x fw_tempo / 42,187.5: in quarter frames, q2 x fw_tempo / 168,750. Measured: at
 * 86 BPM, TIME -248 is 32 frames, -240 64, -128 522, 0 1046. */
static s32 short_word(s32 kq)
{
    s32 k, f, q2, t;
    kq = clampi(kq, 0, P_MAX);
    k = kq >> 8;
    f = kq & 255;
    q2 = fs_short_q2[k] + (k < 127 ? (((s32)fs_short_q2[k + 1] - fs_short_q2[k]) * f) >> 8 : 0);
    t = (s32)(((u32)q2 * (u32)fw_tempo + 168749u) / 168750u);      /* rounded up: never under one block */
    return t - 256;
}

void fs_params(int16_t *words)
{
    s16 *fx = words + FX_REC;
    s32 mod_time = 0;
    u32 t0 = DTCN0;
    int i;
    fs_blocks++;
    for (i = 0; i < 2; i++) {
        const volatile u8 *v = &fs_val[L1_RATE + 8 * i];
        s32 y = lfo_run(i), off;
        int d = v[L_DEST], slot;
        fs_lfo_now[i] = (s16)y;
        if (d <= 0 || d >= DEST_N || v[L_DEPTH] == 64)
            continue;
        off = (y * ((s32)v[L_DEPTH] - 64) * 127) >> 13;    /* +-127.0 at full depth */
        if (d == DEST_DTIME && fs_val[D_MODE] == DT_SHORT) {
            mod_time += off;
            continue;
        }
        slot = fs_dest_slot[d];
        fx[slot] = (s16)clampi((s32)fx[slot] + off, 0, P_MAX);
    }
    if (fs_val[D_MODE] == DT_SHORT)
        fx[24] = (s16)short_word(((s32)(fs_val[D_TIME] & 127) << 8) + mod_time);
    r_mine += DTCN0 - t0;
}

/* ---- the LUSH chorus (stage 0) ----------------------------------------------------------------------- */
#define LN          2048                /* the line: 42 ms */
#define LMASK       (LN - 1)
static s16 ch_line[LN];
static u32 ch_w;                        /* the next frame written */
static u32 ch_ph, ch_vph;               /* the sweep's and the vibrato's phase */
static s32 ch_hp, ch_lpi, ch_lpl, ch_lpr, ch_fb;
static s32 ch_d[4];                     /* each voice's delay at the end of the last block, Q8 frames */
static s32 ch_drift[4], ch_dtgt[4];     /* the random walk, Q8 frames */
static u8 ch_live;                      /* the line holds the chorus's own audio (0: clear it first) */

/* Per voice count (2, 3, 4): the centre delays (Q8 frames), the sweep's and the vibrato's phase offsets (the
 * voices' gains to L and R are in the mix, below) */
static const s32 ch_centre[3][4] = {
    { 336 * 256, 336 * 256, 0, 0 },                         /* 7.0 ms twice, opposite sweeps: the classic pair */
    { 288 * 256, 360 * 256, 432 * 256, 0 },                 /* 6.0, 7.5, 9.0 ms: an ensemble */
    { 264 * 256, 336 * 256, 408 * 256, 480 * 256 },         /* 5.5 .. 10 ms: wide */
};
static const u32 ch_off[3][4] = {
    { 0, 0x80000000u, 0, 0 },
    { 0, 0x55555555u, 0xaaaaaaaau, 0 },
    { 0, 0x40000000u, 0x80000000u, 0xc0000000u },
};
static const u32 ch_voff[3][4] = {
    { 0x40000000u, 0xc0000000u, 0, 0 },
    { 0x2aaaaaaau, 0x80000000u, 0xd5555555u, 0 },
    { 0x20000000u, 0xa0000000u, 0x60000000u, 0xe0000000u },
};
static void chorus_clear(void)
{
    int i;
    u32 *p = (u32 *)ch_line;
    for (i = 0; i < LN / 2; i++)
        p[i] = 0;
    ch_hp = ch_lpi = ch_lpl = ch_lpr = ch_fb = 0;
    for (i = 0; i < 4; i++)
        ch_d[i] = -1;
    ch_live = 1;
}

static inline s32 tap(u32 w, s32 d)     /* the line d (Q8) frames behind frame w, read between two frames */
{
    u32 pos = (w << 8) - (u32)d;
    s32 i0 = (s32)(pos >> 8), f = (s32)(pos & 255);
    s32 a = ch_line[i0 & LMASK], b = ch_line[(i0 + 1) & LMASK];
    return a + (((b - a) * f) >> 8);
}

static void chorus(s32 *out, const s32 *in, const s16 *p)
{
    s32 dpth = clampi(p[0], 0, P_MAX) >> 8, spd = clampi(p[1], 0, P_MAX) >> 8;
    s32 hpk = clampi(p[2], 0, P_MAX) >> 8, wdth = clampi(p[3], 0, P_MAX) >> 8;
    int nv = fs_val[C_VOICES] < 3 ? fs_val[C_VOICES] : 1;
    int voices = nv + 2, v, s, j;
    u32 rate = fs_ch_rate[spd], vrate = fs_vib_rate[fs_val[C_VRATE] & 127];
    s32 depth = dpth * 290;                         /* up to 3.0 ms (144 frames), Q8 */
    s32 vib = (s32)(fs_val[C_VIBE] & 127) * 48;     /* up to 0.5 ms */
    s32 drift = (s32)(fs_val[C_DRIFT] & 127) * 97;  /* up to 1.0 ms */
    s32 a_hp = fs_hpf[hpk], a_lp = fs_lpf[fs_val[C_TONE] & 127];
    s32 fb = ((s32)(fs_val[C_FDBK] & 127) - 64) * 358;       /* -0.7 .. +0.7, Q15 */
    s32 sat = fs_val[C_SAT] & 127, sg = 256 + sat * 6;       /* into the line: x1 .. x4 (Q8) */
    s32 sc = (256 * 32767) / sg;                              /* and back out: the level stays */
    s32 w_side = wdth * 258;                                  /* Q15 */
    s32 d0[4], dd[4], y[4][8];

    if (!ch_live)
        chorus_clear();
    /* the random walk: a new target for each voice every 1024 blocks (0.7 s), glided to */
    if ((fs_blocks & 1023) == 0)
        for (v = 0; v < 4; v++)
            ch_dtgt[v] = (s32)((rnd() >> 16) & 0xffff) - 32768;
    for (v = 0; v < voices; v++)
        ch_drift[v] += (((ch_dtgt[v] * drift) >> 15) - ch_drift[v]) >> 10;
    /* four segments of eight frames, each voice's delay a straight line across a segment */
    for (s = 0; s < 4; s++) {
        u32 ph_end = ch_ph + rate * 8, vph_end = ch_vph + vrate * 8, w0 = ch_w;
        const s32 *x2 = in + 16 * s;
        s32 *o = out + 16 * s;
        for (v = 0; v < voices; v++) {
            s32 d = ch_centre[nv][v] + ((depth * triq(ph_end + ch_off[nv][v])) >> 15)
                    + ((vib * sinq(vph_end + ch_voff[nv][v])) >> 15) + ch_drift[v];
            d0[v] = ch_d[v] < 0 ? d : ch_d[v];
            dd[v] = (d - d0[v]) >> 3;
            ch_d[v] = d;
        }
        ch_ph = ph_end;
        ch_vph = vph_end;
        /* the line's input, frame by frame; voice 0 feeds back, so it is read first (every voice reads at least
         * 1.5 ms back: all of a segment's reads are of frames written before it) */
        for (j = 0; j < 8; j++) {
            s32 x = ((x2[2 * j] >> 16) + (x2[2 * j + 1] >> 16)) >> 1;
            ch_fb = y[0][j] = tap(ch_w, d0[0] + dd[0] * (j + 1));
            if (a_hp) {                                 /* the stock HPF: the input less its low end */
                ch_hp += ((x - ch_hp) * a_hp) >> 15;
                x -= ch_hp;
            }
            x += (ch_fb * fb) >> 15;
            if (sat) {                                  /* a cubic soft clip, x1.5 of full scale to full */
                s32 x2c, x3;
                x = clampi((x * sg) >> 8, -49152, 49152);
                x2c = (x * x) >> 15;
                x3 = (x2c * x) >> 15;
                x = ((x - ((x3 * 4855) >> 15)) * sc) >> 15;
            }
            ch_lpi += ((x - ch_lpi) * a_lp) >> 15;      /* the line's input filter */
            ch_line[ch_w & LMASK] = (s16)sat16(ch_lpi);
            ch_w++;
        }
        for (v = 1; v < voices; v++) {                  /* the other voices */
            s32 d = d0[v];
            for (j = 0; j < 8; j++) {
                d += dd[v];
                y[v][j] = tap(w0 + (u32)j, d);
            }
        }
        for (j = 0; j < 8; j++) {
            s32 yl, yr, m, sd;
            if (nv == 0) {                              /* the pair: one a side */
                yl = y[0][j];
                yr = y[1][j];
            } else if (nv == 1) {                       /* three: left, both, right */
                yl = (y[0][j] * 26800 + y[1][j] * 18950) >> 15;
                yr = (y[2][j] * 26800 + y[1][j] * 18950) >> 15;
            } else {                                    /* four: left, right, and two between */
                yl = (y[0][j] * 24500 + y[2][j] * 19400 + y[3][j] * 9700) >> 15;
                yr = (y[1][j] * 24500 + y[3][j] * 19400 + y[2][j] * 9700) >> 15;
            }
            m = (yl + yr) >> 1;                         /* WDTH: 0 mono, 127 as wide as the voices */
            sd = (((yl - yr) >> 1) * w_side) >> 15;
            ch_lpl += ((m + sd - ch_lpl) * a_lp) >> 15; /* the line's output filter */
            ch_lpr += ((m - sd - ch_lpr) * a_lp) >> 15;
            o[2 * j] = sat16(2 * ch_lpl) << 16;         /* x2: the stock chorus's level (+6 dB) */
            o[2 * j + 1] = sat16(2 * ch_lpr) << 16;
        }
    }
}

/* ---- the master drive (stage 3) ---------------------------------------------------------------------- */
static s32 dv_lp[2], dv_dc[2];

static int drive(s32 *out, const s32 *in, s32 movd)
{
    int mode = fs_val[V_MODE], c;
    s32 a = clampi(movd, 0, P_MAX) >> 8, boost = fs_val[V_BOOST] & 127;
    s32 g, db, a_lp = fs_lpf[fs_val[V_TONE] & 127], lev = (s32)(fs_val[V_LEVEL] & 127) * 4;   /* Q8: 64 unity */
    if (mode <= DRV_STOCK || mode >= DRV_N || (a == 0 && boost == 0))
        return 0;                               /* the stock drive (clean at 0) */
    g = ((s32)fs_drv_gain[a] * fs_boost[boost]) >> 8;     /* Q8, up to +60 dB */
    db = (a * 36 + boost * 24 + 63) / 127;     /* the whole gain, dB */
    if (mode == DRV_FOLD && db > 18)
        db = 18;                                /* FOLD stays as loud past its first folds */
    lev = (lev * fs_drv_comp[db]) >> 15;       /* the make-up (gentables.py) */
    for (c = 0; c < 64; c++) {
        int ch = c & 1;
        s32 y = ((in[c] >> 16) * (g >> 4)) >> 4;
        if (mode == DRV_HARD) {
            y = clampi(y, -24576, 24576);
        } else if (mode == DRV_FUZZ) {          /* uneven: the top hard, the bottom softer and lower */
            if (y > 16384)
                y = 16384;
            else if (y < -8192) {
                y = -8192 - ((-8192 - y) >> 2);
                if (y < -26000)
                    y = -26000;
            }
        } else {                                /* FOLD: back and forth between -16384 and 16384 */
            u32 m = (u32)(clampi(y, -8000000, 8000000) + 16384 + 65536 * 128) & 65535u;
            y = (s32)(m < 32768u ? m : 65536u - m) - 16384;
        }
        dv_dc[ch] += (y - dv_dc[ch]) >> 9;      /* no DC (FUZZ's unevenness makes some) */
        y -= dv_dc[ch];
        dv_lp[ch] += ((y - dv_lp[ch]) * a_lp) >> 15;
        out[c] = sat16((dv_lp[ch] * lev) >> 8) << 16;
    }
    return 1;
}

/* ---- ev_render_fx -------------------------------------------------------------------------------------- */
int32_t fs_fx(int32_t stage, int32_t *out, int32_t *in, void *params)
{
    u32 t0 = DTCN0;
    int took = 0;
    if (stage == 0) {
        if (fs_val[C_ENG] != ENG_LUSH) {
            ch_live = 0;                        /* back to it later: from a clear line */
        } else {
            chorus(out, in, (const s16 *)params);
            took = 1;
        }
    } else if (stage == 3) {
        took = drive(out, in, (s32)params);
    }
    if (stage == 0 || stage == 3)
        fs_taken = took ? fs_taken | (1u << stage) : fs_taken & ~(1u << stage);
    r_mine += DTCN0 - t0;
    return took;
}

/* ---- the transport (core-dn1 5.0's ev_transport): PLAY restarts the LFOs whose RESET is PLAY ------------- */
int32_t fs_transport(int32_t what, int32_t arg)
{
    (void)arg;
    if (what == CORE_TRANSPORT_START)
        fs_reset = (u8)(fs_reset | (fs_val[L1_RESET] ? 1 : 0) | (fs_val[L2_RESET] ? 2 : 0));
    return 0;
}
