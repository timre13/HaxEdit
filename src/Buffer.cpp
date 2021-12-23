#include "Buffer.h"
#include "config.h"
#include "Timer.h"
#include "types.h"
#include "modes.h"
#include "Syntax.h"
#include "dialogs/MessageDialog.h"
#include "unicode/ustdio.h"
#include "common.h"
#include "Clipboard.h"
#include <fstream>
#include <sstream>
#include <chrono>
#include <filesystem>
using namespace std::chrono_literals;

// Separates lines in a block selection delete BufferHistory::Entry
#define BLOCK_SEL_HIST_ENTRY_LINE_SEP_CHAR ((uint32_t)-1)
#define BLOCK_SEL_HIST_ENTRY_LINE_SEP_INDEX ((size_t)-1)

Buffer::Buffer()
{
    /*
     * This thread runs in the background while the buffer is alive.
     * It constantly checks if an update is needed and if it does,
     * it sets `m_isHighlightUpdateNeeded` to false and runs the updater function.
     * The updater function constantly checks if the `m_isHighlightUpdateNeeded` has been
     * set to true because it means that a new update has been requested and the current
     * update is outdated, so the function exits and the loop executes it again.
     */
    m_highlighterThread = std::thread([&](){
            Logger::dbg << "Syntax highlighter thread started (buffer: " << this << ')' << Logger::End;
            while (m_shouldHighlighterLoopRun)
            {
                if (m_isHighlightUpdateNeeded)
                {
                    m_isHighlightUpdateNeeded = false;
                    Logger::dbg << "[Highl. Thread]: Updating highlighting (buffer: "
                        << this << ')' << Logger::End;
                    _updateHighlighting();
                }
                std::this_thread::sleep_for(10ms);
            }
            Logger::dbg << "Syntax highlighter thread exited (buffer: " << this << ')' << Logger::End;
    });

    Logger::dbg << "Setting up autocomplete for buffer: " << this << Logger::End;
    m_autocompPopup = std::make_unique<Autocomp::Popup>();
    Autocomp::dictProvider->get(m_autocompPopup.get());

    Logger::log << "Created a buffer: " << this << Logger::End;
}

void Buffer::open(const std::string& filePath)
{
    TIMER_BEGIN_FUNC();

    glfwSetCursor(g_window, Cursors::busy);

    m_filePath = std::filesystem::canonical(filePath);
    m_isReadOnly = false;
    m_history.clear();
    m_gitRepo.reset();
    m_signs.clear();
    Logger::dbg << "Opening file: " << filePath << Logger::End;

    try
    {
        m_content = loadUnicodeFile(filePath);
        m_highlightBuffer = std::u8string(m_content.length(), Syntax::MARK_NONE);
        m_isHighlightUpdateNeeded = true;
        m_numOfLines = strCountLines(m_content);
        m_gitRepo = std::make_unique<Git::Repo>(filePath);
        updateGitDiff();

        Logger::dbg << "Read "
            << m_content.length() << " characters ("
            << m_numOfLines << " lines) from file" << Logger::End;
    }
    catch (InvalidUnicodeError& e)
    {
        Logger::err << "Error while opening file: " << quoteStr(filePath)
            << ": " << e.what() << Logger::End;
        MessageDialog::create(Dialog::EMPTY_CB, nullptr,
                "Error while opening file: "+quoteStr(filePath)+": "+e.what(),
                MessageDialog::Type::Error);

        // Not a fatal error, so just set the buffer to read-only and continue
        m_isReadOnly = true;
    }
    catch (std::exception& e)
    {
        m_content.clear();
        m_highlightBuffer.clear();
        m_isHighlightUpdateNeeded = true;
        m_numOfLines = 0;
        m_gitRepo.reset();
        m_signs.clear();

        Logger::err << "Failed to open file: " << quoteStr(filePath) << ": " << e.what() << Logger::End;
        g_statMsg.set("Failed to open file: "+quoteStr(filePath)+": "+e.what(), StatusMsg::Type::Error);
        MessageDialog::create(Dialog::EMPTY_CB, nullptr,
                "Failed to open file: "+quoteStr(filePath)+": "+e.what(),
                MessageDialog::Type::Error);

        glfwSetCursor(g_window, nullptr);
        TIMER_END_FUNC();
        return;
    }

    // Try to open the file as writable. If fails, read-only file.
    {
        std::fstream file;
        file.open(filePath, std::ios_base::out | std::ios_base::app);
        if (!file.is_open())
        {
            Logger::warn << "File is read only" << Logger::End;
            m_isReadOnly = true;
        }
        file.close();
    }

    g_statMsg.set("Opened file"+std::string(m_isReadOnly ? " (read-only)" : "")+": \""+filePath+"\"",
            StatusMsg::Type::Info);

    glfwSetCursor(g_window, nullptr);
    TIMER_END_FUNC();
}

int Buffer::saveToFile()
{
    TIMER_BEGIN_FUNC();

    Logger::dbg << "Writing buffer to file: " << m_filePath << Logger::End;
    try
    {
        const icu::UnicodeString str = icu::UnicodeString::fromUTF32(
                (const UChar32*)m_content.data(), m_content.length());
        UFILE* outFile = u_fopen(m_filePath.c_str(), "w", NULL, NULL);
        if (!outFile)
        {
            throw std::runtime_error{"Call to `u_fopen()` failed"};
        }
        if (u_file_write(str.getBuffer(), str.length(), outFile) != str.length())
        {
            u_fclose(outFile);
            throw std::runtime_error{"Call to `u_file_write()` failed"};
        }
        u_fclose(outFile);
    }
    catch (std::exception& e)
    {
        Logger::err << "Failed to write to file: \"" << m_filePath << "\": " << e.what() << Logger::End;
        g_statMsg.set("Failed to write to file: \""+m_filePath+"\": "+e.what(), StatusMsg::Type::Error);
        TIMER_END_FUNC();
        return 1;
    }
    Logger::log << "Wrote " << m_content.size() << " characters" << Logger::End;
    g_statMsg.set("Wrote buffer to file: \""+m_filePath+"\"", StatusMsg::Type::Info);

    m_isModified = false;
    m_isReadOnly = false;
    updateGitDiff();

    TIMER_END_FUNC();
    return 0;
}

int Buffer::saveAsToFile(const std::string& filePath)
{
    const std::string originalPath = filePath;
    m_filePath = filePath;
    Logger::dbg << "Saving as: " << filePath << Logger::End;

    if (saveToFile())
    {
        Logger::err << "Save as failed" << Logger::End;
        m_filePath = originalPath;
        return 1;
    }
    return 0;
}

static size_t getLineLenAt(const String& str, int lineI)
{
    int _lineI{};
    String line;
    LineIterator it{str};
    while (it.next(line) && _lineI != lineI)
    {
        ++_lineI;
    }
    return line.length();
}

void Buffer::renderAutocompPopup()
{
    m_autocompPopup->setPos(
            {m_cursorXPx+2, m_cursorYPx+g_fontSizePx+4});
    m_autocompPopup->render();
}

