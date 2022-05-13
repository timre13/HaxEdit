#pragma once

#include <filesystem>
#include <stack>
#include <thread>
#include <glm/glm.hpp>
#include "unicode/uchar.h"
#include "Timer.h"
#include "TextRenderer.h"
#include "UiRenderer.h"
#include "config.h"
#include "Logger.h"
#include "globals.h"
#include "Syntax.h"
#include "signs.h"
#include "Git.h"
#include "autocomp/Popup.h"
#include "autocomp/DictionaryProvider.h"
#include "autocomp/BufferWordProvider.h"
#include "autocomp/PathProvider.h"

namespace std_fs = std::filesystem;

#define FILENAME_NEW "<new>"

class BufferHistory final
{
public:
    struct Entry
    {
        struct LineEntry
        {
            String from  = U"\xffffffff";
            String to    = U"\xffffffff";
            size_t lineI = -1_st;
        };
        std::vector<LineEntry> lines;
        size_t oldCursPos  = -1_st;
        int    oldCursLine = -1;
        int    oldCursCol  = -1;
        size_t newCursPos  = -1_st;
        int    newCursLine = -1;
        int    newCursCol  = -1;
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
        Logger::dbg << "New history entry added (" << entry.lines.size() << " lines)" << Logger::End;
        // Check if the entry has uninitialized values
        assert(!entry.lines.empty());
        assert(entry.oldCursPos  != -1_st);
        assert(entry.oldCursLine != -1);
        assert(entry.oldCursCol  != -1);
        assert(entry.newCursPos  != -1_st);
        assert(entry.newCursLine != -1);
        assert(entry.newCursCol  != -1);
        for (const auto& line : entry.lines)
        {
            assert(line.from != U"\xffffffff");
            assert(line.to != U"\xffffffff");
            assert(line.lineI != -1_st);
            assert(line.from != U"" || line.to != U"");
        }

        while (!m_redoStack.empty()) m_redoStack.pop(); // Clear the redo stack
        m_undoStack.push(entry);
    }

    inline bool canGoBack() const
    {
        return !m_undoStack.empty();
    }

    [[nodiscard]] inline Entry goBack()
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

    [[nodiscard]] inline Entry goForward()
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
        SWordEnd,       // Note: Words are whitespace-separated here (WORD in VIM)
        SWordBeginning, // Note: Words are whitespace-separated here (WORD in VIM)
        SNextWord,      // Note: Words are whitespace-separated here (WORD in VIM)
        PageUp,
        PageDown,
    };

    struct Selection
    {
        enum class Mode
        {
            None, // No selection is in progress, `fromCol`, `fromLine` and `fromCharI` should be ignored
            Normal, // The classic selection mode, used by most software
            Line, // Selection of whole lines
            Block // Selection of rectangular region of characters
        } mode;
        int fromCol{};
        int fromLine{};
        size_t fromCharI{};
    };

    using fileModTime_t = std::filesystem::file_time_type::rep;

protected:
    std::string m_filePath = FILENAME_NEW;
    std::vector<String> m_content;
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
    int m_cursorXPx{};
    int m_cursorYPx{};
    bool m_isCursorShown{true}; // Used to blink the cursor
    CursorMovCmd m_cursorMovCmd{CursorMovCmd::None};

    int m_charUnderMouseCol{};
    int m_charUnderMouseRow{};
    int m_charUnderMouseI{};

    struct StatusLineStr
    {
        std::string str;
        size_t maxLen{};
    } m_statusLineStr{};

    // To handle Undo and Redo
    BufferHistory m_history;

    std::unique_ptr<Autocomp::Popup> m_autocompPopup;
    std::unique_ptr<Autocomp::BufferWordProvider> m_buffWordProvid;
    std::unique_ptr<Autocomp::PathProvider> m_pathProvid;

    Selection m_selection{};

    // Line number : Sign
    std::vector<std::pair<int, Sign>> m_signs;
    std::unique_ptr<Git::Repo> m_gitRepo;

    String m_toFind;
    std::vector<size_t> m_findResultIs{};
    int m_findCurrResultI{};

    friend void _autoReloadDialogCb(int, Dialog*, void*);
    float m_msUntilAutoReloadCheck{};
    fileModTime_t m_lastFileUpdateTime{};
    bool m_isReloadAskerDialogOpen{};

    virtual void renderAutocompPopup();

    /*
     * Make the cursor visible by scrolling the viewport.
     */
    virtual void scrollViewportToCursor();
    /*
     * Scroll so that the cursor line is at the window center.
     */
    virtual void centerCursor();

    virtual size_t deleteSelectedChars();

    virtual void _updateHighlighting();
    virtual void updateGitDiff();
    virtual std::string getCheckedOutObjName(int hashLen=-1) const;

    virtual inline Char getCharAt(size_t pos) const
    {
        if (m_content.empty())
            return '\0';

        size_t i{};
        for (const String& line : m_content)
        {
            if (i + line.size() - 1 < pos)
            {
                i += line.size();
            }
            else
            {
                return line[pos-i];
            }
        }
        Logger::fatal << "Tried to get character at invalid position: "
            << pos << " / " << std::to_string((int)pos) << Logger::End;
        return '\0';
    }

    // -------------------- Rendering functions -----------------------------
    void _renderDrawCursor(const glm::ivec2& textPos, int initTextY, int width);
    void _renderDrawSelBg(const glm::ivec2& textPos, int initTextY, int width) const;
    void _renderDrawIndGuid(const glm::ivec2& textPos, int initTextY) const;
    void _renderDrawIndRainbow(const glm::ivec2& textPos, int initTextY, int colI) const;
    void _renderDrawLineNumBar(const glm::ivec2& textPos, int lineI) const;
    void _renderDrawFoundMark(const glm::ivec2& textPos, int initTextY) const;

    void _goToCurrFindResult(bool showStatMsg);

    friend class App;
    /*
     * Do not call this. Use `App::openFileInNewBuffer`.
     */
    virtual void open(const std::string& filePath, bool isReload=false);

