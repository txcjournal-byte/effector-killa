# TrapWin Effector – zadání nového vzhledu „Hardware Rack“

> **11. 10. 2026 – PŘEDNOST MÁ `Docs/TRAPWIN-STYLE.md`:** vzhled podle trapwin.com, žádné dřevo, rack ucha, šrouby ani vintage panely. Funkce a ovládání z tohoto zadání platí dál.

Cíl: **lepší Effector než Effect Rack** – vzhled a ovládání ve stylu Effect Rack (Soundtoys) – stejné rozložení, stejný způsob práce,
stejně „živý“ hardwarový dojem – ale **vlastní grafika TrapWin**. Nic nudného: každý efekt vypadá
jako jiný kus hardwaru, svítí LEDky, hýbou se metry.

## 0. Pravidla (povinné)

- **Nekopírovat jejich grafiku ani značku:** žádné logo/nápis Soundtoys, žádné jejich názvy efektů
  (EchoBoy, Decapitator, Devil-Loc, PrimalTap, SuperPlate…), žádné obkreslené panely, fonty ani
  jejich rozmístění knobů na konkrétních efektech. Převzít jen **rozložení okna, princip ovládání
  a žánr „rack s hardwarovými panely“**. Panely efektů navrhnout vlastní.
- Logo: vždy oficiální `Resources/Brand/trapwin_logo.png` (hvězdičky + AUDIO · UNDERGROUND),
  nikdy ho nepřekreslovat textem. Kreslí se přes `ui/Brand.h` (`brand::logo()`, `paintLogoBlock`).
- **Existující audio jádro a ID parametrů neměnit** (automatizace uživatelů musí dál fungovat).
  Nové funkce ze sekce 12 se do jádra **přidávají** (nové parametry jen na konec, s novými ID),
  každá s unit testy (stabilita, žádné NaN, real-time safety, bez lupanců, uložení/načtení stavu).
- Nesahat na CI podepisování (`.github/workflows/build.yml`, sekce macOS) – to řeší jiný chat.
- Před každým pushem `git pull --rebase` (větev `claude/exciting-knuth-lxevz2`).
- Po dokončení: unit testy zelené, CI zelené, snímky UI (`Tools/Snapshot.cpp`), nový obrázek do obchodu
  (`Tools/make_cover.py`), aktualizovat `MANUAL.md`, `README.md`, `HANDOFF.md`.

## 1. Co už v repu je (stav k 11. 10. 2026)

Rack UI (funkčně hotové, vizuálně se předělává):

| Soubor | Co dělá |
|---|---|
| `Source/PluginEditor.*` | okno, škálování (Auto / % / tažení rohu), klávesy, layout |
| `Source/ui/rack/ControlPanel.*` | horní panel: presety, tempo, úrovně, KILL, makra, přepínače |
| `Source/ui/rack/RackView.*` | rack jednotek + drag & drop (vložit / nahradit / přidat / přesun) |
| `Source/ui/rack/GearList.*` | seznam efektů na přetahování (zpět = odebrat) |
| `Source/ui/rack/PresetBrowser.*` | prohlížeč presetů s kategoriemi, oblíbenými a hledáním |
| `Source/ui/rack/Widgets.*` | `TwButton`, `LabeledKnob`, `PeakMeter`, `drawCard` |
| `Source/ui/Controls.*`, `Theme.*` | knob (`Knob`), paleta, fonty, `drawKnob`, `drawLed`, `drawGlowText`… |

Processor API (vše hotové): `insertEffect(at,type)`, `removeEffect`, `moveSlot`, `playTape`,
`setSlotType`, `setSlotParam(slot, 0-7 / 8 = mix / 9 = pause)`, `setSolo`, `setScreening`,
`setSlotMs`, `setWriteProtect`, `copySlot/pasteSlot`, `save/loadSlotPreset`, `tapTempo()`,
`getEffectiveBpm()`, `kill()`, `bootleg()`, `killLevel`, `undo/redo`, `setSide`, `stepPreset`,
`loadFactory`, `saveUserPreset`, `toggleFavourite`, `uiScale` (0 = auto), `showGear`.
Parametry: `ParamIDs::inGain, outGain, blend (MIX), recycle, tempoMode (0 HOST / 1 MANUAL), tempoBpm,
autoTracking, pocketTv, offAir`, makra `ParamIDs::macro(m)`, sloty `slotParam/slotMix/slotPause`.
Metry: `engine.meters.inLevel, outPeak, outLevel, macroValue[m], bpm`.

