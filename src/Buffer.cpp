#include "Buffer.h"
#include "config.h"
#include "Timer.h"
#include "types.h"
#include "modes.h"
#include "Syntax.h"
#include "dialogs/MessageDialog.h"
#include "unicode/ustdio.h"
#include "common/file.h"
#include "common/string.h"
#include "Clipboard.h"
#include "ThemeLoader.h"
#include <fstream>
#include <sstream>
#include <chrono>
#include <filesystem>
#include <cctype>
using namespace std::chrono_literals;

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
    m_buffWordProvid = std::make_unique<Autocomp::BufferWordProvider>();
    m_pathProvid = std::make_unique<Autocomp::PathProvider>();

    Logger::log << "Created a buffer: " << this << Logger::End;
}

static inline Buffer::fileModTime_t getFileModTime(const std::string& path)
{
    return std::filesystem::last_write_time(std::filesystem::path{path}).time_since_epoch().count();
}

void Buffer::open(const std::string& filePath, bool isReload/*=false*/)
{
    TIMER_BEGIN_FUNC();

    glfwSetCursor(g_window, Cursors::busy);

    m_filePath = filePath;
    m_isReadOnly = false;
    m_history.clear();
    m_gitRepo.reset();
    m_signs.clear();
    m_lastFileUpdateTime = 0;
    if (isReload)
        Logger::dbg << "Reloading file: " << filePath << Logger::End;
    else
        Logger::dbg << "Opening file: " << filePath << Logger::End;

    try
    {
        m_filePath = std::filesystem::canonical(filePath);
        m_content = splitStrToLines(loadUnicodeFile(filePath), true);
        m_highlightBuffer = std::u8string(countLineListLen(m_content), Syntax::MARK_NONE);
        m_isHighlightUpdateNeeded = true;
        m_gitRepo = std::make_unique<Git::Repo>(filePath);
        m_lastFileUpdateTime = getFileModTime(m_filePath);
        updateGitDiff();

        Logger::dbg << "Read "
            << countLineListLen(m_content) << " characters ("
            << m_content.size() << " lines) from file" << Logger::End;
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
        m_gitRepo.reset();
        m_signs.clear();

        if (isReload)
        {
            Logger::err << "Failed to reload file: " << quoteStr(filePath) << ": " << e.what() << Logger::End;
            g_statMsg.set("Failed to reload file: "+quoteStr(filePath)+": "+e.what(), StatusMsg::Type::Error);
            MessageDialog::create(Dialog::EMPTY_CB, nullptr,
                    "Failed to reload file: "+quoteStr(filePath)+": "+e.what(),
                    MessageDialog::Type::Error);
        }
        else
        {
            Logger::err << "Failed to open file: " << quoteStr(filePath) << ": " << e.what() << Logger::End;
            g_statMsg.set("Failed to open file: "+quoteStr(filePath)+": "+e.what(), StatusMsg::Type::Error);
            MessageDialog::create(Dialog::EMPTY_CB, nullptr,
                    "Failed to open file: "+quoteStr(filePath)+": "+e.what(),
                    MessageDialog::Type::Error);
        }

        glfwSetCursor(g_window, nullptr);
        TIMER_END_FUNC();
        return;
    }

    if (isReadOnlyFile(filePath))
    {
        Logger::warn << "File is read only" << Logger::End;
        m_isReadOnly = true;
    }

    { // Regenerate the initial autocomplete list for `m_buffWordProvid`
        m_buffWordProvid->clear();

        for (const auto& line : m_content)
        {
            String word;
            for (Char c : line)
            {
                // TODO: Filter out operators and such
                if (u_isspace(c))
                {
                    // End of word
                    if (!word.empty())
                    {
                        //Logger::dbg << "Feeding word: " << quoteStr(strToAscii(word)) << Logger::End;
                        m_buffWordProvid->add(word);
                        word.clear();
                    }
                }
                else
                {
                    word += c;
                }
            }
        }
    }
    regenAutocompList();

    if (isReload)
    {
        g_statMsg.set("Reloaded file"+std::string(m_isReadOnly ? " (read-only)" : "")+": \""+filePath+"\"",
                StatusMsg::Type::Info);
    }
    else
    {
        g_statMsg.set("Opened file"+std::string(m_isReadOnly ? " (read-only)" : "")+": \""+filePath+"\"",
                StatusMsg::Type::Info);
    }

    glfwSetCursor(g_window, nullptr);
    TIMER_END_FUNC();
}

