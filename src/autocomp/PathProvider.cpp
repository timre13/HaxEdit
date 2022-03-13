#include "PathProvider.h"
#include "Popup.h"
#include "../Logger.h"
#include "../common/string.h"
#include <filesystem>
#include <vector>

namespace Autocomp
{

void PathProvider::get(Popup *popupP)
{
    std::vector<std::string> paths;
    std::string dirToList = m_prefix;
    const bool isDir = std::filesystem::is_directory(dirToList);
    if (!isDir)
        dirToList = std::filesystem::path{dirToList}.parent_path();
    Logger::log << "PathProvider: Listing dir: " << dirToList << Logger::End;

    try
    {
        auto it = std::filesystem::directory_iterator{dirToList, std::filesystem::directory_options::skip_permission_denied};
        for (const auto& entry : it)
        {
            if (entry.path().string().starts_with(m_prefix))
                paths.push_back(entry.path().string().substr(m_prefix.size() == 1 ? 1 : m_prefix.size()+1));
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
        popupP->addItem(Popup::Item{Popup::Item::Type::Path, strToUtf32(path)});
    }
}

void PathProvider::setPrevix(const std::string& prefix)
{
    m_prefix = prefix;
    if (m_prefix.size() > 1 && m_prefix.back() == '/')
        m_prefix.pop_back();
    else if (m_prefix.empty())
        m_prefix = "/";
    Logger::dbg << "Prefix: " << m_prefix << Logger::End;
}

} // namespace Autocomp