Build v cloudu bez X11 hlaviček: `CXXFLAGS="-DJUCE_USE_XRANDR=0 -DJUCE_USE_XINERAMA=0 -DJUCE_USE_XCURSOR=0
-DJUCE_USE_XSHM=0 -DJUCE_USE_XRENDER=0"`, `-DEK_BUILD_TOOLS=ON`, snapshot běží bez displeje.
Na Windows PC normálně přes CMake + Visual Studio.

## 2. Okno – rozložení (základní velikost 1280 × 900, s Gear; bez Gear 980 × 900)

```
┌──────────────────────────────────────────────────────────────────────────────────────┐
│ TOOLBAR  ◀ ▶ [ NIGHT DRIVE ▾ ] 💾 ↺ ★  A B  ↶ ↷              ⚙  i  [ BYPASS ]        │ 40 px
├──┬──────────────────────────────────────────────────────────┬──┬────────────────┬──┤
│  │ CONTROL UNIT (logo, TEMPO, LEVELS, SIZE, TOOLBOX)         │  │  GEAR          │  │
│ O│──────────────────────────────────────────────────────────│ O│  miniatury     │ O│
│ ř│ PERFORMER UNIT (KILL, rating, BOOTLEG, 5 maker, VU)       │ ř│  panelů        │ ř│
│ e│──────────────────────────────────────────────────────────│ e│  (scroll)      │ e│
│ c│ [S1 panel efektu ...........................] ⚙ ⏻ S 🔒    │ c│                │ c│
│ h│ [S2 panel efektu ...........................] ⚙ ⏻ S 🔒    │ h│                │ h│
│  │ ...                                         (scroll)     │  │                │  │
│  │ [ DROP EFFECTS HERE ]                                    │  │                │  │
├──┴──────────────────────────────────────────────────────────┴──┴────────────────┴──┤
│ ořech (spodní lišta)                                                                  │
└──────────────────────────────────────────────────────────────────────────────────────┘
```

Souřadnice (základ 1280 × 900, škáluje se transformací jako teď):

| Oblast | Rect (x, y, w, h) |
|---|---|
| Toolbar | 0, 0, W, 40 |
| Ořechový rám | levá bočnice 0–20, pravá W-20–W, horní lišta y 40–58, spodní y 882–900, přepážka 960–980 (jen s Gear) |
| Vnitřek racku (tmavá šachta) | 20, 58, 940, 824 – barva `#07090d`, svislé „rack rails“ s otvory po stranách |
| Control unit | 24, 62, 932, 168 |
| Performer unit | 24, 234, 932, 104 |
| Rack (Viewport, svislý scroll) | 20, 342, 940, 540 |
| Gear | 980, 58, 280, 824 |

**Ořech:** tmavý ořech (`#3b2414` → `#5a3a22`), jemná léta dřeva (procedurálně: sinusové pruhy + šum),
tmavé hrany, lehký lesk nahoře. Byl už v původním zadání (CLAUDE.md), není to kopie.
Show Gear vypne Gear **a zúží okno na 980** (jako u originálu).

## 3. Toolbar (horní lišta 40 px)

Tmavě šedý kov `#1b1d22` → `#121317`, spodní linka.

Zleva: `◀` `▶` (čtvercová kovová tlačítka 28 × 26) · **LCD presetu** 300 × 26 (černé sklo, text ledově
modrý VFD `#9fdcff` s glow, `★` když je oblíbený; klik = prohlížeč presetů; při změně presetu krátký „scan“
efekt) · `💾` uložit · `↺` vrátit preset (znovu načte aktuální factory/user preset) · `★` · `A` `B` · `↶` `↷`.
Vpravo: `⚙` (menu: velikost okna, oversampling, složka presetů, klávesy) · `i` (O pluginu + otevřít manuál)
· **BYPASS** (= COMPARE: klik přepne, podržení = jen při držení; když je zapnutý, svítí červeně).
Všechna tlačítka: ikonky kreslené vektorově, tooltip.

## 4. Control unit (168 px, rackový panel)

Materiál: černý kartáčovaný hliník `#16181d` (jemné vodorovné linky), 4 šrouby v rozích (křížové,
chromové), tenké „ucho“ racku vlevo a vpravo s oválnými otvory.

