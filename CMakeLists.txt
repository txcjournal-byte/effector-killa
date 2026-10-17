# =====================================================================================
#  TrapWin BEND DEMON - MPE gesture instrument host
#  JUCE 8 / CMake. JUCE is fetched from GitHub with FetchContent (or use -DJUCE_DIR=...).
# =====================================================================================
cmake_minimum_required(VERSION 3.22)

# Universal binary by default on macOS (override with -DCMAKE_OSX_ARCHITECTURES=arm64 for speed)
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures" FORCE)
endif()
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")

project(BendDemon VERSION 1.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(MSVC)
    # Static runtime: no VC++ redistributable needed on users' machines
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# -------------------------------------------------------------------------------------
# Options
# -------------------------------------------------------------------------------------
set(BEND_DEMON_JUCE_TAG "8.0.9" CACHE STRING "JUCE git tag to fetch")
option(BEND_DEMON_BUILD_TESTS  "Build the engine unit tests (BendDemonTests console app)" ON)
option(BEND_DEMON_BUILD_PLUGIN "Build the plugin targets (VST3/AU/Standalone)" ON)

if(APPLE)
    set(_bd_gl_default ON)   # CoreGraphics is slow for many glowing paths - render via OpenGL on macOS
else()
    set(_bd_gl_default OFF)  # Windows: JUCE 8 renders through Direct2D (GPU) anyway
endif()
option(BEND_DEMON_USE_OPENGL "Attach a juce::OpenGLContext to the editor" ${_bd_gl_default})

# Minimal-dependency Linux build (used for CI / container verification; Linux is not a shipping target)
option(BEND_DEMON_LINUX_MINIMAL "Linux: disable ALSA/JACK/curl/webkit/Xrandr/Xinerama/Xcursor" OFF)

# -------------------------------------------------------------------------------------
# JUCE
# -------------------------------------------------------------------------------------
if(UNIX AND NOT APPLE AND BEND_DEMON_LINUX_MINIMAL)
    # juceaide (JUCE's build helper) is configured in a child CMake process that only
    # inherits the environment, so pass the "no optional X11 extensions" switches that way.
    set(ENV{CXXFLAGS} "$ENV{CXXFLAGS} -DJUCE_USE_XRANDR=0 -DJUCE_USE_XINERAMA=0 -DJUCE_USE_XCURSOR=0")
endif()

if(DEFINED JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(STATUS "Using local JUCE at ${JUCE_DIR}")
    add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        ${BEND_DEMON_JUCE_TAG}
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# -------------------------------------------------------------------------------------
# Embedded fonts (SIL Open Font License - see resources/fonts/OFL-*.txt)
# -------------------------------------------------------------------------------------
juce_add_binary_data(BendDemonBinaryData
    NAMESPACE BinaryData
    HEADER_NAME BinaryData.h
    SOURCES
        resources/fonts/Unbounded-Medium.ttf
        resources/fonts/Unbounded-Bold.ttf
        resources/fonts/Unbounded-Black.ttf
        resources/fonts/Inter-Medium.ttf
        resources/fonts/Inter-SemiBold.ttf)

if(UNIX AND NOT APPLE)
    set_target_properties(BendDemonBinaryData PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

# -------------------------------------------------------------------------------------
# Common compile definitions
# -------------------------------------------------------------------------------------
set(BD_COMMON_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_PLUGINHOST_VST3=1
    JUCE_MODAL_LOOPS_PERMITTED=0
    JUCE_VST3_CAN_REPLACE_VST2=0)   # BEND DEMON never ships a VST2 -> clean VST3 parameter IDs

if(APPLE)
    list(APPEND BD_COMMON_DEFS JUCE_PLUGINHOST_AU=1)
endif()

if(UNIX AND NOT APPLE AND BEND_DEMON_LINUX_MINIMAL)
    list(APPEND BD_COMMON_DEFS
        JUCE_ALSA=0 JUCE_JACK=0
        JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0)
endif()

set(BD_ENGINE_SOURCES
    src/engine/EngineTypes.h
    src/engine/ScaleLock.h          src/engine/ScaleLock.cpp
    src/engine/VoiceAllocator.h     src/engine/VoiceAllocator.cpp
    src/engine/MpeOutput.h          src/engine/MpeOutput.cpp
    src/engine/GestureEngine.h      src/engine/GestureEngine.cpp
    src/engine/Pattern.h            src/engine/Pattern.cpp
    src/engine/PatternExchange.h
    src/engine/PatternPlayer.h      src/engine/PatternPlayer.cpp
    src/engine/PatternTools.h       src/engine/PatternTools.cpp
    src/engine/PatternFactory.cpp
    src/engine/CaptureFifo.h
    src/engine/CaptureRecorder.h    src/engine/CaptureRecorder.cpp
    src/engine/InternalSynth.h      src/engine/InternalSynth.cpp)

# -------------------------------------------------------------------------------------
# Plugin
# -------------------------------------------------------------------------------------
if(BEND_DEMON_BUILD_PLUGIN)
    set(BD_FORMATS VST3 Standalone)
    if(APPLE)
        list(APPEND BD_FORMATS AU)
    endif()

    juce_add_plugin(BendDemon
        PRODUCT_NAME                "TrapWin BEND DEMON"
        COMPANY_NAME                "TrapWin"
        COMPANY_WEBSITE             "https://trapwin.com"
        COMPANY_EMAIL               ""
        BUNDLE_ID                   "com.trapwin.benddemon"
        VERSION                     ${PROJECT_VERSION}
        PLUGIN_MANUFACTURER_CODE    Trwn
        PLUGIN_CODE                 BdDm
        FORMATS                     ${BD_FORMATS}
        IS_SYNTH                    TRUE
        NEEDS_MIDI_INPUT            TRUE
        NEEDS_MIDI_OUTPUT           TRUE
        IS_MIDI_EFFECT              FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        VST3_CATEGORIES             Instrument Synth
        AU_MAIN_TYPE                kAudioUnitType_MusicDevice
        DESCRIPTION                 "MPE gesture engine that hosts your synth"
        MICROPHONE_PERMISSION_ENABLED FALSE
        HARDENED_RUNTIME_ENABLED    TRUE
        # Hosting third-party plugins inside a hardened-runtime app needs these:
        HARDENED_RUNTIME_OPTIONS
            com.apple.security.cs.disable-library-validation
            com.apple.security.cs.allow-unsigned-executable-memory
            com.apple.security.cs.allow-jit
        COPY_PLUGIN_AFTER_BUILD     FALSE)

    juce_generate_juce_header(BendDemon)

    set(BD_PLUGIN_SOURCES
        ${BD_ENGINE_SOURCES}
        src/Parameters.h                src/Parameters.cpp
        src/PluginProcessor.h           src/PluginProcessor.cpp
        src/PluginEditor.h              src/PluginEditor.cpp
        src/PatternStore.h              src/PatternStore.cpp
        src/VisualFifo.h
        src/host/HostedPluginManager.h  src/host/HostedPluginManager.cpp
        src/presets/PresetManager.h     src/presets/PresetManager.cpp
        src/ui/Palette.h
        src/ui/Fonts.h                  src/ui/Fonts.cpp
        src/ui/LookAndFeel.h            src/ui/LookAndFeel.cpp
        src/ui/GlowKnob.h               src/ui/GlowKnob.cpp
        src/ui/GesturePad.h             src/ui/GesturePad.cpp
        src/ui/TopBar.h                 src/ui/TopBar.cpp
        src/ui/VoiceField.h             src/ui/VoiceField.cpp
        src/ui/VoiceFieldDraw.cpp
        src/ui/LineEditor.h             src/ui/LineEditor.cpp
        src/ui/ExpressionLane.h         src/ui/ExpressionLane.cpp
        src/ui/EditToolbar.h            src/ui/EditToolbar.cpp
        src/ui/Inspector.h              src/ui/Inspector.cpp
        src/ui/SynthBrowser.h           src/ui/SynthBrowser.cpp)

    target_sources(BendDemon PRIVATE ${BD_PLUGIN_SOURCES})

    target_include_directories(BendDemon PRIVATE src)

    target_compile_definitions(BendDemon PUBLIC ${BD_COMMON_DEFS}
        BEND_DEMON_USE_OPENGL=$<BOOL:${BEND_DEMON_USE_OPENGL}>)

    set(BD_JUCE_MODULES
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_extra
        juce::juce_dsp)

    if(BEND_DEMON_USE_OPENGL)
        list(APPEND BD_JUCE_MODULES juce::juce_opengl)
    endif()

    target_link_libraries(BendDemon
        PRIVATE
            BendDemonBinaryData
            ${BD_JUCE_MODULES}
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endif()

# -------------------------------------------------------------------------------------
# Engine unit tests (console app, no GUI / audio device dependencies)
# -------------------------------------------------------------------------------------
if(BEND_DEMON_BUILD_TESTS)
    juce_add_console_app(BendDemonTests PRODUCT_NAME "BendDemonTests")

    target_sources(BendDemonTests PRIVATE
        ${BD_ENGINE_SOURCES}
        tests/TestMain.cpp
        tests/EngineTests.cpp)

    target_include_directories(BendDemonTests PRIVATE src)
    target_compile_definitions(BendDemonTests PRIVATE
        JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0 JUCE_STANDALONE_APPLICATION=1)

    target_link_libraries(BendDemonTests
        PRIVATE
            juce::juce_core
            juce::juce_audio_basics
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)

    enable_testing()
    add_test(NAME BendDemonEngineTests COMMAND BendDemonTests)
endif()

# -------------------------------------------------------------------------------------
# Dev tool: headless UI snapshot app (renders the editor to PNG while a melody plays).
# Off by default; used for UI checks on CI / under Xvfb.
# -------------------------------------------------------------------------------------
# Dev tools (off by default, not shipped):
#   BendDemonSnapshot  - renders the editor to PNG while a melody plays (UI check under Xvfb / CI)
#   BendDemonHostTest  - loads a VST3 through HostedPluginManager, checks MPE pitch end to end + state restore
#   MpeProbe (VST3)    - minimal MPE sine synth used as the hosted plugin in that test
option(BEND_DEMON_BUILD_DEV_TOOLS "Build dev tools (snapshot app, host test, MPE probe VST3)" OFF)

if(BEND_DEMON_BUILD_DEV_TOOLS AND BEND_DEMON_BUILD_PLUGIN)
    foreach(tool BendDemonSnapshot BendDemonHostTest)
        juce_add_gui_app(${tool} PRODUCT_NAME "${tool}")
        # Compiles its own copy of the processor/editor (not the plugin wrapper).
        target_sources(${tool} PRIVATE ${BD_PLUGIN_SOURCES})
        target_include_directories(${tool} PRIVATE src)
        target_compile_definitions(${tool} PRIVATE ${BD_COMMON_DEFS}
            BEND_DEMON_USE_OPENGL=0
            JucePlugin_Name="TrapWin BEND DEMON"
            JucePlugin_VersionString="${PROJECT_VERSION}")
        target_link_libraries(${tool} PRIVATE
            BendDemonBinaryData
            juce::juce_audio_utils juce::juce_audio_processors juce::juce_gui_extra juce::juce_dsp
            juce::juce_recommended_config_flags)
    endforeach()
    target_sources(BendDemonSnapshot PRIVATE tools/SnapshotApp.cpp)
    target_sources(BendDemonHostTest PRIVATE tools/HostTestApp.cpp)

    juce_add_plugin(MpeProbe
        PRODUCT_NAME "MPE Probe" COMPANY_NAME "TrapWin Dev" BUNDLE_ID "com.trapwin.mpeprobe"
        PLUGIN_MANUFACTURER_CODE Trwn PLUGIN_CODE MpPr FORMATS VST3
        IS_SYNTH TRUE NEEDS_MIDI_INPUT TRUE VST3_CATEGORIES Instrument Synth)
    target_sources(MpeProbe PRIVATE tools/MpeProbePlugin.cpp)
    target_compile_definitions(MpeProbe PUBLIC ${BD_COMMON_DEFS})
    target_link_libraries(MpeProbe PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)
endif()