public:
    Buffer();

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&& other) = default;
    Buffer& operator=(Buffer&& other) = default;

    virtual int saveToFile();
    virtual int saveAsToFile(const std::string& filePath);

    virtual void updateCursor();
    virtual void updateRStatusLineStr();
    virtual void render();
    virtual void setDimmed(bool val) final { m_isDimmed = val; }

    //virtual inline const String& getContent() const { return m_content; }
    virtual inline const std::string& getFilePath() const final { return m_filePath; }
    virtual inline const std::string getFileName() const final {
         return std_fs::path{m_filePath}.filename().string(); }
    virtual inline const std::string getFileExt() const final {
        return std_fs::path{m_filePath}.extension().string(); }
    virtual inline bool isNewFile() const final { return m_filePath.compare(FILENAME_NEW) == 0; }
    virtual inline int getNumOfLines() const { return m_content.size(); }

    virtual inline int getCursorLine() const { return m_cursorLine; }
    virtual inline int getCursorCol() const { return m_cursorCol; }
    virtual inline int getCursorCharPos() const { return m_cursorCharPos; }

    virtual inline void moveCursor(CursorMovCmd cmd) { m_cursorMovCmd = cmd; }

    virtual inline int getCursorWordBeginning()
    {
        // TODO: Reimplement
#if 0
        int begin = m_cursorCharPos;
        if (isspace((uchar)m_content[begin]))
            return -1;

        // Go to beginning
        while (begin >= 0 && !isspace((uchar)m_content[begin]))
            --begin;
        return begin+1;
#endif
        return 0;
    }

    virtual inline int getCursorWordEnd()
    {
        // TODO: Reimplement
#if 0
        int end = m_cursorCharPos;
        if (isspace((uchar)m_content[end]))
            return -1;
        // Go to end
        while (end < (int)m_content.size() && !isspace((uchar)m_content[end]))
            ++end;
        return end;
#endif
        return 0;
    }
    virtual inline String getCursorWord()
    {
        if (isspace(m_content[m_cursorLine][m_cursorCol]))
            return U"";

        int beginCol = m_cursorCol;
        while (beginCol >= 0 && !u_isspace(m_content[m_cursorLine][beginCol]))
            --beginCol;
        ++beginCol;

        int endCol = m_cursorCol;
        while (endCol < (int)m_content[m_cursorLine].length() && !u_isspace(m_content[m_cursorLine][endCol]))
            ++endCol;

        return m_content[m_cursorLine].substr(beginCol, endCol-beginCol);
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
    virtual void replaceChar(Char character);
    virtual void deleteCharBackwards();
    virtual void deleteCharForwardOrSelected();
    virtual inline bool isModified() const final { return m_isModified; }
    virtual inline void setModified(bool isModified) final { m_isModified = isModified; }

    virtual inline void setReadOnly(bool isReadOnly) { m_isReadOnly = isReadOnly; }
    virtual inline bool isReadOnly() final { return m_isReadOnly; }

    virtual inline void setPos(const glm::ivec2& pos) final { m_position = pos; }
    virtual inline void setXPos(int x) final
    {
        m_position.x = x;
        if (g_isDebugDrawMode)
            Logger::dbg << "Set buffer x pos to " << x << Logger::End;
    }
    virtual inline void setYPos(int y) final
    {
        m_position.y = y;
        if (g_isDebugDrawMode)
            Logger::dbg << "Set buffer y pos to " << y << Logger::End;
    }
    virtual inline const glm::ivec2& getPos() const final { return m_position; }
    virtual inline int getXPos() const final { return m_position.x; }
    virtual inline int getYPos() const final { return m_position.y; }

    virtual inline void setSize(const glm::ivec2& size) final { m_size = size; }
    virtual inline void setWidth(int w) final
    {
        m_size.x = w;
        if (g_isDebugDrawMode)
            Logger::dbg << "Set buffer width to " << w << Logger::End;
    }
    virtual inline void setHeight(int h) final
    {
        m_size.y = h;
        if (g_isDebugDrawMode)
            Logger::dbg << "Set buffer height to " << h << Logger::End;
    }
    virtual inline const glm::ivec2& getSize() const final { return m_size; }
    virtual inline int getWidth() const final { return m_size.x; }
    virtual inline int getHeight() const final { return m_size.y; }

    virtual void undo();
    virtual void redo();

    virtual void triggerAutocompPopup();
    virtual void autocompPopupSelectNextItem();
    virtual void autocompPopupSelectPrevItem();
    virtual void autocompPopupHide();
    virtual bool isAutocompPopupShown() const { return m_autocompPopup->isEnabled(); }
    virtual void autocompPopupInsert();
    virtual void regenAutocompList();

    virtual void startSelection(Selection::Mode mode);
    virtual void closeSelection();
    virtual bool isSelectionInProgress() const { return m_selection.mode != Selection::Mode::None; }
    virtual bool isCharSelected(int lineI, int colI, size_t charI) const;

    virtual void pasteFromClipboard();
    virtual size_t copySelectionToClipboard(bool shouldUnselect=true);
    virtual void cutSelectionToClipboard();

    virtual void findGoToNextResult();
    virtual void findGoToPrevResult();
    virtual void find(const String& str);
    virtual void findUpdate();
    virtual void findClear();

    virtual void indentSelectedLines();
    virtual void unindentSelectedLines();

    virtual void goToMousePos();

    virtual void tickAutoReload(float frameTimeMs);

    virtual ~Buffer();
};
