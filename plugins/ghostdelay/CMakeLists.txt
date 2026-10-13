cmake_minimum_required(VERSION 3.22)

# Single place for the version
project(TRAPWIN_GHOSTDELAY VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# macOS: universal binary, macOS 10.13+
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")

# Static MSVC runtime -> no VC++ redistributable needed
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(GHOSTDELAY_BUILD_PLUGIN "Build the plugin (needs JUCE)" ON)
option(GHOSTDELAY_BUILD_TESTS "Build the headless DSP tests" ON)

# Headless engine tests (plain C++, no JUCE)
if(GHOSTDELAY_BUILD_TESTS)
    add_executable(GhostDelayTests tests/EngineTests.cpp Source/DSP/GhostEngine.cpp Source/Presets.cpp)
    target_include_directories(GhostDelayTests PRIVATE Source)
endif()

if(NOT GHOSTDELAY_BUILD_PLUGIN)
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

# Plugin + manufacturer codes are fixed forever: TwGd / Trpw
juce_add_plugin(GhostDelay
    COMPANY_NAME "TrapWin"
    COMPANY_WEBSITE "https://trapwin.com"
    BUNDLE_ID com.trapwin.ghostdelay
    PLUGIN_MANUFACTURER_CODE Trpw
    PLUGIN_CODE TwGd
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "TrapWin GHOST DELAY"
    DESCRIPTION "Multi-mode creative delay"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Delay
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(GhostDelayAssets SOURCES
    Resources/bg.png
    Resources/knob_big.png
    Resources/knob_small.png
    Resources/fonts/ArchivoBlack-Regular.ttf
    Resources/fonts/BarlowCondensed-SemiBold.ttf
    Resources/fonts/BarlowCondensed-Bold.ttf
    Resources/fonts/ShareTechMono-Regular.ttf)

target_sources(GhostDelay PRIVATE
    Source/DSP/GhostEngine.cpp
    Source/Presets.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(GhostDelay PRIVATE Source)

target_compile_definitions(GhostDelay PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(GhostDelay
    PRIVATE
        GhostDelayAssets
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Store screenshot renderer (dev tool)
option(GHOSTDELAY_BUILD_TOOLS "Build dev tools" OFF)
if(GHOSTDELAY_BUILD_TOOLS)
    juce_add_console_app(GhostDelaySnapshot PRODUCT_NAME "GhostDelaySnapshot")
    target_sources(GhostDelaySnapshot PRIVATE tools/Snapshot.cpp)
    target_include_directories(GhostDelaySnapshot PRIVATE Source)
    target_compile_definitions(GhostDelaySnapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
    target_link_libraries(GhostDelaySnapshot PRIVATE GhostDelay GhostDelayAssets juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()
