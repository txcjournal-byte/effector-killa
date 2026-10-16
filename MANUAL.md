# BEND DEMON: User Manual

*TrapWin + AUDIO · UNDERGROUND +*

BEND DEMON turns plain piano-roll melodies into expressive, bending performances. It plays
**your** synth (Serum 2, Vital, Pigments, Phase Plant …) from inside itself and talks to it in
**MPE**, so every note gets its own pitch bend, pressure and timbre. You write normal notes, and
BEND DEMON adds the slides, dives and vibrato.

---

## 1. Install

* **Windows**: run `BEND-DEMON-x.y.z-win64-setup.exe`. The VST3 goes to
  `C:\Program Files\Common Files\VST3`.
* **macOS**: run `BEND-DEMON-x.y.z-mac.pkg`. VST3 goes to `/Library/Audio/Plug-Ins/VST3`, the
  Audio Unit to `/Library/Audio/Plug-Ins/Components`, the app to `/Applications`.

Then in FL Studio: **Options → Manage plugins → Find plugins** (enable *Verify plugins*).
BEND DEMON appears under **Generators** (it is an instrument).

## 2. Set it up in FL Studio

**New channel:** Channel Rack → **+** → *BEND DEMON*.

**Replace an existing synth channel:** right-click the channel's button in the Channel Rack
(for example your Serum channel) → **Replace** → *BEND DEMON*. The notes in that channel's
patterns stay. Then load the same synth *inside* BEND DEMON (step 3).

> Route the BEND DEMON channel to a mixer insert like any other instrument. The hosted synth's
> audio comes out of BEND DEMON.

## 3. Load your synth

1. Click **+ LOAD SYNTH** in the top bar. The first time, BEND DEMON scans your VST3 (and, on Mac,
   AU) folders. The results are cached, so later it opens instantly. Use **SCAN** to find new
   plugins and **FULL RESCAN** to start over.
2. Type to search, then double-click a synth (or press Enter).
   A **lime dot** marks synths known to support MPE.
3. Click **EDIT** to open the synth's own window and choose a patch.
4. **Turn on MPE inside the synth**, and set its pitch-bend range to **±48** (BEND DEMON's
   default; you can change it under *Bend Range*, but both must match):

| Synth | Where |
|---|---|
| **Serum 2** | Global/Settings → *MPE* on. Set the MPE pitch-bend range to 48 |
| **Vital** | Click the *MPE* toggle (top-right of the keyboard area / settings). Bend range is set per patch (pitch bend range ±48) |
| **Pigments** | *Settings* (gear) → *MPE* → enable, bend range 48 |
| **Phase Plant** | Settings → MIDI → *MPE* enabled, bend range 48 |
| **Serum 1 / non-MPE synths** | No MPE: every bend applies to **all** notes at once (whole-chord bends). Glides and dives on single-note lines still work. Set the synth's bend range to match BEND DEMON's *Bend Range* |

BEND DEMON sends the MPE setup message (RPN 6 + bend range) automatically when the synth loads.
If a synth ignores it, press the **!** (panic) button: it releases everything and resends the setup.

The synth, its patch and all BEND DEMON settings are saved in your FL project.

## 4. Write a melody

Write notes in the piano roll as usual:

* **Glide (legato slide):** make notes **touch or overlap**. The next note starts where the
  previous one ends, or before it. Small gaps up to *Glide Window* (default 60 ms) also count.
  The first note keeps sounding and slides into the new pitch instead of retriggering.
* **808 slides:** write a long 808 note and overlap a second note → it glides. With **808 DIVE**
  on, every note bends down when you release it.
* **Chord melts:** turn on **CHORD GLIDE** and write chords that touch. Each voice slides to the
  nearest note of the next chord.
* A **gap** larger than the glide window starts a fresh note.
* Very large jumps (more than the bend range, ±48 by default) retrigger instead of gliding.

## 5. The gestures

Click a **pad** to switch the gesture on or off. Its controls show up below. Right-click a pad to
show its controls without toggling it, or use the tabs **GLIDE / VIBRATO / DIVE / MOTION / EXPRESSION**.

