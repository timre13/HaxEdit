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
    size_t m_cursorLine{};
    size_t m_cursorCol{};
    bool m_isCursorShown{true}; // Used to blink the cursor

    TextRenderer* m_textRenderer{};
    UiRenderer* m_uiRenderer{};

    enum class CursorMovCmd
    {
        None,
        Right,
        Left,
        Up,
        Down,
    };
    CursorMovCmd m_cursorMovCmd{CursorMovCmd::None};

public:
    Buffer(TextRenderer* textRenderer, UiRenderer* uiRenderer);

    int open(const std::string& filePath);

    void updateCursor();
    void render();

    inline const std::string& getContent() const { return m_content; }
    inline const std::string& getFilePath() const { return m_filePath; }
    inline const std::string getFileName() const {
        return std_fs::path{m_filePath}.filename().string(); }
    inline const std::string getFileExt() const {
        return std_fs::path{m_filePath}.extension().string(); }
    inline bool isNewFile() const { return m_filePath.compare(FILENAME_NEW); }
    inline size_t getNumOfLines() const { return m_numOfLines; }

    inline size_t getCursorLine() const { return m_cursorLine; }
    inline size_t getCursorCol() const { return m_cursorCol; }

    inline void moveCursorRight() { m_cursorMovCmd = CursorMovCmd::Right; }
    inline void moveCursorLeft()  { m_cursorMovCmd = CursorMovCmd::Left; }
    inline void moveCursorUp()    { m_cursorMovCmd = CursorMovCmd::Up; }
    inline void moveCursorDown()  { m_cursorMovCmd = CursorMovCmd::Down; }

    inline void setCursorVisibility(bool isVisible) {
        m_isCursorShown = isVisible; }
    inline void toggleCursorVisibility() {
        m_isCursorShown = !m_isCursorShown; }
};
