#pragma once

#include <string>
#include "glstuff.h"
#include "globals.h"

namespace Clipboard
{

inline std::string get()
{
    const char* str = glfwGetClipboardString(g_window);
    return str ? str : "";
}

inline void set(const std::string& str) { glfwSetClipboardString(g_window, str.c_str()); }

} // namespace Clipboard
