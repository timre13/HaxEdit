#pragma once

#include <vector>
#include <string>
#include <memory>

class FileTypeHandler final
{
public:
    struct FileType
    {
        std::string iconName;
        std::vector<std::string> extensions;
        std::vector<std::string> filenames;
    };

private:
    std::vector<FileType> m_fileTypes;

public:
    FileTypeHandler(const std::string& databasePath);

    std::string getIconNameForExtension(const std::string& ext);
    std::string getIconNameForFilename(const std::string& fname);
};

extern std::unique_ptr<FileTypeHandler> g_fileTypeHandler;
