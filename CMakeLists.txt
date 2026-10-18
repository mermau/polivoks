cmake_minimum_required(VERSION 3.22)

project(Volks82 VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Versione minima di macOS")
endif()

option(VOLKS_BUILD_TESTS "Compila i test del motore e l'istantanea dell'interfaccia" OFF)
set(JUCE_DIR "" CACHE PATH "Cartella locale di JUCE (vuoto = scaricato automaticamente)")

if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(Volks82
    COMPANY_NAME "Collateral Effects"
    COMPANY_WEBSITE "https://github.com/mermau"
    BUNDLE_ID "com.collateraleffects.volks82"
    PLUGIN_MANUFACTURER_CODE Clfx
    PLUGIN_CODE Vl82
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Volks82"
    DESCRIPTION "Emulazione del sintetizzatore Polivoks"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.png"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Volks82 usa l'ingresso audio come sorgente esterna (ВНЕШ.) del mixer.")

juce_add_binary_data(Volks82_Data SOURCES
    resources/fonts/RussoOne.ttf
    resources/fonts/PTSansNarrowBold.ttf
    resources/fonts/PTSansBold.ttf
    resources/fonts/PTSansRegular.ttf)

target_sources(Volks82 PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp)

target_include_directories(Volks82 PRIVATE src)

target_compile_definitions(Volks82 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Volks82
    PRIVATE
        Volks82_Data
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(VOLKS_BUILD_TESTS)
    # test del motore DSP (nessuna dipendenza da JUCE)
    add_executable(engine_test tests/engine_test.cpp)
    target_include_directories(engine_test PRIVATE src)

    # istantanea dell'interfaccia in PNG (usata per la verifica visiva senza schermo)
    juce_add_console_app(gui_snapshot PRODUCT_NAME "gui_snapshot")
    target_sources(gui_snapshot PRIVATE tests/gui_snapshot.cpp src/PluginProcessor.cpp src/PluginEditor.cpp)
    target_include_directories(gui_snapshot PRIVATE src)
    target_compile_definitions(gui_snapshot PRIVATE
        JucePlugin_Name="Volks82"
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(gui_snapshot PRIVATE
        Volks82_Data juce::juce_audio_utils juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