- **Vlevo nahoře:** oficiální logo TrapWin (výška ~44 px).
- **Vpravo nahoře:** „EFFECTOR“ chromovým písmem (`drawChromeText`) + malé „RACK“ ledově modře.
- Sekce mají **hlavičku = světlý pruh s nápisem** (ledově modrá linka `#9fc3ff` 1 px + popisek v IBM Plex Mono):
  1. **TEMPO / SYNC:** kulaté kovové tlačítko **TAP** (při ťuknutí blikne LED), **7-segmentový LED displej**
     BPM (ledově modrý `#7fd0ff`, zhasnuté segmenty `#0d2230`, vlastní kreslený 7-seg – ne font),
     tahem/kolečkem mění, dvojklik = psaní. Vedle dvě čtvercová LED tlačítka **HOST** a **FREE**
     (svítí zeleně / modře; FREE = MANUAL). LED „beat“ bliká do tempa.
  2. **LEVELS:** 4 velké knoby INPUT · OUTPUT · RECYCLE · MIX (tmavé „chicken-head“ s chromovou čepičkou),
     pod nimi MIN/MAX (u MIX DRY/WET) a malá hodnota. RECYCLE má jantarový prstenec, který svítí víc,
     čím je hodnota vyšší (varování).
  3. **SIZE:** LED tlačítka **SMALL / FULL / AUTO** (SMALL = 80 %, FULL = 100 %, AUTO = podle obrazovky).
  4. **TOOLBOX:** **SHOW GEAR**, **AUTO GAIN**, **PHONE CHECK** (bliká, když je zapnutý).

## 5. Performer unit (104 px) – TrapWin navíc

Materiál: tmavě modrý eloxovaný kov `#0e1626` s ledově modrou linkou.

- Vlevo **KILL**: velké červené podsvícené tlačítko (`theme::killRed`), při stisku „probliknutí“ celého racku.
  Pod ním přepínač **PG · R · UNRATED** (3 LED) a **BOOTLEG**.
- Uprostřed **5 maker** jako středně velké knoby s barevnými prstenci (VILLAIN ARC fialová, CRASH OUT
  červená, AURA azurová, DRIP zelená, KNOCK oranžová); prstenec ukazuje i modulovanou hodnotu.
- Vpravo **dva analogové VU metry** IN / OUT (ručička s balistikou, podsvícená stupnice teplou bílou)
  + malé LED OVER.

## 6. Jednotka efektu v racku (výška 140 px, šířka 932)

Panel 898 × 140 + **boční proužek** 34 px vpravo.

**Panel („faceplate“):**
- Vlastní materiál a barva pro každý efekt (tabulka níže), 4 šrouby, ucha racku.
- Vlevo nahoře **název efektu** velkým vlastním písmem (Archivo Black, s „nálepkou“/rámečkem podle stylu),
  pod ním malý typ (SATURATION…) a „TRAPWIN“.
- Vpravo nahoře **LCD presetu jednotky** (malý displej s názvem programu ▾ → menu programů).
- Řada knobů: všechny použité parametry + **DRY/WET**; nad knobem popisek, pod ním hodnota
  (Hz, dB, ms, 1/4D…). Přepínací parametry (TYPE, MODE, SYNC…) jako **otočný přepínač s pozicemi**
  nebo řada malých LED tlačítek – ne obyčejný knob.
- Malé tlačítko **ST / MID / SIDE** v rohu panelu.
- Každá jednotka má **jeden „živý“ prvek** (viz tabulka): LED, mini displej, ručička… reaguje na zvuk/parametry.
- **Tažení za panel** (kamkoli mimo ovládací prvek) = přesun jednotky; při tažení se panel „nadzvedne“
  (stín) a v racku se otevře mezera.

**Boční proužek (svisle, 3 + 1 čtvercová tlačítka 26 × 26):**
1. `⚙` – menu jednotky: programy, moje presety jednotky, uložit, kopírovat/vložit, nahradit efektem,
   reset, „poslouchat až po sem“, odebrat.
2. `⏻` **POWER** – svítí zeleně, když je jednotka zapnutá. Vypnutá: panel ztmavne (~60 %), ozvěny dál dobíhají.
3. `S` **SOLO** – svítí žlutě a bliká; ostatní jednotky ztmavnou.
4. `🔒` **LOCK** – svítí červeně; KILL jednotku nemění, nejde odebrat.

