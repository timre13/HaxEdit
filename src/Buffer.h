#pragma once

#include <filesystem>
#include <string>
#include <glm/glm.hpp>
#include "Timer.h"
#include "TextRenderer.h"
#include "UiRenderer.h"
#include "config.h"
#include "Logger.h"
#include "globals.h"

namespace std_fs = std::filesystem;

#define FILENAME_NEW "<new>"

class Buffer
{
public:
    enum class CursorMovCmd
    {
        None,
        Right,
        Left,
        Up,
        Down,
        LineBeginning,
        LineEnd,
        FirstChar,
        LastChar,
    };

protected:
    std::string m_filePath = FILENAME_NEW;
    std::string m_content;
    int m_numOfLines{};
    bool m_isModified{};
    bool m_isReadOnly{};

    std::string m_highlightBuffer;

    glm::ivec2 m_position{0, TABLINE_HEIGHT_PX};

    int m_scrollY{};

    // 0-based indices!
    int m_cursorLine{};
    int m_cursorCol{};
    size_t m_cursorCharPos{};
    bool m_isCursorShown{true}; // Used to blink the cursor
    CursorMovCmd m_cursorMovCmd{CursorMovCmd::None};

    struct StatusLineStr
    {
        std::string str;
        size_t maxLen{};
    } m_statusLineStr{};

    /*
     * Make the cursor visible by scrolling the viewport.
     */
    virtual void scrollViewportToCursor();

    virtual void updateHighlighting();

    friend class App;

public:
    inline Buffer() { Logger::log << "Created a buffer: " << this << Logger::End; }

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&& other) = default;
    Buffer& operator=(Buffer&& other) = default;

    virtual int open(const std::string& filePath);
    virtual int saveToFile();
    virtual int saveAsToFile(const std::string& filePath);

    virtual void updateCursor();
    virtual void updateRStatusLineStr();
    virtual void render();

    virtual inline const std::string& getContent() const { return m_content; }
    virtual inline const std::string& getFilePath() const final { return m_filePath; }
    virtual inline const std::string getFileName() const final {
         return std_fs::path{m_filePath}.filename().string(); }
    virtual inline const std::string getFileExt() const final {
        return std_fs::path{m_filePath}.extension().string(); }
    virtual inline bool isNewFile() const final { return m_filePath.compare(FILENAME_NEW) == 0; }
    virtual inline int getNumOfLines() const { return m_numOfLines; }

    virtual inline int getCursorLine() const { return m_cursorLine; }
    virtual inline int getCursorCol() const { return m_cursorCol; }
    virtual inline int getCursorCharPos() const { return m_cursorCharPos; }

    virtual inline void moveCursor(CursorMovCmd cmd) { m_cursorMovCmd = cmd; }

    virtual inline void setCursorVisibility(bool isVisible) {
        m_isCursorShown = isVisible; }
    virtual inline void toggleCursorVisibility() {
        m_isCursorShown = !m_isCursorShown; }

    virtual inline void scrollBy(int val)
    {
        TIMER_BEGIN_FUNC();

        m_scrollY += val;
        // Don't scroll above the first line
        if (m_scrollY > 0)
        {
            m_scrollY = 0;
        }
        // Always show the last line when scrolling down
        // FIXME: Line wrapping makes the document longer, so this breaks
        else if (m_scrollY < -(int)(m_numOfLines-1)*g_fontSizePx)
        {
            m_scrollY = -(int)(m_numOfLines-1)*g_fontSizePx;
        }

        TIMER_END_FUNC();
    }

    // Text editing
    virtual void insert(char character);
    virtual void deleteCharBackwards();
    virtual void deleteCharForward();
    virtual inline bool isModified() const final { return m_isModified; }
    virtual inline void setModified(bool isModified) final { m_isModified = isModified; }

    virtual inline void setReadOnly(bool isReadOnly) { m_isReadOnly = isReadOnly; }
    virtual inline bool isReadOnly() final { return m_isReadOnly; }

    virtual inline ~Buffer()
    {
        Logger::log << "Destroyed a buffer: " << this << " (" << m_filePath << ')' << Logger::End;
    }
};
