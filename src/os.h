#pragma once

#include <string>
#include "types.h"
#include "TextRenderer.h"

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#define OS_WIN
#elif defined(__linux__) || defined(__linux) || defined(linux)
#define OS_LINUX
#else
#error "Unknown OS"
// TODO: Support Mac OS/OS X/Mac OS X/whatever it is called
#endif

namespace OS
{

std::string runExternalCommand(const std::string& command);
std::string getFontFilePath(const std::string& fontName, FontStyle style);

}

