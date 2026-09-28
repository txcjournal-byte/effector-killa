# EFFECTOR KILLA v1.0 – zadání pro Claude Code

Tento soubor je zadání projektu. Postupuj po milestonech (sekce 14). Po každém milestonu projekt
zkompiluj, spusť testy a krátce shrň, co je hotové a co mám vyzkoušet ve FL Studiu.

---

## 1. Produkt

- **Název:** Effector Killa, slogan „Every effect. One Killa.“
- **Série:** Killa pluginy (808 Killa, Keys Killa, Rolls Killa, Time Killa).
- **Co to je:** trap multi-efekt, **rack s 8 sloty** (celý efektový řetězec v jednom pluginu). Pro melodie, vokály, 808, drumy, FX i bus/master.
- **Hlavní myšlenka:** uživatel vybere **kanál (kategorii) a preset**, dolaďuje **5 maker** a tlačítkem **KILL 🎲** generuje
  náhodné, ale **hudebně smysluplné** řetězce. Pokročilý uživatel otevře sloty a mění jednotlivé efekty.
- **Vizuál:** 80s VHS obývák – TV skříň s velkou CRT televizí (interaktivní obrazovka), policí s 8 VHS kazetami (= sloty) a videem (sekce 10).
- **Rozdíl oproti Time Killa:** žádné time efekty (halftime, tape stop, stutter, reverse). Effector = barva, špína, prostor, dynamika.
- **Cena:** 49–59 €. **Hlavní DAW:** FL Studio (Windows); musí fungovat i v Ableton Live a dalších VST3 hostech.
- **Priorita:** kvalita zvuku 14 jádrových efektů > počet funkcí. Co není ve v1.0, je v roadmapě (sekce 15).

## 2. Technologie

- **C++17, JUCE 8, CMake** (JUCE přes `FetchContent`, zafixovaný tag). DSP může používat `juce::dsp`.
- Formáty **VST3 + Standalone** (AU volitelně na macOS).
- Typ: **audio efekt**, stereo in/out. (Sidechain až ve v1.1.)
- Bez externích knihoven kromě JUCE. Grafika: JUCE `Graphics` + **OpenGL** (`juce::OpenGLContext`) pro VHS efekty obrazu.
- 44.1–192 kHz, libovolný buffer. **Oversampling 1×/2×/4×** pro saturaci, distorzi, bitcrush a clipper (výchozí 2×).

### Struktura

```
EffectorKilla/
  CMakeLists.txt
  Source/
    PluginProcessor.*          // APVTS, rack, makra, AUTO TRACKING, POCKET TV, stav
    PluginEditor.*
    engine/Rack.*              // 8 slotů, pořadí, PAUSE, SCREENING, mix, M/S, lock-free swap řetězce
    engine/Effect.h            // společné rozhraní efektu (prepare/process/reset/params/serialize)
    engine/effects/*.h/.cpp    // efekty ze sekce 4
    engine/Macro.*             // mapování maker na parametry
    engine/Randomizer.*        // KILL + BOOTLEG (sekce 7)
    engine/Preset.*            // JSON (de)serializace, kanály, oblíbené
    engine/LoudnessMatch.*     // AUTO TRACKING
    ui/cabinet/…               // skříň, knoby, kazety, video, tlačítka, layout
    ui/tv/…                    // TV obrazovka: scény kanálů, AV1–AV3, OSD, CRT shader
  Resources/
    Presets/factory.json       // 96 presetů ze sekce 11
    Design/reference.png       // referenční obrázek UI (dodám)
    Design/cabinet.png         // pozadí skříně (dodám; do té doby placeholder)
    Fonts/                     // OSD, VFD a ručně psaný font (dodám, s licencí)
  Tests/
```

## 3. Rack

- **8 slotů**, každý má:
  - typ efektu (nebo prázdný),
  - **PAUSE** (bypass, bez lupance),
  - **MIX** (dry/wet slotu),
  - **SCREENING** (solo – posloucháš výstup jen tohoto slotu, tj. signál po tomto slotu bez dalších slotů),
  - **M/S** režim: STEREO / MID / SIDE (efekt se aplikuje jen na mid nebo side složku),
  - **WRITE PROTECT 🔒** (KILL slot nemění).
- Pořadí slotů měnitelné přetažením.
- Globálně: **ANTENNA IN** (input gain), **RF OUT** (output gain), **BLEND** (dry/wet celého racku), **AUTO TRACKING** (sekce 6).
- **Výměna řetězce** (preset, KILL, přetažení, změna typu efektu) **bez lupanců**: crossfade 40 ms mezi starým a novým řetězcem.
  Nový řetězec se připraví mimo audio thread (předalokovaný), předá se lock-free. Dozvuky starého řetězce se při crossfade ořežou – to je OK.
- **Parametry pro host (fixní sada)**, aby automatizace přežila změnu efektu:
  - 8 slotů × (P1–P8 + MIX + PAUSE) = 80
  - 5 maker, ANTENNA IN, RF OUT, BLEND, AUTO TRACKING, POCKET TV, OFF AIR (bypass pro porovnání)
  - Každý efekt si P1–P8 mapuje na své parametry; UI ukazuje skutečné názvy. Nepoužité P = neaktivní.
- **Latence:** hlásit hostu **fixní** hodnotu podle nastavení oversamplingu (+ fixní lookahead Red Line), **nezávisle na obsahu racku**,
  aby se při KILL/presetu neměnila kompenzace. Kratší cesty dorovnat interním delayem.

## 4. Efekty v1.0 (14 + Red Line)

Notace P1–P8. Rozsahy navrhni rozumně; hodnoty 0–1 v presetech jsou relativní polohy.

