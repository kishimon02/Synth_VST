cmake_minimum_required(VERSION 3.22)

project(WaveForge VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# JUCE (fetched on first configure; cached under build/_deps)
# ---------------------------------------------------------------------------
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------------------
# Plugin target
# ---------------------------------------------------------------------------
# VST3 copy destination. Studio One 4 only picks up VST3s under the system
# folder "C:/Program Files/Common Files/VST3" (custom locations did not work).
# That folder needs admin, so a vendor sub-folder is created once with
# elevation and given Modify rights for the current user (see README).
# Override with -DWAVEFORGE_VST3_DIR=...
if(NOT DEFINED WAVEFORGE_VST3_DIR)
    set(WAVEFORGE_VST3_DIR "$ENV{ProgramFiles}/Common Files/VST3/kishimon")
endif()
file(TO_CMAKE_PATH "${WAVEFORGE_VST3_DIR}" WAVEFORGE_VST3_DIR)

juce_add_plugin(WaveForge
    PRODUCT_NAME                "WaveForge"
    COMPANY_NAME                "kishimon"
    BUNDLE_ID                   "com.kishimon.waveforge"
    PLUGIN_MANUFACTURER_CODE    Kshm
    PLUGIN_CODE                 Wvfg
    FORMATS                     VST3 Standalone
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD     FALSE   # we copy the single-file form below
    VST3_CATEGORIES             Instrument Synth
)

# Studio One 4.x only scans single-file VST3s (Foo.vst3 as a DLL), not the
# newer bundle folder layout (Foo.vst3/Contents/x86_64-win/Foo.vst3) that JUCE
# produces. Copy the inner DLL directly as <dir>/WaveForge.vst3 after each build.
add_custom_command(TARGET WaveForge_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -DSRC=$<TARGET_FILE:WaveForge_VST3>
        -DDST=${WAVEFORGE_VST3_DIR}/WaveForge.vst3
        -P ${CMAKE_SOURCE_DIR}/cmake/InstallVst3.cmake
    COMMENT "Installing single-file VST3 to ${WAVEFORGE_VST3_DIR}/WaveForge.vst3"
    VERBATIM)

set(WAVEFORGE_AI_SOURCES
    src/ai/Settings.cpp
    src/ai/LlmClient.cpp
    src/ai/ParamCatalog.cpp
    src/ai/RequestPresets.cpp
    src/ai/ChordLibrary.cpp
    src/ai/MusicContext.cpp
    src/ai/MidiExport.cpp
    src/ai/ReplyFormat.cpp
    src/ai/ChatStore.cpp
)

set(WAVEFORGE_DSP_SOURCES
    src/dsp/Wavetable.cpp
    src/dsp/WavetableLoader.cpp
    src/dsp/Voice.cpp
    src/dsp/SynthEngine.cpp
    src/dsp/Arpeggiator.cpp
    src/dsp/fx/FxChain.cpp
)

target_sources(WaveForge PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/Params.cpp
    src/PresetManager.cpp
    src/ui/Panels.cpp
    src/ui/LineStack3DView.cpp
    src/ui/Wavetable3DView.cpp
    src/ui/WavetableEditor.cpp
    src/ui/ChordPage.cpp
    src/ui/ai/SettingsDialog.cpp
    src/ui/ai/AiPage.cpp
    src/ai/Assistant.cpp
    ${WAVEFORGE_AI_SOURCES}
    ${WAVEFORGE_DSP_SOURCES}
)

target_include_directories(WaveForge PRIVATE src)
if(WIN32)
    target_link_libraries(WaveForge PRIVATE crypt32)   # DPAPI for the API key
endif()

# Optional audio-thread instrumentation (writes %TEMP%/WaveForge_diag.log every
# 2 s: block sizes, output discontinuities, processing time, host call gaps).
# Enable with -DWAVEFORGE_DIAGNOSTICS=1 when investigating glitches.
# Note: changing this recompiles the whole target (JUCE modules included).
if(NOT DEFINED WAVEFORGE_DIAGNOSTICS)
    set(WAVEFORGE_DIAGNOSTICS 0)
endif()

target_compile_definitions(WaveForge PUBLIC
    WAVEFORGE_DIAGNOSTICS=${WAVEFORGE_DIAGNOSTICS}
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0                 # Windows: juce::URL uses WinHTTP for HTTPS
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_MODAL_LOOPS_PERMITTED=1
)

target_link_libraries(WaveForge
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_audio_formats
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_opengl
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Performance flags for MSVC release builds (dev machine supports AVX2).
if(MSVC)
    target_compile_options(WaveForge PRIVATE
        $<$<CONFIG:Release>:/O2 /fp:fast /arch:AVX2>
        /utf-8
    )
endif()

# ---------------------------------------------------------------------------
# DSP unit tests (console app, not part of the default build)
#   cmake --build --preset vs2022-release --target WaveForgeTests
# ---------------------------------------------------------------------------
juce_add_console_app(WaveForgeTests PRODUCT_NAME "WaveForgeTests")
set_target_properties(WaveForgeTests PROPERTIES EXCLUDE_FROM_ALL TRUE)

target_sources(WaveForgeTests PRIVATE
    tests/WavetableTests.cpp
    src/Params.cpp
    src/PresetManager.cpp
    ${WAVEFORGE_AI_SOURCES}
    ${WAVEFORGE_DSP_SOURCES}
)
target_include_directories(WaveForgeTests PRIVATE src)
if(WIN32)
    target_link_libraries(WaveForgeTests PRIVATE crypt32)
endif()
target_compile_definitions(WaveForgeTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=0
    JUCE_STANDALONE_APPLICATION=1
)
target_link_libraries(WaveForgeTests
    PRIVATE
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
if(MSVC)
    target_compile_options(WaveForgeTests PRIVATE /utf-8)
endif()
