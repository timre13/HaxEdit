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

    struct FolderType
    {
        std::string iconName;
        std::vector<std::string> names;
        std::unique_ptr<Image> icon;
    };

private:
    std::unique_ptr<Image> m_defFileIcon{};
    std::vector<FileType> m_fileTypes;

    std::unique_ptr<Image> m_defFolderIcon{};
    std::vector<FolderType> m_folderTypes;

    void loadFileTypes(const std::string& databasePath);
    void loadFolderTypes(const std::string& databasePath);

public:
    FileTypeHandler(
            const std::string& fileDbPath,
            const std::string& folderDbPath);

    const Image* getIconFromFilename(std::string fname, bool isDir);
};

extern std::unique_ptr<FileTypeHandler> g_fileTypeHandler;