int Buffer::saveToFile()
{
    TIMER_BEGIN_FUNC();

    Logger::dbg << "Writing buffer to file: " << m_filePath << Logger::End;
    size_t contentLen{};
    try
    {
        // TODO: Is there a way to write the content line-by-line to the file?
        //       That would be faster.
        const String content = lineVecConcat(m_content);
        contentLen = content.length();
        const icu::UnicodeString str = icu::UnicodeString::fromUTF32(
                (const UChar32*)content.data(), content.length());
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
    Logger::log << "Wrote " << contentLen << " characters ("
        << m_content.size() << " lines)" << Logger::End;
    g_statMsg.set("Wrote buffer to file: \""+m_filePath+"\"", StatusMsg::Type::Info);

    m_isModified = false;
    m_isReadOnly = false;
    m_gitRepo = std::make_unique<Git::Repo>(m_filePath);
    m_lastFileUpdateTime = getFileModTime(m_filePath);
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
    else if (m_cursorLine+m_scrollY/g_fontSizePx+5 > m_size.y/g_fontSizePx)
    {
        scrollBy(-(m_cursorLine*g_fontSizePx+m_scrollY-m_size.y+g_fontSizePx*5));
    }
}

void Buffer::centerCursor()
{
    m_scrollY = -(m_cursorLine*g_fontSizePx-m_size.y/2);
    if (m_scrollY > 0)
        m_scrollY = 0;
}

void Buffer::_updateHighlighting()
{
    // TODO: This should really be optimized, don't generate a concatenated buffer

    Logger::dbg("Highighter");
    Logger::log << "Updating syntax highlighting" << Logger::End;

    m_findResultIs.clear();
    g_isRedrawNeeded = true;

    const auto buffer = lineVecConcat(m_content);

    // This is the temporary buffer we are working with
    // `m_highlightBuffer` is replaced with this at the end
    auto highlightBuffer = std::u8string(buffer.length(), Syntax::MARK_NONE);

    auto highlightWord{[&](const String& word, char colorMark, bool shouldBeWholeWord){
        assert(!word.empty());
        size_t start{};
        while (true)
        {
            if (m_isHighlightUpdateNeeded) break;
            size_t found = buffer.find(word, start);
            if (found == String::npos)
                break;
            if (!shouldBeWholeWord |
                ((found == 0 || !isalnum((uchar)buffer[found-1]))
                 && (found+word.size()-1 == buffer.length()-1 || !isalnum((uchar)buffer[found+word.size()]))))
                highlightBuffer.replace(found, word.length(), word.length(), colorMark);
            start = found+word.length();
        }
    }};

    auto highlightStrings{[&](){
        bool isInsideString = false;
        for (size_t i{}; i < buffer.length(); ++i)
        {
            if (m_isHighlightUpdateNeeded) break;
            if (highlightBuffer[i] == Syntax::MARK_NONE && buffer[i] == '"'
                    && (i == 0 || (buffer[i-1] != '\\' || (i >= 2 && buffer[i-2] == '\\'))))
                isInsideString = !isInsideString;
            if (isInsideString || (highlightBuffer[i] == Syntax::MARK_NONE && buffer[i] == '"'))
                highlightBuffer[i] = Syntax::MARK_STRING;
        }
    }};

    auto highlightNumbers{[&](){
        size_t i{};
        while (i < buffer.length())
        {
            if (m_isHighlightUpdateNeeded) break;
            // Go till a number
            while (i < buffer.length() && !isdigit((uchar)buffer[i]) && buffer[i] != '.')
                ++i;
            // Color the number
            while (i < buffer.length() && (isxdigit((uchar)buffer[i]) || buffer[i] == '.' || buffer[i] == 'x'))
                highlightBuffer[i++] = Syntax::MARK_NUMBER;
        }
    }};

    auto highlightPrefixed{[&](const String& prefix, char colorMark){
        size_t charI{};
        LineIterator<String> it{buffer};
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
        LineIterator<String> it{buffer};
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
        for (size_t i{}; i < buffer.length(); ++i)
        {
            if (m_isHighlightUpdateNeeded) break;
            if (highlightBuffer[i] != Syntax::MARK_STRING
                    && buffer.substr(i, Syntax::blockCommentBegin.size()) == Syntax::blockCommentBegin)
                isInsideComment = true;
            else if (highlightBuffer[i] != Syntax::MARK_STRING
                    && buffer.substr(i, Syntax::blockCommentEnd.size()) == Syntax::blockCommentEnd)
                isInsideComment = false;
            if (isInsideComment
                    || (highlightBuffer[i] != Syntax::MARK_STRING
                    && (buffer.substr(i, Syntax::blockCommentEnd.size()) == Syntax::blockCommentEnd
                    || (i > 0 && buffer.substr(i-1, Syntax::blockCommentEnd.size()) == Syntax::blockCommentEnd))))
                highlightBuffer[i] = Syntax::MARK_COMMENT;
        }
    }};

    auto highlightCharLiterals{[&](){
        bool isInsideCharLit = false;
        for (size_t i{}; i < buffer.length(); ++i)
        {
            if (m_isHighlightUpdateNeeded) break;
            if (highlightBuffer[i] == Syntax::MARK_NONE && buffer[i] == '\''
                    && (i == 0 || (buffer[i-1] != '\\' || (i < 2 || buffer[i-2] == '\\'))))
                isInsideCharLit = !isInsideCharLit;
            if (buffer[i] == '\n')
                isInsideCharLit = false;
            if (isInsideCharLit || (highlightBuffer[i] == Syntax::MARK_NONE && buffer[i] == '\''))
                highlightBuffer[i] = Syntax::MARK_CHARLIT;
        }
    }};

    auto highlightChar{[&](Char c, char colorMark){
        for (size_t i{}; i < buffer.length(); ++i)
        {
            if (m_isHighlightUpdateNeeded) break;
            if (highlightBuffer[i] != Syntax::MARK_STRING && highlightBuffer[i] != Syntax::MARK_COMMENT && buffer[i] == c)
                highlightBuffer[i] = colorMark;
        }
    }};

    auto highlightFilePathsAndUrls{[&](){
        auto _isValidFilePathOrUrl{[&](const String& path){ // -> bool
            if (isFormallyValidUrl(path))
                return true;
            try
            {
                if (path[0] == '/')
                    // Absolute path
                    return isValidFilePath(path);
                else
                    // Relative path
                    return isValidFilePath((getParentPath(m_filePath)/std::filesystem::path{path}).string());
            }
            catch (std::exception&)
            {
                return false;
            }
        }};

        for (size_t i{}; i < buffer.length();)
        {
            if (m_isHighlightUpdateNeeded) break;
            while (i < buffer.length() && (isspace((uchar)buffer[i]) || buffer[i] == '"'))
                ++i;

            String word;
            while (i < buffer.length() && !isspace((uchar)buffer[i]) && buffer[i] != '"')
                word += buffer[i++];

            if (_isValidFilePathOrUrl(word))
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

    //--------------------------------------------------------------------------

    if (m_isHighlightUpdateNeeded) return;
    timer.reset();
    highlightPrefixed(Syntax::lineCommentPrefix, Syntax::MARK_COMMENT);
    Logger::dbg << "Highlighting of line comments took " << timer.getElapsedTimeMs() << "ms" << Logger::End;

    if (m_isHighlightUpdateNeeded) return;
    timer.reset();
    highlightBlockComments();
    Logger::dbg << "Highlighting of block comments took " << timer.getElapsedTimeMs() << "ms" << Logger::End;

    //--------------------------------------------------------------------------

    if (m_isHighlightUpdateNeeded) return;
    timer.reset();
    highlightCharLiterals();
    Logger::dbg << "Highlighting of character literals took " << timer.getElapsedTimeMs() << "ms" << Logger::End;

    if (m_isHighlightUpdateNeeded) return;
    timer.reset();
    highlightStrings();
    Logger::dbg << "Highlighting of strings took " << timer.getElapsedTimeMs() << "ms" << Logger::End;

    //--------------------------------------------------------------------------

    if (m_isHighlightUpdateNeeded) return;
    timer.reset();
    highlightPreprocessors();
    Logger::dbg << "Highlighting preprocessor directives took " << timer.getElapsedTimeMs() << "ms" << Logger::End;

    //--------------------------------------------------------------------------

    if (m_isHighlightUpdateNeeded) return;
    timer.reset();
    highlightWord(U"TODO:", Syntax::MARK_TODO, true);
    Logger::dbg << "Highlighting of TODOs took " << timer.getElapsedTimeMs() << "ms" << Logger::End;

    if (m_isHighlightUpdateNeeded) return;
    timer.reset();
    highlightWord(U"FIXME:", Syntax::MARK_FIXME, true);
    Logger::dbg << "Highlighting of FIXMEs took " << timer.getElapsedTimeMs() << "ms" << Logger::End;

    if (m_isHighlightUpdateNeeded) return;
    timer.reset();
    highlightWord(U"XXX:", Syntax::MARK_XXX, true);
    Logger::dbg << "Highlighting of XXXs took " << timer.getElapsedTimeMs() << "ms" << Logger::End;

    //--------------------------------------------------------------------------

    if (m_isHighlightUpdateNeeded) return;
    timer.reset();
    highlightChar(U';', Syntax::MARK_SPECCHAR);
    if (m_isHighlightUpdateNeeded) return;
    highlightChar(U'{', Syntax::MARK_SPECCHAR);
    if (m_isHighlightUpdateNeeded) return;
    highlightChar(U'}', Syntax::MARK_SPECCHAR);
    if (m_isHighlightUpdateNeeded) return;
    highlightChar(U'(', Syntax::MARK_SPECCHAR);
    if (m_isHighlightUpdateNeeded) return;
    highlightChar(U')', Syntax::MARK_SPECCHAR);
    Logger::dbg << "Highlighting special characters took " << timer.getElapsedTimeMs() << "ms" << Logger::End;

    //--------------------------------------------------------------------------

    if (m_isHighlightUpdateNeeded) return;
    timer.reset();
    highlightFilePathsAndUrls();
    Logger::dbg << "Highlighting of paths and URLs took " << timer.getElapsedTimeMs() << "ms" << Logger::End;


    m_highlightBuffer = highlightBuffer;

    Logger::log << "Syntax highlighting updated" << Logger::End;
    Logger::log(Logger::End);
    g_isRedrawNeeded = true;

    findUpdate();
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
    assert(m_content.empty() || (size_t)m_cursorLine < m_content.size());
    assert(m_cursorCol >= 0);
    assert(m_content.empty() || m_cursorCharPos < countLineListLen(m_content));

    auto getCursorLineLen{[&](){
        return m_content[m_cursorLine].size();
    }};

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
        const size_t cursorLineLen = getCursorLineLen();
        if (cursorLineLen > 0 && (size_t)m_cursorCol < cursorLineLen-1)
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
                m_cursorCol = std::min((int)getCursorLineLen()-1, m_cursorCol);

            m_cursorCharPos -= prevCursorCol + ((int)getCursorLineLen()-m_cursorCol);
        }
        break;

    case CursorMovCmd::Down:
        if ((size_t)m_cursorLine < m_content.size()-1)
        {
            const int prevCursorCol = m_cursorCol;
            const int prevCursorLineLen = getCursorLineLen();

            ++m_cursorLine;

            if (m_cursorCol != 0) // Column 0 always exists
                // If the new line is smaller than the current cursor column, step back to the end of the line
                m_cursorCol = std::min((int)getCursorLineLen()-1, m_cursorCol);

            m_cursorCharPos += (prevCursorLineLen-prevCursorCol) + m_cursorCol;
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
        m_cursorCharPos += cursorLineLen-1-prevCursorCol;
        m_cursorCol = cursorLineLen-1;
        break;
    }

    case CursorMovCmd::FirstChar:
        m_cursorLine = 0;
        m_cursorCol = 0;
        m_cursorCharPos = 0;
        break;

    case CursorMovCmd::LastChar:
        m_cursorLine = m_content.empty() ? 0 : m_content.size()-1;
        m_cursorCol = getCursorLineLen()-1;
        m_cursorCharPos = countLineListLen(m_content)-1;
        break;

    case CursorMovCmd::SWordEnd:
    {
        const int contentSize = countLineListLen(m_content);

        // Skip spaces
        while ((int)m_cursorCharPos < contentSize)
        {
            int newCCol = m_cursorCol + 1;
            int newCLine = m_cursorLine;
            size_t newCPos = m_cursorCharPos + 1;
            if ((size_t)newCCol == m_content[newCLine].length()) // If at the end of line
            {
                ++newCLine;
                newCCol = 0;
                if ((size_t)newCLine >= m_content.size()) // End of bufer?
                    break;
            }

            if (!u_isspace(m_content[newCLine][newCCol]))
                break;

            m_cursorCharPos = newCPos;
            m_cursorLine = newCLine;
            m_cursorCol = newCCol;
        }

        // Go until spaces
        while ((int)m_cursorCharPos < contentSize)
        {
            int newCCol = m_cursorCol + 1;
            int newCLine = m_cursorLine;
            size_t newCPos = m_cursorCharPos + 1;
            if ((size_t)newCCol == m_content[newCLine].length()) // If at the end of line
            {
                ++newCLine;
                newCCol = 0;
                if ((size_t)newCLine >= m_content.size()) // End of bufer?
                    break;
            }

            if (u_isspace(m_content[newCLine][newCCol]))
                break;

            m_cursorCharPos = newCPos;
            m_cursorLine = newCLine;
            m_cursorCol = newCCol;
        }
        break;
    }

    case CursorMovCmd::SWordBeginning:
    {
        // FIXME: In VIM an empty line is also a WORD

        // Skip spaces
        while ((int)m_cursorCharPos >= 0)
        {
            int newCCol = m_cursorCol - 1;
            int newCLine = m_cursorLine;
            size_t newCPos = m_cursorCharPos - 1;
            if (newCCol == -1) // If at the beginning of line
            {
                --newCLine;
                if (newCLine == -1) // Buffer beginning?
                    break;
                newCCol = m_content[newCLine].length()-1; // Go to line end
            }

            if (!u_isspace(m_content[newCLine][newCCol]))
                break;

            m_cursorCharPos = newCPos;
            m_cursorLine = newCLine;
            m_cursorCol = newCCol;
        }

        // Go until spaces
        while ((int)m_cursorCharPos >= 0)
        {
            int newCCol = m_cursorCol - 1;
            int newCLine = m_cursorLine;
            size_t newCPos = m_cursorCharPos - 1;
            if (newCCol == -1) // If at the beginning of line
            {
                --newCLine;
                if (newCLine == -1) // Buffer beginning?
                    break;
                newCCol = m_content[newCLine].length()-1; // Go to line end
            }

            if (u_isspace(m_content[newCLine][newCCol]))
                break;

            m_cursorCharPos = newCPos;
            m_cursorLine = newCLine;
            m_cursorCol = newCCol;
        }
        break;
    }

    case CursorMovCmd::SNextWord:
    {
        // FIXME: In VIM an empty line is also a WORD

        const int contentSize = countLineListLen(m_content);

        // If inside spaces
        if (u_isspace(m_content[m_cursorLine][m_cursorCol]))
        {
            // Skip spaces
            while ((int)m_cursorCharPos < contentSize)
            {
                if (!u_isspace(m_content[m_cursorLine][m_cursorCol]))
                    break;

                int newCCol = m_cursorCol + 1;
                int newCLine = m_cursorLine;
                size_t newCPos = m_cursorCharPos + 1;
                if ((size_t)newCCol == m_content[newCLine].length()) // If at the end of line
                {
                    ++newCLine;
                    newCCol = 0;
                    if ((size_t)newCLine >= m_content.size()) // End of bufer?
                        break;
                }
                m_cursorCharPos = newCPos;
                m_cursorLine = newCLine;
                m_cursorCol = newCCol;
            }
        }
        // If inside a word
        else
        {
            // Go until spaces
            while ((int)m_cursorCharPos < contentSize)
            {
                if (u_isspace(m_content[m_cursorLine][m_cursorCol]))
                    break;

                int newCCol = m_cursorCol + 1;
                int newCLine = m_cursorLine;
                size_t newCPos = m_cursorCharPos + 1;
                if ((size_t)newCCol == m_content[newCLine].length()) // If at the end of line
                {
                    ++newCLine;
                    newCCol = 0;
                    if ((size_t)newCLine >= m_content.size()) // End of bufer?
                        break;
                }

                m_cursorCharPos = newCPos;
                m_cursorLine = newCLine;
                m_cursorCol = newCCol;
            }

            // Skip spaces
            while ((int)m_cursorCharPos < contentSize)
            {
                if (!u_isspace(m_content[m_cursorLine][m_cursorCol]))
                    break;

                int newCCol = m_cursorCol + 1;
                int newCLine = m_cursorLine;
                size_t newCPos = m_cursorCharPos + 1;
                if ((size_t)newCCol == m_content[newCLine].length()) // If at the end of line
                {
                    ++newCLine;
                    newCCol = 0;
                    if ((size_t)newCLine >= m_content.size()) // End of bufer?
                        break;
                }

                m_cursorCharPos = newCPos;
                m_cursorLine = newCLine;
                m_cursorCol = newCCol;
            }
        }
        break;
    }

    case CursorMovCmd::PageUp:
    {
        const size_t pageSize = m_size.y / g_fontSizePx;
        for (size_t i{}; i < pageSize && m_cursorLine > 0; ++i)
        {
            const int prevCursorCol = m_cursorCol;

            --m_cursorLine;

            if (m_cursorCol != 0) // Column 0 always exists
                // If the new line is smaller than the current cursor column, step back to the end of the line
                m_cursorCol = std::min((int)getCursorLineLen()-1, m_cursorCol);

            m_cursorCharPos -= prevCursorCol + ((int)getCursorLineLen()-m_cursorCol);
        }
        break;
    }

    case CursorMovCmd::PageDown:
    {
        const size_t pageSize = m_size.y / g_fontSizePx;
        for (size_t i{}; i < pageSize && (size_t)m_cursorLine < m_content.size()-1; ++i)
        {
            const int prevCursorCol = m_cursorCol;
            const int prevCursorLineLen = getCursorLineLen();

            ++m_cursorLine;

            if (m_cursorCol != 0) // Column 0 always exists
                // If the new line is smaller than the current cursor column, step back to the end of the line
                m_cursorCol = std::min((int)getCursorLineLen()-1, m_cursorCol);

            m_cursorCharPos += (prevCursorLineLen-prevCursorCol) + m_cursorCol;
        }
        break;
    }

    case CursorMovCmd::None:
        break;
    }

    assert(m_cursorLine >= 0);
    assert(m_content.empty() || (size_t)m_cursorLine < m_content.size());
    assert(m_cursorCol >= 0);
    assert(m_content.empty() || m_cursorCharPos < countLineListLen(m_content));

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
    EDITMODE_STATLINE_STR_PWIDTH /* Mode string */\
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
            + " | \033[32m" + std::to_string(m_cursorLine + 1) + "\033[0m"
            + ":\033[33m" + std::to_string(m_cursorCol + 1) + "\033[0m"
            + " | \033[31m" + std::to_string(m_cursorCharPos) + "\033[0m"
            + " | \033[95m" + std::to_string(getCharAt(m_cursorCharPos)) + "\033[0m"
            + "/\033[95m" + intToHexStr((int32_t)getCharAt(m_cursorCharPos)) + "\033[0m"
            ;
        m_statusLineStr.maxLen = std::max((size_t)STATUS_LINE_STR_LEN_MAX, strPLen(m_statusLineStr.str));
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
                strPLen(m_statusLineStr.str));
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
        //assert(charI < countLineListLen(m_content));

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
        assert(lineI >= 0 && lineI < (int)m_content.size());

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
        assert(lineI >= 0 && lineI < (int)m_content.size());
        assert(colI >= 0 && colI < (int)m_content[lineI].length());

        // Newlines can't be selected with block selection
        if (m_content[lineI][colI] == '\n')
            return false;

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
        const size_t delCount = deleteSelectedChars(); // Overwrite selected chars if a selection is active
        for (char c : aClipbText)
        {
            assert((c & 0b10000000) == 0); // Detect non-ASCII chars
            insert(c);
        }
        g_statMsg.set("Pasted text from clipboard ("
                +std::to_string(aClipbText.size())+" chars)"
                +(delCount ? " (overwrote "+std::to_string(delCount)+" chars)" : ""),
                StatusMsg::Type::Info);

    }
    else
    {
        g_statMsg.set("Nothing to paste", StatusMsg::Type::Error);
    }
}