| # | Název v UI | Typ | P1–P8 |
|---|---|---|---|
| 1 | **Cassette Plug** | saturace | mode (TAPE / TUBE), drive, bias (sudé harmonické), tone, wow, hiss, **low keep** (20–250 Hz), mix |
| 2 | **Menace** | distorze | type (FUZZ / CLIP / FOLDBACK / RECTIFY), drive, tone, pre-HP, post-LP, **low keep**, mix |
| 3 | **Brainrot** | bitcrush / lo-fi | bits (4–16), rate (1–44 kHz), jitter, noise, crackle, tone, mix |
| 4 | **Through The Wall** | filtr | type (LP12 / LP24 / HP / BP / NOTCH / COMB), cutoff, reso, drive, LFO rate (sync), LFO depth, env follower, BP width (oktávy) |
| 5 | **Tone Up** | EQ | low-cut, low shelf gain (100 Hz), low-mid gain, low-mid freq, high-mid gain, high-mid freq, high shelf gain (8 kHz), tilt |
| 6 | **Squeeze** | kompresor | threshold, ratio, attack, release, knee, makeup, mix (parallel) |
| 7 | **Overcooked** | 3pásmová up/down komprese | depth, time, upward, downward, low gain, mid gain, high gain, mix |
| 8 | **Slap** | transient shaper | attack, sustain, clip output, mix |
| 9 | **Doubles** | chorus | rate, depth, voices (1–4), spread, **low keep** (pod touto frekvencí mono a bez chorusu), mix |
| 10 | **Swirl** | phaser | rate, sync on/off, stages (2–12), feedback, depth, mix |
| 11 | **Ad-Lib Throw** | delay | time, sync on/off, feedback, ping-pong, LP, HP, ducking, mix |
| 12 | **Aura Room** | reverb | type (ROOM / PLATE / HALL / DARK), size, decay, pre-delay, damping, width, ducking, mix |
| 13 | **Wide Body** | stereo | width (0–200 %), Haas (0–30 ms), mono-below (0–300 Hz) |
| 14 | **Chopped** | gate | mode (PATTERN / PUMP / NOISE GATE), pattern (16 vzorů), rate (sync), depth, smooth, threshold (noise gate), mix |
| 15 | **Red Line** | clipper / limiter | ceiling, input drive, character (SOFT ↔ HARD), lookahead on/off |

Poznámky:

- **low keep:** signál pod danou frekvencí jde efektem nedotčený (Linkwitz-Riley crossover, fázově správně sečíst). Klíčové pro 808.
- **BP v presetech** se zapisuje jako „BP 500–3k“ = center √(500·3000), width log2(3000/500) oktáv.
- **COMB + LFO** v Through The Wall slouží jako flanger (než přijde Jet Lag ve v1.1).
- **Chopped PUMP** = sidechain-like pumpování do tempa (1/4, 1/8), bez sidechain vstupu.
- **Red Line** je typicky poslední slot; lookahead má fixní latenci, vždy hlášenou (sekce 3).
- Všechny efekty: `SmoothedValue` na parametrech, **žádné alokace v `process`**, stabilní při extrémních hodnotách, DC blocker kde je potřeba.
- Tempo (sync) bere z hostu (`AudioPlayHead`); bez hostu 140 BPM.

## 5. Makra (5 knobů)

| Makro | Co dělá (výchozí mapování) |
|---|---|
| **VILLAIN ARC** | ztmavení: LP filtry dolů, Tone Up tilt tmavší, Aura Room damping ↑ a typ DARK-like |
| **CRASH OUT** | špína: drive Cassette Plug / Menace ↑, Brainrot bits ↓, Red Line drive ↑ |
| **AURA** | prostor: mix Aura Room / Ad-Lib Throw ↑, decay ↑, Wide Body width ↑ |
| **DRIP** | pohyb: rychlost/hloubka LFO (Through The Wall, Doubles, Swirl), wow ↑, Chopped depth ↑ |
| **KNOCK** | úder: Slap attack ↑, Squeeze ratio/makeup ↑, Red Line drive ↑ |

- Každý preset může definovat vlastní mapování (cíl, min/max, křivka). Když ho nedefinuje, platí výchozí mapování (makro najde odpovídající efekty v racku).
- Makra respektují pravidla zdroje (např. u 808 AURA nezvedne reverb mix nad 0.15).

## 6. Zdroj, AUTO TRACKING, POCKET TV

- **Zdroj (Source):** MELODY / VOX / 808 / DRUMS / FX / BUS. Ovlivňuje pravidla KILL a rozsahy maker. Každý kanál má výchozí zdroj (sekce 11).
- **AUTO TRACKING (auto-gain):** měří krátkodobou hlasitost vstupu a výstupu (RMS, ~3 s okno, přibližná K-váha), pomalu dorovnává výstup
  (časová konstanta ~1 s, max ±12 dB), při tichu se zmrazí. Nesmí pumpovat.
- **POCKET TV:** jen monitorovací přepínač: výstup mono + HP 300 Hz + LP 8 kHz + lehká komprese. V UI bliká, dokud je zapnutý
  (aby na něj uživatel nezapomněl při exportu).

## 7. KILL 🎲 a BOOTLEG

**KILL** generuje nový řetězec. Tři úrovně (přepínač **PG / R / UNRATED**):

- **PG:** stejné typy efektů, parametry ±25 %, max. 1 slot vyměněn.
- **R:** nový řetězec 3–5 slotů, střední hodnoty.
- **UNRATED:** nový řetězec 4–7 slotů, vyšší drive a mixy – ale pořád v rozumných mezích a srovnaný na hlasitost.

**Pravidla (platí pro KILL, ne pro factory presety):**

