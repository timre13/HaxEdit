#pragma once

#include <string>
#include <vector>
#include <array>

namespace Langs
{

class LangInfo
{
public:
    LangInfo() = delete;

    LangInfo(
            const std::string_view& dispName_,
            const std::string_view& lspId_,
            const std::vector<std::string_view>& exts_)
        : displayName{dispName_}, lspId{lspId_}, extensions{exts_}
    {
    }

    std::string_view                displayName;
    std::string_view                lspId;
    std::vector<std::string_view>   extensions;
};

enum class LangId
{
    Cpp,
    Python,
    Lua,
    Go,
    // Add new here
    Unknown,
    __Count,
};

extern std::array<LangInfo, (int)LangId::__Count-1> languages;

LangId lookupExtension(const std::string& ext);
std::string_view langIdToName(LangId id);
std::string_view langIdToLspId(LangId id);

} // namespace Langs
