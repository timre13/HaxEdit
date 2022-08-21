#include "PathProvider.h"
#include "Popup.h"
#include "../Logger.h"
#include "../common/string.h"
#include <filesystem>
#include <vector>

namespace Autocomp
{

void PathProvider::get(bufid_t bufid, Popup *popupP)
{
    if (m_prefixes.find(bufid) == m_prefixes.end())
        return;

    const std::string prefix = m_prefixes.find(bufid)->second;

    std::vector<std::string> paths;
    std::string dirToList = prefix;
    const bool isDir = std::filesystem::is_directory(dirToList);
    if (!isDir)
        dirToList = std::filesystem::path{dirToList}.parent_path();
    Logger::log << "PathProvider: Listing dir: " << dirToList << Logger::End;

    try
    {
        auto it = std::filesystem::directory_iterator{dirToList, std::filesystem::directory_options::skip_permission_denied};
        for (const auto& entry : it)
        {
            if (entry.path().string().starts_with(prefix))
                paths.push_back(entry.path().string().substr(prefix.size() == 1 ? 1 : prefix.size()+1));
        }
    }
    catch (std::exception&)
    {
        Logger::warn << "PathProvider: Failed to list dir: \"" << dirToList << '"' << Logger::End;
    }

    Logger::log << "PathProvider: feeding paths into Popup (count: " << paths.size() << ")" << Logger::End;
    for (const auto& path : paths)
    {
        Logger::dbg << path << Logger::End;
        Popup::Item item;
        item.label = path;
        item.kind.emplace(lsCompletionItemKind::File);
        popupP->addItem(std::move(item));
    }
}

void PathProvider::setPrefix(bufid_t bufid, const std::string& prefix)
{
    if (m_prefixes.find(bufid) == m_prefixes.end())
        m_prefixes.insert({bufid, prefix});

    auto& pref = m_prefixes.find(bufid)->second;
    pref = prefix;

    if (pref.size() > 1 && pref.back() == '/')
        pref.pop_back();
    else if (pref.empty())
        pref = "/";
    Logger::dbg << "Prefix: " << pref << Logger::End;
}

} // namespace Autocomp