- Max 1 × Menace, 1 × Cassette Plug, 1 × Brainrot. Cassette Plug vždy **před** Menace.
- Prostor (Ad-Lib Throw, Aura Room) vždy **na konci**, jen Wide Body a Red Line smí být za ním.
- Through The Wall a Tone Up **před** prostorem. Squeeze a Overcooked **po** saturaci.
- Red Line, pokud je v řetězci, je vždy poslední.
- **808:** Aura Room mix ≤ 0.15; při jakémkoli stereo/modulačním efektu musí být Wide Body s mono-below ≥ 120 Hz;
  Doubles low keep ≥ 150 Hz; Menace a Cassette Plug low keep ≥ 60 Hz.
- **DRUMS:** Aura Room size < 0.4; Slap a Red Line preferované.
- **VOX:** Tone Up s low-cut ≥ 80 Hz a Squeeze preferované.
- **BUS:** max 4 sloty, bez Brainrot a Chopped, Aura Room ≤ 0.1, Red Line na konci povinně.
- Výsledek je **srovnaný na hlasitost vstupu (±1 dB)** – odhad offline na testovacím signálu zdroje, pak jemně dorovná AUTO TRACKING.
- **WRITE PROTECT 🔒** sloty KILL nemění (ani jejich pozici).

**BOOTLEG:** jemná variace – ±10–20 % náhodná změna parametrů, bez změny efektů.

- Obojí **deterministické podle seedu** (undo/redo, sdílení).
- Každý KILL/BOOTLEG je jeden krok v undo historii.

## 8. Presety – funkce

- **12 kanálů (CH 01–12) × 8 presetů = 96 factory presetů** (sekce 11). Factory jsou read-only.
- Uživatelské presety do `Documents/Killa/Effector Killa/Presets` (JSON), s kanálem a tagy.
- **STAFF PICK ⭐** (oblíbené), **SIDE A / SIDE B** (A/B), **REWIND / FAST FWD** (undo/redo, min. 30 kroků).
- PROGRAM ▲ ▼ přepínání bez výpadku, stav se ukládá do projektu DAW.
- Uložení jednotlivého slotu jako „slot preset“.

## 9. Ovládání navíc

- **Klávesové zkratky** (když má plugin fokus): mezerník = KILL, ←/→ = preset, ↑/↓ = kanál, 1–8 = výběr slotu, Ctrl+Z / Ctrl+Y = undo/redo.
- **Knoby:** tah nahoru/dolů = hodnota, Shift = jemně, dvojklik = výchozí hodnota, kolečko = krok.

## 10. UI – 80s VHS obývák

**Koncept:** celé UI je jeden kus nábytku z 80. let – dřevěná TV skříň (ořech), uprostřed velká CRT televize, pod ní police s 8 VHS kazetami,
vedle videorekordér, vlevo ovládací panel s KILL. **Designovaná grafika pluginu** (stylizovaná, semi-realistická, čistá), ne fotka.
Útulné, ale trochu creepy noční 80s VHS. **Každý viditelný prvek je ovládání.** Referenční obrázek: `Resources/Design/reference.png`.

- Velikost **1100×650**, škálovatelné 100/125/150 % (grafika v 2× rozlišení).
- Paleta: tmavé dřevo, hnědý/béžový plast, broušený kov, krémové štítky s černou fixou; akcenty teplá jantarová a vybledlá oranžová.
  **Červená `#FF2E3E` jen pro KILL, OFF AIR LED a varování (clip).** Každý kanál má barvu (sekce 11) – tónuje scénu na TV.
- **Grafika:** pozadí skříně = obrázek (`cabinet.png`, dodám; do té doby placeholder – hnědé panely s rozmístěním podle schématu).
  Knoby = filmstrip nebo kreslený kov + rotace, kazety a tlačítka = samostatné vrstvy/sprity. Pozice všech prvků v jednom layout souboru.

### Rozložení (podle reference)

```
┌──────────────┬──────────────────────────────────────┬──────────────────┐
│ EFFECTOR     │                                      │ volič kanálů     │
│ KILLA (logo) │        CRT TELEVIZE – OBRAZOVKA       │ 01–12            │
│              │   ▶ AV2 SCRIBBLE        CH 10 DRIFT   │ PROGRAM ▲ ▼      │
│  [ KILL ]    │                                      ├──────────────────┤
│ PG · R · UNR │   CRASH OUT ▮▮▮▮▮▮▯▯ 72     1 BAR     │ ◉ VILLAIN ARC    │
│              ├──────────────────────────────────────┤ ◉ CRASH OUT      │
│ BOOTLEG      │ AV1 REMOTE · AV2 SCRIBBLE · AV3 SCR.  │ ◉ AURA           │
│ POCKET TV    │                        ● [OFF AIR]    │ ◉ DRIP           │
│ AUTO TRACKING├──────────────────────────────────────┤ ◉ KNOCK          │
│ BASIC ◐ PREM.│ ▮▮▮▮▮▮▮▮  8 kazet v polici  │ VIDEO: [NIGHT DRIVE]       │
│ ⭐ STAFF PICK │ 1 … 8 (vybraná vysunutá)     │ REC ◀◀ ▶▶ EJECT  SIDE A/B  │
│ ANTENNA IN  RF OUT                          │                            │
└──────────────┴──────────────────────────────┴────────────────────────────┘
```

### Prvky a co dělají

