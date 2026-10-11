# TrapWin Effector – brand style (overrides the "hardware rack" materials in DESIGN-RACK.md)

Decision 11. 10. 2026 (owner): the plugin must look like **trapwin.com**, and nobody may think it is a
copy of Effect Rack. Functions and workflow from DESIGN-RACK.md stay; the LOOK changes completely.

## Forbidden (reads as an Effect Rack / vintage-hardware copy)
- wood / walnut of any kind, rack ears, rack screws, rack rails, oval rack holes, brushed-metal
  "19-inch rack unit" faceplates, cream/orange/yellow vintage-lacquer panels, chicken-head knobs,
  analogue VU meters with warm-white backlit paper scales, a narrow right-hand "gear" column that
  looks like theirs.
- any Soundtoys name, logo, panel layout, font or colour scheme.

## TrapWin look (taken from trapwin.com CSS)
| Token | Value |
|---|---|
| background | `#04060A` (near black), with a very subtle deep-blue radial glow and sparse ✦ sparkles |
| card | `#0A0F18`, card-2 `#0B111C`, corner radius 20 px (16 px on small items), 1 px line `rgba(159,195,255,.18)` |
| text / text-2 / muted | `#E6ECF7` / `#A9B6CC` / `#8794AB` |
| accent / accent-light | `#7FA6E8` / `#9FC3FF` |
| glow | `rgba(70,118,196,.5)` – soft outer glow on hover/active, ice-blue |
| display font | **Archivo 900 italic**, uppercase, *chrome*: vertical gradient `#F4F6FB → #C9D0DD (48 %) → #8E98AB (52 %) → #E6EAF2` with a soft blue glow (like the "BUNDLES." headline) |
| labels / values | **IBM Plex Mono** 400/500, uppercase, letter-spacing +1 px |
| body | Assistant / Inter |
| buttons | pills (radius 999), primary = chrome gradient with dark text `#05070B`; secondary = outline 1 px line, text `#E6ECF7`; active = ice-blue fill/glow |
| badges | small pill, background `#04060A`, border `#E6ECF7`, IBM Plex Mono ("BEST VALUE · −45 %" style) |
| logo | official `Resources/Brand/trapwin_logo.png` only (chrome TRAPWIN + stars + AUDIO · UNDERGROUND) |

Fonts Archivo and IBM Plex Mono are SIL OFL – embed via BinaryData if not already (Resources/Fonts), keep the licence files.

## Layout (deliberately different from Effect Rack)
- No window "frame": the whole window is the dark background with sparkles.
- Top: slim **header bar** (logo left, preset pill with ◀ ▶ in the centre, A/B, undo/redo, ⚙, BYPASS pill right).
- **Left sidebar "FX LIBRARY"** (~240 px, collapsible): searchable list of effect **chips** (rounded, effect accent dot + chrome-free Plex Mono name + tiny live glyph); drag into the chain. Hidden library = narrower window.
- Centre: **"CHAIN"** – effect **cards** stacked top→bottom with a glowing ice-blue signal line on the left joining them (nodes at each card, animated pulse when audio flows). Split = the line forks into two columns A | B and merges again.
- Right: **"MASTER" column** (~220 px card): TEMPO (big Plex Mono digits with glow, TAP pill, HOST/FREE pills), IN/OUT/RECYCLE/MIX knobs, KILL (big pill with red glow `#FF2E3E`, rating PG · R · UNRATED pills, BOOTLEG), 5 macro knobs, slim **LED bar meters** (ice-blue → white → red), PHONE CHECK / AUTO GAIN pills, SIZE.
- MOD sources as a card strip under the header or at the top of the chain ("MOD" card) with glowing patch jacks; modulation cables are thin ice-blue glowing curves.

## Effect cards (replace hardware faceplates)
- Card `#0A0F18`, radius 20, 1 px line; a per-effect **accent colour** used only as: a 2 px gradient edge on the left, the knob value arcs, the live-screen tint and the hover glow.
- Title: effect name in Archivo 900 italic chrome (smaller than headline), category in Plex Mono muted ("SATURATION").
- The **live element** stays (reels, hazard → "drive heat" bar, bit columns, filter/EQ curves, GR meter, band bars, HIT flash, voice dots, spiral, delay time readout, starfield tail, vectorscope, 16-step dots, limiter bar) but drawn as a **glowing glass screen** (dark inset, thin line, accent-tinted vector graphics with glow) – no illustrated physical objects/materials.
- Knobs: flat dark disc with thin line, accent value arc with glow, small white pointer dot; label above + value below in Plex Mono.
- Side controls: small round icon pills (⚙, POWER, SOLO, LOCK) inside the card's top-right, not a separate rack strip.
- Accent colours: Cassette Plug `#E8B86B`, Menace `#FF4D5E`, Brainrot `#7CFC6B`, Through The Wall `#5B8CFF`, Tone Up `#C9D0DD`, Squeeze `#3DDC97`, Overcooked `#FF8A3D`, Slap `#FFD23F`, Doubles `#3FD0E0`, Swirl `#B07CFF`, Ad-Lib Throw `#9FC3FF`, Aura Room `#D6B8FF`, Wide Body `#5FE0B0`, Chopped `#8BE04E`, Red Line `#FF2E3E`.
