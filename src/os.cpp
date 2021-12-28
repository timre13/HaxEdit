#include "os.h"
#include "common/string.h"
#include "Logger.h"
#ifdef OS_LINUX
#include <stdio.h>
#endif

namespace OS
{

std::string runExternalCommand(const std::string& command)
{
    Logger::dbg << "Running command: " << quoteStr(command) << Logger::End;
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

std::string getFontFilePath(const std::string& fontName, FontStyle style)
{
#ifdef OS_LINUX
    return runExternalCommand("fc-match --format=%{file} \""+fontName+":style="+fontStyleToStr(style)+'"');
#else
#error "TODO: Unimplemented"
#endif
}

}

