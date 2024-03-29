cmake_minimum_required(VERSION 3.10)

project(haxed VERSION 1.0)

option(USE_SANITIZERS "Link with address, leak and UB sanitizers" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)

set(CMAKE_EXPORT_COMPILE_COMMANDS true)

include_directories(
    /usr/include/freetype2
    /usr/include/libpng16
    ./external/LspCpp/include
)

find_library(ICUUC_LIB icuuc REQUIRED)
find_library(ICUDATA_LIB icudata REQUIRED)
find_library(ICUIO_LIB icuio  REQUIRED)
find_library(ICUI18N_LIB icui18n  REQUIRED)
find_library(LSPCPP_LIB lspcpp PATHS ./external/LspCpp/build REQUIRED)
find_library(BOOST_FS_LIB boost_filesystem)

link_directories(./external/LspCpp/build/third_party/uri/src)
link_libraries(GLEW GL glfw freetype ${ICUUC_LIB} ${ICUDATA_LIB} ${ICUIO_LIB} ${ICUI18N_LIB} git2
    ${LSPCPP_LIB} network-uri ${BOOST_FS_LIB})


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
    src/types.cpp
    src/common/string.cpp
    src/common/file.cpp
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
    src/Document.cpp
    src/Split.cpp
    src/Image.cpp
    src/Timer.cpp
    src/Prompt.cpp
    src/FloatingWin.cpp
    src/ProgressFloatingWin.cpp
    src/dialogs/Dialog.cpp
    src/dialogs/MessageDialog.cpp
    src/dialogs/FileDialog.cpp
    src/dialogs/AskerDialog.cpp
    src/dialogs/FindDialog.cpp
    src/dialogs/FindListDialog.cpp
    src/SessionHandler.cpp
    src/autocomp/Popup.cpp
    src/autocomp/DictionaryProvider.cpp
    src/autocomp/BufferWordProvider.cpp
    src/autocomp/PathProvider.cpp
    src/autocomp/LspProvider.cpp
    src/ThemeLoader.cpp
    src/languages.cpp
    src/Git.cpp
    src/doxygen.cpp
    src/markdown.cpp
    external/cJSON/cJSON.c
    external/md4c/src/md4c.c
)
