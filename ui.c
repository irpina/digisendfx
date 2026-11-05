/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SEND FX's UI (the UI task, and the MIDI task for CCs): its Mod Menu entry, its page, its keys and knobs, its
 * project data and its MIDI CCs.
 *
 *   SEND FX in core-dn1's Mod Menu (hold a track key) opens its page over the Digitone's, as one more page:
 *   the knobs, their pushes, PAGE, YES and NO are the page's, every other key the Digitone's (the sequencer,
 *   the trig and track keys, RECORD, the arrows). TRIG, SYN1, SYN2, FLTR, AMP, LFO, the menus, MIDI, TEMPO and
 *   FUNC's combinations go to their stock screens and close the page; NO closes it too, and the Mod Menu opens
 *   it again where it was. What the page sets stays on with the page closed: it is the project's, saved with it.
 *   PAGE: the four pages in turn (CHORUS, DELAY + DRIVE, LFO 1, LFO 2). Turn a knob: its value, shown in the
 *   title bar a second. Push it: its default, or an option's next value. YES: SETUP (the MIDI CC channel, and
 *   the CPU meter; the arrows step the channel); NO back.
 *   MIDI: on SETUP's CC channel (OFF by default) CC 20-51 are the 32 controls, page by page, A to H. */
#include "fxsend.h"
#include "tables.h"                     /* the curves the screen shows */

#pragma GCC optimize ("Os")            /* the UI: small beats fast */

#define KEY_FUNC    1
#define KEY_MIDI    2
#define KEY_RECORD  10
#define KEY_PLAY    11
#define KEY_STOP    12
#define KEY_YES     13
#define KEY_NO      14
#define KEY_UP      15
#define KEY_DOWN    16
#define KEY_LEFT    17
#define KEY_RIGHT   18
#define KEY_PAGE    19
#define KEY_TRIG    20
#define KEY_LFO     25
#define KEY_PUSH_A  46
#define EV_KEY(ev)   (*(u32 *)((char *)(ev) + 12))
#define EV_FLAGS(ev) (*(u32 *)((char *)(ev) + 16))
#define EV_DELTA(ev) (*(s32 *)((char *)(ev) + 16))
#define COUNTS      4                   /* encoder counts a detent */
#define NPAGE       4
#define CC_FIRST    20                  /* CC 20-51: the controls */

#define FILLRECT  fw_fillrect
#define FRAMERECT fw_framerect
#define TEXTF     fw_textf
#define FONT5     fw_font5              /* 3 x 5: the stock value boxes' */
#define FONT_LBL  fw_font_label         /* the stock labels' */
#define FONT_TTL  fw_font_title         /* the stock title bar's */

/* ---- the controls ---------------------------------------------------------------------------------- */
enum { K_NONE, K_KNOB, K_BIP, K_OPT };
struct ctl {
    const char *lbl;                    /* the knob's label, four capitals */
    const char *name;                   /* the title bar's */
    u8 kind, n, def;                    /* K_*, an option's count, the default */
    const char *const *opts;            /* an option's values, four characters at most */
};

static const char *const o_eng[2] = { "STK", "LUSH" };
static const char *const o_voices[3] = { "2", "3", "4" };
static const char *const o_dmode[2] = { "STK", "SHRT" };
static const char *const o_drv[DRV_N] = { "STK", "HARD", "FUZZ", "FOLD" };
static const char *const o_sync[SYNC_N] = { "FREE", "16B", "8B", "4B", "2B", "1B", "1/2", "1/4.", "1/4", "1/8.",
                                            "1/8", "1/16", "1/32" };
static const char *const o_wave[W_N] = { "TRI", "SIN", "SQR", "SAW", "RAMP", "S&H", "RND" };
static const char *const o_dest[DEST_N] = {
    "OFF", "CDEP", "CSPD", "CHPF", "CWID", "C>DL", "C>RV", "CVOL", "DTIM", "DWID", "DFB", "DHPF", "DLPF", "D>RV",
    "DVOL", "RPRE", "RDEC", "RFRQ", "RGAN", "RHPF", "RLPF", "RVOL", "DRV" };
