cmake_minimum_required(VERSION 3.10)

project(haxed VERSION 1.0)

option(USE_SANITIZERS "Link with address, leak and UB sanitizers" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED true)

set(CMAKE_EXPORT_COMPILE_COMMANDS true)

include_directories(
    /usr/include/freetype2
    /usr/include/libpng16
)

link_libraries(GLEW GL glfw freetype)


set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wpedantic -g3")

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
)
