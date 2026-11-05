# SEND FX, Digitone mk1

More from the Digitone mk1's and Digitone Keys' own send effects and master
drive, while the synths play as ever:

- **LUSH**, a chorus of its own in place of the stock one: two to four
  voices on a bucket-brigade-style line, swept by a slow triangle, with a
  faster shimmer (VIBE), a slow random detune (DRIFT), the line's darkening
  (TONE), saturation (SAT) and feedback (FDBK). Its depth, speed, high-pass
  and width are still the kit's DPTH, SPD, HPF and WDTH (set on the stock
  FX page, or by their CCs), and its VOL and sends to the delay and
  reverb are the stock's. Written for this mod; it uses no Elektron code or
  tables.
- **Two LFOs** that move any chorus, delay, reverb or drive parameter: free
  (0.02-20 Hz) or in time with the tempo (16 bars to 1/32), seven waves,
  depth either way, a start phase, and a restart on PLAY.
- **SHORT**, a delay time from 0.67 ms to 40 ms (the stock's shortest is a
  1/128 note, 21 ms at 86 BPM) for combs and, with an LFO on it, flanging.
  It is the stock delay, fed a shorter time; FDBK, the filters, WID,
  pingpong and VOL stay the stock's.
- **HARD, FUZZ and FOLD** in place of the master drive: the stock MOVD is
  their amount (0 to +36 dB), BOOST adds up to +24 dB, TONE and LEVEL
  follow them; half of the gain comes back off, so a harder drive is louder,
  not 60 dB louder. With MOVD and BOOST at 0 the stock drive runs (clean).

Everything off (its defaults), it changes nothing the unit does.

