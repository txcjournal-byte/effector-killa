# Licence notes: read before you sell BEND DEMON

> **IMPORTANT: JUCE licensing.**
> BEND DEMON is built on **JUCE 8**, which is **dual-licensed**:
>
> 1. **AGPLv3**: free, but then BEND DEMON's *complete source code* must be released under the
>    AGPLv3 to everyone who receives the plugin. That is incompatible with selling it closed-source.
> 2. **JUCE commercial licence** (Starter / Indie / Pro, see <https://juce.com/get-juce/>):
>    lets you ship closed-source. Which tier you need depends on TrapWin's annual revenue/funding
>    (the Starter tier has a revenue cap; Indie and Pro have higher or no caps).
>
> **To sell BEND DEMON as a closed-source commercial product, TrapWin must buy a JUCE licence
> before the first public release.** Check the current terms and revenue limits on juce.com:
> they change between JUCE versions. This document is not legal advice.

## Third-party components

| Component | Licence | Notes |
|---|---|---|
| JUCE 8.0.9 (fetched at build time) | AGPLv3 **or** JUCE commercial licence | See above. JUCE 8 has no "Made with JUCE" splash screen. |
| Steinberg VST3 SDK (bundled inside JUCE 8.0.9 at `modules/juce_audio_processors/format_types/VST3_SDK`) | BSD-style 3-clause licence (`LICENSE.txt` in that folder, "(c) 2024 Steinberg") | Binary distributions must **reproduce Steinberg's copyright notice + licence text** in the documentation. Add it to the installer licence screen / an About box. "VST" is a Steinberg trademark: follow Steinberg's VST logo/usage guidelines on packaging and the website. |
| Apple Audio Unit SDK | Part of macOS SDK / Xcode | – |
| Unbounded font (Medium, Bold, Black) | SIL Open Font License 1.1 | Embedded; licence text in `resources/fonts/OFL-Unbounded.txt` |
| JetBrains Mono font (Regular, Bold) | SIL Open Font License 1.1 | Embedded; licence text in `resources/fonts/OFL-JetBrainsMono.txt` |

The OFL allows embedding the fonts in commercial software. Keep the licence texts with the
distribution (they are in the source tree; an "About / Licences" note in the installer covers the
binary distribution: see `scripts/installer/*/license.txt`).

## Hosted plugins

BEND DEMON loads third-party plugins chosen by the user. It does not redistribute them. Mentioning
Serum, Vital, Pigments or Phase Plant by name in documentation is descriptive use. Don't use their
logos without permission.

## Your own licence

The final EULA is in `EULA.txt` and is copied into both installers
(`scripts/installer/mac-resources/license.txt`, `scripts/installer/license-win.txt`). It includes the
Steinberg VST3 SDK notice and font notices. TrapWin holds a commercial JUCE licence.

**Steinberg:** the bundled VST3 SDK (2024 licence) requires a signed Steinberg VST3 License Agreement
before publishing a proprietary VST3 plugin. If TrapWin already sells VST3 plugins this is normally
already in place; otherwise request it at https://www.steinberg.net/en/company/developers.html.
