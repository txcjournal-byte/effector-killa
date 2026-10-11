# TrapWin Effector – předávací zadání (stav k 11. 10. 2026, večer)

Repo: `txcjournal-byte/effector-killa`, větev `claude/exciting-knuth-lxevz2` (vše pushnuté).
Spec: `CLAUDE.md` (původní zadání „Effector Killa“), produkt je přejmenovaný na **TrapWin Effector**.

## Hotovo
- Plugin (JUCE 8, VST3 + Standalone, AU na macOS), 15 efektů, 96 presetů (popisy anglicky), KILL, makra.
- **Nové UI ve stylu racku (11. 10.)** místo VHS skříně/TV: horní panel (presety + prohlížeč s hledáním,
  A/B, undo, tempo HOST/MANUAL/TAP, Input/Recycle/Mix/Output s metry, KILL), lišta maker a přepínačů,
  rack jednotek pod sebou se všemi knoby (preset jednotky, POWER s doznívajícími ozvěnami, SOLO, ST/MID/SIDE,
  LOCK, ✕, •••), GEAR seznam s přetahováním (vložit / nahradit / přidat / přesun / zpět = odebrat),
  velikost okna Auto / 80–150 % / tažení za roh. Funkce srovnané s Effect Rack (Soundtoys) a lepší;
  jejich grafiku, názvy ani zvuk nekopírujeme.
- Jádro: RECYCLE (zpětná smyčka, testovaná na stabilitu), SOLO jedné jednotky, tempo MANUAL/TAP,
  názvy parametrů v DAW podle racku („S1: Cassette Plug: Drive“) s pevnými ID.
- Oficiální logo TrapWin (`Resources/Brand/trapwin_logo.png`, z repa trapwin-limiter) – nikdy nepřekreslovat textem.
- Obrázek do obchodu z 2× renderu UI: `Tools/Snapshot.cpp` (00_cover_ui.png) + `Tools/make_cover.py`.
- Lokální sestavení v cloudu: JUCE 8.0.4 klon + `CXXFLAGS="-DJUCE_USE_XRANDR=0 -DJUCE_USE_XINERAMA=0
  -DJUCE_USE_XCURSOR=0 -DJUCE_USE_XSHM=0 -DJUCE_USE_XRENDER=0"`, `-DEK_BUILD_TOOLS=ON`; snapshot běží bez displeje.
- CI hlásí chyby kompilátoru jako anotace (logy nejdou přes proxy stáhnout, anotace přes `gh api .../check-runs/ID/annotations` ano).
- CI `.github/workflows/build.yml`: Windows / macOS (universal) / Linux, testy, pluginval, Windows instalátor (Inno Setup),
  volitelný macOS podpis+notarizace (secrets), artefakt **trapwin.com** (instalátor, mac zip, Docs, Shop/cover).
- Shopify (obchod TrapWin, trapwin.com): koncept produktu `gid://shopify/Product/11222681649490`,
  varianta `gid://shopify/ProductVariant/55018289660242`, cena 59 USD, SKU TW-PL-01, popis hotový (Windows teď, macOS „coming soon“).

## Zbývá
1. Zkontrolovat poslední běh CI (commit 668e5f9 nebo novější). Když selže → logy, oprava, push, opakovat do zelené.
2. Poslat uživateli přímý odkaz na artefakt `trapwin.com` (stáhne a rozbalí na plochu – z cloudu to na jeho PC uložit nejde).
3. Shopify: `upload-digital-product-file` (soubor `Windows/TrapWinEffector-1.0.0-Windows-Setup.exe`) a
   `upload-digital-product-media` (`Shop/TrapWinEffector-cover.png`) – uživatel jen vybere soubory v okně.
   Popis konceptu je už přepsaný na rack (macOS „coming soon“). Titulek v konceptu je „(VST3 / AU)“ –
   dokud není macOS podepsaný, zveřejnit s „(VST3)“, nebo počkat na macOS.
   Pak `publish-digital-product` a `get-digital-product` pro kontrolu.
4. macOS: uživatel UŽ MÁ Apple Developer účet (stačí se přihlásit, uživatel potvrdí 2FA).
   Na Windows PC: vytvořit CSR + klíč přes openssl, na developer.apple.com → Certificates vytvořit
   „Developer ID Application“, stáhnout .cer, spojit s klíčem do .p12 (openssl pkcs12), na appleid.apple.com
   vytvořit app-specific password, Team ID z Membership. Pak doplnit GitHub Secrets (MACOS_CERT_P12, MACOS_CERT_PASSWORD,
   MACOS_SIGN_IDENTITY, APPLE_ID, APPLE_TEAM_ID, APPLE_APP_PASSWORD), ověřit podepsaný build, pak přidat macOS do Shopify
   (nahrát mac zip, upravit popis a titulek na „(VST3 / AU)“).
5. Volitelně: Windows code-signing certifikát (SmartScreen), EULA doplnit údaje firmy, licence JUCE (ověřit podmínky na juce.com).


## Stav 11. 10. 2026, noc – nový vzhled TrapWin + funkce priority 1
- **Vzhled podle trapwin.com** (`Docs/TRAPWIN-STYLE.md`, má přednost před hardware-rack částmi DESIGN-RACK.md):
  žádné dřevo, rack ucha ani vintage panely. Okno 1360×900 (bez knihovny 1124): hlavička (logo, preset, A/B, undo, ⚙, BYPASS),
  vlevo FX LIBRARY, uprostřed MOD karta + řetězec karet se svítící signálovou linkou, vpravo MASTER (tempo, úrovně, KILL, makra, LED metry).
  Karty efektů: `Faceplates*.cpp` + `CardStyle.*`; řetězec `RackView`/`UnitControls`; okno `PluginEditor`, `Toolbar`, `MasterColumn`.
- **Nové funkce:** MOD SOURCES (2 LFO, ENV, STEP SEQ, kabel přetažením na knob, prstenec = hloubka), MACRO LEARN (pravý klik na knob,
  rozsah tažením), SPLIT A/B (5 režimů, MERGE), SPECTRAL VIEW (najetí na kartu), LEVEL-MATCHED POWER (podržení ⏻).
  Engine: `ModSources.*`, `Split.*`, `SlotTap.*`, `UnitTaps.h`. Testy: 0 chyb (pozor: `Tests/Main.cpp` dřív skrýval chyby – opraveno).
- Obrázek do obchodu `Resources/Marketing/TrapWinEffector-cover.png` (nový UI, „VST3 / AU · WINDOWS & macOS“).
  `Tools/make_cover.py` bere hlavičku ze starého obrázku; spodní text se po něm musí přepsat (viz historie session).
- Lokální build v cloudu navíc potřebuje `-DJUCE_ALSA=0 -DJUCE_JACK=0` a snapshoty běží přes `xvfb-run -a`.
- **macOS podpis:** secrets v GitHubu (MACOS_CERT_P12, MACOS_CERT_PASSWORD, APPLE_TEAM_ID, APPLE_API_KEY_P8/ID, APPLE_API_ISSUER_ID);
  identita se bere z .p12, notarizace přes App Store Connect API klíč. Klíče zálohované v Dokumenty\TrapWin-signing (NESDÍLET).
- Známé drobnosti: rack interně zpožďuje o 1 vzorek méně, než hlásí (starší chyba, oprava by změnila zvuk presetů);
  „Reduce motion“ a CPU UI ověřit ve FL Studiu; Archivo italic je zatím zkosená Archivo Black.

## Rozhodnutí, která lze změnit
Cena 59 USD; macOS zdarma pro dřívější kupce (věta v popisu); factory presety srovnané na hlasitost (kromě CH 12).