void Buffer::scrollViewportToCursor()
{
    // Scroll up when the cursor goes out of the viewport
    if (m_cursorLine-5 < -m_scrollY/g_fontSizePx)
    {
        scrollBy(-(m_scrollY+(m_cursorLine-5)*g_fontSizePx));
    }
    // Scroll down when the cursor goes out of the viewport
    // FIXME: Line wrapping breaks this, too
    else if (m_cursorLine+m_scrollY/g_fontSizePx+5 > g_textRenderer->getWindowHeight()/g_fontSizePx)
    {
        scrollBy(-(m_cursorLine*g_fontSizePx+m_scrollY-g_textRenderer->getWindowHeight()+g_fontSizePx*5));
    }
}

void Buffer::_updateHighlighting()
{
    Logger::dbg("Highighter");
    Logger::log << "Updating syntax highlighting" << Logger::End;

    // This is the temporary buffer we are working with
    // `m_highlightBuffer` is replaced with this at the end
    auto highlightBuffer = std::u8string(m_content.length(), Syntax::MARK_NONE);

    auto highlightWord{[&](const String& word, char colorMark, bool shouldBeWholeWord){
        assert(!word.empty());
        size_t start{};
        while (true)
        {
            if (m_isHighlightUpdateNeeded) break;
            size_t found = m_content.find(word, start);
            if (found == String::npos)
                break;
            if (!shouldBeWholeWord |
                ((found == 0 || !isalnum((uchar)m_content[found-1]))
                 && (found+word.size()-1 == m_content.length()-1 || !isalnum((uchar)m_content[found+word.size()]))))
                highlightBuffer.replace(found, word.length(), word.length(), colorMark);
            start = found+word.length();
        }
    }};

    auto highlightStrings{[&](){
        bool isInsideString = false;
        for (size_t i{}; i < m_content.length(); ++i)
        {
            if (m_isHighlightUpdateNeeded) break;
            if (highlightBuffer[i] == Syntax::MARK_NONE && m_content[i] == '"'
                    && (i == 0 || (m_content[i-1] != '\\' || (i >= 2 && m_content[i-2] == '\\'))))
                isInsideString = !isInsideString;
            if (isInsideString || (highlightBuffer[i] == Syntax::MARK_NONE && m_content[i] == '"'))
                highlightBuffer[i] = Syntax::MARK_STRING;
        }
    }};

    auto highlightNumbers{[&](){
        size_t i{};
        while (i < m_content.length())
        {
            if (m_isHighlightUpdateNeeded) break;
            // Go till a number
            while (i < m_content.length() && !isdigit((uchar)m_content[i]) && m_content[i] != '.')
                ++i;
            // Color the number
            while (i < m_content.length() && (isxdigit((uchar)m_content[i]) || m_content[i] == '.' || m_content[i] == 'x'))
                highlightBuffer[i++] = Syntax::MARK_NUMBER;
        }
    }};

    auto highlightPrefixed{[&](const String& prefix, char colorMark){
        size_t charI{};
        LineIterator it{m_content};
        String line;
        while (it.next(line))
        {
            if (m_isHighlightUpdateNeeded) break;
            const size_t found = line.find(prefix);
            if (found != String::npos)
            {
                const size_t beginning = charI+found;
                const size_t size = line.size()-found;
                highlightBuffer.replace(beginning, size, size, colorMark);
            }
            charI += line.size()+1;
        }
    }};

    auto highlightPreprocessors{[&](){
        size_t charI{};
        LineIterator it{m_content};
        String line;
        while (it.next(line))
        {
            if (m_isHighlightUpdateNeeded) break;
            const size_t prefixPos = line.find(Syntax::preprocessorPrefix);
            if (prefixPos != String::npos)
            {
                const size_t beginning = charI+prefixPos;
                if (highlightBuffer[beginning] == Syntax::MARK_NONE)
                {
                    const size_t preprocessorEnd = line.find_first_of(' ', prefixPos);
                    const size_t size = (preprocessorEnd != String::npos ? preprocessorEnd : line.size())-prefixPos;
                    highlightBuffer.replace(beginning, size, size, Syntax::MARK_PREPRO);
                }
            }
            charI += line.size()+1;
        }
    }};

    auto highlightBlockComments{[&](){
        bool isInsideComment = false;
        for (size_t i{}; i < m_content.length(); ++i)
        {
            if (m_isHighlightUpdateNeeded) break;
            if (highlightBuffer[i] != Syntax::MARK_STRING
                    && m_content.substr(i, Syntax::blockCommentBegin.size()) == Syntax::blockCommentBegin)
                isInsideComment = true;
            else if (highlightBuffer[i] != Syntax::MARK_STRING
                    && m_content.substr(i, Syntax::blockCommentEnd.size()) == Syntax::blockCommentEnd)
                isInsideComment = false;
            if (isInsideComment
                    || (highlightBuffer[i] != Syntax::MARK_STRING
                    && (m_content.substr(i, Syntax::blockCommentEnd.size()) == Syntax::blockCommentEnd
                    || (i > 0 && m_content.substr(i-1, Syntax::blockCommentEnd.size()) == Syntax::blockCommentEnd))))
                highlightBuffer[i] = Syntax::MARK_COMMENT;
        }
    }};

    auto highlightCharLiterals{[&](){
        bool isInsideCharLit = false;
        for (size_t i{}; i < m_content.length(); ++i)
        {
            if (m_isHighlightUpdateNeeded) break;
            if (highlightBuffer[i] == Syntax::MARK_NONE && m_content[i] == '\''
                    && (i == 0 || (m_content[i-1] != '\\')))
                isInsideCharLit = !isInsideCharLit;
            if (isInsideCharLit || (highlightBuffer[i] == Syntax::MARK_NONE && m_content[i] == '\''))
                highlightBuffer[i] = Syntax::MARK_CHARLIT;
        }
    }};

    auto highlightChar{[&](char c, char colorMark){
        for (size_t i{}; i < m_content.length(); ++i)
        {
            if (m_isHighlightUpdateNeeded) break;
            if (highlightBuffer[i] != Syntax::MARK_STRING && highlightBuffer[i] != Syntax::MARK_COMMENT
                    && m_content[i] == c)
                highlightBuffer[i] = colorMark;
        }
    }};

    auto highlightFilePaths{[&](){
        auto _isValidFilePath{[&](const String& path){ // -> bool
            try
            {
                if (path[0] == '/')
                    // Absolute path
                    return isValidFilePath(path);
                else
                    // Relative path
                    return isValidFilePath(getParentPath(m_filePath)/std::filesystem::path{path});
            }
            catch (std::exception&)
            {
                return false;
            }
        }};

        for (size_t i{}; i < m_content.length();)
        {
            if (m_isHighlightUpdateNeeded) break;
            while (i < m_content.length() && (isspace((uchar)m_content[i]) || m_content[i] == '"'))
                ++i;

            String word;
            while (i < m_content.length() && !isspace((uchar)m_content[i]) && m_content[i] != '"')
                word += m_content[i++];

            if (_isValidFilePath(word))
            {
                highlightBuffer.replace(i-word.size(), word.size(), word.size(), Syntax::MARK_FILEPATH);
            }

            ++i;
        }
    }};

    if (m_isHighlightUpdateNeeded) return;
    timer.reset();
    for (const auto& word : Syntax::operatorList) highlightWord(word, Syntax::MARK_OPERATOR, false);
    Logger::dbg << "Highlighting of operators took " << timer.getElapsedTimeMs() << "ms" << Logger::End;

    if (m_isHighlightUpdateNeeded) return;
    timer.reset();
    for (const auto& word : Syntax::keywordList) highlightWord(word, Syntax::MARK_KEYWORD, true);
    Logger::dbg << "Highlighting of keywords took " << timer.getElapsedTimeMs() << "ms" << Logger::End;

    if (m_isHighlightUpdateNeeded) return;
    timer.reset();
    for (const auto& word : Syntax::typeList) highlightWord(word, Syntax::MARK_TYPE, true);
    Logger::dbg << "Highlighting of types took " << timer.getElapsedTimeMs() << "ms" << Logger::End;

    if (m_isHighlightUpdateNeeded) return;
    timer.reset();
    highlightNumbers();
    Logger::dbg << "Highlighting of numbers took " << timer.getElapsedTimeMs() << "ms" << Logger::End;

    if (m_isHighlightUpdateNeeded) return;
    timer.reset();
    highlightPrefixed(Syntax::lineCommentPrefix, Syntax::MARK_COMMENT);
    Logger::dbg << "Highlighting of line comments took " << timer.getElapsedTimeMs() << "ms" << Logger::End;

    if (m_isHighlightUpdateNeeded) return;
    timer.reset();
    highlightBlockComments();
    Logger::dbg << "Highlighting of block comments took " << timer.getElapsedTimeMs() << "ms" << Logger::End;

    if (m_isHighlightUpdateNeeded) return;
    timer.reset();
    highlightCharLiterals();
    Logger::dbg << "Highlighting of character literals took " << timer.getElapsedTimeMs() << "ms" << Logger::End;

    if (m_isHighlightUpdateNeeded) return;
    timer.reset();
    highlightStrings();
    Logger::dbg << "Highlighting of strings took " << timer.getElapsedTimeMs() << "ms" << Logger::End;

    if (m_isHighlightUpdateNeeded) return;
    timer.reset();
    highlightPreprocessors();
    Logger::dbg << "Highlighting preprocessor directives took " << timer.getElapsedTimeMs() << "ms" << Logger::End;

    if (m_isHighlightUpdateNeeded) return;
    timer.reset();
    highlightChar(';', Syntax::MARK_SPECCHAR);
    highlightChar('{', Syntax::MARK_SPECCHAR);
    highlightChar('}', Syntax::MARK_SPECCHAR);
    highlightChar('(', Syntax::MARK_SPECCHAR);
    highlightChar(')', Syntax::MARK_SPECCHAR);
    Logger::dbg << "Highlighting special characters took " << timer.getElapsedTimeMs() << "ms" << Logger::End;

    if (m_isHighlightUpdateNeeded) return;
    timer.reset();
    highlightFilePaths();
    Logger::dbg << "Highlighting of paths took " << timer.getElapsedTimeMs() << "ms" << Logger::End;


    m_highlightBuffer = highlightBuffer;

    Logger::log << "Syntax highlighting updated" << Logger::End;
    Logger::log(Logger::End);

    g_isRedrawNeeded = true;
}