| Prvek | Funkce |
|---|---|
| **KILL** (velké červené tlačítko) | nový řetězec (sekce 7) |
| přepínač **PG / R / UNRATED** | síla KILL |
| **BOOTLEG** | jemná variace (sekce 7) |
| **POCKET TV** | poslech „jako z mobilu“ (sekce 6); tlačítko bliká, dokud je zapnuté |
| **AUTO TRACKING** | auto-gain (sekce 6) |
| **BASIC CABLE / PREMIUM CABLE** | jednoduchý / pokročilý režim: BASIC skryje detail kazety a ztlumí animace; PREMIUM ukáže vše |
| **STAFF PICK ⭐** | přidat preset do oblíbených |
| **ANTENNA IN / RF OUT** | vstupní / výstupní gain |
| **volič kanálů 01–12** | kategorie presetů |
| **PROGRAM ▲ ▼** | předchozí / další preset v kanálu |
| **5 knobů** | makra VILLAIN ARC, CRASH OUT, AURA, DRIP, KNOCK |
| **8 kazet** | 8 slotů. Klik = výběr (kazeta se vysune a rozsvítí). Tažení = změna pořadí. Pravý klik = menu (změnit efekt, kopírovat, vložit, EJECT, uložit slot preset, WRITE PROTECT). Barevná tečka = typ efektu. Zamčená kazeta má červený štítek WRITE PROTECT, PAUSE kazeta je zašedlá. Prázdný slot = prázdné místo v polici. |
| **video – displej** | název presetu (zelený VFD font); klik = seznam presetů kanálu |
| **REC** | uložit uživatelský preset |
| **◀◀ REWIND / ▶▶ FAST FWD** | undo / redo |
| **EJECT** | vyndat efekt z vybrané kazety (slot prázdný) |
| **SIDE A / SIDE B** | A/B porovnání dvou stavů |
| **přepínač AV1 / AV2 / AV3** | režim interaktivní obrazovky (níže) |
| **OFF AIR** (vypínač na TV) | porovnání s originálem (níže) |

**Detail kazety (PREMIUM CABLE):** po výběru kazety se přes spodní část TV obrazovky vysune OSD panel s parametry P1–P8 dané kazety
(blokové lišty se skutečnými názvy, hodnota při hoveru) + MIX, PAUSE, SCREENING, M/S, WRITE PROTECT. Esc / klik mimo = zavřít.
Tažení vlevo/vpravo = hodnota, Shift = jemně, dvojklik = výchozí hodnota, kolečko = krok.

### Televize – interaktivní obrazovka

Obrazovka zabírá ~45 % šířky okna. Na pozadí vždy běží **„pořad“ kanálu** – abstraktní animovaná VHS scéna (smyčka),
která pulzuje do tempa a reaguje na hlasitost výstupu. **Neukazuje waveform samplu.** 12 scén, jedna na kanál, např.:
808 = chvějící se basová vlna / reproduktor, DREAMCORE = mraky, VHS = barevné pruhy a šum, RAGE = stroboskop, DRIFT = noční dálnice.
Scény generuj **procedurálně shaderem** (stylizované, low-fi), ne z videí – lehké, bez licencí, barva podle kanálu.

Přes scénu je hratelná vrstva podle přepínače **AV**. Všechny režimy ovládají **cíl** – výchozí CRASH OUT (osa X) a AURA (osa Y);
cíle lze změnit klikem na OSD popisek osy (libovolné makro nebo parametr vybrané kazety).

- **AV1 REMOTE (XY pad):** táhneš svítící bod po obrazovce, X a Y ovládají dva cíle. Scéna se kolem bodu deformuje.
  Po puštění bod zůstane (hodnota drží); dvojklik = návrat na výchozí.
- **AV2 SCRIBBLE (kreslená modulace):** myší nakreslíš čáru/smyčku; bod po ní jede dokola v tempu (délka 1 / 2 / 4 takty, přepínač v OSD „1 BAR“).
  Pozice bodu moduluje cíle (X a Y). Nové kreslení přepíše starou čáru; pravý klik = smazat. Modulace je relativní kolem aktuální hodnoty makra
  a synchronizovaná s pozicí transportu hostu (při stopce stojí).
- **AV3 SCREENSAVER:** logo EFFECTOR KILLA se odráží po obrazovce jako starý spořič, rychlost podle tempa (volitelně 1/4–4 takty na přejezd).
  Pozice loga moduluje cíle. **Když logo trefí přesně roh, přijde krátký glitch výbuch** (~1 dobu: krátký nárůst CRASH OUT + vizuální glitch).
- Modulace z AV2/AV3 se ukládá do presetu. Do hostu jde jako výsledná hodnota makra (automatizace makra má přednost – když host automatizuje, AV se odpojí).

**Hlášky na TV (OSD, blokový VCR font):**

- rohy: „▶ AV2 SCRIBBLE“ vlevo nahoře, „CH 10 DRIFT“ vpravo nahoře
- při točení knobem: velký pruh `CRASH OUT ▮▮▮▮▮▮▯▯ 72` v dolní třetině, zmizí po ~1,5 s
- **KILL:** obraz se roztrhá (~300 ms) → modrá obrazovka „NO SIGNAL“ (~250 ms) → šum → nový obraz + karta „NOW PLAYING“ se seznamem kazet (~2 s)
- změna presetu/kanálu: krátké přepnutí se šumem (~150 ms)
- POCKET TV: obraz se zmenší a rozmaže
- clip na výstupu: v rohu problikne červené „OVERLOAD“

**OFF AIR (porovnání s originálem):** stisk vypínače „vypne“ TV (CRT animace smrsknutí do bílé tečky), rozsvítí červenou LED a výstup je
**originální signál srovnaný na stejnou hlasitost** (bypass celého racku, crossfade 20 ms, bez lupanců). Další stisk zapne.
Podržení = poslech originálu jen po dobu držení. Hostu vystavit jako parametr **OFF AIR**.

### Pohyb a vykreslování