static const char *const dest_long[DEST_N] = {
    "Off", "Chorus Depth", "Chorus Speed", "Chorus HPF", "Chorus Width", "Chorus > Delay", "Chorus > Reverb",
    "Chorus Volume", "Delay Time", "Delay Width", "Delay Feedback", "Delay HPF", "Delay LPF", "Delay > Reverb",
    "Delay Volume", "Reverb Pre-delay", "Reverb Decay", "Reverb Shelf Freq", "Reverb Shelf Gain", "Reverb HPF",
    "Reverb LPF", "Reverb Volume", "Master Drive" };
static const char *const o_reset[2] = { "FREE", "PLAY" };

#define LFO_CTLS(n) \
    { "RATE", "Rate", K_KNOB, 0, 64, 0 }, { "SYNC", "Sync", K_OPT, SYNC_N, 0, o_sync }, \
    { "WAVE", "Wave", K_OPT, W_N, W_TRI, o_wave }, { "DEST", "Dest", K_OPT, DEST_N, 0, o_dest }, \
    { "DEP", "Depth", K_BIP, 0, 64, 0 }, { "SPH", "Start Phase", K_KNOB, 0, 0, 0 }, \
    { "RST", "Reset", K_OPT, 2, 0, o_reset }, { "", "", K_NONE, 0, 0, 0 }

static const struct ctl ctls[FS_N] = {
    { "ENG", "Chorus", K_OPT, 2, ENG_STOCK, o_eng }, { "VOIC", "Voices", K_OPT, 3, 1, o_voices },
    { "TONE", "Tone", K_KNOB, 0, 80, 0 }, { "DRFT", "Drift", K_KNOB, 0, 32, 0 },
    { "VIBE", "Vibe", K_KNOB, 0, 20, 0 }, { "VRAT", "Vibe Rate", K_KNOB, 0, 64, 0 },
    { "FDBK", "Feedback", K_BIP, 0, 64, 0 }, { "SAT", "Saturation", K_KNOB, 0, 24, 0 },
    { "TIME", "Delay Time", K_OPT, 2, DT_STOCK, o_dmode }, { "SHRT", "Short Time", K_KNOB, 0, 40, 0 },
    { "", "", K_NONE, 0, 0, 0 }, { "", "", K_NONE, 0, 0, 0 },
    { "DRV", "Drive", K_OPT, DRV_N, DRV_STOCK, o_drv }, { "BST", "Boost", K_KNOB, 0, 0, 0 },
    { "TONE", "Drive Tone", K_KNOB, 0, 100, 0 }, { "LEV", "Drive Level", K_KNOB, 0, 64, 0 },
    LFO_CTLS(1),
    LFO_CTLS(2),
};
static const char *const page_title[NPAGE] = { "Chorus", "Delay+Drive", "LFO 1", "LFO 2" };

static int ctl_get(int i) { return fs_val[i]; }

static int ctl_off(int i)               /* nonzero: the control does nothing now (drawn dotted) */
{
    int p = i >> 3, k = i & 7;
    if (p == 0)
        return k != C_ENG && fs_val[C_ENG] != ENG_LUSH;
    if (p == 1)
        return k == 1 ? fs_val[D_MODE] != DT_SHORT : k > 4 ? fs_val[V_MODE] == DRV_STOCK : 0;
    if (k == L_RATE)
        return fs_val[i + L_SYNC] != 0;
    return k != L_DEST && fs_val[i - k + L_DEST] == 0;
}

static void ctl_set(int i, int v)
{
    const struct ctl *c = &ctls[i];
    if (c->kind == K_NONE)
        return;
    if (c->kind == K_OPT)
        v = v < 0 ? 0 : v >= c->n ? c->n - 1 : v;
    else
        v = v < 0 ? 0 : v > 127 ? 127 : v;
    fs_val[i] = (u8)v;
}

/* ---- project data: the controls and the CC channel, saved with the project (36 bytes) ---------------- */
#define SAVE_VER    1
struct fs_save {
    u8 ver, cc_ch, r2, r3;              /* fs_cc_ch: 0 off, 1-16 */
    u8 val[FS_N];
} __attribute__((aligned(4)));
static struct fs_save save;
volatile u8 fs_cc_ch;                  /* SETUP's MIDI channel for the CCs: 0 off, 1-16 */
static u8 ui_inited;

static void defaults(void)
{
    int i;
    for (i = 0; i < FS_N; i++)
        fs_val[i] = ctls[i].def;
    fs_cc_ch = 0;
}