void Buffer::updateGitDiff()
{
    const std::string relPath = m_filePath.substr(m_gitRepo->getRepoRoot().size());
    Logger::dbg << "Filepath: " << m_filePath
        << ", git repo root: " << m_gitRepo->getRepoRoot()
        << ", relative path: " << relPath << Logger::End;
    auto diff = m_gitRepo->getDiff(relPath);
    for (const auto& pair : diff)
    {
        Sign sign;
        switch (pair.second)
        {
        case Git::ChangeType::Add: sign = Sign::GitAdd; break;
        case Git::ChangeType::Delete: sign = Sign::GitRemove; break;
        case Git::ChangeType::Modify: sign = Sign::GitModify; break;
        default: assert(false);
        }
        m_signs.push_back({pair.first, sign});
    }
}

void Buffer::updateCursor()
{
    TIMER_BEGIN_FUNC();

    assert(m_cursorLine >= 0);
    assert(m_numOfLines == 0 || m_cursorLine < m_numOfLines);
    assert(m_cursorCol >= 0);

    auto getCursorLineLen{ // -> int
        [&]()
        {
            return getLineLenAt(m_content, m_cursorLine);
        }
    };

    switch (m_cursorMovCmd)
    {
    case CursorMovCmd::Left:
        if (m_cursorCol > 0)
        {
            --m_cursorCol;
            --m_cursorCharPos;
        }
        break;

    case CursorMovCmd::Right:
    {
        auto cursorLineLen = getCursorLineLen();
        if (cursorLineLen > 0 && m_cursorCol < cursorLineLen)
        {
            ++m_cursorCol;
            ++m_cursorCharPos;
        }
        break;
    }

    case CursorMovCmd::Up:
        if (m_cursorLine > 0)
        {
            const int prevCursorCol = m_cursorCol;

            --m_cursorLine;

            if (m_cursorCol != 0) // Column 0 always exists
                // If the new line is smaller than the current cursor column, step back to the end of the line
                m_cursorCol = std::min((int)getCursorLineLen(), m_cursorCol);

            m_cursorCharPos -= prevCursorCol + ((int)getCursorLineLen()+1-m_cursorCol);
        }
        break;

    case CursorMovCmd::Down:
        if (m_cursorLine < m_numOfLines-1)
        {
            const int prevCursorCol = m_cursorCol;
            const int prevCursorLineLen = getCursorLineLen();

            ++m_cursorLine;

            if (m_cursorCol != 0) // Column 0 always exists
                // If the new line is smaller than the current cursor column, step back to the end of the line
                m_cursorCol = std::min((int)getCursorLineLen(), m_cursorCol);

            m_cursorCharPos += (prevCursorLineLen+1-prevCursorCol) + m_cursorCol;
        }
        break;

    case CursorMovCmd::LineBeginning:
        m_cursorCharPos -= m_cursorCol;
        m_cursorCol = 0;
        break;

    case CursorMovCmd::LineEnd:
    {
        const int prevCursorCol = m_cursorCol;
        const int cursorLineLen = getCursorLineLen();
        m_cursorCharPos += cursorLineLen-prevCursorCol;
        m_cursorCol = cursorLineLen;
        break;
    }

    case CursorMovCmd::FirstChar:
        m_cursorLine = 0;
        m_cursorCol = 0;
        m_cursorCharPos = 0;
        break;

    case CursorMovCmd::LastChar:
        m_cursorLine = m_numOfLines == 0 ? 0 : m_numOfLines-1;
        m_cursorCol = getCursorLineLen();
        m_cursorCharPos = m_content.length()-1;
        break;

    case CursorMovCmd::None:
        break;
    }

    assert(m_cursorLine >= 0);
    assert(m_numOfLines == 0 || m_cursorLine < m_numOfLines);
    assert(m_cursorCol >= 0);

    // Scroll up when the cursor goes out of the viewport
    if (m_cursorMovCmd != CursorMovCmd::None)
    {
        scrollViewportToCursor();
        m_isCursorShown = true;
        m_cursorMovCmd = CursorMovCmd::None;
    }

    TIMER_END_FUNC();
}