For OS 1.43 and 1.44, on
[core-dn1](https://github.com/irpina/elekloader/tree/main/mods/core-dn1) 5.0. It names no firmware
address but core's exports: its 1.44 port is empty, and both builds carry the same code.

| | |
|---|---|
| needs | core-dn1 5.0 (`resources.core`) |
| sites | none: core's `ev_render_fx` (the chorus, the drive), `ev_render_params` (the LFOs, SHORT), `ev_render_in`, `ev_render_out`, `ev_transport`, `ev_midi_in`, `ev_key`, `ev_enc`, `ev_tick`, `ev_draw`, `core_menu` and `core_projdata` |
| firmware names | `fw_tempo`, `fw_textf`, `fw_fillrect`, `fw_framerect` and the fonts |
| project data | `FXSD`, 36 bytes: the 32 controls and the CC channel |
| RAM | 18.5 KB (14.2 KB code and tables, 4.3 KB the chorus's line and state) |

## Install

Not in elekloader's shop yet. You need:
- **core-dn1 5.0,** which is not released yet: build it from elekloader's source
  (`mods/core-dn1`, its README says how) with the stock OS file. Older cores refuse this mod.
- **elekloader from source** (main has core-dn1 5.0), with Python 3.9 or newer, to build the mod
  and the OS.
- **This mod:** `fxsend-0.1.elemod` for OS 1.43, `fxsend-0.1-os1.44.elemod` for 1.44, from
  [this repository's releases](https://github.com/irpina/digisendfx/releases) once there are any, or
  built from this folder (Build, below).
- **The stock OS file:** `Digitone_and_Digitone_Keys_OS1.44.syx` or `..._OS1.43.syx`, from
  [Elektron's Digitone downloads](https://www.elektron.se/support-downloads/digitone).
  elekloader recognises the file by its hash.

Build your OS (or in elekloader's window, run from source: **+ Install from file...** both files):

```bash
python -m elekloader.patch --stock Digitone_and_Digitone_Keys_OS1.44.syx \
    --mod core-5.0-os1.44.elemod --mod fxsend-0.1-os1.44.elemod \
    --out Digitone_OS1.44-sendfx.syx --version XXXX
```

`--version` is the 4 characters the unit will show. Flash the `.syx` with
Elektron Transfer, as for any OS update: connect the unit over USB, select
it in Transfer and **Connect**, drag the `.syx` onto **Drop files here**,
and press **YES** on the unit. Don't turn it off until the upgrade is done.

**Recovery:** elekloader never changes the bootloader, so the stock OS
file always restores the unit. Hold **FUNC** while powering on for the
startup menu, and press **TRIG 4** for OS UPGRADE. Then send the stock
`.syx` with Transfer.

## Using it

Hold a track key for the Mod Menu and pick **SEND FX**: its page opens over
the Digitone's. While it is open every key but PLAY, STOP and TEMPO is the
page's; NO (or MIDI) closes it. What it sets stays on with the page closed,
and is the project's, saved with it.

| key | |
|---|---|
| SYN1 | CHORUS |
| SYN2 | DELAY + DRIVE |
| FLTR, AMP | LFO 1, LFO 2 (LFO: one, then the other) |
| LEFT, RIGHT, PAGE | the four pages in turn |
| knob A-H | its control; the title bar shows its name and value a second |
| push A-H | its default, or an option's next value |
| YES | SETUP: the MIDI CC channel, and the CPU meter; NO back |

The left column shows what the mod runs (C the chorus, S SHORT, D the
drive) over the two LFOs' bars.

| page | A | B | C | D | E | F | G | H |
|---|---|---|---|---|---|---|---|---|
| CHORUS | ENG: STK, LUSH | VOIC: 2, 3, 4 | TONE | DRFT | VIBE | VRAT | FDBK (+/-) | SAT |
| DELAY + DRIVE | TIME: STK, SHRT | SHRT: 0.67-40 ms | | | DRV: STK, HARD, FUZZ, FOLD | BST: 0-24 dB | TONE | LEV |
| LFO 1, LFO 2 | RATE: 0.02-20 Hz | SYNC: FREE, 16B ... 1/32 | WAVE: TRI SIN SQR SAW RAMP S&H RND | DEST | DEP (+/-) | SPH | RST: FREE, PLAY | |

- **VOIC** 2: two voices a side, sweeping opposite (the classic stereo
  pair); 3: an ensemble, left, both and right, a third of a turn apart; 4:
  wider, a quarter turn apart. Centre delays 5.5-10 ms; DPTH sweeps up to
  3 ms either way.
- **VIBE**, **VRAT**: a second, faster sine on each voice (2-10 Hz, up to
  0.5 ms); **DRFT**: each voice wanders up to 1 ms, slowly.
- **TONE**: the line's input and output low-pass, 1.2 kHz to 18 kHz.
  **SAT**: drive into the line with a soft clip, the level kept. **FDBK**:
  voice 1 back into the line, up to 0.7 either way.
- **DEST**: OFF; CDEP CSPD CHPF CWID C>DL C>RV CVOL (chorus); DTIM DWID DFB
  DHPF DLPF D>RV DVOL (delay); RPRE RDEC RFRQ RGAN RHPF RLPF RVOL (reverb);
  DRV (the master drive). The LFO moves the parameter's value by up to
  127 either way at full depth, inside its range, every block; the kit
  keeps its value. DTIM moves SHORT's time when TIME is SHRT.
- **MIDI**: SETUP's channel (OFF by default) takes CC 20-51 as the 32
  controls, page by page, A to H (CC 20 ENG ... CC 51 LFO 2's H); an
  option takes the CC's value in equal bands. Only what MIDI CONFIG > PORT
  CONFIG > INPUT FROM lets in arrives.

## What the stages get

Found in digikit's emulator with a probe on both events (1.43 and 1.44 the
same). This is what a mod on `ev_render_fx` can count on.

- **The buffers.** Each block is 32 frames (1500 blocks a second), L,R
  interleaved, **Q1.31 in an `int32_t`** (the typedef in
  `digitone-mk1/core3.h` says `int16_t *`; the words are 32-bit). Stage 0-2:
  `in` is the stage's send bus (the tracks' sends; the chorus's sends join
  the delay's and the reverb's), `out` its return, which the stock then
  mixes by the stage's VOL. In 1.43 `in` is 0x8000ef70 for every stage
  (refilled between them) and `out` 0x8000eb70, 0x8000ec70, 0x8000ed70.
  Stage 3, the master drive: `in` is the whole master (the synths, the
  inputs and the three returns), `out` (0x8000ee70) is what goes on to the
  pattern volume, the codec and USB. A handler that copies `in` to `out` and
  takes stage 3 sounds exactly as stock with MOVD 0 (the same samples);
  one that writes zeros silences the unit.
- **`params`, stages 0-2,** is a pointer into the render's own copy of the
  FX record (0x80001ff4 + 2 x (9 + 632 + slot) in 1.43), 8.8 words, 127.0 =
  32512, each stage's first slot:

  | stage | words |
  |---|---|
  | 0 chorus (slot 17) | DPTH SPD HPF WDTH >DEL >REV VOL |
  | 1 delay (slot 24) | TIME X(pingpong) WID FDBK HPF LPF >REV VOL |
  | 2 reverb (slot 32) | PRE DEC FREQ GAIN HPF LPF VOL |

  The master's are further on: 39-49 the inputs and their sends, 50 MOVD,
  51 PVOL. The firmware's parameter records (`fw_params`, 60 bytes an id,
  the slot at +4) name every one: ids 67-73 chorus, 74-81 delay, 82-88
  reverb, 89-106 the master.
- **`params`, stage 3,** is not a pointer: it is MOVD's word itself.
- **Changing them in place and returning 0** works for one block for what
  the stage reads itself: delay FDBK set to 0 there took the echoes away,
  and the next block had the stock value back. The delay's **TIME is read
  before the stage** (the master stage sets the line's read point at its
  start), so TIME changed in place does nothing. **`ev_render_params`**
  reaches every word, TIME included: what a handler writes at
  `words[632 + slot]` is in the stages' copy in the same block, and lasts
  that block. This mod moves parameters there.
- **The delay's TIME** waits (TIME + 1) / 128 of a whole note: at 86 BPM,
  TIME 0 is 1046 frames, 23 (the default) 25100. Its word goes below 0 in
  the render: -128 522 frames, -224 130, -240 64, -248 32 (one block). Under
  one block the delay is silent. SHORT computes the word from the time and
  `fw_tempo` (rounded up, never under a block).
- **Levels.** The stock chorus returns about twice its input (+6 dB, with a
  wide stereo image); LUSH matches it.

## Build

```bash
python3 gentables.py > tables.h        # only after changing gentables.py
python -m elekloader.sdk.build . --stock Digitone_and_Digitone_Keys_OS1.43.syx   # out/fxsend-0.1.elemod
python -m elekloader.sdk.build . --stock Digitone_and_Digitone_Keys_OS1.44.syx   # out/fxsend-0.1-os1.44.elemod
python -m elekloader.lint out/fxsend-0.1.elemod --stock Digitone_and_Digitone_Keys_OS1.43.syx --with core-5.0.elemod
```

## What was checked

In digikit's emulator (`digikit-mk1` with the Session PC fix of
irpina/digiemu#39), 1.43 and 1.44, with core-dn1 5.0 from elekloader's main
and a probe mod beside it for the measurements (not part of this mod):

- builds, lints and links for both (an empty port: the same code and
  relocations), beside elekloader's 5.0 hooks check, fxkit, DigiHalo and the
  machines mod; refused beside the released core-dn1 4.1;
- with its defaults, the factory pattern's audio is the same samples as
  without it, and nothing is taken (`fs_taken` 0);
- LUSH takes stage 0; on a 375 Hz tone into the chorus bus it returns the
  stock chorus's level (RMS 1433-1588 against 1439) with its stereo spread;
- SHORT: echoes at 32, 60, 118, 252 and 804 frames for SHRT 0, 20, 40, 64
  and 100 (the table says 32, 61, 116, 252, 804);
- an LFO (SQR, DEST DVOL) moves the delay's VOL word between 0 and 127; a
  synced RAMP restarts on PLAY with RST PLAY and keeps the bar; an LFO on
  DTIM in SHORT sweeps TIME (a flanger);
- HARD, FUZZ and FOLD take stage 3: at MOVD 64 the master is 4.6, 2.5 and
  2.5 dB louder than the stock drive's at 64; at full MOVD and BOOST about
  as loud as the stock drive at full;
- the page: the Mod Menu entry, the four pages and SETUP draw; knobs, pushes,
  page keys, YES and NO do what is above; CC 21 and 29 on SETUP's channel
  set VOIC and SHRT, a CC on another channel is left alone (with the
  emulator's INPUT FROM set to its port);
- CPU, with digikit's cycle model (250 MHz) and the factory pattern
  playing, the whole render a block: stock 59.6%; LUSH with 2, 3 and 4
  voices (and SAT) 60.1%, 61.3%, 62.4%; SHORT and both LFOs 59.8%; HARD
  55.9% (it costs less than the stock drive); all of them at once 58.9%.
  LUSH and the drive skip the stock stage they replace;
- `emu.fwcheck` against stock, its defaults, both OS: every stage passes and
  the ten screens are the same; the render's margin 39.8% (1.43) and 39.9%
  (1.44) against stock's 40.5%, the CPU busy 59% against 58%. The audio is
  identical on 1.43; on 1.44 it differs in 33 windows of 100 ms from 4.6 s,
  as with core-dn1's own builds there (the same sound slightly shifted in
  time; not compared with a core-only build in this run).

Not checked: on a unit; how it sounds to a listener (the emulator gives
samples, not a judgement); the SETUP page's CPU meter (it reads DMA timer 0,
which the emulator does not run: 0.0% there); a project saved and loaded
with its `FXSD` block (core's project data, as the pedals use it); a
Digitone Keys; another mod taking the same stages (the first handler that
takes a stage wins, by `order`).

## Licence

GPL-2.0-or-later, as elekloader's core it builds on (LICENSE).
