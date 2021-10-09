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
)

find_library(ICUUC_LIB icuuc REQUIRED)
find_library(ICUDATA_LIB icudata REQUIRED)
link_libraries(GLEW GL glfw freetype ${ICUUC_LIB} ${ICUDATA_LIB})


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
    src/App.cpp
    src/Logger.cpp
    src/Shader.cpp
    src/FileTypeHandler.cpp
    src/os.cpp
    src/Bindings.cpp
    src/UiRenderer.cpp
    src/TextRenderer.cpp
    src/Buffer.cpp
    src/ImageBuffer.cpp
    src/Split.cpp
    src/Image.cpp
    src/Timer.cpp
    src/Dialog.cpp
    src/MessageDialog.cpp
    src/FileDialog.cpp
    src/AskerDialog.cpp
    src/SessionHandler.cpp
    external/cJSON/cJSON.c
)
