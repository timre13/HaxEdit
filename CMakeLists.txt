cmake_minimum_required(VERSION 3.10)

project(haxed VERSION 1.0)

option(USE_SANITIZERS "Link with address, leak and UB sanitizers" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)

set(CMAKE_EXPORT_COMPILE_COMMANDS true)

include_directories(
    /usr/include/freetype2
    /usr/include/libpng16
    ../external/icu/icu4c/source/common
    ../external/icu/icu4c/source/io
)

find_library(ICUUC_LIB icuuc REQUIRED)
find_library(ICUDATA_LIB icudata REQUIRED)
find_library(ICUIO_LIB icuio  REQUIRED)
find_library(ICUI18N_LIB icui18n  REQUIRED)

link_libraries(GLEW GL glfw freetype ${ICUUC_LIB} ${ICUDATA_LIB} ${ICUIO_LIB} ${ICUI18N_LIB} git2)


set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wpedantic -g3 -pthread")

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # Clang needs more flags to generate good debug info
    message("Clang detected")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-limit-debug-info")
else()
    message("Non-Clang compiler detected")
endif()

if (USE_SANITIZERS)
    message("Using sanitizers")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,leak,undefined")
endif()


add_executable(haxed
    src/main.cpp
    src/common.cpp
    src/modes.cpp
    src/StatusMsg.cpp
    src/App.cpp
    src/Logger.cpp
    src/Shader.cpp
    src/FileTypeHandler.cpp
    src/os.cpp
    src/Bindings.cpp
    src/UiRenderer.cpp
    src/TextRenderer.cpp
    src/signs.cpp
    src/Buffer.cpp
    src/ImageBuffer.cpp
    src/Split.cpp
    src/Image.cpp
    src/Timer.cpp
    src/dialogs/Dialog.cpp
    src/dialogs/MessageDialog.cpp
    src/dialogs/FileDialog.cpp
    src/dialogs/AskerDialog.cpp
    src/SessionHandler.cpp
    src/autocomp/Popup.cpp
    src/autocomp/DictionaryProvider.cpp
    src/autocomp/BufferWordProvider.cpp
    src/Git.cpp
    external/cJSON/cJSON.c
)