size_t Buffer::copySelectionToClipboard(bool shouldUnselect/*=true*/)
{
    // Note: Mostly copied from `deleteSelectedChars()`

    if (m_selection.mode == Selection::Mode::None)
        return 0;

    //----------------------------------------------------------------------

    struct CharPos_t
    {
        size_t line  = -1_st;
        size_t col   = -1_st;
        size_t index = -1_st;
    };

    String toCopy;
    {
        size_t charI{};
        for (size_t lineI{}; lineI < m_content.size(); ++lineI)
        {
            const auto& line = m_content[lineI];

            for (size_t colI{}; colI < line.length(); ++colI)
            {
                if (isCharSelected(lineI, colI, charI))
                {
                    // We can't select newlines in block selection mode
                    assert(m_selection.mode != Selection::Mode::Block || line[colI] != U'\n');

                    toCopy += m_content[lineI][colI];

                    // If we are in block selection mode and this is at the right border of the selection
                    if (m_selection.mode == Selection::Mode::Block && colI == (size_t)std::max(m_selection.fromCol, m_cursorCol))
                        toCopy += U'\n';
                }

                ++charI;
            }
        }
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

    g_statMsg.set("Copied text to clipboard ("+std::to_string(_toCopy.size())+" chars)",
            StatusMsg::Type::Info);

    scrollViewportToCursor();
    m_isCursorShown = true;
    g_isRedrawNeeded = true;
    return toCopy.size();
}

void Buffer::cutSelectionToClipboard()
{
    const size_t selLen = copySelectionToClipboard(false);
    deleteSelectedChars();

    g_statMsg.set("Moved text to clipboard ("+std::to_string(selLen)+" chars)",
            StatusMsg::Type::Info);
}

static RGBColor calcLinenBgColor(RGBColor col)
{
    if ((col.r + col.g + col.b) / 3.f > .5f) // If light color
    {
        col.r *= 0.8f;
        col.g *= 0.8f;
        col.b *= 0.8f;
    }
    else // Dark color
    {
        col.r *= 1.3f;
        col.g *= 1.3f;
        col.b *= 1.3f;
    }
    return col;
}

void Buffer::_renderDrawCursor(const glm::ivec2& textPos, int initTextY, int width)
{
    m_cursorXPx = textPos.x;
    m_cursorYPx = initTextY+textPos.y-m_scrollY-m_position.y;

    if ((m_cursorMovCmd != CursorMovCmd::None || m_isCursorShown))
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
                    {textPos.x-2, initTextY+textPos.y-m_scrollY-m_position.y-2},
                    {textPos.x+width+2, initTextY+textPos.y-m_scrollY-m_position.y+g_fontSizePx+2},
                    {UNPACK_RGB_COLOR(cursColor), 1.0f},
                    2
            );
            g_uiRenderer->renderFilledRectangle(
                    {textPos.x-2, initTextY+textPos.y-m_scrollY-m_position.y-2},
                    {textPos.x+width+2, initTextY+textPos.y-m_scrollY-m_position.y+g_fontSizePx+2},
                    {UNPACK_RGB_COLOR(cursColor), 0.4f}
            );
        }
        else if (g_editorMode.get() == EditorMode::_EditorMode::Replace)
        {
            g_uiRenderer->renderFilledRectangle(
                    {textPos.x-2,       initTextY+textPos.y-m_scrollY-m_position.y+g_fontSizePx-2},
                    {textPos.x+width+2, initTextY+textPos.y-m_scrollY-m_position.y+g_fontSizePx+2},
                    {UNPACK_RGB_COLOR(cursColor), 1.0f}
            );
        }
        else
        {
            g_uiRenderer->renderFilledRectangle(
                    {textPos.x-1, initTextY+textPos.y-m_scrollY-m_position.y-2},
                    {textPos.x+1, initTextY+textPos.y-m_scrollY-m_position.y+g_fontSizePx+2},
                    {UNPACK_RGB_COLOR(cursColor), 0.4f}
            );
        }

        // Bind the text renderer shader again
        g_textRenderer->prepareForDrawing();
    }
}

