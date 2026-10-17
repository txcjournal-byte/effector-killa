# BEND DEMON 2.0: User Manual

*TrapWin + AUDIO · UNDERGROUND +*

BEND DEMON is an instrument you **draw** into. Each line you draw is **one note that can slide
anywhere**: up, down, hold, dive, wobble. It plays in time with FL Studio, with its own built-in
sounds or with your own synth.

---

## 1. Install

* **Windows**: run `TrapWinBendDemon-2.0.0-Windows-Setup.exe`. The VST3 goes to
  `C:\Program Files\Common Files\VST3`.
* **macOS**: run the `.pkg`. VST3 → `/Library/Audio/Plug-Ins/VST3`, Audio Unit →
  `/Library/Audio/Plug-Ins/Components`, the app → `/Applications`.

FL Studio: **Options → Manage plugins → Find installed plugins**. BEND DEMON is a **Generator**.

## 2. Hear it in 10 seconds

1. Channel Rack → **+** → *TrapWin BEND DEMON*.
2. It opens with a demo pattern (*Trap Glide Lead*) and a built-in sound.
3. Press **play** in FL Studio (or the **PLAY** button in the plugin). You hear the slides.
4. Route the channel to a mixer insert like any other instrument.

## 3. The screen

```
 top bar   brand | DRAW / LIVE | SOUND [Built-in · Glide Lead] | LOOP [4 bars] | PLAY | preset | settings
 tool bar  pencil line select eraser | Snap | Key Scale Lock | Magnet | undo redo | MORE | MIDI | lane
 canvas    piano keys on the left, bar numbers on top, your lines           | inspector: NOTE
 lane      pressure / brightness / velocity / release of the selected notes |            SHAPES / SOUND
 status    what the thing under the mouse does
```

Hover over anything: the **status line** at the bottom says in plain words what it does.

## 4. Drawing

| Tool | Key | What it does |
|---|---|---|
| **Pencil** | P or D | **Click** = a note on that row, one grid step long. **Drag** = draw a sliding note freehand. Hold **Shift** for whole semitones. **Right-click** a note = delete |
| **Line** | L | **Click-drag** = a straight slide from A to B. **Click again further right** = add a corner and keep going. **Esc** / **Enter** = finish. **Alt** = free pitch (between semitones) |
| **Select** | V or S | Click / **Shift-click** / drag a box. **Drag** = move (time on the grid, pitch in semitones; hold **Ctrl** for free). **Alt-drag** = copy. Drag the **ends** = make longer/shorter. Drag a **dot** (node) = reshape. **Double-click** a line = add a dot. Right-click a dot = remove it |
| **Eraser** | E | Click or sweep over notes |

* **Snap** = the time grid (1/1 … 1/32, triplets, off).
* **Key / Scale / Lock**: with **Lock** on, drawn notes and shapes land on your scale.
* **Magnet** = how hard hand-drawn lines are pulled onto the notes. Slides between notes stay
  smooth; only the places where the line rests are pulled.
* Keys: **Ctrl+Z / Ctrl+Shift+Z** undo / redo, **Ctrl+C / X / V / D** copy / cut / paste /
  duplicate, **Delete**, **Ctrl+A**, **Up / Down** = ±1 semitone (**Shift** = octave),
  **Left / Right** = nudge by the grid, **Q** = quantize starts, **M** = smooth out.
* The keyboard works after you click into the canvas and stops when the mouse leaves it.
  **The space bar always goes to FL Studio** (play / stop).
* Mouse wheel = scroll up/down, **Shift**+wheel = scroll in time, **Ctrl**+wheel = zoom in time,
  **Alt**+wheel = zoom pitch. Double-click the piano keys or the bar numbers = fit the view.
* **LOOP** (top bar) = pattern length, 1 to 64 bars. The pattern loops while FL plays.

## 5. Make it slide: the inspector

**NOTE** tab (select some notes first):

* **Make the selected notes…** one click each:
  * **Slide**: joins the selected notes into one voice that slides from note to note
  * **Chord Slide**: every voice of a chord slides to the nearest note of the next chord
  * **Vibrato**: adds a vibrato that fades in
  * **Dive**: the pitch falls at the end of the notes
  * **Rise**: the pitch climbs at the end of the notes
  * **Wobble**: slow pitch wobble in time with the beat
  * **Shake**: fast, small, nervous pitch shake
  * **Trill**: rapidly alternates with the next scale note
  
  Every action can be undone. How far / how fast each one goes is set in the **SHAPES** tab.
* **Velocity** / **Release**: how hard the notes are hit and let go.
* **Smooth / Straight**: soft curves or straight lines between the dots.
* **Pitch mod**: a per-note LFO on the pitch: shape (sine, triangle, square, saw), bipolar or
  only up (unipolar), amount, speed (in note values, follows the tempo), fade-in.
