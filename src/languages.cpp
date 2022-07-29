#include "languages.h"
#include "common/string.h"

namespace Langs
{

std::array<LangInfo, (int)LangId::__Count-1> languages = {
    //       name   |   lsp id    |   extensions
    LangInfo{"C++",     "cpp",      {"cpp", "cxx", "hpp", "h"}},        // Cpp
    LangInfo{"Python",  "python",   {"py", "py3", "pyw"}},              // Python
    LangInfo{"Lua",     "lua",      {"lua"}},                           // Lua
    LangInfo{"Go",      "go",       {"go"}},                            // Go
};

LangId lookupExtension(const std::string& ext)
{
    const std::string& ext_ = strToLower(ext);
    int id{};
    for (const auto& item : languages)
    {
        auto foundIt = std::find(item.extensions.begin(), item.extensions.end(), ext_);
        if (foundIt != item.extensions.end())
        {
            return (LangId)id;
        }
        id++;
    }

    return LangId::Unknown;
}

std::string_view langIdToName(LangId id)
{
    assert(id < LangId::__Count);

    if (id >= LangId::Unknown)
        return "Unknown";

    return languages[(size_t)id].displayName;
}

std::string_view langIdToLspId(LangId id)
{
    assert(id < LangId::__Count);

    if (id >= LangId::Unknown)
        return "";

    return languages[(size_t)id].lspId;
}

} // namespace Langs
