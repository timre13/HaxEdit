#include "os.h"
#ifdef OS_LINUX
#include <stdio.h>
#endif

namespace OS
{

std::string runExternalCommand(const std::string& command)
{
#ifdef OS_LINUX
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return "";
    char buffer[1024]{};
    fread((void*)buffer, 1, 1024, pipe);
    pclose(pipe);
    return buffer;
#else
#error "TODO: Unimplemented"
#endif
}

std::string getFontFilePath(const std::string& fontName)
{
#ifdef OS_LINUX
    return runExternalCommand("fc-match --format=%{file} "+fontName);
#else
#error "TODO: Unimplemented"
#endif
}

};

