#include "FileTypeHandler.h"
#include "Logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>

static std::string loadFile(const std::string& filePath)
{
    try
    {
        std::fstream file;
        file.open(filePath, std::ios::in);
        if (file.fail())
        {
            throw std::runtime_error{"Open failed"};
        }
        std::stringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }
    catch (std::exception& e)
    {
        Logger::fatal << "Failed to open file: " << filePath << ": " << e.what() << Logger::End;
        return "";
    }
}

FileTypeHandler::FileTypeHandler(const std::string& databasePath)
{
    Logger::log << "Loading file icon database: " << databasePath << Logger::End;
    std::stringstream ss; ss << loadFile(databasePath);
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
            std::string filenames;
            while (i < line.size() && line[i] != '|')
            {
                filenames += line[i++];
            }

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

std::string FileTypeHandler::getIconNameForExtension(const std::string& ext)
{
    for (const auto& value : m_fileTypes)
    {
        if (std::find(value.extensions.begin(), value.extensions.end(), ext) != value.extensions.end())
            return value.iconName;
    }
    return "file"; // Return the generic value
}

std::string FileTypeHandler::getIconNameForFilename(const std::string& fname)
{
    for (const auto& value : m_fileTypes)
    {
        if (std::find(value.filenames.begin(), value.filenames.end(), fname) != value.filenames.end())
            return value.iconName;
    }
    return "file"; // Return the generic value
}
