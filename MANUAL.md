# TRAPWIN EFFECTOR – User Manual

**Every effect. One win.** An 8-slot trap multi-effect: pick a channel and a preset, shape it with
5 macros, or hit **KILL** for a new chain that is musical and level-matched.

Formats: VST3 (Windows / macOS), AU (macOS), Standalone. Stereo or mono.
The whole chain lives in one rack: drag effects in, reorder them, and every control is right there.

---

## Install

**Windows:** run `TrapWinEffector-…-Setup.exe`. The plugin goes to `C:\Program Files\Common Files\VST3`.
**macOS:** VST3 to `~/Library/Audio/Plug-Ins/VST3`, AU to `~/Library/Audio/Plug-Ins/Components`.

Then rescan plugins in your DAW. In FL Studio: *Options → Manage plugins → Find plugins*.

---

## Quick start

1. Put TrapWin Effector on a mixer insert.
2. Click the **preset display** (or press **B**) to open the browser: pick a channel, a favourite or
   one of your presets, or type in the search box. Clicking a preset loads it right away.
3. Shape the sound with the 5 **macros**, or with any knob of any unit in the rack.
4. Drag effects from the **GEAR** list into the rack to build your own chain.
5. Press **KILL** (or the space bar) for a new chain.

Factory presets and KILL results are **level-matched to your input**, so louder never fools you.
The CH 12 FINAL BOSS bus / master presets are the exception: they keep their designed loudness.

---

## The window

**Top:** control panel (presets, tempo, levels, KILL). **Under it:** the macro strip and global switches.
**Left:** the rack - the signal flows from the top unit down. **Right:** the GEAR list.
The window can be resized: *SIZE / SETUP → Window size* (Auto, 80 - 150 %) or drag the bottom-right corner.

### Presets

| Control | What it does |
|---|---|
| **Display / BROWSE** | Opens the preset browser: all presets, ★ favourites, your presets, the 12 channels, and search. Click or use the arrow keys to load and listen, double-click / Enter loads and closes, Esc closes. |
| **◀ ▶** | Previous / next preset. |
| **SAVE** | Saves the whole rack as your preset (`Documents/TrapWin/TrapWin Effector/Presets`). Ctrl+S. |
| **★** | Adds the preset to your favourites. |
| **A / B** | Two complete rack states to compare. |
| **UNDO / REDO** | 64 steps (Ctrl+Z / Ctrl+Y). |

### Tempo

| Control | What it does |
|---|---|
| **HOST** | Follows your DAW tempo (default). |
| **MANUAL** | Uses its own tempo, also when your DAW is stopped. |
| **BPM display** | Drag up / down (Shift = fine), use the mouse wheel, or double-click and type. Changing it switches to MANUAL. |
| **TAP** | Tap twice or more to set the tempo (or press **T**). |

The tempo drives the synced delays, LFOs, gate patterns and pumping.

### Levels

| Control | What it does |
|---|---|
| **INPUT / OUTPUT** | Level into the first unit and out of the rack (±24 dB), with peak meters. |
| **RECYCLE** | Feeds part of the rack output back into its input: repeats, extra saturation, self-oscillation at high settings. Turn it up slowly with loud units. |
| **MIX** | Dry / wet of the whole rack. Use 100 % on a send / aux. |

### KILL

| Control | What it does |
|---|---|
| **KILL** | Generates a new chain. Follows musical rules, for example saturation comes before compression and reverb sits at the end. Locked units stay. |
| **PG / R / UNRATED** | KILL strength. **PG** keeps the units and tweaks their settings (±25 %), swapping at most one. **R** builds a new chain of 3–5 units. **UNRATED** builds a wilder chain of 4–7 units. |
| **BOOTLEG** | Subtle variation of the current chain: settings move ±10–20 % and the units stay. |

### Macros and switches

| Control | What it does |
|---|---|
| **VILLAIN ARC** | Darker: filters close and the reverb is damped. |
| **CRASH OUT** | Dirtier: more drive and crush, and harder clipping. |
| **AURA** | More space: reverb and delay go up, the image gets wider. |
| **DRIP** | More movement: LFO rate and depth, wow and chorus. |
| **KNOCK** | More punch: transients, compression and drive. |
| **AUTO GAIN** | Slow automatic gain (±12 dB). It keeps the output as loud as the input. |
| **PHONE CHECK** | Monitor as if on a phone speaker: mono, 300 Hz – 8 kHz. It **blinks** while on – switch it off before export! |
| **COMPARE** | Your original signal at the same loudness. Click to toggle, hold to listen only while held. |
| **GEAR** | Shows / hides the gear list (or press **G**). |

Every macro sits at the **middle (50) = the preset as designed**. Turn it left for less, right for more.
Macros respect the source. On an 808, for example, AURA never pushes the reverb above 15 %.

### The rack and its units

Up to **8 units**. Each unit shows **all its controls** with real values (Hz, dB, ms, note values).

