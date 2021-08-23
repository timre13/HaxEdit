#pragma once

#include <filesystem>
#include <string>
#include <glm/glm.hpp>
#include "TextRenderer.h"
#include "UiRenderer.h"

namespace std_fs = std::filesystem;

#define FILENAME_NEW "<new>"

class Buffer
{
private:
    std::string m_filePath = FILENAME_NEW;
    std::string m_content;
    size_t m_numOfLines{};

    glm::ivec2 m_position{};

    // 0-based indexes!
    size_t m_cursorRow{};
    size_t m_cursorCol{};

    TextRenderer* m_textRenderer{};
    UiRenderer* m_uiRenderer{};

public:
    Buffer(TextRenderer* textRenderer, UiRenderer* uiRenderer);

    int open(const std::string& filePath);

    void render();

    inline const std::string& getContent() const { return m_content; }
    inline const std::string& getFilePath() const { return m_filePath; }
    inline const std::string getFileName() const { return std_fs::path{m_filePath}.filename().string(); }
    inline const std::string getFileExt() const { return std_fs::path{m_filePath}.extension().string(); }
    inline bool isNewFile() const { return m_filePath.compare(FILENAME_NEW); }
    inline size_t getNumOfLines() const { return m_numOfLines; }

    inline size_t getCursorRow() const { return m_cursorRow; }
    inline size_t getCursorCol() const { return m_cursorCol; }

    inline void moveCursorRight() { ++m_cursorCol; }
    inline void moveCursorLeft() { --m_cursorCol; }
    inline void moveCursorUp() { --m_cursorRow; }
    inline void moveCursorDown() { ++m_cursorRow; }
};
