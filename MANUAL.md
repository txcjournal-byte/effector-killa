# EFFECTOR KILLA – User Manual

**Every effect. One Killa.** An 8-slot trap multi-effect: pick a channel and a preset, shape it with
5 macros, or hit **KILL** for a new chain that is musical and level-matched.

Formats: VST3 (Windows / macOS), AU (macOS), Standalone. Stereo or mono.

---

## Install

**Windows:** run `EffectorKilla-…-Setup.exe`. The plugin goes to `C:\Program Files\Common Files\VST3`.
**macOS:** VST3 to `~/Library/Audio/Plug-Ins/VST3`, AU to `~/Library/Audio/Plug-Ins/Components`.

Then rescan plugins in your DAW. In FL Studio: *Options → Manage plugins → Find plugins*.

---

## Quick start

1. Put Effector Killa on a mixer insert.
2. Turn the **channel dial** (01–12), or click a number, to choose a category.
3. Use **PROGRAM ▲ ▼** to step through the 8 presets of the channel.
4. Shape the sound with the 5 macro knobs.
5. Press **KILL** (or the space bar) for a new chain.

Factory presets and KILL results are **level-matched to your input**, so louder never fools you.
The CH 12 FINAL BOSS bus / master presets are the exception: they keep their designed loudness.

---

## The cabinet

### Left panel

| Control | What it does |
|---|---|
| **KILL** | Generates a new chain. Follows musical rules, for example saturation comes before compression and reverb sits at the end. |
| **PG / R / UNRATED** | KILL strength. **PG** keeps the effects and tweaks their parameters (±25 %), swapping at most one slot. **R** builds a new chain of 3–5 slots. **UNRATED** builds a wilder chain of 4–7 slots. |
| **BOOTLEG** | Subtle variation of the current chain: parameters move ±10–20 % and the effects stay. |
| **POCKET TV** | Monitor as if on a phone speaker: mono, 300 Hz – 8 kHz. It **blinks** while on – switch it off before export! |
| **AUTO TRACKING** | Slow automatic gain (±12 dB). It keeps the output as loud as the input. |
| **BASIC / PREMIUM CABLE** | BASIC is the simple mode, with calmer animations and no slot detail. PREMIUM shows everything. |
| **STAFF PICK ⭐** | Adds the preset to your favourites. |
| **ANTENNA IN / RF OUT** | Input and output gain (±24 dB). |

### Macros (right)

| Macro | What it does |
|---|---|
| **VILLAIN ARC** | Darker: filters close and the reverb is damped. |
| **CRASH OUT** | Dirtier: more drive and crush, and harder clipping. |
| **AURA** | More space: reverb and delay go up, the image gets wider. |
| **DRIP** | More movement: LFO rate and depth, wow and chorus. |
| **KNOCK** | More punch: transients, compression and drive. |

Every macro sits at the **middle (50) = the preset as designed**. Turn it left for less, right for more.
Some presets map a macro to their own targets. The preset's tuned macro is marked with an amber dot.
Macros respect the source. On an 808, for example, AURA never pushes the reverb above 15 %.

### Cassettes = the 8 slots

- **Click** a cassette to select it. It slides out and the TV shows its parameters (PREMIUM).
- **Drag** a cassette to change the order.
- **Right-click** for the slot menu: change the effect, copy / paste, EJECT, save / load a slot
  preset, WRITE PROTECT, PAUSE, SCREENING (solo) and M/S (process only the mid or the side).
- The coloured dot shows the effect type.
- A red **WRITE PROTECT** tag means KILL will never touch that slot.
- A grey cassette is paused.

**Slot detail on the TV:**
- Drag a bar left / right to set its value. Hold Shift for fine steps.
- Double-click a bar to reset it; the mouse wheel changes it step by step.
- **PAUSE**, **SCREEN** (solo), **ST/MID/SIDE** and **LOCK** are at the top.
- Esc or a click outside closes the detail.

### Video recorder

