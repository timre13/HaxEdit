#pragma once

#include "Image.h"
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
        std::unique_ptr<Image> icon;
    };

private:
    FileType m_defFT{};
    std::vector<FileType> m_fileTypes;

    void loadFileTypes(const std::string& databasePath);

public:
    FileTypeHandler(const std::string& databasePath);

    const Image* getIconFromFilename(std::string fname);
};

extern std::unique_ptr<FileTypeHandler> g_fileTypeHandler;
