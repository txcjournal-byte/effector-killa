# EFFECTOR KILLA v1.0

**Every effect. One Killa.** – trap multi-efekt z řady Killa pluginů. Rack s 8 sloty (celý efektový řetězec
v jednom pluginu), 12 kanálů × 8 presetů, 5 maker, KILL 🎲 generátor řetězců a 80s VHS obývák jako UI.

- Formáty: **VST3 + Standalone** (AU na macOS), stereo in/out (mono taky), 44.1–192 kHz, libovolný buffer
- C++17, JUCE 8.0.4 (FetchContent), CMake, žádné další knihovny
- Grafika: JUCE + OpenGL (CRT televize), softwarový fallback

---

## Instalace (Windows / FL Studio)

1. Zkopíruj složku **`Effector Killa.vst3`** do
   **`C:\Program Files\Common Files\VST3`**.
2. FL Studio → *Options → Manage plugins* → **Find plugins** (nebo *Find more plugins*).
3. V mixeru na insert slotu vyber *Effector Killa* (kategorie Effects).

macOS: `~/Library/Audio/Plug-Ins/VST3` (VST3) a `~/Library/Audio/Plug-Ins/Components` (AU).
Uživatelské presety: `Dokumenty/Killa/Effector Killa/Presets` (JSON), slot presety v `.../Slot Presets`.

## Build