static void loaded(int32_t found)
{
    int i;
    if (!found || save.ver != SAVE_VER) {
        defaults();
    } else {
        for (i = 0; i < FS_N; i++) {
            fs_val[i] = ctls[i].def;
            ctl_set(i, save.val[i]);
        }
        fs_cc_ch = save.cc_ch <= 16 ? save.cc_ch : 0;
    }
    ui_inited = 1;
}

const struct core_projdata fs_projdata = { 0x46585344u /* "FXSD" */, sizeof(struct fs_save), &save, loaded };

static void keep_save(void)
{
    int i;
    save.ver = SAVE_VER;
    save.cc_ch = fs_cc_ch;
    for (i = 0; i < FS_N; i++)
        save.val[i] = fs_val[i];
}

/* ---- the Mod Menu entry --------------------------------------------------------------------------- */
static const u16 fs_icon[16] = {        /* three waves, one under the other */
    0x0000, 0x6060, 0x9090, 0x0909, 0x0606, 0x0000, 0x6060, 0x9090,
    0x0909, 0x0606, 0x0000, 0x6060, 0x9090, 0x0909, 0x0606, 0x0000,
};
static u8 ui_open, ui_setup, ui_pg, ui_flash, ui_flash_i;
static s32 enc_acc[8];

static void fs_open(void *brain, void *ev, int32_t track)
{
    (void)brain, (void)ev, (void)track;
    ui_open = 1;
    ui_setup = 0;
    ui_flash = 0;
}

const struct core_menu_item fs_menu = { "SEND FX", fs_open, CORE_MENU_ICON, fs_icon };

/* ---- knobs and keys ---------------------------------------------------------------------------------- */
static void turn(int i, int d)
{
    const struct ctl *c = &ctls[i];
    if (c->kind == K_NONE)
        return;
    ctl_set(i, ctl_get(i) + d);         /* an option: a value a detent, as the stock lists */
    ui_flash = 30;
    ui_flash_i = (u8)i;
}

static void push(int i)
{
    const struct ctl *c = &ctls[i];
    if (c->kind == K_NONE)
        return;
    ctl_set(i, c->kind == K_OPT ? (ctl_get(i) + 1) % c->n : c->def);
    ui_flash = 30;
    ui_flash_i = (u8)i;
}

/* The page is one more page of the Digitone's, not a mode: it takes only what it needs (the knobs, their
 * pushes, PAGE, YES and NO, and in SETUP the arrows), and every other key goes on to the stock as ever, so
 * the sequencer, the trig and track keys, RECORD and the arrows work under it. A key that brings up a stock
 * screen (TRIG, SYN1-LFO, the menus, MIDI, TEMPO, or anything with FUNC held) closes it first; the Mod Menu
 * opens it again on the page it had. A release goes where its press went. */
static u8 took[64];                     /* 1: the page took this key's press, so it takes its release */

static int screen_key(u32 key, u32 fl)
{
    if (key == KEY_FUNC || key == KEY_PLAY || key == KEY_STOP || key == KEY_RECORD)
        return 0;
    if (fl & 2)                         /* with FUNC: copy, paste, the menus, a save ... */
        return 1;
    return key < KEY_RECORD || (key >= KEY_TRIG && key <= KEY_LFO);
}

static int own_key(u32 key, u32 fl)
{
    if (fl & 2)
        return 0;
    if (key == KEY_YES || key == KEY_NO || key == KEY_PAGE || (key >= KEY_PUSH_A && key < KEY_PUSH_A + 8))
        return 1;
    return ui_setup && key >= KEY_UP && key <= KEY_RIGHT;
}

