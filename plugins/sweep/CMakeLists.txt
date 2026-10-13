cmake_minimum_required(VERSION 3.22)

# Single place for the version
project(TRAPWIN_SWEEP VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# macOS: universal binary, macOS 10.13+
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")

# Static MSVC runtime -> no VC++ redistributable needed
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(SWEEP_BUILD_PLUGIN "Build the plugin (needs JUCE)" ON)
option(SWEEP_BUILD_TESTS "Build the headless DSP tests" ON)

# Headless engine tests (plain C++, no JUCE)
if(SWEEP_BUILD_TESTS)
    add_executable(SweepTests tests/EngineTests.cpp Source/DSP/SweepEngine.cpp Source/Presets.cpp)
    target_include_directories(SweepTests PRIVATE Source)
endif()

if(NOT SWEEP_BUILD_PLUGIN)
    return()
endif()

# JUCE: local checkout (-DJUCE_SOURCE_DIR=...) or download
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

# Plugin + manufacturer codes are fixed forever: TwSw / Trpw
juce_add_plugin(Sweep
    COMPANY_NAME "TrapWin"
    COMPANY_WEBSITE "https://trapwin.com"
    BUNDLE_ID com.trapwin.sweep
    PLUGIN_MANUFACTURER_CODE Trpw
    PLUGIN_CODE TwSw
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "TrapWin SWEEP"
    DESCRIPTION "Tempo-synced filter modulation"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Filter
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(SweepAssets SOURCES
    Resources/bg.png
    Resources/knob_big.png
    Resources/knob_small.png
    Resources/fonts/ArchivoBlack-Regular.ttf
    Resources/fonts/BarlowCondensed-SemiBold.ttf
    Resources/fonts/BarlowCondensed-Bold.ttf
    Resources/fonts/ShareTechMono-Regular.ttf)

target_sources(Sweep PRIVATE
    Source/DSP/SweepEngine.cpp
    Source/Presets.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(Sweep PRIVATE Source)

target_compile_definitions(Sweep PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Sweep
    PRIVATE
        SweepAssets
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Store screenshot renderer (dev tool)
option(SWEEP_BUILD_TOOLS "Build dev tools" OFF)
if(SWEEP_BUILD_TOOLS)
    juce_add_console_app(SweepSnapshot PRODUCT_NAME "SweepSnapshot")
    target_sources(SweepSnapshot PRIVATE tools/Snapshot.cpp)
    target_include_directories(SweepSnapshot PRIVATE Source)
    target_compile_definitions(SweepSnapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
    target_link_libraries(SweepSnapshot PRIVATE Sweep SweepAssets juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()