**Návrhy panelů (vlastní, žádná kopie):**

| Efekt | Materiál / barva | Živý prvek |
|---|---|---|
| Cassette Plug | krémový lak `#e8dcc2`, hnědé písmo, okénko s **kazetou, cívky se točí** | cívky (rychlost ~ WOW) |
| Menace | černý elox s **červenými diagonálními pruhy** (výstražná páska) | červená LED „DRIVE“ svítí podle driveu |
| Brainrot | **zelená deska plošných spojů** s cestičkami, bílý potisk | mini displej „bitů“ (sloupce podle BITS) |
| Through The Wall | tmavě modrý lak `#1d2c55`, bílé písmo | **křivka filtru** na malém displeji |
| Tone Up | světlý kartáčovaný hliník `#c9ced6`, černé písmo, barevné čepičky knobů | mini **EQ křivka** |
| Squeeze | grafitová `#24262b`, zelené akcenty | **ručkový GR metr** |
| Overcooked | oranžový lak `#d9661f` | 3 LED sloupce (LOW/MID/HIGH) |
| Slap | žlutá `#e6c229` s černým | LED „HIT“ bliká na transienty |
| Doubles | metalická azurová `#2aa6c4` | 4 LED hlasů (podle VOICES) |
| Swirl | fialový lak `#5b2a86` | **točící se spirála** podle RATE |
| Ad-Lib Throw | půlnoční modrá `#14203d` s ledovými LED | **LED displej času** (1/4D, 350 ms) |
| Aura Room | černá s **hvězdným polem** | „dozvukový“ ocas svítí podle DECAY |
| Wide Body | mátová `#5fc9a0` | **stereo vektorskop** (Lissajous) |
| Chopped | tmavě zelená `#163a23` | **16 LED krokového vzoru**, běží v tempu |
| Red Line | tmavě šedá s červenou linkou | LED pásek omezování |

Barvy LED a displejů: ledově modrá `#9fc3ff`, jantar `#ffc861`, zelená `#3dff6e`, červená `#ff2e3e`.

## 7. Gear (pravý sloupec 280 px)

- Tmavé pozadí, hlavička „GEAR“ + malý text „DRAG INTO THE RACK“, pod tím volitelně pole hledání.
- **Miniatury skutečných panelů** (vyrenderovat stejnou kreslicí funkcí jako jednotku, zmenšit na ~264 × 41),
  15 kusů pod sebou se scrollem, skupiny COLOR / FILTER & EQ / DYNAMICS / MODULATION / SPACE / STEREO
  jako tenké popisky.
- Hover: miniatura se rozsvítí a ukáže tooltip s popisem efektu. Dvojklik = přidat na konec.
- **Přetažení do racku:** okraj jednotky = vložit (svítící modrá čára), střed = nahradit (žlutý rámeček
  „REPLACE“), pod poslední = přidat. Jednotka přetažená zpět na Gear = odebrat (Gear zčervená „DROP TO REMOVE“).

## 8. Prázdné místo v racku

Pod poslední jednotkou prázdná šachta s rack rails a textem **„DROP EFFECTS HERE“** (+ „or click to add“),
klik = menu všech efektů. Když je 8/8: „RACK FULL – DROP ON A UNIT TO REPLACE IT“.

## 9. Animace a „život“ (nic nudného)

- Timer 30 Hz (už je). LEDky, VU metry, cívky, spirála, krokový vzor, beat LED.
- Při změně presetu: LCD „scan“ + krátký náběh LED.
- KILL: panely jednotek krátce bliknou ledově modře a nové jednotky „zajedou“ do racku (100–200 ms).
- Přetahování: stín, mezera, plynulé posuny.
- Vše vypnutelné v ⚙ menu („Reduce motion“), CPU UI < 3 %.

## 10. Prohlížeč presetů

Zůstává (overlay přes rack), jen restyling: černé sklo, ledově modré písmo, kategorie vlevo jako LED záložky.

## 11. Ověření

1. `Tools/Snapshot.cpp` – snímky: výchozí preset, plný rack (vypnutá + zamčená jednotka), solo, prohlížeč,
   bez Gear, SMALL; prohlédnout je a opravit chyby (překryvy, useknuté texty, kontrast).
