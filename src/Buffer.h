#pragma once

#include <filesystem>
#include <string>

namespace std_fs = std::filesystem;

#define FILENAME_NEW "<new>"

class Buffer
{
private:
    std::string m_filePath = FILENAME_NEW;
    std::string m_content;
    size_t m_numOfLines{};

public:
    int open(const std::string& filePath);

    inline const std::string& getContent() const { return m_content; }
    inline const std::string& getFilePath() const { return m_filePath; }
    inline const std::string getFileName() const { return std_fs::path{m_filePath}.filename().string(); }
    inline const std::string getFileExt() const { return std_fs::path{m_filePath}.extension().string(); }
    inline bool isNewFile() const { return m_filePath.compare(FILENAME_NEW); }
    inline size_t getNumOfLines() const { return m_numOfLines; }
};