| Control | What it does |
|---|---|
| **Display** | Preset name. Click it to open the preset lists: channel, all channels, STAFF PICKS and USER TAPES. |
| **REC** | Saves your own preset to `Documents/Killa/Effector Killa/Presets`. |
| **◀◀ REWIND / ▶▶ FAST FWD** | Undo / redo (64 steps). |
| **EJECT** | Empties the selected cassette. |
| **SIDE A / SIDE B** | Two independent states for A/B comparison. |

### The TV

The picture is the channel's "programme". It pulses with the tempo and reacts to your sound and the macros.
The **AV switch** under the TV turns the screen into a controller. By default X = CRASH OUT and Y = AURA;
click the **X … / Y …** labels to choose any macro or slot parameter instead.

- **AV1 REMOTE** – an XY pad. Drag the glowing dot; it stays where you leave it. Double-click resets.
- **AV2 SCRIBBLE** – draw a line or loop. The dot rides along it in tempo, locked to your DAW transport.
  Click the ruler to choose 1, 2 or 4 bars. Right-click erases.
- **AV3 SCREENSAVER** – the logo bounces around the screen. When it hits a corner exactly, you get a glitch burst.
  Click the ruler to change the speed.
- If you automate a macro in your DAW, the automation wins and the AV modulation of that macro pauses.

**OFF AIR:**
- Click it to hear your original signal at the same loudness (true bypass comparison). Click again to come back.
- Hold it to listen to the original only while held.

**OVERLOAD** flashes on the TV when the output clips.

---

## The 15 effects

| Effect | Type | Highlights |
|---|---|---|
| Cassette Plug | tape / tube saturation | bias (even harmonics), wow, hiss, **low keep** |
| Menace | distortion | FUZZ / CLIP / FOLDBACK / RECTIFY, pre-HP, post-LP, **low keep** |
| Brainrot | bitcrusher / lo-fi | bits, rate, jitter, noise, crackle |
| Through The Wall | filter | LP12 / LP24 / HP / BP / NOTCH / COMB, tempo LFO, envelope |
| Tone Up | EQ | low-cut, 4 bands, tilt |
| Squeeze | compressor | soft knee, parallel mix |
| Overcooked | 3-band up/down compressor | depth, time, band gains |
| Slap | transient shaper | attack, sustain, output clip |
| Doubles | chorus | 1–4 voices, spread, **low keep** (mono bass) |
| Swirl | phaser | 2–12 stages, free or synced |
| Ad-Lib Throw | delay | synced, ping-pong, filtered feedback, ducking |
| Aura Room | reverb | ROOM / PLATE / HALL / DARK, ducking |
| Wide Body | stereo | width 0–200 %, Haas, mono-below |
| Chopped | gate | 16 patterns, tempo PUMP, noise gate |
| Red Line | clipper / limiter | ceiling, drive, SOFT ↔ HARD, lookahead |

**Low keep:** everything below the chosen frequency passes through untouched. This keeps 808s and bass clean.

---

## Keyboard shortcuts

| Key | Action |
|---|---|
| Space | KILL |
| ← → | previous / next preset |
| ↑ ↓ | next / previous channel |
| 1–8 | select a cassette |
| Ctrl+Z / Ctrl+Y | undo / redo |
| Esc | close the slot detail |

Knobs:
- Drag up / down to turn.
- Hold Shift while dragging for fine control.
- Double-click to reset.
- Use the mouse wheel to step.

## Settings (click the logo)

- **UI size:** 100 / 125 / 150 %.
- **Oversampling:** 1× / 2× / 4× for the saturation, distortion, crusher and clipper. 2× is the default.
- **OpenGL TV:** switch it off if the TV stays black on your machine.

## Latency

Effector Killa reports a **fixed** latency that depends only on oversampling. KILL and preset changes never
shift your timing. At 48 kHz this is about 1.5 ms at 1×, 6.7 ms at 2× and 7.8 ms at 4×.

## Troubleshooting

- **TV is black or frozen** → click the logo and switch off *OpenGL TV*.
- **Everything sounds thin or mono** → POCKET TV is on (the blinking button).
- **Preset is too loud or too quiet** → try AUTO TRACKING, or use RF OUT.
- **Presets folder** → click the logo and choose *Open user preset folder*.