#define STATUS_LINE_STR_LEN_MAX\
    EDITMODE_STATLINE_STR_LEN /* Mode string */\
    + 3 /* Separator */\
    + 4 /* Cursor line */\
    + 1 /* Separator*/\
    + 3 /* Cursor column */\
    + 3 /* Separator */\
    + 7 /* Cursor character position */\
    + 3 /* Separator */\
    + 4 /* Cursor character decimal value */\
    + 1 /* Separator */\
    + 4 /* Cursor character hex value */\

void Buffer::updateRStatusLineStr()
{
    if (m_selection.mode == Selection::Mode::None)
    {
        m_statusLineStr.str
            = g_editorMode.asStatLineStr()
            + " | " + std::to_string(m_cursorLine + 1)
            + ':'   + std::to_string(m_cursorCol + 1)
            + " | " + std::to_string(m_cursorCharPos)
            + " | " + std::to_string(m_content[m_cursorCharPos])
            + '/' + intToHexStr((int32_t)m_content[m_cursorCharPos])
            ;
        m_statusLineStr.maxLen = std::max((size_t)STATUS_LINE_STR_LEN_MAX, m_statusLineStr.str.size());
    }
    else
    {
        const bool showSizeInLines = (m_selection.mode == Selection::Mode::Line)
                            || (m_selection.mode == Selection::Mode::Block);
        const bool showSizeInCols = m_selection.mode == Selection::Mode::Block;
        const bool showSizeInChars = m_selection.mode == Selection::Mode::Normal;

        m_statusLineStr.str.clear();

        if (showSizeInCols)
            m_statusLineStr.str +=
                std::to_string(abs(m_cursorCol-m_selection.fromCol)+1); // Size in columns

        if (showSizeInLines && showSizeInCols)
            m_statusLineStr.str += 'x';

        if (showSizeInLines)
            m_statusLineStr.str +=
                std::to_string(abs(m_cursorLine-m_selection.fromLine)+1); // Size in lines

        if (showSizeInChars)
            m_statusLineStr.str +=
                std::to_string(abs((int)m_cursorCharPos-(int)m_selection.fromCharI)+1); // Size in chars

        m_statusLineStr.maxLen = std::max(
                size_t(
                    (showSizeInCols ? 3 : 0)
                    + (showSizeInLines && showSizeInCols ? 1 : 0)
                    + (showSizeInLines ? 4 : 0)
                    + (showSizeInChars ? 7 : 0)),
                m_statusLineStr.str.size());
    }
}

bool Buffer::isCharSelected(int lineI, int colI, size_t charI) const
{
    bool isSelected = false;
    switch (m_selection.mode)
    {
    case Selection::Mode::None:
        isSelected = false;
        break;

    case Selection::Mode::Normal:
        if (m_selection.fromCharI < m_cursorCharPos)
        {
            isSelected = charI >= m_selection.fromCharI && charI <= m_cursorCharPos;
        }
        else
        {
            isSelected = charI <= m_selection.fromCharI && charI >= m_cursorCharPos;
        }
        break;

    case Selection::Mode::Line:
        if (m_selection.fromLine < m_cursorLine)
        {
            isSelected = lineI >= m_selection.fromLine && lineI <= m_cursorLine;
        }
        else
        {
            isSelected = lineI <= m_selection.fromLine && lineI >= m_cursorLine;
        }
        break;

    case Selection::Mode::Block:
    {
        bool isLineOk = false;
        if (m_selection.fromLine < m_cursorLine)
        {
            isLineOk = lineI >= m_selection.fromLine && lineI <= m_cursorLine;
        }
        else
        {
            isLineOk = lineI <= m_selection.fromLine && lineI >= m_cursorLine;
        }

        bool isColOk = false;
        if (m_selection.fromCol < m_cursorCol)
        {
            isColOk = colI >= m_selection.fromCol && colI <= m_cursorCol;
        }
        else
        {
            isColOk = colI <= m_selection.fromCol && colI >= m_cursorCol;
        }

        isSelected = isLineOk && isColOk;
        break;
    }
    }
    return isSelected;
}

void Buffer::pasteFromClipboard()
{
    // TODO: Unicode support
    const std::string aClipbText = Clipboard::get();
    if (!aClipbText.empty())
    {
        Logger::dbg << "Pasting: " << quoteStr(aClipbText) << Logger::End;
        for (char c : aClipbText)
        {
            assert((c & 0b10000000) == 0); // Detect non-ASCII chars
            insert(c);
        }
        g_statMsg.set("Pasted text from clipboard ("+std::to_string(aClipbText.size())+" chars)",
                StatusMsg::Type::Info);
    }
    else
    {
        g_statMsg.set("Nothing to paste", StatusMsg::Type::Error);
    }
}

size_t Buffer::copySelectionToClipboard(bool shouldUnselect/*=true*/)
{
    if (m_selection.mode == Selection::Mode::None)
        return 0;

    // Note: Mostly copied from `deleteSelectedChars()`

    String toCopy;
    int lineI{};
    int colI{};
    for (size_t charI{}; charI < m_content.size(); ++charI)
    {
        if (isCharSelected(lineI, colI, charI))
        {
            toCopy += m_content[charI];
            // If we are in block selection mode and this is at the right border of the selection
            if (m_selection.mode == Selection::Mode::Block && colI == std::max(m_selection.fromCol, m_cursorCol))
                toCopy.push_back('\n'); // End of block line
        }

        switch (m_content[charI])
        {
        case '\n': // New line
        case '\v': // Vertical tab
            ++lineI;
            colI = 0;
            continue;

        case '\r': // Carriage return
            continue;

        case '\t': // Tab
            ++colI;
            continue;
        }

        ++colI;
    }

    assert(!toCopy.empty());

    // TODO: Unicode support
    std::string _toCopy;
    for (Char c : toCopy)
    {
        assert((c & 0b10000000) == 0); // Detect non-ASCII chars
        _toCopy += c;
    }
    Logger::dbg << "Copying: " << quoteStr(_toCopy) << Logger::End;

    Clipboard::set(_toCopy);

    if (shouldUnselect)
        m_selection.mode = Selection::Mode::None; // Cancel the selection
    m_isCursorShown = true;
    scrollViewportToCursor();

    g_statMsg.set("Copied text to clipboard ("+std::to_string(_toCopy.size())+" chars)",
            StatusMsg::Type::Info);

    return toCopy.size();
}

void Buffer::cutSelectionToClipboard()
{
    const size_t selLen = copySelectionToClipboard(false);
    deleteSelectedChars();

    g_statMsg.set("Moved text to clipboard ("+std::to_string(selLen)+" chars)",
            StatusMsg::Type::Info);
}

