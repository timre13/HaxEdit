#pragma once

#include <vector>
#include <string>
#include <memory>
#include <filesystem>
#include "Dialog.h"

class FileDialog final : public Dialog
{
private:
    int m_selectedFileI{};
    std::string m_dirPath;
    struct FileEntry
    {
        std::string name;
        bool isDirectory;
        std::filesystem::file_time_type lastModTime;
        std::string permissionStr;
    };
    std::vector<std::unique_ptr<FileEntry>> m_fileList;
    Dimensions m_titleRect{};
    std::vector<std::unique_ptr<Dimensions>> m_fileRectDims;

    virtual void recalculateDimensions() override;

    void genFileList();

    inline void selectNextFile()
    {
        if (!m_fileList.empty() && (size_t)m_selectedFileI < m_fileList.size()-1)
            ++m_selectedFileI;
    }
    inline void selectPrevFile()
    {
        if (m_selectedFileI > 0)
            --m_selectedFileI;
    }

public:
    FileDialog(const std::string& dirPath);

    virtual void render() override;
    virtual void handleKey(int key, int mods) override;

    inline std::string getSelectedFilePath()
    {
        if (m_fileList.empty())
            return "";

        return std::filesystem::path{m_dirPath}/m_fileList[m_selectedFileI]->name;
    }
};
