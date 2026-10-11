# TRAPWIN EFFECTOR v1.0

**Every effect. One win.** – trap multi-efekt z řady Killa pluginů. Rack s 8 sloty (celý efektový řetězec
v jednom pluginu), 12 kanálů × 8 presetů, 5 maker, KILL 🎲 generátor řetězců a 80s VHS obývák jako UI.

- Formáty: **VST3 + Standalone** (AU na macOS), stereo in/out (mono taky), 44.1–192 kHz, libovolný buffer
- C++17, JUCE 8.0.4 (FetchContent), CMake, žádné další knihovny
- Grafika: JUCE + OpenGL (CRT televize), softwarový fallback

---

## Instalace (Windows / FL Studio)

1. Zkopíruj složku **`TrapWin Effector.vst3`** do
   **`C:\Program Files\Common Files\VST3`**.
2. FL Studio → *Options → Manage plugins* → **Find plugins** (nebo *Find more plugins*).
3. V mixeru na insert slotu vyber *TrapWin Effector* (kategorie Effects).

macOS: `~/Library/Audio/Plug-Ins/VST3` (VST3) a `~/Library/Audio/Plug-Ins/Components` (AU).
Uživatelské presety: `Dokumenty/Killa/TrapWin Effector/Presets` (JSON), slot presety v `.../Slot Presets`.

## Build

```bash
# Windows (Visual Studio 2022)
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release --parallel

# macOS / Linux
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Výstupy: `build/EffectorKilla_artefacts/Release/VST3/TrapWin Effector.vst3` a `.../Standalone/`.
Na Linuxu je potřeba `libasound2-dev libxrandr-dev libxinerama-dev libxcursor-dev libxcomposite-dev libgl1-mesa-dev libfreetype-dev libfontconfig1-dev`.

Volby CMake: `-DEK_BUILD_TESTS=ON` (výchozí) – unit testy, `-DEK_BUILD_TOOLS=ON` – nástroj na snímky UI,
`-DFETCHCONTENT_SOURCE_DIR_JUCE=/cesta/k/JUCE` – lokální JUCE místo stahování.

GitHub Actions (`.github/workflows/build.yml`) staví Windows / macOS / Linux, pouští testy a pluginval
a nahrává hotové VST3 jako artefakty.

## Prodejní balíček

- **Instalátor Windows:** CI vytváří `TrapWinEffector-1.0.0-Windows-Setup.exe` (Inno Setup, `installer/EffectorKilla.iss`) –
  artefakt *TrapWinEffector-Windows-Installer*. Instaluje VST3 do `C:\Program Files\Common Files\VST3`, Standalone,
  manuál a EULA. Instalátor zatím není podepsaný (Windows SmartScreen ukáže varování) – pro podpis je potřeba
  code-signing certifikát.
- **macOS podpis + notarizace:** v CI se spustí automaticky, jakmile v GitHubu (*Settings → Secrets and variables →
  Actions*) doplníš `MACOS_CERT_P12` (base64 .p12 „Developer ID Application“), `MACOS_CERT_PASSWORD`,
  `MACOS_SIGN_IDENTITY`, `APPLE_ID`, `APPLE_TEAM_ID`, `APPLE_APP_PASSWORD`. Výsledek: artefakt *TrapWinEffector-macOS-signed*.
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
  engine/Engine.*          celý audio řetězec (INPUT → rack + RECYCLE → MIX → AUTO GAIN → OUTPUT → COMPARE → PHONE CHECK)
  ui/Brand.*               oficiální logo TrapWin (Resources/Brand/trapwin_logo.png) + „EFFECTOR“
  ui/Controls.*, Theme.*   knoby, tlačítka, paleta a fonty
  ui/rack/ControlPanel.*   horní panel: presety, tempo (HOST/MANUAL/TAP), úrovně, KILL, makra, přepínače
  ui/rack/RackView.*       rack: jednotky pod sebou se všemi knoby, přetahování (vložit/nahradit/přesunout)
  ui/rack/GearList.*       seznam efektů na přetahování (zpět = odebrat)
  ui/rack/PresetBrowser.*  prohlížeč presetů s kategoriemi, oblíbenými a hledáním
Resources/Presets/factory.json   96 presetů – generuje Tools/make_factory.py (reálné hodnoty podle sekce 11)
Tools/                     make_factory.py, Snapshot.cpp (render UI do PNG)
Tests/                     unit testy + benchmark
```