2. Unit testy (`EffectorKillaTests`) zelené, CI zelené na Windows / macOS / Linux.
3. Obrázek do obchodu: `00_cover_ui.png` (2×) → `Tools/make_cover.py`.
4. FL Studio: přetahování, POWER s dobíháním ozvěn, SOLO, TAP, SIZE, SHOW GEAR, KILL, uložení projektu.

## 12. Revoluční funkce – co Effect Rack nemá

Effect Rack umí jen sériový řetězec, Recycle, tap tempo a celkový mix. Tyhle věci znají lidi z jiných
FX pluginů a v Effect Racku chybí. Dělat v pořadí priorit; každá musí mít UI v rack stylu, test a popis v MANUAL.md.

### Priorita 1 (v tomto vydání)

1. **MOD SOURCES – modulace přetažením (jako v moderních syntech / multi-FX)**
   Nová fixní jednotka „MOD“ pod Performerem: **2 LFO** (tvar, rychlost free/sync, fáze), **ENVELOPE
   FOLLOWER** (attack/release, citlivost) a **16krokový STEP SEQ** v tempu. Uživatel chytí malý
   „úchyt“ zdroje a **přetáhne ho na libovolný knob** v racku → vznikne modulace; na knobu se objeví
   barevný prstenec s rozsahem (táhnutím prstence se mění hloubka, ± obousměrně). Pravý klik na knob =
   seznam modulací / odebrat. Až 16 přiřazení. Modulace běží v jádru po vzorcích/blocích (bez alokací).
2. **PARALELNÍ VĚTVE (SPLIT)**
   Jednotka „SPLIT“, kterou jde vložit do racku: rozdělí signál na **A / B** (režimy PARALLEL, L/R, M/S,
   LOW/HIGH s nastavitelnou frekvencí, TRANSIENT/SUSTAIN). Pod ní dva sloupce jednotek vedle sebe,
   „MERGE“ s vyvážením A/B. (Effect Rack je jen sériový.)
3. **MACRO LEARN**
   Pravý klik na libovolný knob → „Assign to VILLAIN ARC / CRASH OUT / … “, rozsah min–max se nastaví
   tažením. Uloží se do presetu. (Mapování maker v jádru už existuje – `ProgramState::maps`.)
4. **SPEKTRÁLNÍ POHLED NA JEDNOTKU**
   Najetím na jednotku se za jejími knoby zobrazí jemný **analyzér PŘED / PO** této jednotce (FFT
   v odděleném vlákně z lock-free FIFO), takže je vidět, co přesně jednotka dělá.
5. **LEVEL-MATCHED POWER**
   Podržení ⏻ = poslech **bez této jednotky srovnaný na stejnou hlasitost** (jako COMPARE, ale pro jednu
   jednotku). Uživatel hned pozná, jestli jednotka pomáhá, nebo jen přidává hlasitost.

### Priorita 2 (další vydání, architektura už s tím počítá – CLAUDE.md sekce 15)

6. **MORPH (SHAPESHIFT)** – XY pad mezi 4 snímky racku (A/B/C/D), plynulé prolínání všech parametrů,
   automatizovatelné; pomalé „evolving“ zvuky.
7. **SCENES (VERSE / HOOK)** – až 4 scény racku, přepínání MIDI notou nebo automatizací, bez lupanců.
8. **SIDECHAIN** – vstup pro ducking/pump (Chopped PUMP, Squeeze, Ad-Lib/Aura ducking podle kopáku).
9. **MIDI LEARN + KILL na MIDI notu** – i pro FL Studio performance.
10. **PÁSMO NA JEDNOTKU** – každá jednotka může zpracovávat jen zvolené frekvenční pásmo (multiband).
11. **RENTAL CODE** – krátký textový kód celého racku na sdílení (kopírovat/vložit do chatu, na Discord).
12. **TAIL FREEZE** – tlačítko u delaye/reverbu, které „zmrazí“ ocas do nekonečného padu.

### UI pravidla pro nové funkce
- Všechno musí vypadat jako součást hardwaru (LED, displeje, kabely/„patch“ čáry pro modulace).
- Modulace a split musí jít úplně vypnout – výchozí preset se chová jako dnes.
- Žádná nová funkce nesmí změnit hlášenou latenci ani rozbít uložené projekty.