void Buffer::_renderDrawSelBg(const glm::ivec2& textPos, int initTextY, int width) const
{
    // Render selection background
    g_uiRenderer->renderFilledRectangle(
            {textPos.x, initTextY+textPos.y-m_scrollY-m_position.y},
            {textPos.x+width, initTextY+textPos.y-m_scrollY-m_position.y+g_fontSizePx},
            RGB_COLOR_TO_RGBA(g_theme->selBg)
    );

    // Bind the text renderer shader again
    g_textRenderer->prepareForDrawing();
}

void Buffer::_renderDrawIndGuid(const glm::ivec2& textPos, int initTextY) const
{
    const RGBColor& defcol = g_theme->values[Syntax::MARK_NONE].color;
    g_uiRenderer->renderFilledRectangle(
            {textPos.x, initTextY+textPos.y-m_scrollY-m_position.y+2},
            {textPos.x+g_fontSizePx/10, initTextY+textPos.y-m_scrollY-m_position.y+2+g_fontSizePx/5},
            {defcol.r, defcol.g, defcol.b, 0.7f}
    );
    g_uiRenderer->renderFilledRectangle(
            {textPos.x, initTextY+textPos.y-m_scrollY-m_position.y+2+g_fontSizePx/5*2},
            {textPos.x+g_fontSizePx/10, initTextY+textPos.y-m_scrollY-m_position.y+2+g_fontSizePx/5*3},
            {defcol.r, defcol.g, defcol.b, 0.7f}
    );
    g_uiRenderer->renderFilledRectangle(
            {textPos.x, initTextY+textPos.y-m_scrollY-m_position.y+2+g_fontSizePx/5*4},
            {textPos.x+g_fontSizePx/10, initTextY+textPos.y-m_scrollY-m_position.y+2+g_fontSizePx/5*5},
            {defcol.r, defcol.g, defcol.b, 0.7f}
    );
}

void Buffer::_renderDrawIndRainbow(const glm::ivec2& textPos, int initTextY, int colI) const
{
#if DRAW_INDENT_RAINBOW
    static constexpr int indentColorCount = 6;
    static constexpr RGBAColor indentColors[indentColorCount]{
        {1.0f, 0.7f, 0.0f, 0.1f},
        {1.0f, 1.0f, 0.0f, 0.1f},
        {0.0f, 0.5f, 0.0f, 0.1f},
        {0.0f, 0.0f, 1.0f, 0.1f},
        {0.3f, 0.0f, 0.5f, 0.1f},
        {0.9f, 0.5f, 0.9f, 0.1f}
    };

    g_uiRenderer->renderFilledRectangle(
            {textPos.x,               initTextY+textPos.y-m_scrollY-m_position.y+2},
            {textPos.x+g_fontWidthPx, initTextY+textPos.y-m_scrollY-m_position.y+2+g_fontSizePx},
            indentColors[colI/TAB_SPACE_COUNT%indentColorCount]
    );
    g_textRenderer->prepareForDrawing();
#else
    (void)textPos;
    (void)initTextY;
    (void)colI;
#endif
}

static RGBColor calcActiveLinenFg(RGBColor col)
{
    if ((col.r + col.g + col.b) / 3.f > .5f) // If light color
    {
        col.r *= 1.3f;
        col.g *= 1.3f;
        col.b *= 1.3f;
    }
    else // Dark color
    {
        col.r *= 0.7f;
        col.g *= 0.7f;
        col.b *= 0.7f;
    }

    return {
        limit(0, 1, col.r),
        limit(0, 1, col.g),
        limit(0, 1, col.b),
    };
}

void Buffer::_renderDrawLineNumBar(const glm::ivec2& textPos, int lineI) const
{
    g_textRenderer->setDrawingColor(
            lineI == m_cursorLine ? calcActiveLinenFg(g_theme->lineNColor) : g_theme->lineNColor);

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
        + (lineI == m_cursorLine ? 0 : (LINEN_BAR_WIDTH_CHAR-lineNumStr.length())*g_fontWidthPx);
#else
    ;
#endif
    for (char digit : lineNumStr)
    {
        auto dimensions = g_textRenderer->renderChar(digit,
                {digitX, textPos.y},
                lineI == m_cursorLine ? FONT_STYLE_BOLD : FONT_STYLE_ITALIC);
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
                    {m_position.x+LINEN_BAR_WIDTH_CHAR*g_fontSizePx-16*(getSignColumn(pair.second)+1), textPos.y},
                    {16, 16});
        }
    }
    g_textRenderer->prepareForDrawing();
}

static RGBColor calcFindResultFgColor(RGBColor col)
{
    return calcCompColor(col);
}

void Buffer::_renderDrawFoundMark(const glm::ivec2& textPos, int initTextY) const
{
    g_uiRenderer->renderFilledRectangle(
            {textPos.x, initTextY+textPos.y-m_scrollY-m_position.y},
            {textPos.x+g_fontWidthPx*m_toFind.size(), initTextY+textPos.y-m_scrollY-m_position.y+g_fontSizePx},
            RGB_COLOR_TO_RGBA(g_theme->findResultBg)
    );

    // Bind the text renderer shader again
    g_textRenderer->prepareForDrawing();
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
            RGB_COLOR_TO_RGBA(g_theme->bgColor)
    );

    // Draw line number background
    g_uiRenderer->renderFilledRectangle(
            m_position,
            {m_position.x+g_fontSizePx*LINEN_BAR_WIDTH_CHAR, m_position.y+m_size.y},
            RGB_COLOR_TO_RGBA(calcLinenBgColor(g_theme->bgColor)));

    const float initTextX = m_position.x+g_fontSizePx*LINEN_BAR_WIDTH_CHAR;
    const float initTextY = m_position.y+m_scrollY;
    float textX = initTextX;
    float textY = initTextY;

    g_textRenderer->prepareForDrawing();
    g_textRenderer->setDrawingColor({1.0f, 1.0f, 1.0f});

    const size_t wordBeg = getCursorWordBeginning();
    const size_t wordEnd = getCursorWordEnd();

            // TODO: Reimplement
#if 0
    // This is used for line wrapping
    const int maxLineLenChar = m_size.x/g_fontWidthPx;
