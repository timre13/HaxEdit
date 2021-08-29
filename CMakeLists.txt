cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED true)

set(CMAKE_EXPORT_COMPILE_COMMANDS true)

include_directories(
    /usr/include/freetype2
    /usr/include/libpng16
)

link_libraries(GLEW GL glfw freetype)

project(haxed VERSION 1.0)

set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wpedantic -g3 -fno-limit-debug-info")

add_executable(haxed
    src/main.cpp
    src/Logger.h
    src/Logger.cpp
    src/Shader.h
    src/Shader.cpp
    src/os.h
    src/os.cpp
    src/TextRenderer.h
    src/TextRenderer.cpp
    src/Buffer.h
    src/Buffer.cpp
    src/types.h
    src/UiRenderer.h
    src/UiRenderer.cpp
    src/Timer.h
    src/Timer.cpp
    src/Dialog.h
    src/Dialog.cpp
)

