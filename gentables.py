"""SEND FX's constant tables (tables.h).
python3 gentables.py > tables.h"""
import math

FS = 48000.0                    # frames a second
BLOCKS = 1500.0                 # render blocks a second (32 frames each)
out = []


def arr(ctype, name, vals, per=8):
    out.append('static const %s %s[%d] = {' % (ctype, name, len(vals)))
    for i in range(0, len(vals), per):
        out.append('    ' + ', '.join(str(v) for v in vals[i:i + per]) + ',')
    out.append('};')


def u32(x):
    return '%du' % int(round(x))


# The two mod LFOs' free RATE, 0-127: 0.02 Hz to 20 Hz on an exponential scale; the phase step a block (2^32 a turn)
arr('uint32_t', 'fs_lfo_rate', [u32(0.02 * 1000 ** (k / 127.0) * 2 ** 32 / BLOCKS) for k in range(128)], 6)
# Their RATE in Hz x 100, for the screen
arr('uint16_t', 'fs_lfo_hz100', [int(round(0.02 * 1000 ** (k / 127.0) * 100)) for k in range(128)], 12)

# The chorus's sweep from the stock SPD, 0-127: 0.05 Hz to 12 Hz; the phase step a frame
arr('uint32_t', 'fs_ch_rate', [u32(0.05 * 2 ** (k / 16.0) * 2 ** 32 / FS) for k in range(128)], 6)
# Its vibrato (VRAT), 0-127: 2 Hz to 10 Hz; the phase step a frame
arr('uint32_t', 'fs_vib_rate', [u32(2.0 * 5 ** (k / 127.0) * 2 ** 32 / FS) for k in range(128)], 6)


def onepole(fc):                # y += a (x - y): a in Q15
    return int(round(32767 * (1 - math.exp(-2 * math.pi * fc / FS))))


# The chorus's input high-pass (the stock HPF, 0-127): 0 off, then 20 Hz to 2 kHz, as a one-pole low-pass to subtract
arr('int16_t', 'fs_hpf', [0] + [onepole(20 * 100 ** (k / 126.0)) for k in range(127)], 12)
# A one-pole low-pass for TONE (0-127): 1.2 kHz to 18 kHz
arr('int16_t', 'fs_lpf', [onepole(1200 * 15 ** (k / 127.0)) for k in range(128)], 12)

# The short delay's TIME, 0-127: 32 frames (one block, 0.67 ms, the stock delay's shortest) to 1920 (40 ms), in
# quarter frames
arr('uint16_t', 'fs_short_q2', [int(round(4 * 32 * 60 ** (k / 127.0))) for k in range(128)], 12)

# The drive's gain, Q8: AMOUNT (the stock MOVD, 0-127) 0 dB to +36 dB; BOOST (0-127) adds 0 to +24 dB
arr('uint16_t', 'fs_drv_gain', [int(round(256 * 10 ** (36 * k / 127.0 / 20))) for k in range(128)], 12)
arr('uint16_t', 'fs_boost', [int(round(256 * 10 ** (24 * k / 127.0 / 20))) for k in range(128)], 12)
# Their make-up, Q15, by the whole gain in dB (0-60): half the gain back off up to 24 dB, a sixth past it (-12 dB at
# 24, -18 dB at 60), so a harder drive is louder, not 60 dB louder, and a drive clipped through stays about as loud
def comp_db(db):
    return -db / 2.0 if db <= 24 else -12 - (db - 24) / 6.0


arr('uint16_t', 'fs_drv_comp', [int(round(32767 * 10 ** (comp_db(db) / 20.0))) for db in range(61)], 12)

# A sine, a whole turn in 256 steps (and one more), Q15
arr('int16_t', 'fs_sine', [int(round(32767 * math.sin(2 * math.pi * i / 256))) for i in range(257)], 12)

print('/* SPDX-License-Identifier: GPL-2.0-or-later */')
print('/* SEND FX\'s constant tables: written by gentables.py, do not edit. */')
print('\n'.join(out))
