cmake_minimum_required(VERSION 3.22)

# Single place for the version (shown in the plugin under ADVANCED > SETTINGS)
project(DRUM_KILLA VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# macOS: universal binary, macOS 11+
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

# Static MSVC runtime -> the plugin runs without installing VC++ redistributables
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(K808_BUILD_TESTS "Build the DSP unit tests" ON)
option(K808_LTO "Link-time optimisation (release builds; off = faster test builds)" ON)

# JUCE: use a local checkout if given (-DJUCE_SOURCE_DIR=...), otherwise download it
set(JUCE_SOURCE_DIR "" CACHE PATH "Optional local JUCE checkout")
if(JUCE_SOURCE_DIR)
    add_subdirectory(${JUCE_SOURCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.15.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# Plugin + manufacturer codes are fixed forever: TwDk / Trpw (TrapWin)
juce_add_plugin(K808
    COMPANY_NAME "TrapWin"
    COMPANY_WEBSITE "https://trapwin.com"
    BUNDLE_ID com.trapwin.drumkilla
    PLUGIN_MANUFACTURER_CODE Trpw
    PLUGIN_CODE TwDk
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "DRUM KILLA"
    DESCRIPTION "Drum damage unit"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Distortion
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(K808Assets SOURCES
    Resources/background.jpg
    Resources/jaw.png
    Resources/knob_face.png
    Resources/ShareTech-Regular.ttf
    Resources/ShareTechMono-Regular.ttf
    Resources/Rajdhani-Bold.ttf)

set(K808_DSP_SOURCES
    Source/DSP/Engine.cpp
    Source/Parameters.cpp
    Source/Presets.cpp)

target_sources(K808 PRIVATE
    ${K808_DSP_SOURCES}
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/UI/Look.cpp
    Source/UI/Widgets.cpp)

target_include_directories(K808 PRIVATE Source)

target_compile_definitions(K808 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(K808
    PRIVATE
        K808Assets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

if(K808_LTO)
    target_link_libraries(K808 PUBLIC juce::juce_recommended_lto_flags)
endif()

# Windows: keep symbols for Release so crash reports show function names
if(MSVC)
    target_compile_options(K808 PRIVATE /Z7)
    target_link_options(K808_VST3 PRIVATE /DEBUG /OPT:REF /OPT:ICF)
endif()

if(K808_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# Store screenshot renderer (dev tool)
option(DK_BUILD_TOOLS "Build dev tools" OFF)
if(DK_BUILD_TOOLS)
    juce_add_console_app(DrumKillaSnapshot PRODUCT_NAME "DrumKillaSnapshot")
    target_sources(DrumKillaSnapshot PRIVATE tools/Snapshot.cpp)
    target_include_directories(DrumKillaSnapshot PRIVATE Source)
    target_compile_definitions(DrumKillaSnapshot PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
    target_link_libraries(DrumKillaSnapshot PRIVATE K808 K808Assets juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
