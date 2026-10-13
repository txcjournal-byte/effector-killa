# TrapWin DUST TAPE

Lo-fi & vintage color plugin (VST3 / AU / Standalone, Windows + macOS) – tape saturation,
wow & flutter, vinyl crackle & hiss, bitcrush, tone and the AGE macro. JUCE 8, C++17.

- `Source/DSP/DustEngine.*` – the sound engine (plain C++, no JUCE), tested headless by `tests/EngineTests.cpp`
- `Source/Presets.cpp` – 20 factory presets
- `.github/workflows/build.yml` – builds, tests, pluginval, auval, signs + notarizes the Mac build
  (same secrets as TrapWin Effector: MACOS_CERT_P12, MACOS_CERT_PASSWORD, MACOS_SIGN_IDENTITY and either
  APPLE_ID/APPLE_TEAM_ID/APPLE_APP_PASSWORD or APPLE_API_KEY_P8/APPLE_API_KEY_ID/APPLE_API_ISSUER_ID)
- Artifact **SHOP-TrapWin-DUST** = the two files for the shop (Windows zip, macOS zip)

Plugin codes (never change): manufacturer `Trpw`, plugin `TwDs`.
