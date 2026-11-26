cmake_minimum_required(VERSION 3.22)
project(EstudioDesktop VERSION 0.7.2 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE 8 (AGPLv3/Starter); o Stumix é GPLv3
if(DEFINED JUCE_DIR)
  add_subdirectory(${JUCE_DIR} juce)
else()
  include(FetchContent)
  FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.15 GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)
endif()

# Interface web do Stumix embutida no executável (cópia com acentos em códigos ASCII)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(WEB_GERADA ${CMAKE_CURRENT_BINARY_DIR}/web/index.html)
execute_process(COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/gerar_web.py ${CMAKE_CURRENT_BINARY_DIR}/web)
add_custom_command(OUTPUT ${WEB_GERADA}
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/gerar_web.py ${CMAKE_CURRENT_BINARY_DIR}/web
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../index.html ${CMAKE_CURRENT_SOURCE_DIR}/gerar_web.py)
juce_add_binary_data(EstudioWeb SOURCES
  ${WEB_GERADA} ../lame.min.js ../manifest.webmanifest
  ../fonts/manrope.woff2 ../fonts/jetbrains-mono-500.woff2 ../fonts/unbounded-800-stumix.woff2
  ../icons/icon-192-v2.png ../icons/icon-512-v2.png ../icons/favicon-32-v2.png ../icons/apple-touch-icon-v2.png ../icons/maskable-512-v2.png)

set(ESTUDIO_DEFS JUCE_WEB_BROWSER=1 JUCE_USE_CURL=0 JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0
  JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1 JUCE_WASAPI=1 JUCE_DIRECTSOUND=1 JUCE_ALSA=1 JUCE_JACK=0
  JUCE_PLUGINHOST_VST3=1 JUCE_USE_MP3AUDIOFORMAT=1)

if(DEFINED ASIO_SDK_DIR)
  list(APPEND ESTUDIO_DEFS JUCE_ASIO=1)
endif()

if(WIN32)
  set(WV2 NEEDS_WEBVIEW2 TRUE)
endif()

juce_add_gui_app(EstudioDesktop
  PRODUCT_NAME "Stumix"
  COMPANY_NAME "Fbiol"
  BUNDLE_ID "io.github.fbi0l.estudio"
  VERSION ${PROJECT_VERSION}
  ICON_BIG ${CMAKE_CURRENT_SOURCE_DIR}/../icons/icon-512-v2.png
  NEEDS_WEB_BROWSER TRUE
  ${WV2})

target_sources(EstudioDesktop PRIVATE Source/Main.cpp Source/AudioEngine.cpp Source/Sessao.cpp Source/Lancador_windows.cpp)
target_compile_definitions(EstudioDesktop PRIVATE ${ESTUDIO_DEFS}
  JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:EstudioDesktop,JUCE_PRODUCT_NAME>"
  JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:EstudioDesktop,JUCE_VERSION>")
if(DEFINED ASIO_SDK_DIR)
  target_include_directories(EstudioDesktop PRIVATE ${ASIO_SDK_DIR}/common)
endif()
target_link_libraries(EstudioDesktop PRIVATE EstudioWeb juce::juce_audio_utils juce::juce_gui_extra juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# Teste do motor sem placa de som (roda no CI e aqui)
juce_add_console_app(EngineTest PRODUCT_NAME "EngineTest")
target_sources(EngineTest PRIVATE Source/EngineTest.cpp Source/AudioEngine.cpp Source/Sessao.cpp Source/Lancador_windows.cpp)
target_compile_definitions(EngineTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_ALSA=1 JUCE_JACK=0 JUCE_PLUGINHOST_VST3=1 JUCE_USE_MP3AUDIOFORMAT=1)
target_link_libraries(EngineTest PRIVATE juce::juce_audio_utils juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)

# Plugin VST3 mínimo (ganho), usado só pelo teste do motor para conferir o hospedeiro de plugins
juce_add_plugin(StumixTesteGanho
  PRODUCT_NAME "StumixTesteGanho"
  COMPANY_NAME "Fbiol"
  PLUGIN_MANUFACTURER_CODE Fbio
  PLUGIN_CODE Stgn
  FORMATS VST3
  COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(StumixTesteGanho PRIVATE testes/PluginGanho.cpp)
target_compile_definitions(StumixTesteGanho PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(StumixTesteGanho PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)
