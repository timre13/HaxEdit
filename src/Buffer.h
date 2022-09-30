#pragma once

#include <filesystem>
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
#include "autocomp/LspProvider.h"
#include "FloatingWin.h"
#include "languages.h"
class Document;

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

    struct CodeAction
    {
        uint forLine{};
        Autocomp::LspProvider::codeActionResult_t actions;
    };

    using fileModTime_t = std::filesystem::file_time_type::rep;

protected:
    std::string m_filePath = FILENAME_NEW;
    static bufid_t s_lastUsedId;
    bufid_t m_id{};
    Langs::LangId m_language = Langs::LangId::Unknown;
    std::unique_ptr<Document> m_document;
    bool m_isModified{};
    bool m_isReadOnly{};

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
    int m_cursorHoldTime{};

    CodeAction m_lineCodeAction;

    int m_charUnderMouseCol{};
    int m_charUnderMouseRow{};
    int m_charUnderMouseI{};

    struct StatusLineStr
    {
        std::string str;
        size_t maxLen{};
    } m_statusLineStr{};

    std::string m_breadcBarVal;

    std::unique_ptr<Autocomp::Popup> m_autocompPopup;

    Selection m_selection{};

    // Line number : Sign
    std::vector<std::pair<int, Sign>> m_signs;

    std::unique_ptr<Git::Repo> m_gitRepo;
    Git::Repo::GitObjectName m_gitBranchName;
    float m_msUntilGitBranchUpdate{};

    String m_toFind;
    std::vector<size_t> m_findResultIs{};
    int m_findCurrResultI{};

    friend void _autoReloadDialogCb(int, Dialog*, void*);
    float m_msUntilAutoReloadCheck{};
    fileModTime_t m_lastFileUpdateTime{};
    bool m_isReloadAskerDialogOpen{};

    // This is incremented after each change.
    // Sent to the LSP server.
    int m_version{};

    struct TabStop
    {
        int col{};
        int index{};
        std::string placeholder;
    };
    int m_tabstopIndex{};
    size_t m_tabstopCount{};

    struct LineInfo
    {
        std::vector<TabStop> tabstops;
    };
    std::vector<LineInfo> m_lineInfoList;

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

    virtual Char getCharAt(size_t pos) const;

    // -------------------- Rendering functions -----------------------------
    void _renderDrawCursor(const glm::ivec2& textPos, int initTextY, int width);
    void _renderDrawSelBg(const glm::ivec2& textPos, int initTextY, int width) const;
    void _renderDrawIndGuid(const glm::ivec2& textPos, int initTextY) const;
    void _renderDrawIndRainbow(const glm::ivec2& textPos, int initTextY, int colI) const;
    void _renderDrawLineNumBar(const glm::ivec2& textPos, int lineI) const;
    void _renderDrawFoundMark(const glm::ivec2& textPos, int initTextY) const;
    void _renderDrawDiagnosticMsg(
        const Autocomp::LspProvider::diagList_t& diags, int lineI, bool isCurrent,
        const glm::ivec2& textPos, int initTextY
    ) const;
    void _renderDrawDiagnosticUnderline(
        const Autocomp::LspProvider::diagList_t& diags, int lineI, int colI,
        const glm::ivec2& textPos, int initTextY
    ) const;
    void _renderDrawCursorLineCodeAct(int yPos, int initTextY);
    void _renderDrawBreadcrumbBar();

    void _goToCurrFindResult(bool showStatMsg);

    void _goToDeclOrDefOrImp(const Autocomp::LspProvider::Location& loc);

    friend class App;
    friend class Document; // So we can make Buffer::open() friend of Document
    /*
     * Do not call this. Use `App::openFileInNewBuffer`.
     */
    virtual void open(const std::string& filePath, bool isReload=false);

    virtual size_t lineColToPos(const lsPosition& pos) const;

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
    virtual int getNumOfLines() const;

    virtual inline int getCursorLine() const { return m_cursorLine; }
    virtual inline int getCursorCol() const { return m_cursorCol; }
    virtual inline int getCursorCharPos() const { return m_cursorCharPos; }

    virtual inline void moveCursor(CursorMovCmd cmd) { m_cursorMovCmd = cmd; }
    virtual void moveCursorToLineCol(int line, int col);
    virtual void moveCursorToLineCol(const lsPosition& pos);
    virtual void moveCursorToChar(int pos);

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
    virtual String getCursorWord() const;

    virtual inline void setCursorVisibility(bool isVisible) {
        m_isCursorShown = isVisible; }
    virtual inline void toggleCursorVisibility() {
        m_isCursorShown = !m_isCursorShown; }

    virtual void scrollBy(int val);

    /*
     * These are the main editing functions.
     * Only these can actually edit the Document.
     * All the other editing functions call these two.
     * @warning Make sure to call `beginHistoryEntry()` before starting editing
     *          and when the cursor is still at the original position.
     * @warning Make sure to call `endHistoryEntry()` when you finished editing
     *          and adjusted the cursor.
     */
    /*
     * @returns The number of characters deleted.
     */
    virtual size_t applyDeletion(const lsRange& range);
    /*
     * @returns The position AFTER the last inserted character.
     */
    virtual lsPosition applyInsertion(const lsPosition& pos, const String& text);

    virtual void beginHistoryEntry();
    virtual void endHistoryEntry();

    // Text editing
    virtual void insertCharAtCursor(Char character);
    virtual void replaceCharAtCursor(Char character);
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
    virtual void insertSnippet(const std::string& snippet);

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

    virtual void tickCursorHold(float frameTimeMs);
    virtual void tickAutoReload(float frameTimeMs);
    virtual void tickGitBranchUpdate(float frameTimeMs);

    virtual void showSymbolHover(bool atMouse=false);
    virtual void showSignatureHelp();
    virtual void gotoDef();
    virtual void gotoDecl();
    virtual void gotoImp();
    virtual inline const CodeAction& getLineCodeAct() const { return m_lineCodeAction; }
    virtual void applyLineCodeAct();
    virtual void applyEdit(const lsAnnotatedTextEdit& edit);
    virtual void applyEdits(const std::vector<lsTextEdit>& edits);
    virtual void renameSymbolAtCursor();

    virtual void insertCustomSnippet();
    virtual void goToNextSnippetTabstop();

    virtual ~Buffer();

    friend class TestRunner;
};