| Control | What it does |
|---|---|
| **Grip (left edge)** | Drag the unit to a new position. Drag it onto the GEAR list to remove it. |
| **Unit preset ▾** | 8 ready-made settings of that effect (e.g. *Aura Room*: Small Room, Vocal Plate, Big Hall…), your saved unit presets, save, copy / paste. |
| **Knobs** | Drag up / down (Shift = fine), double-click = default, wheel = step. Hover for a tip. |
| **DRY/WET** | Blend of this unit only (many effects also have their own MIX). |
| **POWER** | Switches the unit off. It keeps running in the background, so delay and reverb tails are still there when you switch it back on. |
| **SOLO** | Hear only this unit (it blinks). |
| **ST / MID / SIDE** | Process the whole stereo signal, only the middle, or only the sides. |
| **LOCK** | KILL never touches a locked unit and it cannot be removed by mistake. |
| **✕** | Removes the unit; the rack closes the gap. |
| **•••** | Replace with another effect, reset, *listen up to here* (mutes the units below), copy / paste. |

### GEAR list: building a chain

- **Drag an effect onto the top or bottom edge of a unit** – it is inserted there and the rack moves down.
- **Drag it onto the middle of a unit** – it replaces that unit (yellow frame).
- **Drag it under the last unit** (or double-click it in the list) – it is added at the end.
- **Click the empty area** under the rack for a menu of all effects.
- The rack holds 8 units; when it is full, drop onto a unit to replace it.

### MOD SOURCES: modulation by drag & drop

The **MOD** unit sits under the macro strip. It has four modulation sources, each with its own
colour, a small display and a **patch jack**:

| Source | Controls | What it does |
|---|---|---|
| **LFO 1** (ice blue) / **LFO 2** (violet) | **SHAPE** (SINE, TRI, SAW, SQUARE, S&H), **RATE**, **PHASE**, **FREE / SYNC** | A repeating wave. FREE runs at 0.01–20 Hz; SYNC locks to the tempo (4 bars … 1/32, dotted and triplet values). PHASE shifts the start of the cycle (e.g. two LFOs 180° apart). |
| **ENV FOLLOW** (amber) | **ATTACK**, **RELEASE**, **SENS** | Follows the loudness of the plugin input. SENS boosts the input (0 to +36 dB) before the follower, so quiet sources still open it fully. |
| **STEP SEQ** (green) | 16 steps, **RATE** (length of one step), **GLIDE** | Click or drag in the display to draw the steps. GLIDE slides from one step to the next instead of jumping. |

**Patching:** grab a jack and drag it onto **any knob in the rack** (unit knobs, DRY/WET or a macro).
A coloured ring appears around the knob and shows the range the knob now moves through.
Drag the ring to change the depth – it works in both directions (+ / −). LFOs swing around the knob
value; ENV and SEQ push it from the knob value up (positive depth) or down (negative depth).
Right-click a knob or a jack to list the modulations and remove them. Up to **16 modulations** in total.

- The display LED and the green **ACTIVE** LED show that modulation is running.
- Synced LFOs and the sequencer follow the song position while your DAW plays, so they are in the
  same place on every playback. When the DAW is stopped they keep running at the current tempo.
- If your DAW automates a macro, modulation of that macro pauses while the automation moves.
- Modulation is saved with presets, A/B, undo and your project. A preset without modulation sounds
  exactly as before. A unit's modulations follow it when you move it and are removed with it;
  KILL keeps modulation only on units it did not replace.

### MACRO LEARN

Right-click any knob in the rack → **Assign to VILLAIN ARC / CRASH OUT / AURA / DRIP / KNOCK**, then set
the range: the value at macro **0** (min) and at macro **100** (max). At **50** the knob stays where you
set it. Learned assignments **add** to what the macro already does, so one macro can drive your own
selection of knobs across the rack. They are saved in the preset; right-click the knob again to remove.

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
| B | preset browser |
| G | show / hide the gear list |
| T | tap tempo |
| 1–8 | unit 1–8 on / off |
| Ctrl+Z / Ctrl+Y | undo / redo |
| Ctrl+S | save preset |
| Esc | close the browser |

## Third-party

Fonts: Archivo Black and IBM Plex Mono (SIL Open Font License 1.1), Permanent Marker (Apache License 2.0) –
licence texts are installed in the *Licenses* folder. Built with JUCE. VST is a trademark of Steinberg Media Technologies GmbH.

## Size / setup menu

- **Window size:** Auto (fits your screen), 80 – 150 %, or drag the bottom-right corner.
- **Show gear list.**
- **Oversampling:** 1× / 2× / 4× for the saturation, distortion, crusher and clipper. 2× is the default.
- **Open user preset folder.**

## Host automation

Every unit parameter is automatable. The names follow the rack, for example **S1: Cassette Plug: Drive**,
and the values are shown in real units. Each slot keeps its own fixed parameters, so automation you have
written never jumps to a different control when you move, add or replace units.

## Latency

TrapWin Effector reports a **fixed** latency that depends only on oversampling. KILL and preset changes never
shift your timing. At 48 kHz this is about 1.5 ms at 1×, 6.7 ms at 2× and 7.8 ms at 4×.

## Troubleshooting

- **Everything sounds thin or mono** → PHONE CHECK is on (the blinking button).
- **One unit does nothing** → it is powered off, or another unit is soloed (the rack shows *MUTED BY SOLO*).
- **Preset is too loud or too quiet** → try AUTO GAIN, or use OUTPUT.
- **It howls or keeps ringing** → turn RECYCLE down.
- **Presets folder** → SIZE / SETUP → *Open user preset folder*.