- TV obrazovka se kreslí přes **OpenGL** (scéna, hratelná vrstva a OSD do textury → CRT efekt: zakřivení, scanlines, lehký šum, chroma, vinětace).
- Makra jemně mění scénu: CRASH OUT = víc glitchů, VILLAIN ARC = tmavší, AURA = záře/rozostření, DRIP = vlnění, KNOCK = záblesk na transienty.
- Zbytek UI je statický (jen rotace knobů, LED, vysunutí kazety, stisk tlačítek s krátkou animací).
- Animace **30–60 fps**, CPU UI < 3 % jádra; bez OpenGL fallback na software (30 fps, jednodušší scény). BASIC CABLE ztlumí animace.
- **Fonty:** VCR/OSD pixel font (obrazovka), VFD font (displej videa), ručně psaný font (štítky) – dodám s licencí; do té doby vestavěné.
- Klávesové zkratky (sekce 9) platí dál.
- Žádná cizí loga, značky ani trademarky.

## 11. Factory presety – 12 kanálů × 8 = 96

Formát: `Název | co dělá | řetězec (slot → slot) | makro, na které je preset laděný`.
Hodnoty 0–1 jsou relativní polohy. Doladit poslechem je OK, zachovej charakter a pořadí.
„[1.1: …]“ = po přidání efektu ve v1.1 preset upravit.
Factory presety **nemusí** splňovat pravidla KILL (sekce 7).

### CH 01 – MELODIA · melodie, klávesy · #8A2BE2 · zdroj MELODY
1. Melodia | teplá čistá melodie | Cassette Plug 0.3 → Tone Up tilt −1 → Doubles 0.25 → Aura Room hall 0.25 | default
2. Dark Keys | tmavé klávesy | Cassette Plug 0.4 → Through The Wall LP 2.5k → Aura Room 0.35 → Wide Body 130 % | VILLAIN ARC: LP 800–6k
3. Glass Bells | jasné bells | Tone Up high +3 → Ad-Lib Throw 1/8D 0.3 → Aura Room plate (damping nízko) 0.35 | AURA [1.1: Heaven Mode]
4. Villain Piano | temné piano | Cassette Plug TUBE 0.4 → Through The Wall LP 1.8k → Swirl slow 0.2 → Aura Room DARK 0.4 | VILLAIN ARC
5. Wide Dream | široký dreamy | Doubles 0.5 → Wide Body 180 % → Aura Room hall 0.5 decay dlouhý | DRIP
6. Detroit Pluck | tvrdý pluck | Slap attack 0.5 → Menace CLIP 0.3 → Tone Up low-cut 200 → Ad-Lib Throw 1/16 0.2 | KNOCK
7. Guitar Soul | emo kytara | Cassette Plug 0.35 → Doubles 0.3 → Ad-Lib Throw 1/4 0.25 → Aura Room 0.3 | AURA
8. Melodia Mid | nenápadné vylepšení | Tone Up tilt +1 → Squeeze light → Wide Body 120 % | default

### CH 02 – VOX DEI · vokály · #40E0D0 · zdroj VOX
1. Vox Dei | moderní trap vokál | Tone Up (low-cut 120, high +3) → Squeeze 4:1 → Cassette Plug 0.2 → Ad-Lib Throw 1/4 0.15 → Aura Room plate 0.15 | default
2. Rage Vox | agresivní | Menace FUZZ 0.35 → Overcooked 0.4 → Tone Up high +4 → Ad-Lib Throw 1/8 0.2 | CRASH OUT
3. Phone Ghost | telefon | Through The Wall BP 500–3k → Menace CLIP 0.3 → Aura Room 0.2 | default
4. Demon Double | temný dubl | Doubles 0.5 → Menace 0.2 → Through The Wall LP 2k → Wide Body 140 % | VILLAIN ARC [1.1: Alter Ego −12]
5. Angel Air | vzdušný | Tone Up high shelf +5 → Squeeze light → Aura Room hall 0.35 (damping nízko) → Wide Body 150 % | AURA [1.1: Heaven Mode]
6. Throw It Back | ad-lib do prostoru | Through The Wall HP 300 → Ad-Lib Throw 1/4 ping-pong 0.45 → Aura Room 0.4 | AURA
7. Crash Out Vox | přetížený | Overcooked 0.6 → Menace CLIP 0.5 → Red Line | CRASH OUT
8. Whisper Tape | lo-fi šepot | Brainrot 12bit → Cassette Plug 0.4 → Through The Wall LP 5k → Aura Room 0.25 | default

### CH 03 – SVBTERRA · 808 / bas · #B22222 · zdroj 808
1. Svbterra | 808 slyšitelná na mobilu | Cassette Plug bias 0.3 drive 0.5 low keep 60 → Tone Up (+2 @150, +3 @900) → Wide Body mono-below 120 | CRASH OUT: drive
2. Grave Digger | tmavá těžká | Cassette Plug TUBE 0.5 low keep 50 → Through The Wall LP 3k → Red Line SOFT | VILLAIN ARC
3. Blown 808 | rage clipped | Menace CLIP 0.7 low keep 50 → Tone Up low-cut 25 → Red Line HARD | CRASH OUT
4. Fuzz Tonka | fuzz střed | Menace FUZZ 0.5 mix 0.5 low keep 80 → Tone Up mid +4 @800 → Red Line | CRASH OUT
5. Spinz Warm | teplá dlouhá | Cassette Plug 0.35 bias 0.2 → Squeeze 3:1 slow → Tone Up low shelf +1.5 | default
6. Reese Motion | pohyb v basu | Doubles 0.3 low keep 150 → Swirl 0.2 → Menace 0.2 low keep 80 → Wide Body mono-below 150 | DRIP
7. Sub Only | čistý sub | Through The Wall LP 200 → Squeeze → Wide Body width 0 | VILLAIN ARC [1.1: Big Back]
8. Mid Bite | agresivní středy | Overcooked 0.5 → Menace FOLDBACK 0.3 low keep 100 → Tone Up +4 @1k | CRASH OUT

