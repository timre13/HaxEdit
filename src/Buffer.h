#pragma once

#include <filesystem>
#include <string>
#include <glm/glm.hpp>
#include "TextRenderer.h"
#include "UiRenderer.h"
#include "config.h"

namespace std_fs = std::filesystem;

#define FILENAME_NEW "<new>"

class Buffer
{
private:
    std::string m_filePath = FILENAME_NEW;
    std::string m_content;
    int m_numOfLines{};

    glm::ivec2 m_position{};

    int m_scrollY{};

    // 0-based indices!
    int m_cursorLine{};
    int m_cursorCol{};
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
    inline int getNumOfLines() const { return m_numOfLines; }

    inline int getCursorLine() const { return m_cursorLine; }
    inline int getCursorCol() const { return m_cursorCol; }

    inline void moveCursorRight() { m_cursorMovCmd = CursorMovCmd::Right; }
    inline void moveCursorLeft()  { m_cursorMovCmd = CursorMovCmd::Left; }
    inline void moveCursorUp()    { m_cursorMovCmd = CursorMovCmd::Up; }
    inline void moveCursorDown()  { m_cursorMovCmd = CursorMovCmd::Down; }

    inline void setCursorVisibility(bool isVisible) {
        m_isCursorShown = isVisible; }
    inline void toggleCursorVisibility() {
        m_isCursorShown = !m_isCursorShown; }

    inline void scrollBy(int val)
    {
        m_scrollY += val;
        // Don't scroll above the first line
        if (m_scrollY > 0)
        {
            m_scrollY = 0;
        }
        // Always show the last line when scrolling down
        // FIXME: Line wrapping makes the document longer, so this breaks
        else if (m_scrollY < -(int)(m_numOfLines-1)*FONT_SIZE_PX)
        {
            m_scrollY = -(int)(m_numOfLines-1)*FONT_SIZE_PX;
        }
    }
};