void Buffer::render()
{
    TIMER_BEGIN_FUNC();

    assert(m_size.x > 0);
    assert(m_size.y > 0);

    // Fill background
    g_uiRenderer->renderFilledRectangle(
            m_position,
            {m_position.x+m_size.x, m_position.y+m_size.y},
            RGB_COLOR_TO_RGBA(BG_COLOR)
    );

    // Draw line number background
    g_uiRenderer->renderFilledRectangle(
            m_position,
            {m_position.x+g_fontSizePx*LINEN_BAR_WIDTH, m_position.y+m_size.y},
            RGB_COLOR_TO_RGBA(LINEN_BG_COLOR));

    const float initTextX = m_position.x+g_fontSizePx*LINEN_BAR_WIDTH;
    const float initTextY = m_position.y+m_scrollY;
    float textX = initTextX;
    float textY = initTextY;

    g_textRenderer->prepareForDrawing();
    g_textRenderer->setDrawingColor({1.0f, 1.0f, 1.0f});

    const size_t wordBeg = getCursorWordBeginning();
    const size_t wordEnd = getCursorWordEnd();

    bool isLineBeginning = true;
    bool isLeadingSpace = true;
    int lineI{};
    int colI{};
    size_t charI{(size_t)-1};
    for (Char c : m_content)
    {
        // Don't draw the part of the buffer that is below the viewport
        if (textY > m_position.y+m_size.y)
        {
            break;
        }
        const bool isCharInsideViewport = textY > -g_fontSizePx;

        ++charI;
        if (!isspace((uchar)c))
            isLeadingSpace = false;

        const bool isCharSel = isCharSelected(lineI, colI, charI);
        if (isCharInsideViewport && BUFFER_DRAW_LINE_NUMS && isLineBeginning)
        {
            g_textRenderer->setDrawingColor(
                    lineI == m_cursorLine ? LINEN_ACTIVE_FONT_COLOR : LINEN_FONT_COLOR);

            const std::string lineNumStr =
#if LINEN_DRAW_RELATIVE
                std::to_string(lineI == m_cursorLine ?
                        m_cursorLine+1 :
                        std::abs(lineI-m_cursorLine));
#else
                std::to_string(lineI+1);
#endif

            float digitX = m_position.x
#if LINEN_ALIGN_NONCURR_RIGHT
                + (lineI == m_cursorLine ? 0 : (LINEN_BAR_WIDTH-lineNumStr.length()*0.75f)*g_fontSizePx);
#else
            ;
#endif
            for (char digit : lineNumStr)
            {
                auto dimensions = g_textRenderer->renderChar(digit,
                        {digitX, textY},
                        lineI == m_cursorLine ? FontStyle::Bold : FontStyle::Italic);
                digitX += dimensions.advance/64.0f;
            }

            // Reset color
            g_textRenderer->setDrawingColor({1.0f, 1.0f, 1.0f});


            for (const auto& pair : m_signs)
            {
                // If there is a sign in the current line
                if (pair.first == lineI)
                {
                    // Render the sign
                    signImages[(int)pair.second]->render(
                            {m_position.x+LINEN_BAR_WIDTH*g_fontSizePx-16*(getSignColumn(pair.second)+1), textY},
                            {16, 16});
                    g_textRenderer->prepareForDrawing();
                }
            }
        }

        // Render the cursor line highlight
        if (isCharInsideViewport && isLineBeginning && m_cursorLine == lineI)
        {
            g_uiRenderer->renderFilledRectangle(
                    {textX, initTextY+textY-m_scrollY-m_position.y+2},
                    {textX+m_size.x,
                        initTextY+textY-m_scrollY-m_position.y+2+g_fontSizePx},
                    CURSOR_LINE_COLOR
            );
            // Bind the text renderer shader again
            g_textRenderer->prepareForDrawing();
        }

        // Render indent guides
        if (TAB_SPACE_COUNT > 0 && isCharInsideViewport
         && isLeadingSpace && colI%TAB_SPACE_COUNT==0 && colI != 0)
        {
            g_uiRenderer->renderFilledRectangle(
                    {textX, initTextY+textY-m_scrollY-m_position.y+2},
                    {textX+g_fontSizePx/10, initTextY+textY-m_scrollY-m_position.y+2+g_fontSizePx/5},
                    {0.7f, 0.7f, 0.7f, 0.7f}
            );
            g_uiRenderer->renderFilledRectangle(
                    {textX, initTextY+textY-m_scrollY-m_position.y+2+g_fontSizePx/5*2},
                    {textX+g_fontSizePx/10, initTextY+textY-m_scrollY-m_position.y+2+g_fontSizePx/5*3},
                    {0.7f, 0.7f, 0.7f, 0.7f}
            );
            g_uiRenderer->renderFilledRectangle(
                    {textX, initTextY+textY-m_scrollY-m_position.y+2+g_fontSizePx/5*4},
                    {textX+g_fontSizePx/10, initTextY+textY-m_scrollY-m_position.y+2+g_fontSizePx/5*5},
                    {0.7f, 0.7f, 0.7f, 0.7f}
            );
            // Bind the text renderer shader again
            g_textRenderer->prepareForDrawing();
        }

        /*
         * Render a cursor at the current character position if it is the cursor position.
         */
        auto drawCursorIfNeeded{
            [&](int width)
            {
                if (charI == m_cursorCharPos)
                {
                    m_cursorXPx = textX;
                    m_cursorYPx = initTextY+textY-m_scrollY-m_position.y;
                }

                if ((m_cursorMovCmd != CursorMovCmd::None || m_isCursorShown) && charI == m_cursorCharPos)
                {
                    RGBColor cursColor{};
                    switch (g_editorMode.get())
                    {
                    case EditorMode::_EditorMode::Normal:
                        cursColor = {0.2f, 0.4f, 1.0f};
                        break;

                    case EditorMode::_EditorMode::Insert:
                        cursColor = {0.0f, 1.0f, 0.0f};
                        break;

                    case EditorMode::_EditorMode::Replace:
                        cursColor = {1.0f, 0.0f, 0.0f};
                        break;
                    }

                    if (g_editorMode.get() == EditorMode::_EditorMode::Normal || m_isDimmed)
                    {
                        g_uiRenderer->renderRectangleOutline(
                                {textX-2, initTextY+textY-m_scrollY-m_position.y-2},
                                {textX+width+2, initTextY+textY-m_scrollY-m_position.y+g_fontSizePx+2},
                                {UNPACK_RGB_COLOR(cursColor), 1.0f},
                                2
                        );
                        g_uiRenderer->renderFilledRectangle(
                                {textX-2, initTextY+textY-m_scrollY-m_position.y-2},
                                {textX+width+2, initTextY+textY-m_scrollY-m_position.y+g_fontSizePx+2},
                                {UNPACK_RGB_COLOR(cursColor), 0.4f}
                        );
                    }
                    else if (g_editorMode.get() == EditorMode::_EditorMode::Replace)
                    {
                        g_uiRenderer->renderFilledRectangle(
                                {textX-2,       initTextY+textY-m_scrollY-m_position.y+g_fontSizePx-2},
                                {textX+width+2, initTextY+textY-m_scrollY-m_position.y+g_fontSizePx+2},
                                {UNPACK_RGB_COLOR(cursColor), 1.0f}
                        );
                    }
                    else
                    {
                        g_uiRenderer->renderFilledRectangle(
                                {textX-1, initTextY+textY-m_scrollY-m_position.y-2},
                                {textX+1, initTextY+textY-m_scrollY-m_position.y+g_fontSizePx+2},
                                {UNPACK_RGB_COLOR(cursColor), 0.4f}
                        );
                    }

                    // Bind the text renderer shader again
                    g_textRenderer->prepareForDrawing();
                }
            }
        };

        /*
         * Draws the selection rectangle around the current character if it is selected.
         */
        auto drawCharSelectionMarkIfNeeded{[&](int width){
            if (isCharSel)
            {
                g_uiRenderer->renderFilledRectangle(
                        {textX, initTextY+textY-m_scrollY-m_position.y},
                        {textX+width, initTextY+textY-m_scrollY-m_position.y+g_fontSizePx},
                        {0.156f, 0.541f, 0.862f, 0.2f}
                );

                g_uiRenderer->renderRectangleOutline(
                        {textX, initTextY+textY-m_scrollY-m_position.y},
                        {textX+width, initTextY+textY-m_scrollY-m_position.y+g_fontSizePx},
                        {0.156f, 0.541f, 0.862f},
                        1
                );

                // Bind the text renderer shader again
                g_textRenderer->prepareForDrawing();
            }
        }};

        if (isCharInsideViewport)
        {
            if (charI > wordBeg && charI < wordEnd)
            {
                g_uiRenderer->renderFilledRectangle(
                        {textX, initTextY+textY-m_scrollY-m_position.y+g_fontSizePx+2-g_fontSizePx*0.1f},
                        {textX+g_fontSizePx*0.75f, initTextY+textY-m_scrollY-m_position.y+g_fontSizePx+2},
                        {0.4f, 0.4f, 0.4f, 1.0f}
                );
                g_textRenderer->prepareForDrawing();
            }
        }

        switch (c)
        {
        case '\n': // New line
        case '\v': // Vertical tab
            drawCursorIfNeeded(g_fontSizePx*0.7f);
            drawCharSelectionMarkIfNeeded(g_fontSizePx*0.7f);
            textX = initTextX;
            textY += g_fontSizePx;
            isLineBeginning = true;
            isLeadingSpace = true;
            ++lineI;
            colI = 0;
            continue;

        case '\r': // Carriage return
            // Don't render them or do anything
            continue;

        case '\t': // Tab
            drawCursorIfNeeded(g_fontSizePx*4*0.7f);
            drawCharSelectionMarkIfNeeded(g_fontSizePx*4*0.7f);
            // Draw a horizontal line to mark the character
            g_uiRenderer->renderFilledRectangle(
                    {textX+g_fontSizePx*0.3f,
                     initTextY+textY-m_scrollY-m_position.y+g_fontSizePx/2.0f+2},
                    {textX+g_fontSizePx*0.3f+(g_fontSizePx*0.7f)*4-g_fontSizePx*0.6f,
                     initTextY+textY-m_scrollY-m_position.y+g_fontSizePx/2.0f+4},
                    {0.8f, 0.8f, 0.8f, 0.2f}
            );
            g_textRenderer->prepareForDrawing();
            textX += g_fontSizePx*0.7f*4;
            ++colI;
            continue;
        }

        if (BUFFER_WRAP_LINES && textX+g_fontSizePx > m_position.x+m_size.x)
        {
            textX = initTextX;
            textY += g_fontSizePx;
        }

        uint advance{};
        if (isCharInsideViewport)
        {
            RGBColor charColor;
            FontStyle charStyle;
            if (m_highlightBuffer.size() <= m_content.length()
             && m_highlightBuffer[charI] < Syntax::_SYNTAX_MARK_COUNT)
            {
                charColor = Syntax::syntaxColors[m_highlightBuffer[charI]];
                charStyle = Syntax::syntaxStyles[m_highlightBuffer[charI]];
            }
            else
            {
                charColor = Syntax::syntaxColors[0];
                charStyle = Syntax::syntaxStyles[0];
            }
            g_textRenderer->setDrawingColor(charColor);
            advance = g_textRenderer->renderChar(c, {textX, textY}, charStyle).advance;


#if DRAW_INDENT_RAINBOW
            // Render indent rainbow
            static constexpr int indentColorCount = 6;
            static constexpr RGBAColor indentColors[indentColorCount]{
                {1.0f, 0.7f, 0.0f, 0.1f},
                {1.0f, 1.0f, 0.0f, 0.1f},
                {0.0f, 0.5f, 0.0f, 0.1f},
                {0.0f, 0.0f, 1.0f, 0.1f},
                {0.3f, 0.0f, 0.5f, 0.1f},
                {0.9f, 0.5f, 0.9f, 0.1f}
            };
            if (TAB_SPACE_COUNT > 0 && isLeadingSpace)
            {
                g_uiRenderer->renderFilledRectangle(
                        {textX,            initTextY+textY-m_scrollY-m_position.y+2},
                        {textX+advance/64, initTextY+textY-m_scrollY-m_position.y+2+g_fontSizePx},
                        indentColors[colI/TAB_SPACE_COUNT%indentColorCount]
                );
                g_textRenderer->prepareForDrawing();
            }
#endif


            drawCursorIfNeeded(advance/64.f);
            drawCharSelectionMarkIfNeeded(advance/64.f);
        }
        else
        {
            advance = g_textRenderer->getCharGlyphAdvance(c, FontStyle::Regular);
        }

        textX += (advance/64.0f);
        ++colI;
        isLineBeginning = false;
    }

    if (m_isDimmed)
    {
        g_uiRenderer->renderFilledRectangle(
                m_position,
                {m_position.x+m_size.x, m_position.y+m_size.y},
                {UNPACK_RGB_COLOR(BG_COLOR), 0.5f}
        );
    }
    else
    {
        renderAutocompPopup();
    }

    TIMER_END_FUNC();
}