```bash
# Windows (Visual Studio 2022)
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release --parallel

# macOS / Linux
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Výstupy: `build/EffectorKilla_artefacts/Release/VST3/Effector Killa.vst3` a `.../Standalone/`.
Na Linuxu je potřeba `libasound2-dev libxrandr-dev libxinerama-dev libxcursor-dev libxcomposite-dev libgl1-mesa-dev libfreetype-dev libfontconfig1-dev`.

Volby CMake: `-DEK_BUILD_TESTS=ON` (výchozí) – unit testy, `-DEK_BUILD_TOOLS=ON` – nástroj na snímky UI,
`-DFETCHCONTENT_SOURCE_DIR_JUCE=/cesta/k/JUCE` – lokální JUCE místo stahování.

GitHub Actions (`.github/workflows/build.yml`) staví Windows / macOS / Linux, pouští testy a pluginval
a nahrává hotové VST3 jako artefakty.

## Prodejní balíček

- **Instalátor Windows:** CI vytváří `EffectorKilla-1.0.0-Windows-Setup.exe` (Inno Setup, `installer/EffectorKilla.iss`) –
  artefakt *EffectorKilla-Windows-Installer*. Instaluje VST3 do `C:\Program Files\Common Files\VST3`, Standalone,
  manuál a EULA. Instalátor zatím není podepsaný (Windows SmartScreen ukáže varování) – pro podpis je potřeba
  code-signing certifikát.
- **macOS podpis + notarizace:** v CI se spustí automaticky, jakmile v GitHubu (*Settings → Secrets and variables →
  Actions*) doplníš `MACOS_CERT_P12` (base64 .p12 „Developer ID Application“), `MACOS_CERT_PASSWORD`,
  `MACOS_SIGN_IDENTITY`, `APPLE_ID`, `APPLE_TEAM_ID`, `APPLE_APP_PASSWORD`. Výsledek: artefakt *EffectorKilla-macOS-signed*.
- **`EULA.txt`** – koncept licenční smlouvy (doplnit údaje, nechat zkontrolovat právníkem).
- **`MANUAL.md`** – uživatelský manuál (anglicky, pro zákazníky).
- Hlasitost: factory presety i KILL jsou srovnané na hlasitost vstupu; výjimkou jsou BUS/master presety (CH 12).

## Testy

```bash
build/EffectorKillaTests_artefacts/Release/EffectorKillaTests            # vše kromě benchmarku
build/EffectorKillaTests_artefacts/Release/EffectorKillaTests benchmark  # CPU
```

Pokrývají: stabilitu všech efektů (extrémní hodnoty, 44.1/48/96 kHz, buffery 1–4096, bez NaN/Inf), low keep
(pod crossoverem ±0.3 dB, harmonické < −40 dB), fixní latenci pro libovolný obsah racku, výměnu řetězce bez
lupanců, PAUSE/MIX/SCREENING/M-S, pravidla KILL pro **10 000 seedů × 6 zdrojů × 3 úrovně**, determinismus
seedu, BOOTLEG (nemění typy), WRITE PROTECT, hlasitost KILL (±1 dB), načtení všech 96 presetů, JSON a stav
DAW (uložit → načíst), undo/redo 35 kroků, A/B, uživatelské/slot presety, render všech presetů na 4 testovacích
signálech, AV2/AV3 modulaci a makra.

Naměřeno (Linux, 1 jádro): typický preset **~2 % jádra** (44.1 kHz / 256 / 2× OS), nejhorší ~4 %.
pluginval: **strictness 5 i 10 – SUCCESS**.

## Co kde je

```
Source/
  PluginProcessor.*        APVTS (fixní sada parametrů), stav, KILL/BOOTLEG, undo/redo, A/B, presety
  PluginEditor.*           škálování 100/125/150 %, klávesové zkratky, OpenGL
  engine/Rack.*            8 slotů, PAUSE, SCREENING, MIX, M/S, lock-free výměna řetězce, fixní latence
  engine/Effect.*          rozhraní efektu, ParamSpec (P1–P8), NonlinearHost (low keep + oversampling)
  engine/EffectRegistry    registry efektů (nový efekt = třída + 1 řádek)
  engine/effects/*         15 efektů (sekce 4)
  engine/Macro.*           makra (výchozí + presetová mapování, pravidla zdroje)
  engine/Modulation.*      AV2 SCRIBBLE / AV3 SCREENSAVER (synchronizace s transportem)
  engine/Randomizer.*      KILL (PG / R / UNRATED) + BOOTLEG, pravidla sekce 7
  engine/Preset.*          JSON, kanály, factory banka, uživatelské a slot presety, STAFF PICK
  engine/LoudnessMatch.*   AUTO TRACKING, odhad hlasitosti pro KILL
  engine/Engine.*          celý audio řetězec (ANTENNA IN → rack → BLEND → AUTO TRACKING → RF OUT → OFF AIR → POCKET TV)
  ui/cabinet/*             skříň: Layout.h (všechny pozice na jednom místě), knoby, kazety, video, tlačítka
  ui/tv/*                  TV: OpenGL (12 scén v GLSL + CRT), software fallback, OSD, AV1–AV3, detail kazety
Resources/Presets/factory.json   96 presetů – generuje Tools/make_factory.py (reálné hodnoty podle sekce 11)
Tools/                     make_factory.py, Snapshot.cpp (render UI do PNG)
Tests/                     unit testy + benchmark
```

## Podklady (placeholdery)

- **Pozadí skříně:** dokud nedodáš `Resources/Design/cabinet.png`, používá se `reference.png` jako pozadí
  a všechny interaktivní prvky se kreslí přes něj. Po vložení `cabinet.png` (1630 × 965 nebo 2× – pozice
  jsou v `ui/cabinet/Layout.h`) a znovu spuštění CMake se použije automaticky.
- **Fonty:** vlož `Resources/Fonts/osd.ttf` (VCR OSD), `vfd.ttf` (displej videa), `hand.ttf` (štítky) –
  po přegenerování CMake se načtou samy. Do té doby vestavěné systémové fonty.

## Latence

Plugin hlásí hostu **fixní latenci** podle oversamplingu, nezávisle na obsahu racku (KILL ani preset nemění
kompenzaci): 8 × latence oversamplingu + 1,5 ms lookahead Red Line. Při 48 kHz: 1× = 72, 2× = 320,
4× = 376 vzorků. Oversampling se přepíná v menu (klik na logo).

---

## Testovací postup ve FL Studiu

**Příprava:** projekt 140 BPM, na kanál mixeru dej loop (melodii / vokál / 808). Otevři Effector Killa.

1. **Načtení** – plugin se otevře na *CH 10 DRIFT – Night Drive*, TV ukazuje noční dálnici, na videu
   zelený nápis NIGHT DRIVE. Zvětšení: klik na logo → *UI size* 125 % / 150 %.
2. **Kanály a presety** – klikni na čísla 01–12 kolem voliče (nebo ho otoč), PROGRAM ▲▼, šipky ← → ↑ ↓.
   Při přehrávání nesmí nic lupnout (krátký šum na TV je záměr). Klik na displej videa = seznam presetů.
3. **KILL** – stiskni KILL (nebo mezerník) několikrát při přehrávání: TV se roztrhá → NO SIGNAL → NOW PLAYING
   se seznamem kazet. Hlasitost by měla zůstat zhruba stejná. Vyzkoušej PG / R / UNRATED a BOOTLEG.
4. **Undo / A-B** – REWIND / FAST FWD (Ctrl+Z / Ctrl+Y) vrací kroky; SIDE A / SIDE B přepíná dva stavy.
5. **Kazety** – klik na kazetu = vysune se a na TV se otevře detail (PREMIUM CABLE). Tahej lišty vlevo/vpravo,
   Shift = jemně, dvojklik = výchozí, kolečko = krok. PAUSE / SCREEN / M/S / LOCK v detailu. Esc zavře.
   Přetáhni kazetu na jiné místo (pořadí). Pravý klik = menu (změna efektu, kopírovat/vložit, EJECT,
   slot preset, WRITE PROTECT…). Zamčená kazeta dostane červený štítek a KILL ji nemění.
6. **Makra** – točení knobů VILLAIN ARC / CRASH OUT / AURA / DRIP / KNOCK: na TV se ukáže pruh
   `CRASH OUT ▮▮▮▮▮▮▯▯ 72`, obraz reaguje (tmavne, glitchuje, září, vlní se, bliká).
7. **AV režimy** – přepínač AV1/AV2/AV3 pod TV.
   - AV1 REMOTE: táhni bod po obrazovce (X = CRASH OUT, Y = AURA).
   - AV2 SCRIBBLE: nakresli smyčku, spusť přehrávání – bod po ní jede v tempu (1/2/4 takty – klik na pravítko),
     při stopce stojí. Pravý klik smaže. Cíle os změníš klikem na popisek „X …“ / „Y …“.
   - AV3 SCREENSAVER: logo se odráží; když trefí roh, přijde glitch (CRASH OUT na 1 dobu).
   - Zautomatizuj makro ve FL (automation clip) → AV modulace tohoto makra se odpojí.
8. **OFF AIR** – klik: TV se smrskne do tečky, LED svítí, slyšíš originál srovnaný na hlasitost. Podržení =
   originál jen po dobu držení.
9. **POCKET TV** – zvuk „jako z mobilu“, tlačítko bliká; **AUTO TRACKING** – pomalu dorovná hlasitost.
10. **REC** – uloží uživatelský preset (objeví se v seznamu videa → USER TAPES), ⭐ STAFF PICK přidá do oblíbených.
11. **Automatizace** – ve FL *Browse parameters*: Slot 1–8 P1–P8/Mix/Pause, makra, Antenna In, RF Out, Blend,
    Auto Tracking, Pocket TV, Off Air. Automatizace slotu přežije KILL i změnu presetu.
12. **Uložení projektu** – ulož, zavři FL, otevři znovu: stejný preset, kazety, makra, AV kresba, A/B.
13. **Latence** – v mixeru FL (*Plugin Delay Compensation*) se latence při KILL/presetu nemění.
14. **OpenGL** – pokud by TV byla černá / zamrzlá, klik na logo → vypni *OpenGL TV* (software fallback).

## Roadmapa

v1.1: Heaven Mode, Alter Ego, Big Back, Jet Lag, NPC Glitch, Shh, Ice, Pump Fake (sidechain), Toolbox – presety
označené `v1.1: …` v `tags` se pak upraví. v1.2: SHAPESHIFT, VERSE/HOOK, KILL na MIDI notu, MIDI learn,
RENTAL CODE, pásmo na slot. Architektura s tím počítá (registry efektů, verze stavu/presetů, rezervovaná ID).
