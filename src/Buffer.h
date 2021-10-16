#pragma once

#include <filesystem>
#include <stack>
#include <thread>
#include <glm/glm.hpp>
#include "Timer.h"
#include "TextRenderer.h"
#include "UiRenderer.h"
#include "config.h"
#include "Logger.h"
#include "globals.h"
#include "Syntax.h"

namespace std_fs = std::filesystem;

#define FILENAME_NEW "<new>"

class BufferHistory final
{
public:
    struct Entry
    {
        enum class Action
        {
            None,
            Insert,
            Delete,
        } action;
        size_t pos;
        Char arg;
    };

private:
    std::stack<Entry> m_undoStack;
    std::stack<Entry> m_redoStack;

public:
    BufferHistory() {}

    inline void clear()
    {
        while (!m_undoStack.empty()) m_undoStack.pop();
        while (!m_redoStack.empty()) m_redoStack.pop();
    }

    inline void add(const Entry& entry)
    {
        while (!m_redoStack.empty()) m_redoStack.pop(); // Clear the redo stack
        m_undoStack.push(entry);
    }

    inline bool canGoBack() const
    {
        return !m_undoStack.empty();
    }

    inline Entry goBack()
    {
        assert(canGoBack());
        Entry entry = m_undoStack.top();
        m_undoStack.pop();
        m_redoStack.push(entry);
        return entry;
    }

    inline bool canGoForward() const
    {
        return !m_redoStack.empty();
    }

    inline Entry goForward()
    {
        assert(canGoForward());
        Entry entry = m_redoStack.top();
        m_redoStack.pop();
        m_undoStack.push(entry);
        return entry;
    }
};

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
    String m_content;
    int m_numOfLines{};
    bool m_isModified{};
    bool m_isReadOnly{};

    // Note: We use uint8_t values
    std::u8string m_highlightBuffer;
    bool m_isHighlightUpdateNeeded{};
    std::thread m_highlighterThread;
    bool m_shouldHighlighterLoopRun = true;

    glm::ivec2 m_position{0, TABLINE_HEIGHT_PX};
    glm::ivec2 m_size{};

    bool m_isDimmed = true;

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

    // To handle Undo and Redo
    BufferHistory m_history;

    /*
     * Make the cursor visible by scrolling the viewport.
     */
    virtual void scrollViewportToCursor();

    virtual void _updateHighlighting();

    friend class App;

public:
    Buffer();

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
    virtual void setDimmed(bool val) final { m_isDimmed = val; }

    virtual inline const String& getContent() const { return m_content; }
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

    virtual inline int getCursorWordBeginning()
    {
        int begin = m_cursorCharPos;
        if (isspace((uchar)m_content[begin]))
            return -1;

        // Go to beginning
        while (begin >= 0 && !isspace((uchar)m_content[begin]))
            --begin;
        return begin < 0 ? 0 : begin;
    }

    virtual inline int getCursorWordEnd()
    {
        int end = m_cursorCharPos;
        if (isspace((uchar)m_content[end]))
            return -1;
        // Go to end
        while (end < (int)m_content.size() && !isspace((uchar)m_content[end]))
            ++end;
        return end;
    }

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
        //else if (m_scrollY < -(int)(m_numOfLines-1)*g_fontSizePx)
        //{
        //    m_scrollY = -(int)(m_numOfLines-1)*g_fontSizePx;
        //}

        TIMER_END_FUNC();
    }

    // Text editing
    virtual void insert(Char character);
    virtual void deleteCharBackwards();
    virtual void deleteCharForward();
    virtual inline bool isModified() const final { return m_isModified; }
    virtual inline void setModified(bool isModified) final { m_isModified = isModified; }

    virtual inline void setReadOnly(bool isReadOnly) { m_isReadOnly = isReadOnly; }
    virtual inline bool isReadOnly() final { return m_isReadOnly; }

    virtual inline void setPos(const glm::ivec2& pos) final { m_position = pos; }
    virtual inline void setXPos(int x) final
    {
        m_position.x = x;
        Logger::dbg << "Set buffer x pos to " << x << Logger::End;
    }
    virtual inline void setYPos(int y) final
    {
        m_position.y = y;
        Logger::dbg << "Set buffer y pos to " << y << Logger::End;
    }
    virtual inline const glm::ivec2& getPos() const final { return m_position; }
    virtual inline int getXPos() const final { return m_position.x; }
    virtual inline int getYPos() const final { return m_position.y; }

    virtual inline void setSize(const glm::ivec2& size) final { m_size = size; }
    virtual inline void setWidth(int w) final
    {
        m_size.x = w;
        Logger::dbg << "Set buffer width to " << w << Logger::End;
    }
    virtual inline void setHeight(int h) final
    {
        m_size.y = h;
        Logger::dbg << "Set buffer height to " << h << Logger::End;
    }
    virtual inline const glm::ivec2& getSize() const final { return m_size; }
    virtual inline int getWidth() const final { return m_size.x; }
    virtual inline int getHeight() const final { return m_size.y; }

    virtual void undo();
    virtual void redo();

    virtual ~Buffer();
};