### CH 04 – DRVM CVLT · drumy · #FF6A00 · zdroj DRUMS
1. Drvm Cvlt | víc punch | Slap attack 0.4 → Squeeze parallel mix 0.4 → Red Line SOFT | KNOCK
2. Knock Box | tvrdý knock | Slap 0.6 → Menace CLIP 0.3 → Tone Up (low shelf +3, +2 @4k) | KNOCK
3. Lo-Fi Kit | lo-fi drumy | Brainrot 10bit → Cassette Plug 0.4 → Through The Wall LP 7k | CRASH OUT
4. Room Smash | smashed room | Squeeze 10:1 fast mix 0.4 → Aura Room ROOM size 0.2 mix 0.2 | AURA
5. Hat Shine | lesklé hajtky | Tone Up high +4 → Wide Body 150 % → Slap 0.3 | default
6. Snare Crack | praskavý snare | Slap 0.5 → Menace CLIP 0.25 → Aura Room plate short 0.15 | KNOCK
7. Memphis Tape | kazetové drumy | Cassette Plug 0.6 wow 0.3 → Brainrot 12bit → Through The Wall BP 80–9k | default
8. Clipper Cult | hlasité drumy | Overcooked 0.3 → Red Line HARD ceiling −0.3 dB | CRASH OUT

### CH 05 – RAGE ENGINE · rage leady, synthy · #FF0000 · zdroj MELODY
1. Rage Engine | klasický rage lead | Menace FUZZ 0.5 → Overcooked 0.5 → Doubles 0.3 → Wide Body 170 % → Aura Room 0.2 | CRASH OUT
2. Hellfire Saw | supersaw drive | Cassette Plug TUBE 0.6 → Menace CLIP 0.4 → Tone Up high +3 → Ad-Lib Throw 1/8 0.2 | CRASH OUT
3. Siren Lead | pumpující | Chopped PUMP 1/4 0.6 → Menace 0.4 → Aura Room 0.3 | DRIP
4. Molten Pad | rozžhavený pad | Cassette Plug 0.7 → Swirl 0.4 → Aura Room 0.5 | AURA
5. Crushed Arp | crushed arp | Brainrot 8bit → Through The Wall LP LFO 1/8 → Ad-Lib Throw 1/16 0.3 | DRIP
6. Screamer | foldback | Menace FOLDBACK 0.6 → Through The Wall HP 300 → Wide Body 160 % | CRASH OUT
7. Stadium | velký hall | Overcooked 0.4 → Wide Body 190 % → Aura Room hall 0.5 | AURA
8. Burn It Down | extrém | Menace FUZZ 0.8 → Brainrot 10bit → Red Line HARD | CRASH OUT

### CH 06 – DREAMCORE · plugg, dreamy · #FFB6C1 · zdroj MELODY
1. Dreamcore | plugg dream | Doubles 0.35 → Through The Wall LP 7k → Ad-Lib Throw 1/4D 0.2 → Aura Room hall 0.45 | AURA [1.1: Heaven Mode]
2. Cloud Bells | měkké bells | Tone Up tilt −1 → Ad-Lib Throw 1/8D ping-pong 0.35 → Aura Room 0.4 | AURA
3. Pluggnb Glow | teplá záře | Cassette Plug 0.3 → Doubles 0.3 → Aura Room 0.35 → Wide Body 150 % | default
4. Heaven Gate | andělský | Tone Up low-cut 200 → Doubles 0.3 → Aura Room hall 0.6 (damping nízko) | AURA [1.1: Heaven Mode]
5. Soft Focus | rozmazaný | Through The Wall LP 3.5k → Doubles 0.5 → Aura Room 0.5 | VILLAIN ARC
6. Pink Tape | jemná páska | Cassette Plug 0.4 wow 0.4 → Through The Wall LP 8k → Aura Room 0.3 | DRIP
7. Main Character | široký | Wide Body 190 % → Ad-Lib Throw 1/4 0.3 → Aura Room 0.4 | AURA
8. Floating | plovoucí | Swirl slow 0.3 → Ad-Lib Throw 1/2 0.3 → Aura Room 0.6 | DRIP

### CH 07 – VHS · lo-fi · #C8A165 · zdroj MELODY
1. VHS | kazeta | Cassette Plug 0.5 wow 0.5 hiss 0.2 → Brainrot 12bit → Through The Wall LP 6k | default
2. Basement | sklep | Through The Wall LP 1.2k → Aura Room ROOM 0.3 | VILLAIN ARC
3. Payphone | telefon | Through The Wall BP 400–3k → Menace CLIP 0.3 | default
4. 8-Bit Soul | 8-bit | Brainrot 6bit rate 11k | CRASH OUT
5. Pirate Radio | rádio | Through The Wall BP 600–4k → Brainrot 10bit noise 0.4 | default
6. Warped Vinyl | vinyl | Cassette Plug wow 0.7 hiss 0.3 → Brainrot 16bit crackle 0.3 → Through The Wall LP 7k | DRIP
7. Dusty | prašný | Tone Up tilt −3 → Brainrot 14bit crackle 0.2 → Squeeze light | default
8. Memphis Mist | Memphis | Cassette Plug 0.6 → Brainrot 10bit rate 22k → Through The Wall LP 4k → Aura Room 0.2 | default

