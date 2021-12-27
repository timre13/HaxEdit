#include "FileTypeHandler.h"
#include "Logger.h"
#include "config.h"
#include "paths.h"
#include "common/file.h"
#include "common/string.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

void FileTypeHandler::loadFileTypes(const std::string& databasePath)
{
    Logger::log << "Loading file icon database: " << databasePath << Logger::End;
    std::stringstream ss;
    try
    {
        std::string content = loadAsciiFile(databasePath);
        ss << content;
    }
    catch (std::exception& e)
    {
        Logger::fatal << "Failed to open file type database: " << quoteStr(databasePath)
            << ": " << e.what() << Logger::End;
    }

    std::string line;
    std::getline(ss, line); // Skip header
    while (std::getline(ss, line))
    {
        //Logger::dbg << "Parsing line: " << line << Logger::End;

        FileType ft;
        size_t i{};

        while (i < line.size() && line[i] != '|')
        {
            ft.iconName += line[i++];
        }
        ++i;

        {
            std::string extensions;
            while (i < line.size() && line[i] != '|')
            {
                extensions += line[i++];
            }
            ++i;

            std::string extension;
            for (size_t j{}; j < extensions.size(); ++j)
            {
                if (extensions[j] == '/')
                {
                    ft.extensions.push_back(std::move(extension));
                    continue;
                }
                extension += extensions[j];
            }
            if (!extension.empty())
                ft.extensions.push_back(std::move(extension));
        }

        {
            const std::string filenames = line.substr(i);

            std::string filename;
            for (size_t j{}; j < filenames.size(); ++j)
            {
                if (filenames[j] == '/')
                {
                    ft.filenames.push_back(std::move(filename));
                    continue;
                }
                filename += filenames[j];
            }
            if (!filename.empty())
                ft.filenames.push_back(std::move(filename));
        }

        /*
        Logger::dbg << "Icon name: " << ft.iconName;

        Logger::dbg << "\n       Extensions: ";
        for (size_t i{}; i < ft.extensions.size(); ++i)
        {
            if (i != 0) Logger::dbg << ", ";
            Logger::dbg << ft.extensions[i];
        }

        Logger::dbg << "\n       Filenames: ";
        for (size_t i{}; i < ft.filenames.size(); ++i)
        {
            if (i != 0) Logger::dbg << ", ";
            Logger::dbg << ft.filenames[i];
        }

        Logger::dbg << Logger::End;
        */

        m_fileTypes.push_back(std::move(ft));
    }

    Logger::log << "Loaded " << m_fileTypes.size() << " file type values" << Logger::End;
}

void FileTypeHandler::loadFolderTypes(const std::string& databasePath)
{
    Logger::log << "Loading folder icon database: " << databasePath << Logger::End;
    std::stringstream ss;
    try
    {
        std::string content = loadAsciiFile(databasePath);
        ss << content;
    }
    catch (std::exception& e)
    {
        Logger::fatal << "Failed to open folder icon database: " << quoteStr(databasePath)
            << ": " << e.what() << Logger::End;
    }
    std::string line;
    std::getline(ss, line); // Skip header
    while (std::getline(ss, line))
    {
        //Logger::dbg << "Parsing line: " << line << Logger::End;

        FolderType ft;
        size_t i{};

        while (i < line.size() && line[i] != '|')
        {
            ft.iconName += line[i++];
        }
        ++i;

        {
            const std::string names = line.substr(i);

            std::string name;
            for (size_t j{}; j < names.size(); ++j)
            {
                if (names[j] == '/')
                {
                    ft.names.push_back(std::move(name));
                    continue;
                }
                name += names[j];
            }
            if (!name.empty())
                ft.names.push_back(std::move(name));
        }

        /*
        Logger::dbg << "Icon name: " << ft.iconName;
        Logger::dbg << "\n       Names: ";
        for (size_t i{}; i < ft.names.size(); ++i)
        {
            if (i != 0) Logger::dbg << ", ";
            Logger::dbg << ft.names[i];
        }
        Logger::dbg << Logger::End;
        */

        m_folderTypes.push_back(std::move(ft));
    }

    Logger::log << "Loaded " << m_folderTypes.size() << " folder types" << Logger::End;
}

FileTypeHandler::FileTypeHandler(
        const std::string& fileDbPath,
        const std::string& folderDbPath)
{
    loadFileTypes(fileDbPath);
    loadFolderTypes(folderDbPath);

    // Load file icons
    for (auto& ft : m_fileTypes)
        ft.icon = std::make_unique<Image>(PATH_DIR_FT_ICON"/"+ft.iconName+".png");

    // Load default file icon
    m_defFileIcon = std::make_unique<Image>(PATH_DIR_FT_ICON"/file.png");


    // Load folder icons
    for (auto& ft : m_folderTypes)
        ft.icon = std::make_unique<Image>(PATH_DIR_FT_ICON"/"+ft.iconName+".png");

    // Load default folder icon
    m_defFolderIcon = std::make_unique<Image>(PATH_DIR_FT_ICON"/folder-other.png");
}

const Image* FileTypeHandler::getIconFromFilename(std::string fname, bool isDir)
{
    for (char& c : fname)
        c = tolower(c);

    if (!isDir)
    {
        // Check filename first
        for (const auto& value : m_fileTypes)
        {
            if (std::find(value.filenames.begin(), value.filenames.end(), fname) != value.filenames.end())
                return value.icon.get();
        }

        std::string ext = std::filesystem::path{fname}.extension().string();
        if (!ext.empty()) ext = ext.substr(1);
        // Check extension
        for (const auto& value : m_fileTypes)
        {
            if (std::find(value.extensions.begin(), value.extensions.end(), ext) != value.extensions.end())
                return value.icon.get();
        }

        return m_defFileIcon.get();
    }
    else
    {
        for (const auto& value : m_folderTypes)
        {
            if (std::find(value.names.begin(), value.names.end(), fname) != value.names.end())
                return value.icon.get();
        }

        return m_defFolderIcon.get();
    }
}