* **Transpose** −12 / −1 / +1 / +12.

**Lane** (under the canvas): draw **Pressure** (how loud / intense the note gets over time) or
**Brightness** (CC74, MPE "slide"/timbre) of the selected notes, or set **Velocity** / **Release**.
Nothing selected = it edits the notes under the mouse. Right-click resets. The thickness of each
line on the canvas shows its pressure.

## 6. Sound

**SOUND** (top bar or the SOUND tab): six built-in sounds made for slides: *Glide Lead, Pluck,
808, Pad, Bell, Sub Bass*. Or **Load plugin…** to play your own synth inside BEND DEMON
(**EDIT** opens its window, **USE** switches between it and the built-in sound).

* **Synths with MPE** (Serum 2, Vital, Pigments, Phase Plant, Surge XT, Equator …) slide every
  note on its own, chords included. They are listed first with an **MPE** badge. Turn MPE on in
  the synth and set its MPE pitch-bend range to **48** (the default in BEND DEMON).
* **Synths without MPE** (Serum 1 and most older synths): BEND DEMON switches to **mono slides**
  automatically and shows a notice. One note at a time slides (last note wins). Set the synth's
  pitch-bend range to the value in the settings menu (default 48; for synths that can't do 48,
  pick 24 or 12 in *settings → Pitch-bend range*).

## 7. Settings menu (gear, top right)

Mono slides on/off · Pitch-bend range (±2 … ±96) · Voices (how many notes can slide at once) ·
MIDI out (send everything to another FL channel instead of playing a synth) · Snap piano-roll
notes to the scale · Panic (stop all notes, resend the MPE setup) · Window size.

## 8. Presets

* **Patterns**: *Trap Glide Lead, 808 Slide Bassline, Chord Glide Pad, Vocal Vibrato Lead,
  Bell Bends, Pluck Runs, Sub Wobble, Dark Rise, Unison Melt*: a pattern + a sound + shape settings.
  Loading one replaces your pattern (undoable).
* **Gestures**: the 1.x presets. They only change the shape settings, never your pattern.
* **Save preset as…** stores everything including the pattern (`.bdpreset` in
  `%APPDATA%\TrapWin\BEND DEMON\Presets` or `~/Library/Application Support/TrapWin/BEND DEMON/Presets`).

## 9. Get the pattern out, or in

* **MIDI** button: **drag** it into FL Studio's playlist or a pattern to get a `.mid` file
  (MPE: every note on its own channel with its bends, pressure and CC74). Click = save as.
  **MORE → Save as MIDI file (mono pitch bend)** for non-MPE synths.
* **MORE → Open pattern or MIDI file…** imports a `.mid` (piano roll, MPE or pitch-bend MIDI)
  or a `.bdpattern`. **MORE → Save pattern…** writes a `.bdpattern`.

## 10. Capture: from the piano roll into lines

Write or play notes in FL's piano roll as in 1.x (LIVE mode, or DRAW mode: piano-roll notes always
play live through the shapes that are switched on in the SHAPES tab).

* **MORE → Capture from the piano roll → Record the next N bars**: starts at the next loop start,
  records one pass, and turns it into lines, including every slide and dive the shapes added.
* **… → Grab the last 1 / 2 / 4 / 8 / 16 bars**: takes what was just played.

Then mute or delete the piano-roll notes so they don't play twice, and refine the lines.

## 11. LIVE mode (the 1.x way)

**LIVE** (top bar): BEND DEMON turns your piano-roll notes into bending notes, like version 1:
touching / overlapping notes **slide**, notes **dive** or **rise** when they end, vibrato, wobble,
shake, trill. Switch each one on with **Use on piano-roll notes** in the **SHAPES** tab and set
how it behaves there. **Live feel** = loudness / brightness envelopes for those notes.
In DRAW mode the piano roll still plays live on a few channels, so you can jam over the pattern.

## 12. Troubleshooting

| Problem | Fix |
|---|---|
| No sound | Is the channel routed to a mixer insert? Is FL playing (or PLAY pressed)? Try a built-in sound. Press **settings → Panic** |
| Slides land on the wrong note | The synth's pitch-bend range ≠ BEND DEMON's (settings → Pitch-bend range, default 48) |
| All notes bend together | The synth has no MPE (or MPE is off in it). Turn MPE on, or use mono slides / a built-in sound |
| Space bar doesn't start FL | Move the mouse out of the canvas once (it gives the keyboard back), or click FL's window |
| Notes play twice after Capture | Mute or delete the piano-roll notes |
| "MISSING: …" in the sound slot | The project was saved with a synth that isn't installed here |

---
© TrapWin. VST is a registered trademark of Steinberg Media Technologies GmbH. Serum, Vital,
Pigments, Phase Plant and other product names are trademarks of their respective owners. BEND DEMON
is not affiliated with them.