void Buffer::insert(Char character)
{
    if (m_isReadOnly)
        return;

    TIMER_BEGIN_FUNC();

    assert(m_cursorCol >= 0);
    assert(m_cursorLine >= 0);

    m_history.add(BufferHistory::Entry{
            .action=BufferHistory::Entry::Action::Insert,
            .values=charToStr(character),
            .cursPos=m_cursorCharPos,
            .cursLine=m_cursorLine,
            .cursCol=m_cursorCol,
    });
    m_content.insert(m_cursorCharPos, 1, character);
    m_highlightBuffer.insert(m_cursorCharPos, 1, 0);

    if (character == '\n')
    {
        ++m_cursorLine;
        ++m_cursorCharPos;
        m_cursorCol = 0;
        ++m_numOfLines;
    }
    else
    {
        ++m_cursorCol;
        ++m_cursorCharPos;
    }
    m_isModified = true;

    m_isCursorShown = true;
    m_isHighlightUpdateNeeded = true;
    scrollViewportToCursor();

    TIMER_END_FUNC();
}

void Buffer::replaceChar(Char character)
{
    if (m_isReadOnly)
        return;

    TIMER_BEGIN_FUNC();

    assert(m_cursorCol >= 0);
    assert(m_cursorLine >= 0);

    // Disallow replacing a newline or replacing a char by a newline
    if (character == '\n' || m_content[m_cursorCharPos] == '\n')
    {
        TIMER_END_FUNC();
        return;
    }

    m_history.add(BufferHistory::Entry{
            .action=BufferHistory::Entry::Action::Replace,
            .values=charToStr(m_content[m_cursorCharPos])+charToStr(character),
            .cursPos=m_cursorCharPos,
            .cursLine=m_cursorLine,
            .cursCol=m_cursorCol,
    });
    m_content[m_cursorCharPos] = character;

    ++m_cursorCol;
    ++m_cursorCharPos;
    m_isModified = true;

    m_isCursorShown = true;
    m_isHighlightUpdateNeeded = true;
    scrollViewportToCursor();

    TIMER_END_FUNC();
}