int fs_key(void *brain, void *ev)
{
    u32 key = EV_KEY(ev), fl = EV_FLAGS(ev);
    (void)brain;
    if (key >= 64)
        return 0;
    if (!(fl & 1)) {                    /* a release */
        if (!took[key])
            return 0;
        took[key] = 0;
        return 1;
    }
    if (!ui_open)
        return 0;
    if (!own_key(key, fl)) {
        if (screen_key(key, fl))
            ui_open = 0;                /* the stock's screen, the page shut */
        return 0;
    }
    took[key] = 1;
    if (fl & 8)
        return 1;                       /* repeats */
    if (key == KEY_YES) {
        ui_setup = !ui_setup;
    } else if (key == KEY_NO) {
        if (ui_setup)
            ui_setup = 0;
        else
            ui_open = 0;
    } else if (ui_setup) {
        if (key == KEY_LEFT || key == KEY_DOWN)
            fs_cc_ch = (u8)(fs_cc_ch ? fs_cc_ch - 1 : 16);
        else if (key == KEY_RIGHT || key == KEY_UP)
            fs_cc_ch = (u8)(fs_cc_ch < 16 ? fs_cc_ch + 1 : 0);
    } else if (key == KEY_PAGE) {
        ui_pg = (u8)((ui_pg + 1) % NPAGE);
        ui_flash = 0;
    } else if (key >= KEY_PUSH_A && key < KEY_PUSH_A + 8) {
        push(8 * ui_pg + (int)(key - KEY_PUSH_A));
    }
    return 1;
}

int fs_enc(void *brain, void *ev)
{
    u32 e = EV_KEY(ev);
    s32 d;
    int i;
    (void)brain;
    if (!ui_open || e < 1 || e > 8)
        return 0;                       /* LEVEL/DATA stays the Digitone's */
    i = (int)e - 1;
    enc_acc[i] += EV_DELTA(ev);
    d = enc_acc[i] / COUNTS;
    enc_acc[i] -= d * COUNTS;
    if (!d)
        return 1;
    if (ui_setup)
        fs_cc_ch = (u8)((fs_cc_ch + 17 + (d > 0 ? 1 : -1)) % 17);
    else
        turn(8 * ui_pg + i, (int)d);
    return 1;
}

/* ---- MIDI (core-dn1 5.0's ev_midi_in, the MIDI task): CC 20-51 on SETUP's channel ------------------------ */
int32_t fs_midi(const uint8_t *msg, int32_t len, int32_t source)
{
    int i, v;
    (void)source;
    if (!fs_cc_ch || len < 3 || (msg[0] & 0xf0) != 0xb0 || (msg[0] & 15) != fs_cc_ch - 1)
        return 0;
    i = (int)msg[1] - CC_FIRST;
    v = msg[2] & 127;
    if (i < 0 || i >= FS_N || ctls[i].kind == K_NONE)
        return 0;
    ctl_set(i, ctls[i].kind == K_OPT ? v * ctls[i].n / 128 : v);
    return 1;
}

/* ---- the UI tick ------------------------------------------------------------------------------------- */
static u32 ui_sig;
static u8 ui_was;                       /* ui_open at the last tick */

static u32 signature(void)
{
    u32 h = (u32)(ui_pg | ui_setup << 2 | (ui_flash != 0) << 3 | ui_flash_i << 4 | fs_cc_ch << 9);
    int i;
    for (i = 0; i < FS_N; i++)
        h = h * 31 + fs_val[i];
    if (ui_pg >= 2)                     /* the LFO's bar */
        h = h * 31 + (u32)(fs_lfo_now[ui_pg - 2] >> 11);
    if (ui_setup)
        h = h * 31 + fs_load / 10 + (fs_mine << 8);
    return h * 31 + (u32)fw_tempo;
}

void fs_tick(void *ctrl)
{
    if (!ui_inited) {                   /* no project load reached us */
        defaults();
        ui_inited = 1;
    }
    keep_save();
    if (ui_flash)
        ui_flash--;
    if (ui_open) {
        u32 sg = signature();
        if (sg != ui_sig || !ui_was) {
            ui_sig = sg;
            *((unsigned char *)ctrl + 0x20) = 1;
        }
    } else if (ui_was) {
        *((unsigned char *)ctrl + 0x20) = 1;    /* closed: the stock's screen again */
    }
    ui_was = ui_open;
}

/* ---- the screen: 128 x 64, y = 0 the bottom row; drawn as the pedals draw theirs ------------------------ */
static const u8 col_x[4] = { 33, 59, 86, 112 };
#define ROW_Y(r)    ((r) ? 15 : 42)
#define LBL_Y(r)    ((r) ? 0 : 27)
static const signed char needle[29][2] = {
    {-4, -4}, {-5, -3}, {-5, -3}, {-6, -2}, {-6, -1}, {-6, 0}, {-6, 1}, {-6, 2}, {-5, 3}, {-4, 4}, {-4, 5},
    {-3, 5}, {-2, 6}, {-1, 6}, {0, 6}, {1, 6}, {2, 6}, {3, 5}, {4, 5}, {4, 4}, {5, 3}, {6, 2}, {6, 1}, {6, 0},
    {6, -1}, {6, -2}, {5, -3}, {5, -3}, {4, -4} };
