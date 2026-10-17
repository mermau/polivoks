cmake_minimum_required(VERSION 3.22)
project(PolivoksEmulation VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(POLIVOKS_BUILD_PLUGIN "Build VST3, AU on macOS, and standalone" ON)
option(POLIVOKS_BUILD_TESTS "Build framework-independent synthesis tests" ON)

if(POLIVOKS_BUILD_TESTS)
    enable_testing()
    add_executable(polivoks_dsp_tests tests/dsp_tests.cpp)
    target_include_directories(polivoks_dsp_tests PRIVATE Source)
    add_test(NAME PolivoksDSP COMMAND polivoks_dsp_tests)
    add_executable(polivoks_render_demo tests/render_demo.cpp)
    target_include_directories(polivoks_render_demo PRIVATE Source)
endif()

if(POLIVOKS_BUILD_PLUGIN)
    find_package(JUCE CONFIG QUIET)
    if(NOT JUCE_FOUND)
        if(DEFINED JUCE_SOURCE_DIR AND EXISTS "${JUCE_SOURCE_DIR}/CMakeLists.txt")
            add_subdirectory("${JUCE_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/juce")
        else()
            include(FetchContent)
            FetchContent_Declare(juce
                GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
                GIT_TAG 8.0.15
                GIT_SHALLOW TRUE)
            FetchContent_MakeAvailable(juce)
        endif()
    endif()

    set(POLIVOKS_FORMATS VST3 Standalone)
    if(APPLE)
        list(APPEND POLIVOKS_FORMATS AU)
    endif()

    juce_add_plugin(PolivoksEmulation
        COMPANY_NAME "Independent DSP Study"
        BUNDLE_ID "org.openpolivoks.polivoksemulation"
        PLUGIN_MANUFACTURER_CODE Ipol
        PLUGIN_CODE Pvlk
        PRODUCT_NAME "Polivoks Study"
        PLUGIN_NAME "Polivoks Study"
        FORMATS ${POLIVOKS_FORMATS}
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        VST3_CATEGORIES Instrument Synth
        AU_MAIN_TYPE kAudioUnitType_MusicDevice
        COPY_PLUGIN_AFTER_BUILD FALSE)

    juce_generate_juce_header(PolivoksEmulation)
    target_sources(PolivoksEmulation PRIVATE
        Source/Parameters.h
        Source/PolivoksDSP.h
        Source/PluginProcessor.h Source/PluginProcessor.cpp
        Source/PluginEditor.h Source/PluginEditor.cpp)
    target_compile_definitions(PolivoksEmulation PUBLIC
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)
    target_link_libraries(PolivoksEmulation PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
endif()