### CH 08 – MVTANT · FX, sound design · #7CFC00 · zdroj FX
1. Mvtant | zmutovaný zvuk | Menace FOLDBACK 0.4 → Through The Wall COMB LFO 0.5 → Ad-Lib Throw 1/16 0.4 | DRIP [1.1: Jet Lag]
2. Toxic Comb | kovový comb | Through The Wall COMB 0.6 → Menace 0.3 → Aura Room 0.3 | default
3. Radioactive | vibrující | Swirl fast 0.6 → Brainrot 8bit → Wide Body 180 % | DRIP
4. Robot NPC | robotický | Through The Wall COMB reso vysoko → Brainrot 8bit → Chopped 1/16 0.3 | default [1.1: Alter Ego formant, NPC Glitch]
5. Underwater | pod vodou | Through The Wall LP 700 LFO 0.3 → Doubles 0.5 → Aura Room 0.5 | VILLAIN ARC
6. Portal | vír | Through The Wall COMB LFO slow 0.7 → Swirl 0.4 → Aura Room 0.6 | AURA [1.1: Heaven Mode −12]
7. Glitch Aura | glitchový prostor | Brainrot jitter 0.5 → Chopped 1/32 0.4 → Ad-Lib Throw 1/32 fb 0.6 → Aura Room 0.4 | DRIP
8. Black Hole | nekonečný | Aura Room size 1.0 decay max mix 0.7 → Through The Wall LP 2k → Wide Body 200 % | AURA

### CH 09 – NO FILTER · raw, suché, clipnuté · #F2F2F2 · zdroj VOX
1. No Filter | syrový suchý vokál | Tone Up low-cut 100 → Squeeze 6:1 fast → Menace CLIP 0.2 → Red Line SOFT | CRASH OUT
2. Crash Out | totální přetížení | Overcooked 0.7 → Menace FUZZ 0.4 → Red Line HARD | CRASH OUT
3. Opp Pack | tmavý a tvrdý | Cassette Plug TUBE 0.5 → Through The Wall LP 2k → Slap 0.4 → Red Line | VILLAIN ARC
4. Lock In | čistý suchý vokál vepředu | Squeeze 4:1 → Tone Up high +2 → Red Line SOFT | KNOCK
5. Red Line Rage | hlasitý rage | Menace CLIP 0.6 → Overcooked 0.4 → Red Line HARD ceiling −0.1 | CRASH OUT
6. Dry Menace | suchá distorze | Menace FOLDBACK 0.4 low keep 120 → Tone Up +3 @1.5k | CRASH OUT
7. Big Back 808 | tlustá syrová 808 (zdroj 808) | Cassette Plug drive 0.6 low keep 70 → Menace CLIP 0.3 low keep 90 → Wide Body mono-below 150 → Red Line | CRASH OUT
8. 3AM Studio | suchý, intimní | Cassette Plug 0.3 → Squeeze 3:1 → Tone Up tilt −1 → Chopped NOISE GATE | VILLAIN ARC

### CH 10 – DRIFT · phonk, cowbell, Memphis · #6A5ACD · zdroj MELODY
1. Drift | phonk cowbell | Cassette Plug 0.4 → Menace CLIP 0.5 → Through The Wall LP 6k → Red Line | CRASH OUT
2. Cowbell Curse | přetížený zvon | Menace FUZZ 0.6 → Tone Up +5 @900 → Red Line HARD | CRASH OUT
3. Night Drive | noční jízda | Cassette Plug 0.4 wow 0.2 → Through The Wall LP 4k → Ad-Lib Throw 1/4D 0.25 → Aura Room 0.3 | AURA
4. Smoke Tape | zakouřená kazeta | Brainrot 12bit → Cassette Plug 0.5 hiss 0.3 → Through The Wall BP 100–6k | VILLAIN ARC
5. Tokyo Ghost | éterický pohyb | Doubles 0.4 → Swirl 0.3 → Aura Room 0.4 → Wide Body 160 % | DRIP
6. Burnout | sekaný drive | Overcooked 0.6 → Menace CLIP 0.5 → Chopped 1/8 0.3 | KNOCK
7. Low Rider | phonk bas (zdroj 808) | Cassette Plug TUBE 0.5 low keep 60 → Tone Up low shelf +3 → Squeeze 3:1 → Red Line SOFT | CRASH OUT
8. Midnight Pressure | pumpující temnota | Chopped PUMP 1/4 0.5 → Menace 0.4 → Through The Wall LP 3k → Aura Room 0.25 | DRIP

### CH 11 – HYPER · hyperpop, glitch · #00E5FF · zdroj VOX
1. Hyper | hyperpop vokál | Squeeze 6:1 → Tone Up high +5 → Menace CLIP 0.3 → Doubles 0.3 → Wide Body 160 % | CRASH OUT
2. Brain Melt | rozpadlý glitch | Brainrot 8bit jitter 0.4 → Chopped 1/16 0.4 → Ad-Lib Throw 1/16 0.3 | DRIP
3. Delulu | snový, přeslazený | Doubles 0.5 → Swirl 0.4 → Aura Room 0.4 (damping nízko) → Wide Body 180 % | AURA
4. Screen Time | digitální drť | Brainrot 10bit → Through The Wall HP 400 → Menace CLIP 0.3 → Red Line | CRASH OUT
5. Glitch Princess | sekaný lesk | Chopped 1/32 0.5 → Tone Up high +4 → Ad-Lib Throw 1/8 ping-pong 0.3 | DRIP
6. Sugar Rush | přesycený, jasný | Tone Up (low-cut 250, high +6) → Overcooked 0.5 → Menace FUZZ 0.3 → Red Line | CRASH OUT [1.1: Alter Ego +12]
7. Plot Twist | pohyblivý filtr | Through The Wall HP LFO 1/2 0.6 → Menace 0.3 → Aura Room 0.35 | DRIP
8. Lore Drop | velký glitch prostor | Chopped PATTERN 0.5 → Brainrot 12bit → Aura Room hall 0.5 → Wide Body 170 % | AURA

