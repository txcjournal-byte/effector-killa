# TrapWin Effector – předávací zadání (stav k 11. 10. 2026)

Repo: `txcjournal-byte/effector-killa`, větev `claude/exciting-knuth-lxevz2` (vše pushnuté).
Spec: `CLAUDE.md` (původní zadání „Effector Killa“), produkt je přejmenovaný na **TrapWin Effector**.

## Hotovo
- Plugin (JUCE 8, VST3 + Standalone, AU na macOS), 15 efektů, 96 presetů, KILL, makra, TV (OpenGL + software).
- Design podle trapwin.com (černá / chrom / ledová modř `#9FC3FF`, fonty Archivo Black, IBM Plex Mono, Permanent Marker).
- Kazety = „filmy z flashky“: klik na kazetu → seznam 8 programů (Resources/Presets/tapes.json, Tools/make_tapes.py).
- CI `.github/workflows/build.yml`: Windows / macOS (universal) / Linux, testy, pluginval, Windows instalátor (Inno Setup),
  volitelný macOS podpis+notarizace (secrets), artefakt **trapwin.com** (instalátor, mac zip, Docs, Shop/cover).
- Shopify (obchod TrapWin, trapwin.com): koncept produktu `gid://shopify/Product/11222681649490`,
  varianta `gid://shopify/ProductVariant/55018289660242`, cena 59 USD, SKU TW-PL-01, popis hotový (Windows teď, macOS „coming soon“).

## Zbývá
1. Zkontrolovat poslední běh CI (commit 668e5f9 nebo novější). Když selže → logy, oprava, push, opakovat do zelené.
2. Poslat uživateli přímý odkaz na artefakt `trapwin.com` (stáhne a rozbalí na plochu – z cloudu to na jeho PC uložit nejde).
3. Shopify: `upload-digital-product-file` (soubor `Windows/TrapWinEffector-1.0.0-Windows-Setup.exe`) a
   `upload-digital-product-media` (`Shop/TrapWinEffector-cover.png`) – uživatel jen vybere soubory v okně.
   Pak `publish-digital-product` s titulkem `TRAPWIN EFFECTOR - Trap Multi-Effect Plugin (VST3)` a `get-digital-product` pro kontrolu.
4. macOS: až uživatel dodá Apple Developer certifikát → doplnit GitHub Secrets (MACOS_CERT_P12, MACOS_CERT_PASSWORD,
   MACOS_SIGN_IDENTITY, APPLE_ID, APPLE_TEAM_ID, APPLE_APP_PASSWORD), ověřit podepsaný build, pak přidat macOS do Shopify
   (nahrát mac zip, upravit popis a titulek na „(VST3 / AU)“).
5. Volitelně: Windows code-signing certifikát (SmartScreen), EULA doplnit údaje firmy, licence JUCE (ověřit podmínky na juce.com).

## Rozhodnutí, která lze změnit
Cena 59 USD; macOS zdarma pro dřívější kupce (věta v popisu); factory presety srovnané na hlasitost (kromě CH 12).