void Buffer::deleteCharBackwards()
{
    if (m_isReadOnly)
        return;

    TIMER_BEGIN_FUNC();

    assert(m_cursorCol >= 0);
    assert(m_cursorLine >= 0);

    // If deleting at the beginning of the line and we have stuff to delete
    if (m_cursorCol == 0 && m_cursorLine != 0)
    {
        m_history.add(BufferHistory::Entry{
                .action=BufferHistory::Entry::Action::Delete,
                .values=charToStr(m_content[m_cursorCharPos-1]),
                .cursPos=m_cursorCharPos-1,
                .cursLine=m_cursorLine,
                .cursCol=m_cursorCol-1,
        });
        --m_cursorLine;
        m_cursorCol = getLineLenAt(m_content, m_cursorLine);
        m_content.erase(m_cursorCharPos-1, 1);
        m_highlightBuffer.erase(m_cursorCharPos-1, 1);
        --m_cursorCharPos;
        m_isModified = true;
    }
    // If deleting in the middle/end of the line and we have stuff to delete
    else if (m_cursorCharPos > 0)
    {
        m_history.add(BufferHistory::Entry{
                .action=BufferHistory::Entry::Action::Delete,
                .values=charToStr(m_content[m_cursorCharPos-1]),
                .cursPos=m_cursorCharPos-1,
                .cursLine=m_cursorLine,
                .cursCol=m_cursorCol-1,
        });
        m_content.erase(m_cursorCharPos-1, 1);
        m_highlightBuffer.erase(m_cursorCharPos-1, 1);
        --m_cursorCol;
        --m_cursorCharPos;
        m_isModified = true;
    }

    assert(m_cursorCol >= 0);
    assert(m_cursorLine >= 0);

    m_isCursorShown = true;
    m_isHighlightUpdateNeeded = true;
    scrollViewportToCursor();

    TIMER_END_FUNC();
}

void Buffer::deleteCharForwardOrSelected()
{
    if (m_isReadOnly)
        return;

    if (m_selection.mode != Selection::Mode::None)
    {
        deleteSelectedChars();
        return;
    }

    TIMER_BEGIN_FUNC();

    assert(m_cursorCol >= 0);
    assert(m_cursorLine >= 0);

    int lineLen = getLineLenAt(m_content, m_cursorLine);

    // If deleting at the end of the line and we have stuff to delete
    if (m_cursorCol == lineLen && m_cursorLine < m_numOfLines)
    {
        m_history.add(BufferHistory::Entry{
                .action=BufferHistory::Entry::Action::Delete,
                .values=charToStr(m_content[m_cursorCharPos]),
                .cursPos=m_cursorCharPos,
                .cursLine=m_cursorLine,
                .cursCol=m_cursorCol,
        });
        m_content.erase(m_cursorCharPos, 1);
        m_highlightBuffer.erase(m_cursorCharPos, 1);
        --m_numOfLines;
        m_isModified = true;
    }
    // If deleting in the middle/beginning of the line and we have stuff to delete
    else if (m_cursorCharPos != (size_t)lineLen && m_cursorCharPos < m_content.length())
    {
        m_history.add(BufferHistory::Entry{
                .action=BufferHistory::Entry::Action::Delete,
                .values=charToStr(m_content[m_cursorCharPos]),
                .cursPos=m_cursorCharPos,
                .cursLine=m_cursorLine,
                .cursCol=m_cursorCol,
        });
        m_content.erase(m_cursorCharPos, 1);
        m_highlightBuffer.erase(m_cursorCharPos, 1);
        m_isModified = true;
    }

    assert(m_cursorCol >= 0);
    assert(m_cursorLine >= 0);

    m_isCursorShown = true;
    scrollViewportToCursor();
    m_isHighlightUpdateNeeded = true;

    TIMER_END_FUNC();
}

void Buffer::undo()
{
    if (m_history.canGoBack())
    {
        auto entry = m_history.goBack();
        switch (entry.action)
        {
        case BufferHistory::Entry::Action::None:
            assert(false);
            return;

        case BufferHistory::Entry::Action::Insert:
            m_content.erase(entry.cursPos, 1);
            m_highlightBuffer.erase(entry.cursPos, 1);
            break;

        case BufferHistory::Entry::Action::Replace:
            m_content[entry.cursPos] = entry.values[0];
            break;

        case BufferHistory::Entry::Action::Delete:
            m_content.insert(entry.cursPos, 1, entry.values[0]);
            m_highlightBuffer.insert(entry.cursPos, 1, Syntax::MARK_NONE);
            break;

        case BufferHistory::Entry::Action::DeleteNormalSelection:
        case BufferHistory::Entry::Action::DeleteLineSelection:
            m_content.insert(std::min(entry.cursPos, entry.selBeginPos), entry.values);
            m_highlightBuffer.insert(std::min(entry.cursPos, entry.selBeginPos), entry.values.size(), Syntax::MARK_NONE);
            break;

        case BufferHistory::Entry::Action::DeleteBlockSelection:
        {
            assert(!entry.values.empty());

            // FIXME: When selection starts at the bottom left or top right corner,
            //        the block gets messed up

            int blockSepCount = 0;
            String line;
            int insertLine = std::min(entry.cursLine, entry.selBeginLine);
            int insertCol = std::min(entry.selBeginCol, entry.cursCol);
            int insertPos = std::min(entry.cursPos, entry.selBeginPos);
            for (Char c : entry.values)
            {
                if (c == BLOCK_SEL_HIST_ENTRY_LINE_SEP_CHAR)
                {
                    m_content.insert(insertPos, line);
                    m_highlightBuffer.insert(insertPos, line.size(), Syntax::MARK_NONE);
                    Logger::log << "INSERTED LINE: " << strToAscii(line) << Logger::End;
                    // BUG?
                    insertPos += getLineLenAt(m_content, insertLine)+1;
                    line.clear();
                    ++insertLine;
                    ++blockSepCount;
                }
                else
                {
                    line += c;
                }
            }

            Logger::log << "Undone a block selection of " << entry.values.size()-blockSepCount
                << " characters and " << blockSepCount << " block separators" << Logger::End;

            break;
        }
        }

        assert(entry.cursPos != -1_st);
        assert(entry.cursLine != -1);
        assert(entry.cursCol != -1);
        m_cursorCharPos = entry.cursPos;
        m_cursorLine = entry.cursLine;
        m_cursorCol = entry.cursCol;
        scrollViewportToCursor();
        m_isHighlightUpdateNeeded = true;
        g_isRedrawNeeded = true;
        Logger::dbg << "Went back in history" << Logger::End;
    }
    else
    {
        Logger::dbg << "Cannot go back in history" << Logger::End;
    }
}

