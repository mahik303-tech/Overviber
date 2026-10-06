cmake_minimum_required(VERSION 3.22)
project(Overviber VERSION 0.11.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==============================================================================
# Cross-Platform Target Architectures & Deployment Flags
# ==============================================================================
if(APPLE)
    # std::filesystem (PresetManager, WaveManager) requires macOS 10.15.
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS deployment target" FORCE)
    endif()
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary architectures" FORCE)
    endif()
endif()

# Installing into the system plug-in folders (Windows: Common Files\VST3)
# needs administrator rights, so it is opt-in:
#   cmake -B build -DJUCE_COPY_PLUGIN_AFTER_BUILD=ON
# Declared before add_subdirectory(juce). An existing cache entry keeps its
# value; reconfigure with -D...=OFF/ON to change it in an existing build tree.
option(JUCE_COPY_PLUGIN_AFTER_BUILD
    "Whether or not plugins should be installed to the system after building" OFF)

add_subdirectory(juce)

# ==============================================================================
# Embedded Typography Resources (D-DIN SIL Open Font License 1.1)
# ==============================================================================
juce_add_binary_data(OvercyclerFonts
    SOURCES
        vst/Source/ui/theme/fonts/D-DIN.ttf
        vst/Source/ui/theme/fonts/D-DIN-Bold.ttf
)

# ==============================================================================
# Lua 5.4 for the Modern skin's LUA tab (vst/Source/lua, MIT license)
# ==============================================================================
# Built as C++, so a Lua error unwinds the binding functions with an exception
# (destructors run) instead of a longjmp. Without the io, os, package and debug
# libraries (see vst/Source/lua/README.md).
file(GLOB OVERVIBER_LUA_SOURCES CONFIGURE_DEPENDS vst/Source/lua/*.c)
set_source_files_properties(${OVERVIBER_LUA_SOURCES} PROPERTIES LANGUAGE CXX)
add_library(OverviberLua STATIC ${OVERVIBER_LUA_SOURCES})
target_include_directories(OverviberLua SYSTEM PUBLIC vst/Source/lua)
target_link_libraries(OverviberLua PRIVATE juce::juce_recommended_config_flags)
target_compile_options(OverviberLua PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/W0,-w>)
set_target_properties(OverviberLua PROPERTIES POSITION_INDEPENDENT_CODE ON LINKER_LANGUAGE CXX)
if(UNIX AND NOT APPLE)
    target_compile_options(OverviberLua PRIVATE -fvisibility=hidden)
endif()

# The LUA tab's built-in example script
juce_add_binary_data(OverviberLuaScripts
    HEADER_NAME LuaScripts.h
    NAMESPACE LuaScripts
    SOURCES
        vst/Source/ui/lua/LuaDefaultSkin.lua
        vst/Source/ui/lua/pages/lib.lua
        vst/Source/ui/lua/pages/osc.lua
        vst/Source/ui/lua/pages/filter.lua
        vst/Source/ui/lua/pages/env.lua
        vst/Source/ui/lua/pages/lfo.lua
        vst/Source/ui/lua/pages/afx.lua
        vst/Source/ui/lua/pages/matrix.lua
        vst/Source/ui/lua/pages/settings.lua
)

# Supported plugin formats: VST3 & Standalone by default, + AU on macOS
set(OVERVIBER_PLUGIN_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND OVERVIBER_PLUGIN_FORMATS AU)
endif()

# Mutable Instruments Elements Modal Synthesis Sources
set(ELEMENTS_SOURCES
    vst/Source/dsp/audible/stmlib/dsp/units.cc
    vst/Source/dsp/audible/stmlib/utils/random.cc
    vst/Source/dsp/audible/elements/resources.cc
    vst/Source/dsp/audible/elements/dsp/exciter.cc
    vst/Source/dsp/audible/elements/dsp/multistage_envelope.cc
    vst/Source/dsp/audible/elements/dsp/ominous_voice.cc
    vst/Source/dsp/audible/elements/dsp/part.cc
    vst/Source/dsp/audible/elements/dsp/resonator.cc
    vst/Source/dsp/audible/elements/dsp/string.cc
    vst/Source/dsp/audible/elements/dsp/tube.cc
    vst/Source/dsp/audible/elements/dsp/voice.cc
)

# Third-party DSP (Mutable Instruments Elements and stmlib, Ripples, Shelves)
# in vst/Source/dsp/audible, wrapped by ElementsOsc.h, RipplesFilter.h and
# ShelvesFilter.h. The Elements sources are built once, for the plugin, the
# skin designer and the tests, without warnings; the directory is a system /
# external include directory, so its headers (included with <...>) report no
# warnings in our code either. Under MSVC /W4 they made up several thousand
# lines of every Windows build log.
add_library(OverviberElements STATIC ${ELEMENTS_SOURCES})
target_include_directories(OverviberElements SYSTEM PUBLIC vst/Source/dsp/audible)
target_link_libraries(OverviberElements PRIVATE juce::juce_recommended_config_flags)
target_compile_options(OverviberElements PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/W0,-w>)
set_target_properties(OverviberElements PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(UNIX AND NOT APPLE)
    target_compile_options(OverviberElements PRIVATE -fvisibility=hidden)
endif()

juce_add_plugin(Overviber
    COMPANY_NAME "Overviber"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    PLUGIN_MANUFACTURER_CODE Ovbr
    PLUGIN_CODE Ovbr
    FORMATS ${OVERVIBER_PLUGIN_FORMATS}
    PRODUCT_NAME "Overviber"
    BUNDLE_ID "com.overviber.synth"
    MICROPHONE_PERMISSION_ENABLED FALSE
)

target_sources(Overviber PRIVATE
    vst/Source/PluginProcessor.h
    vst/Source/PluginProcessor.cpp
    vst/Source/dsp/OvercyclerTypes.h
    vst/Source/dsp/ConsoleXProcessor.h
    vst/Source/dsp/AfxKit.h
    vst/Source/dsp/LookupTables.h
    vst/Source/dsp/wtosc.h
    vst/Source/dsp/wtosc.cpp
    vst/Source/dsp/adsr.h
    vst/Source/dsp/adsr.cpp
    vst/Source/dsp/lfo.h
    vst/Source/dsp/lfo.cpp
    vst/Source/dsp/assigner.h
    vst/Source/dsp/assigner.cpp
    vst/Source/dsp/arp.h
    vst/Source/dsp/arp.cpp
    vst/Source/dsp/Ssi2144Filter.h
    vst/Source/dsp/SstLadderFilter.h
    vst/Source/dsp/audible/RackSimd.h
    vst/Source/dsp/audible/sos.hpp
    vst/Source/dsp/audible/ripples_aafilter.hpp
    vst/Source/dsp/audible/ripples.hpp
    vst/Source/dsp/audible/RipplesFilter.h
    vst/Source/dsp/audible/shelves_aafilter.hpp
    vst/Source/dsp/audible/shelves.hpp
    vst/Source/dsp/audible/ShelvesFilter.h
    vst/Source/dsp/audible/ElementsOsc.h
    vst/Source/dsp/Lm13700Vca.h
    vst/Source/dsp/Voice.h
    vst/Source/dsp/SemFilter.h
    vst/Source/dsp/Voice.cpp
    vst/Source/dsp/VoiceConfig.h
    vst/Source/dsp/Modulation.h
    vst/Source/dsp/Modulation.cpp
    vst/Source/dsp/MidiInput.h
    vst/Source/dsp/MidiInput.cpp
    vst/Source/dsp/MidiDispatcher.cpp
    vst/Source/dsp/VoiceAllocator.h
    vst/Source/dsp/VoiceAllocator.cpp
    vst/Source/dsp/MasterBus.h
    vst/Source/dsp/ControlTimes.h
    vst/Source/dsp/SynthEngine.h
    vst/Source/dsp/SynthEngine.cpp
    vst/Source/data/WaveManager.h
    vst/Source/data/WaveManager.cpp
    vst/Source/data/PresetManager.h
    vst/Source/data/PresetData.h
    vst/Source/data/SynthModel.h
    vst/Source/data/DefaultKit.h
    vst/Source/dsp/DefaultWaves.h
    vst/Source/data/PresetData.cpp
    vst/Source/data/SynthModel.cpp
    vst/Source/data/PresetManager.cpp
    vst/Source/data/OverviberPaths.h
    vst/Source/ui/LcdDisplay.h
    vst/Source/ui/LcdDisplay.cpp
    vst/Source/ui/OvercyclerKnob.h
    vst/Source/ui/OvercyclerKeypad.h
    vst/Source/ui/OvercyclerKeypad.cpp
    vst/Source/ui/ModernEditorView.h
    vst/Source/ui/ModernEditorView.cpp
    vst/Source/ui/components/ModernLookAndFeel.h
    vst/Source/ui/components/ModernLookAndFeel.cpp
    vst/Source/ui/components/ModernSectionCard.h
    vst/Source/ui/components/ModernSectionCard.cpp
    vst/Source/ui/components/ModernChoiceButtons.h
    vst/Source/ui/components/ModernChoiceButtons.cpp
    vst/Source/ui/components/ModernGlyphs.h
    vst/Source/ui/components/WaveformEditorComponent.h
    vst/Source/ui/components/WaveformEditorComponent.cpp
    vst/Source/ui/components/FilterCurveComponent.h
    vst/Source/ui/components/FilterCurveComponent.cpp
    vst/Source/ui/components/AdsrCurveComponent.h
    vst/Source/ui/components/AdsrCurveComponent.cpp
    vst/Source/ui/components/ModernTelemetryComponents.h
    vst/Source/ui/components/ModernTelemetryComponents.cpp
    vst/Source/ui/components/ModernTabBar.h
    vst/Source/ui/components/ModernTabBar.cpp
    vst/Source/ui/tabs/ModernTabContext.h
    vst/Source/ui/tabs/OscillatorTab.h
    vst/Source/ui/tabs/ModernTabModule.cpp
    vst/Source/ui/tabs/OscillatorTab.cpp
    vst/Source/ui/tabs/FilterVcaTab.h
    vst/Source/ui/tabs/FilterVcaTab.cpp
    vst/Source/ui/tabs/EnvelopeTab.h
    vst/Source/ui/tabs/EnvelopeTab.cpp
    vst/Source/ui/tabs/LfoArpTab.h
    vst/Source/ui/tabs/LfoArpTab.cpp
    vst/Source/ui/tabs/AfxTab.h
    vst/Source/ui/tabs/AfxTab.cpp
    vst/Source/ui/tabs/ModMatrixTab.h
    vst/Source/ui/tabs/ModMatrixTab.cpp
    vst/Source/ui/tabs/SettingsTab.h
    vst/Source/ui/tabs/SettingsTab.cpp
    vst/Source/ui/lua/LuaEngine.h
    vst/Source/ui/lua/LuaEngine.cpp
    vst/Source/ui/lua/LuaTab.h
    vst/Source/ui/lua/LuaTab.cpp
    vst/Source/ui/ModernPresetManager.h
    vst/Source/ui/ModernPresetManager.cpp
    vst/Source/ui/PluginEditor.h
    vst/Source/ui/PluginEditor.cpp
)

# PUBLIC: the plugin scenario tests link Overviber and include the same
# headers (GCC and Clang, unlike MSVC, do not search the directories of the
# including headers).
target_include_directories(Overviber PUBLIC
    vst/Source
    vst/Source/dsp
    vst/Source/data
    vst/Source/ui
    vst/Source/ui/components
)

target_compile_definitions(Overviber PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Overviber PUBLIC OverviberElements)
target_link_libraries(Overviber PRIVATE
    OvercyclerFonts
    OverviberLua
    OverviberLuaScripts
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

# Linux / Unix visibility flags
if(UNIX AND NOT APPLE)
    target_compile_options(Overviber PRIVATE -fvisibility=hidden)
endif()

# Link-time optimisation of the release plugin: the voice loop calls the
# oscillators, filters and VCA in other files, which only LTO can inline
# (about 5 % less CPU, bit-identical audio; REFACTORING.md). Only the plugin
# and its formats: the scenario tests and the Elements library they share
# link without it.
include(CheckIPOSupported)
check_ipo_supported(RESULT OVERVIBER_IPO_SUPPORTED OUTPUT OVERVIBER_IPO_MESSAGE LANGUAGES C CXX)
if(OVERVIBER_IPO_SUPPORTED)
    foreach(ipo_target IN ITEMS Overviber Overviber_VST3 Overviber_Standalone Overviber_AU)
        if(TARGET ${ipo_target})
            set_property(TARGET ${ipo_target} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
        endif()
    endforeach()
else()
    message(STATUS "Overviber: no link-time optimisation (${OVERVIBER_IPO_MESSAGE})")
endif()

# macOS Bundle Resource Embedding (Factory Presets & Wavetables)
if(APPLE)
    if(TARGET Overviber_VST3)
        add_custom_command(TARGET Overviber_VST3 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:Overviber_VST3>/Resources/disk"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/disk"
                "$<TARGET_BUNDLE_CONTENT_DIR:Overviber_VST3>/Resources/disk"
            COMMENT "Embedding factory presets & wavetables into macOS VST3 bundle"
        )
    endif()
    if(TARGET Overviber_AU)
        add_custom_command(TARGET Overviber_AU POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:Overviber_AU>/Resources/disk"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/disk"
                "$<TARGET_BUNDLE_CONTENT_DIR:Overviber_AU>/Resources/disk"
            COMMENT "Embedding factory presets & wavetables into macOS AU bundle"
        )
    endif()
endif()

# Compile the JUCE-independent DSP once for all scenario executables.
include(CTest)
if(BUILD_TESTING)
    add_library(OverviberTestDsp STATIC
        vst/Source/dsp/wtosc.cpp
        vst/Source/dsp/adsr.cpp
        vst/Source/dsp/lfo.cpp
        vst/Source/dsp/assigner.cpp
        vst/Source/dsp/arp.cpp
        vst/Source/dsp/Voice.cpp
        vst/Source/dsp/Modulation.cpp
        vst/Source/dsp/MidiInput.cpp
        vst/Source/dsp/MidiDispatcher.cpp
        vst/Source/dsp/VoiceAllocator.cpp
        vst/Source/dsp/SynthEngine.cpp
        vst/Source/data/WaveManager.cpp
        vst/Source/data/PresetData.cpp
        vst/Source/data/SynthModel.cpp
        vst/Source/data/PresetManager.cpp
    )
    target_include_directories(OverviberTestDsp PUBLIC
        vst/Source vst/Source/dsp vst/Source/data vst/Source/ui)
    target_link_libraries(OverviberTestDsp PUBLIC OverviberElements)
    # JUCE_USE_CURL=0 as in the plugin: the tests that compile juce_core need
    # no libcurl headers.
    target_compile_definitions(OverviberTestDsp PUBLIC OVERVIBER_DIAGNOSTICS=1 JUCE_USE_CURL=0)

    add_executable(AudioReferenceRender vst/Tests/AudioReferenceRender.cpp)
    target_link_libraries(AudioReferenceRender PRIVATE OverviberTestDsp)
    target_compile_definitions(AudioReferenceRender PRIVATE
        OVERVIBER_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/disk")
    # Bit-exact comparison with a local baseline, created before a refactoring
    # step with: AudioReferenceRender --out <build>/audio-baseline
    # Hashes depend on compiler and platform, so no baseline is committed and
    # the test is skipped while the directory does not exist.
    set(OVERVIBER_AUDIO_BASELINE "${CMAKE_CURRENT_BINARY_DIR}/audio-baseline" CACHE PATH
        "Baseline directory for the AudioReferenceCompare test")
    add_test(NAME AudioReferenceCompare
        COMMAND AudioReferenceRender --compare "${OVERVIBER_AUDIO_BASELINE}")
    set_tests_properties(AudioReferenceCompare PROPERTIES TIMEOUT 900 SKIP_RETURN_CODE 77)
    # The same comparison with every heap allocation filled with 0x7F: a read
    # of uninitialised state then changes the audio in every run.
    add_test(NAME AudioReferencePoison
        COMMAND AudioReferenceRender --compare "${OVERVIBER_AUDIO_BASELINE}")
    set_tests_properties(AudioReferencePoison PROPERTIES TIMEOUT 900 SKIP_RETURN_CODE 77
        ENVIRONMENT "OVERVIBER_POISON=127")
    add_executable(MidiClickScenarioTest vst/Tests/MidiClickScenarioTest.cpp)
    target_link_libraries(MidiClickScenarioTest PRIVATE OverviberTestDsp juce::juce_audio_basics)
    target_compile_definitions(MidiClickScenarioTest PRIVATE
        OVERVIBER_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/disk"
        OVERVIBER_MIDI_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/vst/Tests/fixtures/test.mid")
    add_test(NAME MidiClickScenarioTest COMMAND MidiClickScenarioTest)
    set_tests_properties(MidiClickScenarioTest PROPERTIES TIMEOUT 60)

    add_executable(Preset0048MidiScenarioTest vst/Tests/Preset0048MidiScenarioTest.cpp)
    target_link_libraries(Preset0048MidiScenarioTest PRIVATE OverviberTestDsp juce::juce_audio_basics)
    target_compile_definitions(Preset0048MidiScenarioTest PRIVATE
        OVERVIBER_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/disk")

    set(OVERVIBER_SCENARIO_TESTS
        FilterScenarioTest ArpScenarioTest AdvancedMidiScenarioTest
        ModMatrixScenarioTest ClassicSkinScenarioTest ConsoleXAndAfxScenarioTest
        ElementsScenarioTest ElementsVoiceScenarioTest StorageScenarioTest RefactoringScenarioTest AudioRegressionScenarioTest
        FactoryVoiceDistributionScenarioTest FactoryPresetHeadroomScenarioTest AfxScenarioTest GlideDelayScenarioTest
        FirmwareParamScenarioTest ResonanceCalibrationTest SignalQualityScenarioTest TimingScenarioTest)
    foreach(test_name IN LISTS OVERVIBER_SCENARIO_TESTS)
        add_executable(${test_name} vst/Tests/${test_name}.cpp)
        if(test_name STREQUAL "StorageScenarioTest")
            target_include_directories(${test_name} PRIVATE vst/Source)
            target_link_libraries(${test_name} PRIVATE juce::juce_core)
            target_compile_definitions(${test_name} PRIVATE JUCE_USE_CURL=0)
        else()
            target_link_libraries(${test_name} PRIVATE OverviberTestDsp)
            if(test_name STREQUAL "RefactoringScenarioTest")
                target_link_libraries(${test_name} PRIVATE juce::juce_core)
            endif()
        endif()
        target_compile_definitions(${test_name} PRIVATE
            OVERVIBER_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/disk")
        # Assertions in scenario code must also execute in optimized builds.
        if(MSVC)
            target_compile_options(${test_name} PRIVATE
                "/FI${CMAKE_CURRENT_SOURCE_DIR}/vst/Tests/EnableAsserts.h")
        else()
            target_compile_options(${test_name} PRIVATE -UNDEBUG)
        endif()
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-results/${test_name}")
        add_test(NAME ${test_name} COMMAND ${test_name})
        set_tests_properties(${test_name} PROPERTIES
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-results/${test_name}"
            TIMEOUT 180)
    endforeach()
    # The plugin's own include paths are PRIVATE, so the tests that compile its
    # headers repeat them (MSVC also finds them relative to the including file).
    set(OVERVIBER_PLUGIN_TEST_INCLUDES
        vst/Source vst/Source/dsp vst/Source/data
        vst/Source/ui vst/Source/ui/components juce/modules)
    # Linux: juce_core would otherwise need libcurl; the tests never touch the web.
    foreach(test_name IN ITEMS MidiClickScenarioTest Preset0048MidiScenarioTest
            StorageScenarioTest RefactoringScenarioTest)
        target_compile_definitions(${test_name} PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
    endforeach()
    add_executable(PluginTimingScenarioTest vst/Tests/PluginTimingScenarioTest.cpp)
    target_include_directories(PluginTimingScenarioTest PRIVATE ${OVERVIBER_PLUGIN_TEST_INCLUDES})
    target_compile_definitions(PluginTimingScenarioTest PRIVATE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
        OVERVIBER_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/disk")
    target_link_libraries(PluginTimingScenarioTest PRIVATE Overviber)
    add_test(NAME PluginTimingScenarioTest COMMAND PluginTimingScenarioTest)
    set_tests_properties(PluginTimingScenarioTest PROPERTIES TIMEOUT 60)
    add_executable(PluginSessionScenarioTest vst/Tests/PluginSessionScenarioTest.cpp)
    target_include_directories(PluginSessionScenarioTest PRIVATE ${OVERVIBER_PLUGIN_TEST_INCLUDES})
    target_compile_definitions(PluginSessionScenarioTest PRIVATE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
        OVERVIBER_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/disk")
    target_link_libraries(PluginSessionScenarioTest PRIVATE Overviber)
    add_test(NAME PluginSessionScenarioTest COMMAND PluginSessionScenarioTest)
    set_tests_properties(PluginSessionScenarioTest PROPERTIES TIMEOUT 60)
    # Characterization of the plugin's MIDI input path (CC mapping, channel
    # controllers, MPE, sustain, program change, arp MIDI out). The text
    # fixture is committed; the audio hash is a local baseline in the build.
    add_executable(PluginMidiScenarioTest vst/Tests/PluginMidiScenarioTest.cpp)
    target_include_directories(PluginMidiScenarioTest PRIVATE ${OVERVIBER_PLUGIN_TEST_INCLUDES})
    target_compile_definitions(PluginMidiScenarioTest PRIVATE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
        OVERVIBER_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/disk"
        OVERVIBER_PLUGIN_MIDI_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/vst/Tests/fixtures/plugin-midi")
    target_link_libraries(PluginMidiScenarioTest PRIVATE Overviber)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-results/PluginMidiScenarioTest")
    add_test(NAME PluginMidiScenarioTest COMMAND PluginMidiScenarioTest
        --out "${CMAKE_CURRENT_BINARY_DIR}/test-results/PluginMidiScenarioTest"
        --audio-baseline "${CMAKE_CURRENT_BINARY_DIR}/midi-audio-baseline.txt")
    set_tests_properties(PluginMidiScenarioTest PROPERTIES TIMEOUT 120)
    # Characterization of the host parameters (IDs, names, ranges, defaults).
    add_executable(PluginParameterScenarioTest vst/Tests/PluginParameterScenarioTest.cpp)
    target_include_directories(PluginParameterScenarioTest PRIVATE ${OVERVIBER_PLUGIN_TEST_INCLUDES})
    target_compile_definitions(PluginParameterScenarioTest PRIVATE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
        OVERVIBER_PLUGIN_PARAM_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/vst/Tests/fixtures/plugin-params")
    target_link_libraries(PluginParameterScenarioTest PRIVATE Overviber)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-results/PluginParameterScenarioTest")
    add_test(NAME PluginParameterScenarioTest COMMAND PluginParameterScenarioTest
        --out "${CMAKE_CURRENT_BINARY_DIR}/test-results/PluginParameterScenarioTest")
    set_tests_properties(PluginParameterScenarioTest PROPERTIES TIMEOUT 60)

    # The LUA tab: sandbox, limits, parameters, graphics and the built-in example.
    add_executable(LuaTabScenarioTest vst/Tests/LuaTabScenarioTest.cpp)
    target_include_directories(LuaTabScenarioTest PRIVATE ${OVERVIBER_PLUGIN_TEST_INCLUDES})
    target_compile_definitions(LuaTabScenarioTest PRIVATE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1)
    target_link_libraries(LuaTabScenarioTest PRIVATE Overviber)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-results/LuaTabScenarioTest")
    add_test(NAME LuaTabScenarioTest COMMAND LuaTabScenarioTest)
    set_tests_properties(LuaTabScenarioTest PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-results/LuaTabScenarioTest"
        TIMEOUT 120
        SKIP_RETURN_CODE 77)

    # Characterization of the Modern skin: layout, control bindings and (on
    # request) pixel snapshots of every tab. See the header of the test source.
    add_executable(ModernSkinScenarioTest vst/Tests/ModernSkinScenarioTest.cpp)
    target_include_directories(ModernSkinScenarioTest PRIVATE ${OVERVIBER_PLUGIN_TEST_INCLUDES})
    target_compile_definitions(ModernSkinScenarioTest PRIVATE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
        OVERVIBER_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/disk"
        OVERVIBER_MODERN_SKIN_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/vst/Tests/fixtures/modern-skin")
    target_link_libraries(ModernSkinScenarioTest PRIVATE Overviber)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-results/ModernSkinScenarioTest")
    add_test(NAME ModernSkinScenarioTest COMMAND ModernSkinScenarioTest
        --out "${CMAKE_CURRENT_BINARY_DIR}/test-results/ModernSkinScenarioTest")
    set_tests_properties(ModernSkinScenarioTest PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-results/ModernSkinScenarioTest"
        TIMEOUT 300
        SKIP_RETURN_CODE 77)
endif()
# ==============================================================================
# Modern Skin Standalone Designer & Font / Palette Explorer
# ==============================================================================
juce_add_gui_app(ModernSkinDesigner
    PRODUCT_NAME "Overviber Skin Designer"
)

target_sources(ModernSkinDesigner PRIVATE
    vst/Source/ui/designer/ModernSkinDesignerMain.cpp
    vst/Source/ui/ModernEditorView.cpp
    vst/Source/ui/components/ModernTabBar.cpp
    vst/Source/ui/tabs/ModernTabModule.cpp
    vst/Source/ui/tabs/OscillatorTab.cpp
    vst/Source/ui/tabs/FilterVcaTab.cpp
    vst/Source/ui/tabs/EnvelopeTab.cpp
    vst/Source/ui/tabs/LfoArpTab.cpp
    vst/Source/ui/tabs/AfxTab.cpp
    vst/Source/ui/tabs/ModMatrixTab.cpp
    vst/Source/ui/tabs/SettingsTab.cpp
    vst/Source/ui/lua/LuaEngine.cpp
    vst/Source/ui/lua/LuaTab.cpp
    vst/Source/ui/components/ModernLookAndFeel.cpp
    vst/Source/ui/components/ModernSectionCard.cpp
    vst/Source/ui/components/ModernChoiceButtons.cpp
    vst/Source/ui/components/WaveformEditorComponent.cpp
    vst/Source/ui/components/FilterCurveComponent.cpp
    vst/Source/ui/components/AdsrCurveComponent.cpp
    vst/Source/ui/components/ModernTelemetryComponents.cpp
    vst/Source/ui/ModernPresetManager.cpp
    vst/Source/dsp/wtosc.cpp
    vst/Source/dsp/adsr.cpp
    vst/Source/dsp/lfo.cpp
    vst/Source/dsp/assigner.cpp
    vst/Source/dsp/arp.cpp
    vst/Source/dsp/Voice.cpp
    vst/Source/dsp/Modulation.cpp
    vst/Source/dsp/MidiInput.cpp
    vst/Source/dsp/MidiDispatcher.cpp
    vst/Source/dsp/VoiceAllocator.cpp
    vst/Source/dsp/MasterBus.h
    vst/Source/dsp/ControlTimes.h
    vst/Source/dsp/SynthEngine.cpp
    vst/Source/data/WaveManager.cpp
    vst/Source/data/PresetData.cpp
    vst/Source/data/SynthModel.cpp
    vst/Source/data/PresetManager.cpp
    vst/Source/data/OverviberPaths.h
)

target_include_directories(ModernSkinDesigner PRIVATE
    vst/Source
    vst/Source/dsp
    vst/Source/data
    vst/Source/ui
    vst/Source/ui/components
    vst/Source/ui/theme
    vst/Source/ui/designer
)

target_compile_definitions(ModernSkinDesigner PUBLIC
    MODERN_SKIN_DESIGNER_STANDALONE=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(ModernSkinDesigner PRIVATE
    OverviberElements
    OvercyclerFonts
    OverviberLua
    OverviberLuaScripts
    juce::juce_gui_extra
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)