## Podklady

- **Logo:** `Resources/Brand/trapwin_logo.png` je oficiální logo (hvězdičky + AUDIO · UNDERGROUND), stejné jako
  v ostatních TrapWin pluginech. Nikdy ho nepřekreslovat textem.
- **Fonty:** `Resources/Fonts/*.ttf` se načtou samy při přegenerování CMake.

## Latence

Plugin hlásí hostu **fixní latenci** podle oversamplingu, nezávisle na obsahu racku (KILL ani preset nemění
kompenzaci): 8 × latence oversamplingu + 1,5 ms lookahead Red Line. Při 48 kHz: 1× = 72, 2× = 320,
4× = 376 vzorků. Oversampling se přepíná v menu SIZE / SETUP.

---

## Testovací postup ve FL Studiu

**Příprava:** projekt 140 BPM, na kanál mixeru dej loop (melodii / vokál / 808). Otevři TrapWin Effector.

1. **Okno** – nahoře ovládací panel, pod ním makra, vlevo rack, vpravo GEAR. Velikost: SIZE / SETUP →
   Auto / 80–150 %, nebo táhni pravý dolní roh (poměr stran drží). Velikost se uloží s projektem.
2. **Presety** – klik na displej presetu (nebo B) = prohlížeč: kategorie vlevo, hledání nahoře, klik = načte
   a hraje, šipky ↑↓ v seznamu = procházení poslechem, dvojklik / Enter = načte a zavře. ◀ ▶, ★, A / B,
   UNDO / REDO, SAVE (Ctrl+S). Při přepínání nesmí nic lupnout.
3. **Stavba řetězce** – přetáhni efekt z GEAR na horní/dolní okraj jednotky (modrá čára = vložit),
   doprostřed jednotky (žlutý rámeček = nahradit), pod poslední jednotku (přidat na konec). Dvojklik v GEAR =
   přidat na konec. Klik do prázdné plochy pod rackem = menu efektů.
4. **Jednotky** – táhni za levý okraj (úchyt s tečkami) = přesun; přetažení zpět na GEAR = odebrat. Knoby:
   tah nahoru/dolů, Shift = jemně, dvojklik = výchozí, kolečko. Preset jednotky ▾, POWER (vypni delay během
   ozvěn a zapni – ozvěny musí pokračovat), SOLO (bliká, ostatní „MUTED BY SOLO“), ST/MID/SIDE, LOCK, ✕, •••.
5. **Tempo** – HOST sleduje FL. MANUAL: táhni BPM, kolečko, dvojklik a napiš číslo, nebo TAP (klávesa T).
   Synchronizované delaye / LFO musí jet i při zastaveném FL.
6. **RECYCLE** – pomalu zvyšuj s delayem nebo reverbem: opakování a sytost, na maximu může houkat,
   ale nesmí se utrhnout do nekonečna.
7. **KILL** – několikrát při přehrávání (nebo mezerník); hlasitost zhruba stejná. PG / R / UNRATED, BOOTLEG.
   Zamčené jednotky (LOCK) zůstanou.
8. **Makra** – VILLAIN ARC / CRASH OUT / AURA / DRIP / KNOCK; knoby jednotek se mají hýbat s makry.
9. **COMPARE** – klik = originál srovnaný na hlasitost, podržení = jen po dobu držení. **PHONE CHECK** bliká.
   **AUTO GAIN** pomalu dorovná hlasitost.
10. **Automatizace** – ve FL *Browse parameters*: názvy podle racku („S1: Cassette Plug: Drive“) s reálnými
    hodnotami. Po přidání/přesunu jednotky se názvy aktualizují; nahraná automatizace patří pořád stejnému slotu.
11. **Uložení projektu** – ulož, zavři FL, otevři znovu: stejný rack, presety jednotek, makra, A/B, velikost okna.
12. **Latence** – v mixeru FL (*Plugin Delay Compensation*) se latence při KILL/presetu nemění.

## Roadmapa

v1.1: Heaven Mode, Alter Ego, Big Back, Jet Lag, NPC Glitch, Shh, Ice, Pump Fake (sidechain), Toolbox – presety
označené `v1.1: …` v `tags` se pak upraví. v1.2: SHAPESHIFT, VERSE/HOOK, KILL na MIDI notu, MIDI learn,
RENTAL CODE, pásmo na slot. Architektura s tím počítá (registry efektů, verze stavu/presetů, rezervovaná ID).