### CH 12 – FINAL BOSS · bus, master · #C0C0C0 · zdroj BUS
1. Final Boss | master glue + hlasitost | Squeeze 2:1 slow → Tone Up tilt +0.5 → Wide Body 115 % mono-below 120 → Red Line SOFT −1.0 dB | KNOCK
2. No Cap Master | čistý master | Tone Up low-cut 25 → Squeeze 1.5:1 → Red Line SOFT −1.0 dB | default
3. Glue Gang | lepidlo na bus | Squeeze 4:1 attack 30 ms mix 0.5 → Cassette Plug 0.15 → Red Line SOFT | KNOCK
4. Car Test | basy do auta | Tone Up (low shelf +2, −2 @300) → Overcooked 0.2 → Red Line | KNOCK
5. Phone Speaker | slyšitelné z mobilu | Cassette Plug 0.3 low keep 80 → Tone Up +3 @1k → Wide Body mono-below 150 → Red Line | default
6. Big Stage | široký bus | Tone Up high +1.5 → Wide Body 140 % mono-below 150 → Red Line SOFT | AURA
7. Loud Money | maximální hlasitost | Overcooked 0.3 → Menace CLIP 0.1 mix 0.3 low keep 150 → Red Line HARD −0.3 dB | CRASH OUT
8. Side Quest | jemné pumpování busu | Squeeze parallel mix 0.3 → Chopped PUMP 1/4 0.2 → Red Line SOFT | DRIP

## 12. Kvalita a testy

- **Real-time safety:** žádné alokace, zámky ani I/O v audio threadu; výměna řetězce přes lock-free swap.
- **Bez lupanců** při změně presetu, KILL, PAUSE, přetažení slotu, změně typu efektu.
- **Unit testy:**
  - každý efekt: stabilita, žádné NaN/Inf při extrémních hodnotách, 44.1/48/96 kHz, buffer 1–4096,
  - low keep: pod crossoverem je signál bit-přesně/téměř beze změny,
  - randomizer: pravidla ze sekce 7 platí pro **10 000 seedů** a všechny zdroje a úrovně,
  - determinismus seedu, BOOTLEG nemění typy efektů,
  - načtení všech **96 presetů**, serializace stavu (uložit → načíst = stejný stav),
  - latence je stejná pro libovolný obsah racku.
- **Render test:** všechny presety na testovacích signálech (sinus 55 Hz, pink noise, generovaný drum loop, generovaný „vokál“ – formantový šum) –
  výstup bez NaN, KILL výsledek srovnaný na ±1 dB.
- **pluginval** strictness 5+.
- **CPU cíl:** typický preset < 8 % jádra při 44.1 kHz / 256 / 2× oversampling; UI < 3 %.

## 13. Pravidla

- Před větší změnou krátký plán, pak pokračuj bez čekání.
- Bez nových knihoven/služeb mimo JUCE bez zeptání.
- Commit po každém milestonu (pokud je git).
- Co nejde otestovat automaticky (FL Studio), napiš mi přesný testovací postup.
- Žádné cizí značky, loga, jména umělců ani názvy konkurenčních pluginů v UI a presetech.
- Grafické podklady (pozadí, font) dodám; do té doby placeholdery, ať jde vše postavit a testovat.

## 14. Milestony

1. **M1** – JUCE/CMake projekt, prázdný VST3 + Standalone, načte se ve FL Studiu.
2. **M2** – Rack (8 slotů, pořadí, PAUSE, SCREENING, MIX, M/S, crossfade swap, fixní latence) + rozhraní Effect + 3 efekty (Cassette Plug, Through The Wall, Aura Room) + testy.
3. **M3** – zbytek efektů ze sekce 4 (vč. low keep) + oversampling + testy.
4. **M4** – Makra, Zdroj, AUTO TRACKING, POCKET TV, A/B, undo/redo.
5. **M5** – KILL (3 úrovně) s pravidly, WRITE PROTECT, BOOTLEG + testy pravidel.
6. **M6** – Preset systém (kanály, STAFF PICK, uživatelské presety, slot presety) + všech 96 presetů ve `factory.json`.
7. **M7** – UI skříně: layout, placeholder grafika, knoby, volič kanálů, PROGRAM, 8 kazet (výběr, tažení, pravý klik, WRITE PROTECT, PAUSE), video (displej, REC, REWIND/FAST FWD, EJECT, SIDE A/B), levý panel (KILL, PG/R/UNRATED, BOOTLEG, POCKET TV, AUTO TRACKING, BASIC/PREMIUM CABLE, STAFF PICK, ANTENNA IN/RF OUT), detail kazety, klávesové zkratky, škálování. TV zatím jen jednoduchý náhled.
8. **M8** – TV obrazovka: OpenGL + CRT shader, 12 procedurálních scén kanálů, AV1 REMOTE, AV2 SCRIBBLE (sync s transportem), AV3 SCREENSAVER (roh = glitch), OSD hlášky, animace KILL, OFF AIR, BASIC CABLE ztlumení, software fallback + testy modulace.
9. **M9** – pluginval, CPU optimalizace, Release build, README (instalace: `C:\Program Files\Common Files\VST3`).

## 15. Roadmapa (NEimplementovat ve v1.0, jen s tím počítat v architektuře)

- **v1.1 – nové efekty:** Heaven Mode (shimmer), Alter Ego (pitch/formant), Big Back (sub generátor), Jet Lag (flanger),
  NPC Glitch (ring mod), Shh (de-esser), Ice (exciter), Pump Fake (sidechain duck se sidechain vstupem), Toolbox (utility).
  Po přidání upravit presety označené „[1.1: …]“.
- **v1.2 – performance:** SHAPESHIFT (morph A↔B), VERSE / HOOK (2 stavy, přepínání MIDI notou 36/38),
  **KILL na MIDI notu**, MIDI learn, RENTAL CODE (krátký kód nastavení pro sdílení), pásmo na slot (efekt jen na část spektra).
- Architektura: registry efektů (přidání efektu = nová třída + registrace), verzování presetů/stavu, rezervovat ID parametrů hostu.