static const signed char ring[12][2] = { {0, 8}, {1, 8}, {2, 8}, {3, 7}, {4, 7}, {5, 6}, {6, 5}, {7, 4}, {7, 3},
                                         {8, 2}, {8, 1}, {8, 0} };

static void px(void *bmp, int x, int y, int c) { FILLRECT(bmp, x, y, x, y, c); }

static void corners(void *bmp, int x0, int y0, int x1, int y1, int c)
{
    px(bmp, x0, y0, c);
    px(bmp, x1, y0, c);
    px(bmp, x0, y1, c);
    px(bmp, x1, y1, c);
}

static void rframe(void *bmp, int x0, int y0, int x1, int y1)
{
    FRAMERECT(bmp, x0, y0, x1, y1, 1);
    corners(bmp, x0, y0, x1, y1, 0);
}

static void line(void *bmp, int x0, int y0, int x1, int y1)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    for (;;) {
        px(bmp, x0, y0, 1);
        if (x0 == x1 && y0 == y1)
            break;
        e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static int slen(const char *s) { int n = 0; while (s[n]) n++; return n; }

static void small_text(void *bmp, int cx, int cy, const char *s)
{
    TEXTF(bmp, FONT5, cx - (4 * slen(s) - 1) / 2, cy - 2, -1, "%s", s);
}

static void label(void *bmp, int cx, int y, const char *s)
{
    int n = slen(s);
    if (n)
        TEXTF(bmp, FONT_LBL, cx - (5 * n - 1) / 2, y, -1, "%s", s);
}

static void knob(void *bmp, int cx, int cy, int v, int bipolar, int off)
{
    int i, k = (v * 28 + 63) / 127;
    for (i = 0; i < 12; i += off ? 2 : 1) {
        int dx = ring[i][0], dy = ring[i][1];
        px(bmp, cx + dx, cy + dy, 1);
        px(bmp, cx - dx, cy + dy, 1);
        px(bmp, cx + dx, cy - dy, 1);
        px(bmp, cx - dx, cy - dy, 1);
    }
    line(bmp, cx, cy, cx + needle[k][0], cy + needle[k][1]);
    if (bipolar) {
        FILLRECT(bmp, cx - 10, cy - 7, cx - 8, cy - 7, 1);
        FILLRECT(bmp, cx + 8, cy - 7, cx + 10, cy - 7, 1);
        FILLRECT(bmp, cx + 9, cy - 8, cx + 9, cy - 6, 1);
    }
}

static void box(void *bmp, int cx, int cy, const char *s, int off)
{
    rframe(bmp, cx - 8, cy - 8, cx + 8, cy + 8);
    small_text(bmp, cx, cy, s);
    if (off)
        line(bmp, cx - 7, cy - 7, cx + 7, cy + 7);
}

/* a control's value as the title bar shows it */
static void value_text(void *bmp, int x, int y, int i)
{
    const struct ctl *c = &ctls[i];
    int v = ctl_get(i), k = i & 7;
    if (c->kind == K_OPT) {
        TEXTF(bmp, FONT_TTL, x, y, -1, "%s", k == L_DEST && i >= L1_RATE ? dest_long[v % DEST_N] : c->opts[v % c->n]);
    } else if (c->kind == K_BIP) {
        TEXTF(bmp, FONT_TTL, x, y, -1, "%c%d", v >= 64 ? '+' : '-', v >= 64 ? v - 64 : 64 - v);
    } else if (i == D_TIME) {           /* the short delay, in ms (quarter frames / 192) */
        u32 q2 = fs_short_q2[v];
        TEXTF(bmp, FONT_TTL, x, y, -1, "%d.%d%dms", (int)(q2 / 192), (int)(q2 % 192 * 10 / 192),
              (int)(q2 % 192 * 100 / 192 % 10));
    } else if ((i & ~8) == L1_RATE) {
        u32 h = fs_lfo_hz100[v];
        TEXTF(bmp, FONT_TTL, x, y, -1, "%d.%d%dHz", (int)(h / 100), (int)(h % 100 / 10), (int)(h % 10));
    } else if (i == V_BOOST) {
        TEXTF(bmp, FONT_TTL, x, y, -1, "+%ddB", (v * 24 + 63) / 127);
    } else {
        TEXTF(bmp, FONT_TTL, x, y, -1, "%d", v);
    }
}

static void title_bar(void *bmp)
{
    int t = fw_tempo / 120;
    TEXTF(bmp, FONT_TTL, t >= 100 ? 1 : 3, 56, -1, "%d", t);
    FILLRECT(bmp, 0, 54, 14, 63, -1);
    corners(bmp, 0, 54, 14, 63, 0);
    if (ui_setup) {
        TEXTF(bmp, FONT_TTL, 19, 56, -1, "Send FX setup");
    } else if (ui_flash) {
        const char *n = ctls[ui_flash_i].name;
        TEXTF(bmp, FONT_TTL, 19, 56, -1, "%s", n);
        value_text(bmp, 19 + 5 * slen(n) + 4, 56, ui_flash_i);
    } else {
        TEXTF(bmp, FONT_TTL, 19, 56, -1, "%s (%d/%d)", page_title[ui_pg], ui_pg + 1, NPAGE);
    }
    FILLRECT(bmp, 17, 54, 127, 63, -1);
    corners(bmp, 17, 54, 127, 63, 0);
}

static void left_column(void *bmp)
{
    int k;
    rframe(bmp, 0, 27, 14, 50);         /* what the mod runs now: C(horus) S(hort) D(rive) */
    if (fs_val[C_ENG] == ENG_LUSH)
        small_text(bmp, 7, 45, "C");
    if (fs_val[D_MODE] == DT_SHORT)
        small_text(bmp, 7, 39, "S");
    if (fs_val[V_MODE] != DRV_STOCK)
        small_text(bmp, 7, 33, "D");
    for (k = 0; k < 2; k++) {           /* the two LFOs, as bars from the middle */
        int x = 4 + 5 * k, y = 15 + (fs_lfo_now[k] * 8) / 32768;
        rframe(bmp, x - 1, 5, x + 3, 25);
        if (fs_val[L1_DEST + 8 * k])
            FILLRECT(bmp, x + 1, y < 15 ? y : 15, x + 1, y < 15 ? 15 : y, 1);
    }
    label(bmp, 7, -1, "LFO");
}

static void control(void *bmp, int i)
{
    const struct ctl *c = &ctls[i];
    int k = i & 7, cx = col_x[k & 3], r = k >> 2, cy = ROW_Y(r);
    if (c->kind == K_NONE)
        return;
    if (c->kind == K_OPT)
        box(bmp, cx, cy, c->opts[ctl_get(i) % c->n], ctl_off(i));
    else
        knob(bmp, cx, cy, ctl_get(i), c->kind == K_BIP, ctl_off(i));
    label(bmp, cx, LBL_Y(r), c->lbl);
}

static void draw_setup(void *bmp)
{
    TEXTF(bmp, FONT5, 3, 44, -1, "MIDI CC CHANNEL");
    if (fs_cc_ch)
        TEXTF(bmp, FONT5, 84, 44, -1, "%d", fs_cc_ch);
    else
        TEXTF(bmp, FONT5, 84, 44, -1, "OFF");
    FILLRECT(bmp, 1, 43, 126, 48, -1);
    TEXTF(bmp, FONT5, 3, 34, -1, "CC 20-51: THE CONTROLS, A-H");
    TEXTF(bmp, FONT5, 3, 27, -1, "PAGE BY PAGE");
    TEXTF(bmp, FONT5, 3, 14, -1, "CPU: RENDER %d.%d%%  SEND FX %d.%d%%", fs_load / 10, fs_load % 10, fs_mine / 10,
          fs_mine % 10);
}

void fs_draw(void *bmp, void *ctrl)
{
    int i;
    (void)ctrl;
    if (!ui_open)
        return;
    FILLRECT(bmp, 0, 0, 127, 63, 0);
    title_bar(bmp);
    if (ui_setup) {
        draw_setup(bmp);
        return;
    }
    left_column(bmp);
    for (i = 0; i < 8; i++)
        control(bmp, 8 * ui_pg + i);
}