#endif

    m_charUnderMouseCol = -1;
    m_charUnderMouseRow = -1;
    m_charUnderMouseI = -1;

    bool isLineBeginning = true;
    bool isLeadingSpace = true;
    int lineI{};
    size_t charI{};
    // Chars since last search result character
    int _charFoundOffs = INT_MIN;

    for (const String& line : m_content)
    {
        // Don't draw the part of the buffer that is below the viewport
        if (textY > m_position.y+m_size.y)
        {
            break;
        }

        const bool isLineAboveViewport = textY <= -g_fontSizePx;
        if (isLineAboveViewport)
        {
            ++lineI;
            charI += line.size();
            textX = initTextX;
            textY += g_fontSizePx;
            _charFoundOffs += line.size();

            // TODO: Reimplement
#if 0
            if (BUFFER_WRAP_LINES && textX+g_fontWidthPx > m_position.x+m_size.x)
            {
                textX = initTextX;
                textY += g_fontSizePx*line.size()/maxLineLenChar;
            }
#endif
            continue;
        }

        isLineBeginning = true;
        isLeadingSpace = true;
        // Count the number of spaces at the end of line
        int closingSpaceCount = 1;
        while ((size_t)closingSpaceCount < line.size() && u_isspace(line[line.size()-1-closingSpaceCount]))
            ++closingSpaceCount;
        --closingSpaceCount; // Don't count newline

        for (size_t colI{}; colI < line.size(); ++colI)
        {
            const Char c = line[colI];

            //------------------------------------ Info variables ------------------------------------------------

            if (!isspace((uchar)c))
                isLeadingSpace = false;
            const bool isCharSel = isCharSelected(lineI, colI, charI);

            // Handle if this is the beginning of a search result
            if (std::binary_search(m_findResultIs.begin(), m_findResultIs.end(), charI))
                _charFoundOffs = 0;
            // If the current character is inside a search result
            const bool isCharFound = (!m_toFind.empty() && (size_t)_charFoundOffs < m_toFind.size());
            const bool isClosingSpace = colI < line.size()-1 && ((int)colI > (int)line.size()-closingSpaceCount-2);

            //-------------------------------------- Drawing lambdas ---------------------------------------------

            auto drawCursorIfNeeded{[&](int width){
                if (charI == m_cursorCharPos)
                {
#if 1
#    ifndef NDEBUG
                    bool isError = false;
                    if (lineI != m_cursorLine)
                    {
                        Logger::fatal << "BUG: Mismatching cursor line\n";
                        isError = true;
                    }
                    if (colI != (size_t)m_cursorCol)
                    {
                        Logger::fatal << "BUG: Mismatching cursor column\n";
                        isError = true;
                    }

                    if (isError)
                    {
                        Logger::fatal << "Cursor char pos: " << m_cursorCharPos
                            << "\nCursor line: " << m_cursorLine << ", expected: " << lineI
                            << "\nCursor col:  " << m_cursorCol << ", expected: " << colI
                            << Logger::End;
                    }
#    endif
#endif

                    _renderDrawCursor({textX, textY}, initTextY, width);
                }
            }};

            auto drawCharSelectionMarkIfNeeded{[&](int width){
                if (isCharSel)
                    _renderDrawSelBg({textX, textY}, initTextY, width);
            }};

            auto drawFoundMarkIfNeeded{[&](){
                if (isCharFound && _charFoundOffs == 0)
                    _renderDrawFoundMark({textX, textY}, initTextY);
            }};

            auto drawClosingSpaceMarkIfNeeded{[&](){
                // Render closing space chars if they are not in the cursor line
                if (isClosingSpace && lineI != m_cursorLine)
                {
                    g_uiRenderer->renderFilledRectangle(
                            {textX, initTextY+textY-m_scrollY-m_position.y},
                            {textX+g_fontWidthPx, initTextY+textY-m_scrollY-m_position.y+g_fontSizePx},
                            {1.0f, 0.4f, 0.4f, 0.2f}
                    );
                    g_textRenderer->prepareForDrawing();
                }
            }};

            //----------------------------------------------------------------------------------------------------

            if (BUFFER_DRAW_LINE_NUMS && isLineBeginning)
            {
                _renderDrawLineNumBar({textX, textY}, lineI);
            }

            // Render the cursor line highlight
            if (isLineBeginning && m_cursorLine == lineI)
            {
                g_uiRenderer->renderFilledRectangle(
                        {textX, initTextY+textY-m_scrollY-m_position.y+2},
                        {textX+m_size.x,
                            initTextY+textY-m_scrollY-m_position.y+2+g_fontSizePx},
                        g_theme->currLineColor
                );
            }

            // Render indent guides if needed
            if (TAB_SPACE_COUNT > 0 && isLeadingSpace && colI%TAB_SPACE_COUNT==0 && colI != 0)
                _renderDrawIndGuid({textX, textY}, initTextY);

            // Underline current word
            if (charI > wordBeg && charI < wordEnd)
            {
                g_uiRenderer->renderFilledRectangle(
                        {textX, initTextY+textY-m_scrollY-m_position.y+g_fontSizePx+2-g_fontSizePx*0.1f},
                        {textX+g_fontWidthPx, initTextY+textY-m_scrollY-m_position.y+g_fontSizePx+2},
                        {0.4f, 0.4f, 0.4f, 1.0f}
                );
            }

            // Bind the text renderer shader again
            g_textRenderer->prepareForDrawing();

            // If the mouse is in the current character, take a note
            //
            // If we haven't found a char under the mouse
            if (m_charUnderMouseI == -1
            // If the line is OK
             && g_cursorY >= textY && g_cursorY < textY+g_fontSizePx
            // If the column is OK, or the mouse is past the line
             && ((g_cursorX >= textX && g_cursorX < textX+g_fontWidthPx) || c == U'\n'))
            {
                m_charUnderMouseRow = lineI;
                m_charUnderMouseCol = colI;
                m_charUnderMouseI = charI;
            }

            if (c == '\t') // Tab
            {
                drawCharSelectionMarkIfNeeded(g_fontWidthPx*4);
                drawFoundMarkIfNeeded();
                drawClosingSpaceMarkIfNeeded();
                drawCursorIfNeeded(g_fontWidthPx*4);
                // Draw a horizontal line to mark the tab character
                g_uiRenderer->renderFilledRectangle(
                        {textX+g_fontSizePx*0.3f,
                         initTextY+textY-m_scrollY-m_position.y+g_fontSizePx/2.0f+2},
                        {textX+g_fontSizePx*0.3f+(g_fontSizePx*0.7f)*4-g_fontSizePx*0.6f,
                         initTextY+textY-m_scrollY-m_position.y+g_fontSizePx/2.0f+4},
                        {0.8f, 0.8f, 0.8f, 0.2f}
                );
                g_textRenderer->prepareForDrawing();
                ++charI;
                isLineBeginning = false;
                textX += g_fontWidthPx*4;
                ++_charFoundOffs;
                continue;
            }

            // TODO: Reimplement
#if 0
            if (BUFFER_WRAP_LINES && textX+g_fontSizePx > m_position.x+m_size.x)
            {
                textX = initTextX;
                textY += g_fontSizePx;
            }
#endif

            drawCharSelectionMarkIfNeeded(g_fontWidthPx);
            drawFoundMarkIfNeeded();
            drawClosingSpaceMarkIfNeeded();

            // Render non-whitespace character
            if (!u_isspace(c))
            {
#if 0
                const size_t contentLen = countLineListLen(m_content);
                if (m_highlightBuffer.size() < contentLen)
                {
                    Logger::fatal << "BUG: Invalid highlight buffer length. Expected: " << contentLen
                        << ", got: " << m_highlightBuffer.length() << Logger::End;
                }
#endif
                RGBColor charColor = g_theme->values[0].color;
                FontStyle charStyle = Syntax::defStyles[0];
                // If the character is selected, draw with selection FG color
                if (isCharSel)
                {
                    charColor = g_theme->selFg;
                    charStyle = 0;
                }
                // If the character is part of the search result, draw with a different color
                else if (isCharFound)
                {
                    charColor = calcFindResultFgColor(g_theme->findResultBg);
                    charStyle = FONT_STYLE_BOLD|FONT_STYLE_ITALIC;
                }
                else if (m_highlightBuffer[charI] < Syntax::_SYNTAX_MARK_COUNT)
                {
                    charColor = g_theme->values[m_highlightBuffer[charI]].color;
                    charStyle = Syntax::defStyles[m_highlightBuffer[charI]];
                }
                else // Error: Something is wrong with the highlight buffer
                {
                    assert(false && "Invalid highlight buffer value");
                }
                g_textRenderer->setDrawingColor(charColor);
                g_textRenderer->renderChar(c, {textX, textY}, charStyle);
            }
#if BUF_NL_DISP_CHAR_CODE // Render newline char if configured so
            else if (c == '\n')
            {
                g_textRenderer->setDrawingColor(g_theme->values[Syntax::MARK_COMMENT].color);
                g_textRenderer->renderChar(BUF_NL_DISP_CHAR_CODE, {textX, textY}, 0);
            }
#endif


            // Render indent rainbow
            if (!isClosingSpace && TAB_SPACE_COUNT > 0 && isLeadingSpace)
            {
                _renderDrawIndRainbow({textX, textY}, initTextY, colI);
            }

            drawCursorIfNeeded(g_fontWidthPx);

            ++charI;
            textX += g_fontWidthPx;
            ++_charFoundOffs;
            isLineBeginning = false;
        }

        ++lineI;
        textX = initTextX;
        textY += g_fontSizePx;
        ++_charFoundOffs;
    }

    if (m_isDimmed)
    {
        g_uiRenderer->renderFilledRectangle(
                m_position,
                {m_position.x+m_size.x, m_position.y+m_size.y},
                {UNPACK_RGB_COLOR(g_theme->bgColor), 0.5f}
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

    const int oldCol = m_cursorCol;
    const int oldLine = m_cursorLine;


    BufferHistory::Entry histEntry;
    histEntry.oldCursPos = m_cursorCharPos,
    histEntry.oldCursLine = m_cursorLine,
    histEntry.oldCursCol = m_cursorCol;
    histEntry.lines.emplace_back();
    histEntry.lines[0].lineI = m_cursorLine;

    if (character == '\n')
    {
        histEntry.lines[0].lineI = oldLine;
        histEntry.lines[0].from = m_content[oldLine];
        ++m_cursorLine;
        const String newLineVal = m_content[oldLine].substr(m_cursorCol); // Save chars right to cursor
        m_content[oldLine] = m_content[oldLine].substr(0, m_cursorCol) + U'\n'; // Remove chars right to cursor
        histEntry.lines[0].to = m_content[oldLine];

        m_content.insert(m_content.begin()+m_cursorLine, newLineVal); // Create a new line
        m_cursorCol = 0;
        histEntry.lines.emplace_back();
        histEntry.lines[1].lineI = m_cursorLine;
        histEntry.lines[1].from = U"";
        histEntry.lines[1].to = U"\n";

    }
    else
    {
        histEntry.lines[0].lineI = m_cursorLine;
        histEntry.lines[0].from = m_content[m_cursorLine];
        m_content[m_cursorLine].insert(m_content[m_cursorLine].begin()+m_cursorCol, character);
        histEntry.lines[0].to = m_content[m_cursorLine];
        ++m_cursorCol;
    }
    ++m_cursorCharPos;
    m_highlightBuffer.insert(m_cursorCharPos, 1, 0);

    if (m_autocompPopup->isRendered())
    {
        m_autocompPopup->appendToFilter(character);
    }
    else
    {
        m_autocompPopup->setVisibility(false);
    }

    // If this is the end of a word
    if (u_isspace(character))
    {
        // Find the word
        int wordStart;
        for (wordStart=oldCol-1; wordStart >= 0 && !u_isspace(m_content[oldLine][wordStart]); wordStart--)
            ;
        const String word = m_content[oldLine].substr(wordStart+1, oldCol-wordStart-1);

        // Feed the buffer word provider the word
        if (!word.empty())
        {
            //Logger::dbg << "Feeding word to buffer word provider: " << quoteStr(strToAscii(word)) << Logger::End;
            m_buffWordProvid->add(word);
        }
    }

    histEntry.newCursPos = m_cursorCharPos,
    histEntry.newCursLine = m_cursorLine,
    histEntry.newCursCol = m_cursorCol;
    m_history.add(std::move(histEntry));

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
    if (character == '\n' || m_content[m_cursorLine][m_cursorCol] == '\n')
    {
        TIMER_END_FUNC();
        return;
    }

    BufferHistory::Entry histEntry;
    histEntry.oldCursPos = m_cursorCharPos,
    histEntry.oldCursLine = m_cursorLine,
    histEntry.oldCursCol = m_cursorCol;
    histEntry.lines.emplace_back();
    histEntry.lines.back().from = m_content[m_cursorLine];
    histEntry.lines.back().lineI = m_cursorLine;

    m_content[m_cursorLine][m_cursorCol] = character;

    ++m_cursorCol;
    ++m_cursorCharPos;
    m_isModified = true;

    histEntry.lines.back().to = m_content[m_cursorLine];
    histEntry.newCursPos = m_cursorCharPos,
    histEntry.newCursLine = m_cursorLine,
    histEntry.newCursCol = m_cursorCol;
    m_history.add(std::move(histEntry));

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

    BufferHistory::Entry histEntry;
    histEntry.oldCursPos = m_cursorCharPos,
    histEntry.oldCursLine = m_cursorLine,
    histEntry.oldCursCol = m_cursorCol;
    histEntry.lines.emplace_back();

    // If deleting at the beginning of the line and we have stuff to delete
    if (m_cursorCol == 0 && m_cursorLine != 0)
    {
        histEntry.lines.emplace_back();
        histEntry.lines[0].lineI = m_cursorLine-1;
        histEntry.lines[0].from = m_content[m_cursorLine-1];
        histEntry.lines[1].lineI = m_cursorLine;
        histEntry.lines[1].from = m_content[m_cursorLine];

        m_cursorCol = m_content[m_cursorLine-1].length()-1;
        // Delete newline from end of previous line
        m_content[m_cursorLine-1].erase(m_content[m_cursorLine-1].length()-1, 1);
        // Save current line
        const String currLineVal = m_content[m_cursorLine];
        // Delete current line
        m_content.erase(m_content.begin()+m_cursorLine);
        // Move current line to the end of the prev. one
        m_content[m_cursorLine-1].append(currLineVal);
        histEntry.lines[0].to = m_content[m_cursorLine-1];
        histEntry.lines[1].to = U"";
        m_highlightBuffer.erase(m_cursorCharPos-1, 1);
        --m_cursorLine;
        --m_cursorCharPos;
        m_isModified = true;
    }
    // If deleting in the middle/end of the line and we have stuff to delete
    else if (m_cursorCharPos > 0)
    {
        histEntry.lines[0].lineI = m_cursorLine;
        histEntry.lines[0].from = m_content[m_cursorLine];
        m_content[m_cursorLine].erase(m_cursorCol-1, 1);
        m_highlightBuffer.erase(m_cursorCharPos-1, 1);
        --m_cursorCol;
        --m_cursorCharPos;
        m_isModified = true;
        histEntry.lines[0].to = m_content[m_cursorLine];
    }

    assert(m_cursorCol >= 0);
    assert(m_cursorLine >= 0);

    histEntry.newCursPos = m_cursorCharPos,
    histEntry.newCursLine = m_cursorLine,
    histEntry.newCursCol = m_cursorCol;
    m_history.add(std::move(histEntry));

    m_isCursorShown = true;
    m_isHighlightUpdateNeeded = true;
    scrollViewportToCursor();

    if (m_autocompPopup->isRendered())
        m_autocompPopup->removeLastCharFromFilter();

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

    BufferHistory::Entry histEntry;
    histEntry.oldCursPos = m_cursorCharPos,
    histEntry.oldCursLine = m_cursorLine,
    histEntry.oldCursCol = m_cursorCol;
    histEntry.lines.emplace_back();

    const int lineLen = m_content[m_cursorLine].length();
    // If deleting at the end of the line and we have stuff to delete
    if (m_cursorCol == lineLen-1 && (size_t)m_cursorLine < m_content.size()-1)
    {
        histEntry.lines.emplace_back();
        histEntry.lines[0].lineI = m_cursorLine;
        histEntry.lines[0].from = m_content[m_cursorLine];
        histEntry.lines[1].lineI = m_cursorLine+1;
        histEntry.lines[1].from = m_content[m_cursorLine+1];
        // Save next line
        const String nextLine = m_content[m_cursorLine+1];
        // Remove next line
        m_content.erase(m_content.begin()+m_cursorLine+1);
        // Remove newline from current line
        m_content[m_cursorLine].erase(m_content[m_cursorLine].length()-1, 1);
        // Append the next line to the current one
        m_content[m_cursorLine].append(nextLine);
        histEntry.lines[0].to = m_content[m_cursorLine];
        histEntry.lines[1].to = U"";
        m_highlightBuffer.erase(m_cursorCharPos, 1);
        m_isModified = true;
    }
    // If deleting in the middle/beginning of the line and we have stuff to delete
    else if (m_cursorCol < lineLen-1 && (size_t)m_cursorLine < m_content.size())
    {
        histEntry.lines[0].lineI = m_cursorLine;
        histEntry.lines[0].from = m_content[m_cursorLine];
        m_content[m_cursorLine].erase(m_cursorCol, 1);
        m_highlightBuffer.erase(m_cursorCharPos, 1);
        histEntry.lines[0].to = m_content[m_cursorLine];
        m_isModified = true;
    }

    assert(m_cursorCol >= 0);
    assert(m_cursorLine >= 0);

    histEntry.newCursPos = m_cursorCharPos,
    histEntry.newCursLine = m_cursorLine,
    histEntry.newCursCol = m_cursorCol;
    m_history.add(std::move(histEntry));

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
        //Logger::dbg << "Undoing a history entry with " << entry.lines.size() << " lines" << Logger::End;
        for (size_t i=entry.lines.size()-1; i != -1_st; --i)
        {
            const auto& lineEntry = entry.lines[i];

            // If undoing the creation of a new line
            if (lineEntry.from.empty())
            {
                m_content.erase(m_content.begin()+lineEntry.lineI);
            }
            // If undoing the deletion of a line
            else if (lineEntry.to.empty())
            {
                m_content.insert(m_content.begin()+lineEntry.lineI, lineEntry.from);
            }
            // Just a regular changed line
            else
            {
                m_content[lineEntry.lineI] = lineEntry.from;
            }
        }
        m_cursorCol     = entry.oldCursCol;
        m_cursorLine    = entry.oldCursLine;
        m_cursorCharPos = entry.oldCursPos;

        // TODO: Adjust `m_highlightBuffer`

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
        for (size_t i{}; i < entry.lines.size(); ++i)
        {
            const auto& lineEntry = entry.lines[i];

            // If redoing the creation of a new line
            if (lineEntry.from.empty())
            {
                m_content.insert(m_content.begin()+lineEntry.lineI, lineEntry.to);
            }
            // If redoing the deletion of a line
            else if (lineEntry.to.empty())
            {
                m_content.erase(m_content.begin()+lineEntry.lineI);
            }
            // Just a regular changed line
            else
            {
                m_content[lineEntry.lineI] = lineEntry.to;
            }
        }
        m_cursorCol     = entry.newCursCol;
        m_cursorLine    = entry.newCursLine;
        m_cursorCharPos = entry.newCursPos;

        // TODO: Adjust `m_highlightBuffer`

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

// Guesses the currently typed path based on the POSIX portable file name character set
static std::string getPathFromLine(const String& line)
{
    if (line.empty())
        return "";

    std::string output;
    for (size_t i=line.length()-1; i != -1_st; --i)
    {
        const char c = line[i];
        if (!std::isalnum(c) && c != '.' && c != '_' && c != '-' && c != '/')
            break;
        output = c + output;
    }
    return output;
}

void Buffer::triggerAutocompPopup()
{
    m_pathProvid->setPrevix(getPathFromLine(m_content[m_cursorLine].substr(0, m_cursorCol)));
    m_autocompPopup->setVisibility(true);
    m_isCursorShown = true;
    g_isRedrawNeeded = true;
}

void Buffer::autocompPopupSelectNextItem()
{
    m_autocompPopup->selectNextItem();
    m_isCursorShown = true;
    g_isRedrawNeeded = true;
}

void Buffer::autocompPopupSelectPrevItem()
{
    m_autocompPopup->selectPrevItem();
    m_isCursorShown = true;
    g_isRedrawNeeded = true;
}

void Buffer::autocompPopupHide()
{
    m_autocompPopup->setVisibility(false);
    m_isCursorShown = true;
    g_isRedrawNeeded = true;
}

void Buffer::autocompPopupInsert()
{
    if (m_autocompPopup->isRendered())
    {
        // Hide the popup and insert the current item
        const String toInsert = m_autocompPopup->getSelectedItem()->value
            .substr(m_autocompPopup->getFilterLen());
        m_content[m_cursorLine].insert(m_cursorCol, toInsert);
        m_cursorCharPos += toInsert.length();
        m_cursorCol += toInsert.length();
        m_isHighlightUpdateNeeded = true;
    }
    m_autocompPopup->setVisibility(false);
    m_isCursorShown = true;
    g_isRedrawNeeded = true;
}

void Buffer::regenAutocompList()
{
    Logger::dbg << "Regenerating autocomplete list for buffer " << this << Logger::End;
    m_autocompPopup->clear();
    Autocomp::dictProvider->get(m_autocompPopup.get());
    m_buffWordProvid->get(m_autocompPopup.get());
    m_pathProvid->get(m_autocompPopup.get());
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

size_t Buffer::deleteSelectedChars()
{
    if (m_isReadOnly)
        return 0;

    if (m_selection.mode == Selection::Mode::None)
        return 0;

    BufferHistory::Entry histEntry;

    int delCount{};

    switch (m_selection.mode)
    {
    case Selection::Mode::None:
        break;

    case Selection::Mode::Normal:
    {
        histEntry.oldCursPos = m_cursorCharPos;
        histEntry.oldCursLine = m_cursorLine;
        histEntry.oldCursCol = m_cursorCol;

        const int fromLine = std::min(m_selection.fromLine, m_cursorLine);
        const int toLine = std::max(m_selection.fromLine, m_cursorLine);
        const size_t fromChar = std::min(m_selection.fromCharI, m_cursorCharPos);
        const size_t toChar = std::max(m_selection.fromCharI, m_cursorCharPos);
        // See: `colI` loop
        const int bottLineCol = (toChar == m_selection.fromCharI ? m_selection.fromCol : m_cursorCol);
        const int topLineCol = (fromChar == m_selection.fromCharI ? m_selection.fromCol : m_cursorCol);
        size_t charI = toChar;
        size_t entryI{};
        for (int lineI=toLine; lineI >= fromLine; --lineI)
        {
            histEntry.lines.emplace_back();
            histEntry.lines.back().lineI = lineI;
            histEntry.lines.back().from = m_content[lineI];

            const size_t lineLen = m_content[lineI].length();
            // Note: We start looking through the first (last by index) line in the selection not
            // from the last character but from the selection end column
            for (int colI=(lineI == toLine ? bottLineCol : lineLen-1); colI >= (lineI == fromLine ? topLineCol : 0); --colI)
            {
                if (charI >= fromChar && charI <= toChar)
                {
                    m_content[lineI].erase(colI, 1);
                    ++delCount;
                }

                charI--;
            }

            bool deletedWholeLine = false;
            // Remove line if empty
            if (m_content[lineI].empty())
            {
                m_content.erase(m_content.begin()+lineI);
                deletedWholeLine = true;
            }

            if (deletedWholeLine)
                histEntry.lines[entryI].to = U"";
            else
                histEntry.lines[entryI].to = m_content[lineI];

            // FIXME: Redo history gets messed up
            // FIXME: Somehow there are lines without line break in the history

            // If the newline was deleted from the line
            if (m_content[lineI].back() != U'\n')
            {
                if (lineI+1 < (int)m_content.size())
                {
                    // Append the next line to the end of this line, delete the next line
                    // and mark in the history that it was deleted.
                    // TODO: Maybe do the history saving as a second run?
                    histEntry.lines.emplace_back();
                    ++entryI;
                    histEntry.lines[entryI].lineI = lineI+1;
                    histEntry.lines[entryI].from = m_content[lineI+1];
                    m_content[lineI].append(m_content[lineI+1]);
                    m_content.erase(m_content.begin()+lineI+1);
                    histEntry.lines[entryI].to = U"";
                }
                else
                {
                    m_content[lineI].push_back(U'\n');
                }
            }

            ++entryI;
        }
        break;
    }

    case Selection::Mode::Line:
    {
        histEntry.oldCursPos = m_cursorCharPos-m_cursorCol; // Calculate line beginning
        histEntry.oldCursLine = m_cursorLine;
        histEntry.oldCursCol = 0;

        const int fromLine = std::min(m_selection.fromLine, m_cursorLine);
        const int toLine = std::max(m_selection.fromLine, m_cursorLine);
        for (int lineI=toLine; lineI >= fromLine; --lineI)
        {
            delCount += m_content[lineI].length();
            histEntry.lines.emplace_back();
            histEntry.lines.back().lineI = lineI;
            histEntry.lines.back().from = m_content[lineI];
            m_content.erase(m_content.begin()+lineI);
            histEntry.lines.back().to = U"";
        }
        break;
    }

    case Selection::Mode::Block:
    {
        histEntry.oldCursPos = m_cursorCharPos;
        histEntry.oldCursLine = m_cursorLine;
        histEntry.oldCursCol = m_cursorCol;

        const int fromLine = std::min(m_selection.fromLine, m_cursorLine);
        const int toLine = std::max(m_selection.fromLine, m_cursorLine);
        for (int lineI=toLine; lineI >= fromLine; --lineI)
        {
            histEntry.lines.emplace_back();
            histEntry.lines.back().lineI = lineI;
            histEntry.lines.back().from = m_content[lineI];

            const int lineLen = m_content[lineI].length();
            for (int colI=lineLen-1; colI >= 0; --colI)
            {
                if (isCharSelected(lineI, colI, -1))
                {
                    assert(m_content[lineI][colI] != U'\n'); // We can't select newlines
                    m_content[lineI].erase(colI, 1);
                    ++delCount;
                }
            }

            assert(!m_content[lineI].empty());
            histEntry.lines.back().to = m_content[lineI];
        }

        break;
    }
    }

    // Jump the cursor to the right place
    if (m_cursorCharPos > m_selection.fromCharI)
    {
        m_cursorCol = m_selection.fromCol;
        m_cursorLine = m_selection.fromLine;
        m_cursorCharPos = m_selection.fromCharI;
    }
    { // Limit too large cursor column
        assert(!m_content[m_cursorLine].empty());
        const int cursMaxCol = m_content[m_cursorLine].length()-1;
        if (m_cursorCol > cursMaxCol)
        {
            m_cursorCharPos -= m_cursorCol;
            m_cursorCharPos += cursMaxCol;
            m_cursorCol = cursMaxCol;
        }
    }

    histEntry.newCursPos = m_cursorCharPos,
    histEntry.newCursLine = m_cursorLine,
    histEntry.newCursCol = m_cursorCol;
    m_history.add(std::move(histEntry));

    m_selection.mode = Selection::Mode::None; // Cancel the selection
    m_isModified = true;
    m_isCursorShown = true;
    scrollViewportToCursor();
    m_isHighlightUpdateNeeded = true;
    g_statMsg.set("Deleted "+std::to_string(delCount)+" characters", StatusMsg::Type::Info);
    g_isRedrawNeeded = true;

    return delCount;
}

void Buffer::_goToCurrFindResult(bool showStatMsg)
{
    const size_t goTo = m_findResultIs[m_findCurrResultI];
    {
        size_t i{};
        for (size_t lineI{}; lineI < m_content.size(); ++lineI)
        {
            const size_t lineLen = m_content[lineI].length();
            if (i + lineLen - 1 < goTo)
            {
                i += lineLen;
            }
            else
            {
                m_cursorCol = goTo-i;
                m_cursorLine = lineI;
                m_cursorCharPos = goTo;
                break;
            }
        }
    }
    if (showStatMsg)
    {
        g_statMsg.set("Jumped to search result "+std::to_string(m_findCurrResultI+1)+
                " out of "+std::to_string(m_findResultIs.size()),
                StatusMsg::Type::Info);
    }
    centerCursor();
    m_isCursorShown = true;
    g_isRedrawNeeded = true;
}

void Buffer::findGoToNextResult()
{
    if (m_toFind.empty() || m_findResultIs.empty())
        return;

    ++m_findCurrResultI;
    if ((size_t)m_findCurrResultI >= m_findResultIs.size())
    {
        m_findCurrResultI = 0;
        g_statMsg.set("Starting search from beginning", StatusMsg::Type::Info);
        _goToCurrFindResult(false);
    }
    else
    {
        _goToCurrFindResult(true);
    }
}

void Buffer::findGoToPrevResult()
{
    if (m_toFind.empty() || m_findResultIs.empty())
        return;

    --m_findCurrResultI;
    if (m_findCurrResultI < 0)
    {
        m_findCurrResultI = m_findResultIs.size()-1;
        g_statMsg.set("Starting search from end", StatusMsg::Type::Info);
        _goToCurrFindResult(false);
    }
    else
    {
        _goToCurrFindResult(true);
    }
}

void Buffer::find(const String& str)
{
    m_toFind = str;
    m_findCurrResultI = 0;
    findUpdate();

    if (m_toFind.empty())
        return;

    if (m_findResultIs.empty())
    {
        g_statMsg.set("Not found: "+quoteStr(strToAscii(str)), StatusMsg::Type::Error);
        return;
    }

    g_statMsg.set(
            "Found "+std::to_string(m_findResultIs.size())+" occurences of "+quoteStr(strToAscii(m_toFind)),
            StatusMsg::Type::Info);

    _goToCurrFindResult(false);
}

void Buffer::findUpdate()
{
    m_findResultIs.clear();

    if (m_toFind.empty())
        return;
    {
        size_t index{};
        for (const auto& line : m_content)
        {
            int col = -1;
            while (true)
            {
                col = line.find(m_toFind, col+1);
                if ((size_t)col == line.npos)
                    break;
                m_findResultIs.push_back(index + col);
            }

            index += line.length();
        }
    }

    Logger::log << "Found " << m_findResultIs.size() << " matches" << Logger::End;
}

void Buffer::findClear()
{
    m_toFind.clear();
    m_findResultIs.clear();
}

void Buffer::indentSelectedLines()
{
    if (m_selection.mode == Selection::Mode::None)
        return;

    const size_t startLine = std::min(m_cursorLine, m_selection.fromLine);
    const size_t endLine = std::max(m_cursorLine, m_selection.fromLine);

    for (size_t i{startLine}; i <= endLine; ++i)
    {
        m_content[i].insert(0, (TAB_SPACE_COUNT == 0 ? 1 : TAB_SPACE_COUNT), (TAB_SPACE_COUNT == 0 ? U'\t' : ' '));
    }

    // Place the cursor to the upper (less) position
    if (m_selection.fromCharI < m_cursorCharPos)
    {
        m_cursorCharPos = m_selection.fromCharI;
        m_cursorLine = m_selection.fromLine;
        m_cursorCol = m_selection.fromCol;
    }

    m_selection.mode = Selection::Mode::None;
    // TODO: Insert to highlight buffer instead of reparsing
    m_isHighlightUpdateNeeded = true;
    g_isRedrawNeeded = true;
}

static size_t countTrailingSpaces(const String& str)
{
    size_t count{};
    while (count < str.length() && str[count] == U' ')
        ++count;
    return count;
}

void Buffer::unindentSelectedLines()
{
    if (m_selection.mode == Selection::Mode::None)
        return;

    const size_t startLine = std::min(m_cursorLine, m_selection.fromLine);
    const size_t endLine = std::max(m_cursorLine, m_selection.fromLine);

    for (size_t i{startLine}; i <= endLine; ++i)
    {
#if TAB_SPACE_COUNT == 0
        if (m_content[i][0] == U'\t')
        {
            m_content[i].erase(0, 1);
        }
#else
        const int spaceCount = countTrailingSpaces(m_content[i]);
        m_content[i].erase(0, std::min(spaceCount, TAB_SPACE_COUNT));
#endif
    }

    // Place the cursor to the upper (less) position
    if (m_selection.fromCharI < m_cursorCharPos)
    {
        m_cursorCharPos = m_selection.fromCharI;
        m_cursorLine = m_selection.fromLine;
        m_cursorCol = m_selection.fromCol;
    }

    m_selection.mode = Selection::Mode::None;
    // TODO: Remove from highlight buffer instead of reparsing
    m_isHighlightUpdateNeeded = true;
    g_isRedrawNeeded = true;
}

void Buffer::goToMousePos()
{
    Logger::dbg << "Jumping to "
        "line: " << m_charUnderMouseRow << ", "
        "col: " << m_charUnderMouseCol << Logger::End;
    if (m_charUnderMouseI != -1) // If not past the buffer
    {
        m_cursorLine = m_charUnderMouseRow;
        m_cursorCol = m_charUnderMouseCol;
        m_cursorCharPos = m_charUnderMouseI;
    }
    else // If past the buffer
    {
        // Go to last character
        m_cursorLine = m_content.size()-1;
        m_cursorCol = m_content[m_cursorLine].length()-1;
        m_cursorCharPos = countLineListLen(m_content)-1;
    }
    m_isCursorShown = true;
    g_isRedrawNeeded = true;
}

void _autoReloadDialogCb(int btn, Dialog*, void* userData)
{
    Buffer* buffer = (Buffer*)userData;

    if (btn == 0) // If pressed "Yes"
    {
        Logger::dbg << "User answered \"Yes\". Reloading" << Logger::End;
        buffer->open(buffer->m_filePath, true);
    }
    else // If pressed "No"
    {
        Logger::dbg << "User answered \"No\". Not reloading" << Logger::End;
        buffer->m_lastFileUpdateTime = getFileModTime(buffer->m_filePath);
    }

    buffer->m_isReloadAskerDialogOpen = false;
}

void Buffer::tickAutoReload(float frameTimeMs)
{
    // Don't auto reload if it is disabled
    if constexpr (AUTO_RELOAD_MODE == AUTO_RELOAD_MODE_DONT)
        return;

    m_msUntilAutoReloadCheck -= frameTimeMs;

    if (m_msUntilAutoReloadCheck <= 0)
    {
        Logger::dbg << "Checking file change" << Logger::End;

        if (getFileModTime(m_filePath) == m_lastFileUpdateTime)
        {
            Logger::dbg << "No change" << Logger::End;
        }
        else
        {
            if constexpr (AUTO_RELOAD_MODE == AUTO_RELOAD_MODE_ASK) // Ask to reload
            {
                if (m_isReloadAskerDialogOpen)
                {
                    Logger::log << "Change detected, already asked user" << Logger::End;
                }
                else
                {
                    Logger::log << "Change detected! Asking to reload." << Logger::End;
                    m_isReloadAskerDialogOpen = true;
                    MessageDialog::create(_autoReloadDialogCb, this,
                            "The file "+quoteStr(m_filePath)+"\nhas been changed from outside."
                                "\nDo you want to reload it?",
                            MessageDialog::Type::Information,
                            {{"Yes", GLFW_KEY_Y}, {"No", GLFW_KEY_N}});
                }
            }
            else // Reload without asking
            {
                (void)_autoReloadDialogCb; // Let's "use" this bad boy

                Logger::log << "Change detected! Reloading." << Logger::End;
                open(m_filePath, true);
            }
        }

        m_msUntilAutoReloadCheck = AUTO_RELOAD_CHECK_FREQ_MS;
    }
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