| Pad | What it does | Main controls |
|---|---|---|
| **GLIDE** | Legato slides between touching/overlapping notes | Glide Time, Glide Window, Curve (Linear / Ease / S-Curve) |
| **CHORD GLIDE** | Chord-to-chord voice slides (nearest pitch) | Chord Window (how close in time notes must start to count as one chord) |
| **VIBRATO** | Vibrato fades in on held notes | Rate (Hz or Sync + division), Depth (cents), Delay |
| **808 DIVE** | Each note dives down when released | Dive Amount (semitones), Dive Time (ms or Sync), Curve |
| **RISE** | Same as dive but upwards | uses the Dive controls |
| **WOBBLE** | Slow, wide, tempo-synced pitch LFO | Wobble depth (semitones), rate |
| **SHAKE** | Fast, small, random jitter | Shake depth (cents), rate |
| **TRILL** | Alternates with the next scale note | Trill rate (synced) |

**EXPRESSION** tab: **Pressure** (MPE channel pressure: loudness/brightness in most patches) and
**Timbre** (CC74, the MPE "slide" dimension). Each has an amount, a time, a shape
(*Attack*: quick rise; *Swell*: slow fade-in; *Decay*: starts high, falls) and a velocity
amount. Map pressure/timbre to something in your synth patch to hear them.

**SCALE LOCK:** pick the **Key** and **Scale** (Major, Minor, Harmonic Minor, Phrygian, Dorian,
Major/Minor Pentatonic, Chromatic). Dive/rise targets and trills then land on scale notes.
**Quantize In** also snaps the notes you write to the scale.

**Voices** = how many MPE channels/notes can sound at once (1–15). **Bend Range** must match the
synth. **Output** = output gain.

### Presets

The preset box (top right) holds **gesture presets**. They change only BEND DEMON's gestures,
never the synth patch, so you can audition *808 Dive*, *Liquid Lead*, *Chord Melt*, *Vibrato Soul*,
*Trill Hook*, *Wobble Bass*, *Shaky Pluck* and *Phrygian Rise* against any sound.
**Save preset as…** stores your own (`.bdpreset` files in
`%APPDATA%\TrapWin\BEND DEMON\Presets` or `~/Library/Application Support/TrapWin/BEND DEMON/Presets`).

## 6. The Voice Field

The big display shows the last few bars passing through BEND DEMON:

* each **ribbon** is one voice. **Thickness** = pressure, **colour** = timbre + time
* the **bright core line** is the real pitch, including every bend
* **lime nodes** = note starts, **amber nodes** = glide points, **violet rings** = key releases
  (where dives start)
* the **light beam** is the playhead, synced to FL's song position while playing
* click the field to show **1 / 2 / 4 / 8 bars**. **DIMENSION** tilts it into 3D, with one depth layer per voice

## 7. Render / export

Nothing special: BEND DEMON is a normal instrument, so **File → Export** renders it like any synth.
For best results, render with the same buffer settings you mix with. BEND DEMON reports its
latency to FL (only non-zero while CHORD GLIDE is on), and FL compensates it.

## 8. Alternative: MIDI-out routing

If you want the hosted synth on its own FL channel (for example to use FL's own plugin wrapper
features), turn on **MIDI Out** in BEND DEMON:

1. In BEND DEMON's FL wrapper settings (the gear icon of the plugin window), set **MIDI → Output
   port** to a free number, e.g. **10**.
2. Add your MPE synth as a normal channel and set its **MIDI → Input port** to the same number (10).
3. Enable MPE in that synth as in step 3.
4. BEND DEMON now sends the generated MPE to that synth instead of playing a hosted synth, and
   makes no audio itself.

> Check that your FL version passes all 16 MIDI channels between ports. If the synth bends all
> notes together, it is getting merged channels: use the hosted mode instead.

## 9. Troubleshooting

| Problem | Fix |
|---|---|
| No sound | Synth loaded? Is the BEND DEMON channel routed to a mixer insert? Open **EDIT** and check the synth gets notes. Press **!** |
| All notes bend together | MPE is off in the synth, or the synth has no MPE (Serum 1) |
| Glides go to the wrong pitch | Synth bend range ≠ BEND DEMON *Bend Range* (default 48) |
| Notes don't glide | Notes must touch/overlap (or gap < Glide Window), and GLIDE must be on. Jumps larger than the bend range retrigger |
| Stuck notes | Press **!** (panic). Stopping the transport also releases everything |
| Synth shows "MISSING: …" | The project was saved with a synth that isn't installed on this computer |
| Scan crashed | Restart FL. The plugin that crashed is skipped on the next scan |

---
© TrapWin. VST is a registered trademark of Steinberg Media Technologies GmbH. Serum, Vital,
Pigments and Phase Plant are trademarks of their respective owners. BEND DEMON is not affiliated
with them.