void Buffer::redo()
{
    if (m_history.canGoForward())
    {
        auto entry = m_history.goForward();
        switch (entry.action)
        {
        case BufferHistory::Entry::Action::None:
            assert(false);
            return;

        case BufferHistory::Entry::Action::Insert:
            m_content.insert(entry.cursPos, 1, entry.values[0]);
            m_highlightBuffer.insert(entry.cursPos, 1, Syntax::MARK_NONE);
            break;

        case BufferHistory::Entry::Action::Replace:
            m_content[entry.cursPos] = entry.values[1];
            break;

        case BufferHistory::Entry::Action::Delete:
            m_content.erase(entry.cursPos, 1);
            m_highlightBuffer.erase(entry.cursPos, 1);
            break;

        case BufferHistory::Entry::Action::DeleteNormalSelection:
#pragma message("TODO")
            assert(false);
            break;

        case BufferHistory::Entry::Action::DeleteLineSelection:
#pragma message("TODO")
            assert(false);
            break;

        case BufferHistory::Entry::Action::DeleteBlockSelection:
#pragma message("TODO")
            assert(false);
            break;
        }

        assert(entry.cursPos != -1_st);
        assert(entry.cursLine != -1);
        assert(entry.cursCol != -1);
        m_cursorCharPos = entry.cursPos;
        m_cursorLine = entry.cursLine;
        m_cursorCol = entry.cursCol;
        m_isHighlightUpdateNeeded = true;
        scrollViewportToCursor();
        g_isRedrawNeeded = true;
        Logger::dbg << "Went forward in history" << Logger::End;
    }
    else
    {
        Logger::dbg << "Cannot go forward in history" << Logger::End;
    }
}

void Buffer::triggerAutocompPopup()
{
    m_autocompPopup->setVisibility(true);
    g_isRedrawNeeded = true;
}

void Buffer::autocompPopupSelectNextItem()
{
    m_autocompPopup->selectNextItem();
    g_isRedrawNeeded = true;
}

void Buffer::autocompPopupSelectPrevItem()
{
    m_autocompPopup->selectPrevItem();
    g_isRedrawNeeded = true;
}

void Buffer::hideAutocompPopup()
{
    m_autocompPopup->setVisibility(false);
}

void Buffer::startSelection(Selection::Mode mode)
{
    // If there is no selection in progress, start a new selection.
    // Otherwise, just switch the selection mode
    if (m_selection.mode == Selection::Mode::None)
    {
        m_selection.fromCol = m_cursorCol;
        m_selection.fromLine = m_cursorLine;
        m_selection.fromCharI = m_cursorCharPos;
    }
    m_selection.mode = mode;
}

void Buffer::closeSelection()
{
    m_selection.mode = Selection::Mode::None;
}

void Buffer::deleteSelectedChars()
{
    if (m_isReadOnly)
        return;

    if (m_selection.mode == Selection::Mode::None)
        return;

    std::vector<size_t> charIndicesToDel;
    int lineI{};
    int colI{};
    for (size_t charI{}; charI < m_content.size(); ++charI)
    {
        if (isCharSelected(lineI, colI, charI))
        {
            charIndicesToDel.push_back(charI);
            // If we are in block selection mode and this is at the right border of the selection
            if (m_selection.mode == Selection::Mode::Block && colI == std::max(m_selection.fromCol, m_cursorCol))
                charIndicesToDel.push_back(BLOCK_SEL_HIST_ENTRY_LINE_SEP_INDEX); // End of block line
        }

        switch (m_content[charI])
        {
        case '\n': // New line
        case '\v': // Vertical tab
            ++lineI;
            colI = 0;
            continue;

        case '\r': // Carriage return
            continue;

        case '\t': // Tab
            ++colI;
            continue;
        }

        ++colI;
    }

    assert(!charIndicesToDel.empty());

    // Save the deleted chars first
    String deletedStr;
    for (size_t toDel : charIndicesToDel)
    {
        if (toDel == BLOCK_SEL_HIST_ENTRY_LINE_SEP_INDEX)
            deletedStr += BLOCK_SEL_HIST_ENTRY_LINE_SEP_CHAR;
        else
            deletedStr += m_content[toDel];
    }

    int blockSepCount = 0;
    // Do the delete
    for (size_t i{charIndicesToDel.size()-1}; i != -1_st; --i)
    {
        size_t toDel = charIndicesToDel[i];
        if (toDel == BLOCK_SEL_HIST_ENTRY_LINE_SEP_INDEX)
        {
            ++blockSepCount;
            continue;
        }
        m_content.erase(toDel, 1);
        m_highlightBuffer.erase(toDel, 1);
    }

    Logger::dbg << "Deleted " << deletedStr.size()-blockSepCount << " characters (block has "
        << blockSepCount << " separators)" << Logger::End;

    switch (m_selection.mode)
    {
        case Selection::Mode::None: break; // Unreachable

        case Selection::Mode::Normal:
            m_history.add(BufferHistory::Entry{
                    .action=BufferHistory::Entry::Action::DeleteNormalSelection,
                    .values=deletedStr,
                    .cursPos=m_cursorCharPos,
                    .cursLine=m_cursorLine,
                    .cursCol=m_cursorCol,
                    .selBeginPos=m_selection.fromCharI,
                    .selBeginLine=m_selection.fromLine,
                    .selBeginCol=m_selection.fromCol,
            });
            break;

        case Selection::Mode::Line:
            m_history.add(BufferHistory::Entry{
                    .action=BufferHistory::Entry::Action::DeleteLineSelection,
                    .values=deletedStr,
                    .cursPos=m_cursorCharPos-m_cursorCol, // Calculate line beginning
                    .cursLine=m_cursorLine,
                    .cursCol=0,
                    .selBeginPos=m_selection.fromCharI-m_selection.fromCol, // Calculate line beginning
                    .selBeginLine=m_selection.fromLine,
            });
            break;

        case Selection::Mode::Block:
            m_history.add(BufferHistory::Entry{
                    .action=BufferHistory::Entry::Action::DeleteBlockSelection,
                    .values=deletedStr,
                    .cursPos=m_cursorCharPos,
                    .cursLine=m_cursorLine,
                    .cursCol=m_cursorCol,
                    .selBeginPos=m_selection.fromCharI,
                    .selBeginLine=m_selection.fromLine,
                    .selBeginCol=m_selection.fromCol,
            });
            break;

    }

    // Jump the cursor to the right place
    if (m_cursorCharPos > m_selection.fromCharI)
    {
        m_cursorCol = m_selection.fromCol;
        m_cursorLine = m_selection.fromLine;
        m_cursorCharPos = m_selection.fromCharI;
    }

    m_selection.mode = Selection::Mode::None; // Cancel the selection
    m_isModified = true;
    m_isCursorShown = true;
    scrollViewportToCursor();
    m_isHighlightUpdateNeeded = true;
    g_isRedrawNeeded = true;
}

Buffer::~Buffer()
{
    glfwSetCursor(g_window, Cursors::busy);
    m_isHighlightUpdateNeeded = true; // Exit the current update
    m_shouldHighlighterLoopRun = false; // Signal the thread that we don't want more syntax updates
    m_highlighterThread.join(); // Wait for the thread
    Logger::log << "Destroyed a buffer: " << this << " (" << m_filePath << ')' << Logger::End;
    glfwSetCursor(g_window, nullptr);
}
