cmake_minimum_required(VERSION 3.22)
if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()
project(CSynthLogic VERSION 0.1.0 LANGUAGES C CXX)
if(NOT APPLE)
    message(FATAL_ERROR "This project builds an Audio Unit for macOS / Logic Pro.")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(FetchContent)
include(CTest)

set(CSYNTH_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libcsynth")
if(NOT EXISTS "${CSYNTH_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "libcsynth submodule is missing. Run git submodule update --init --recursive before configuring.")
endif()
set(CSYNTH_BUILD_TESTS OFF CACHE BOOL "Build libcsynth's own tests" FORCE)
set(CSYNTH_BUILD_EXAMPLES OFF CACHE BOOL "Build libcsynth examples" FORCE)
add_subdirectory("${CSYNTH_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/libcsynth")
# Whole/dotted whole notes at slow tempos need more than the default two seconds.
target_compile_definitions(csynth PUBLIC CSYNTH_ECHO_MAX_DELAY_MS=30000)

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 29396c22c93392d6738e021b83196283d6e4d850)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(CSynth
    COMPANY_NAME "Ricardicus"
    BUNDLE_ID "com.ricardicus.csynth"
    PLUGIN_MANUFACTURER_CODE Rica
    PLUGIN_CODE Csyn
    PRODUCT_NAME "CSynth"
    DESCRIPTION "libcsynth FM instrument"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    AU_EXPORT_PREFIX CSynthAU
    AU_SANDBOX_SAFE TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    FORMATS AU Standalone)

target_sources(CSynth PRIVATE Source/Parameters.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_include_directories(CSynth PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Source")
target_compile_definitions(CSynth PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(CSynth PRIVATE csynth::csynth
    juce::juce_audio_utils juce::juce_audio_processors juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

find_program(CSYNTH_CODESIGN codesign REQUIRED)
foreach(bundle_target CSynth_AU CSynth_Standalone)
    add_custom_command(TARGET ${bundle_target} POST_BUILD
        COMMAND "${CSYNTH_CODESIGN}" --force --sign - "$<TARGET_BUNDLE_DIR:${bundle_target}>"
        COMMENT "Locally signing ${bundle_target}" VERBATIM)
endforeach()

if(BUILD_TESTING)
    add_executable(CSynthTests tests/ProcessorTests.cpp)
    target_link_libraries(CSynthTests PRIVATE CSynth csynth::csynth juce::juce_audio_utils juce::juce_dsp)
    add_test(NAME processor COMMAND CSynthTests)

    add_executable(CSynthAUTest tests/AudioUnitTests.cpp)
    target_link_libraries(CSynthAUTest PRIVATE "-framework AudioToolbox" "-framework CoreFoundation")
    add_dependencies(CSynthAUTest CSynth_AU)
    add_test(NAME audio_unit COMMAND CSynthAUTest "$<TARGET_BUNDLE_DIR:CSynth_AU>")
endif()
