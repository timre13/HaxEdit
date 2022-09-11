#include "Buffer.h"
#include "Document.h"
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
#include "App.h"
#include <fstream>
#include <sstream>
#include <chrono>
#include <filesystem>
#include <cctype>
using namespace std::chrono_literals;

bufid_t Buffer::s_lastUsedId = 0;

Buffer::Buffer()
{
    m_id = s_lastUsedId++;
    m_document = std::make_unique<Document>();

    /*
     * This thread runs in the background while the buffer is alive.
     * It constantly checks if an update is needed and if it does,
     * it sets `m_isHighlightUpdateNeeded` to false and runs the updater function.
     * The updater function constantly checks if the `m_isHighlightUpdateNeeded` has been
     * set to true because it means that a new update has been requested and the current
     * update is outdated, so the function exits and the loop executes it again.
     */
    m_highlighterThread = std::thread([&](){
#ifndef TESTING
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
#endif
    });

    Logger::dbg << "Setting up autocomplete for buffer: " << this << Logger::End;
    m_autocompPopup = std::make_unique<Autocomp::Popup>();

    Logger::log << "Created a buffer: " << this << Logger::End;
}

static inline Buffer::fileModTime_t getFileModTime(const std::string& path)
{
    return std::filesystem::last_write_time(std::filesystem::path{path}).time_since_epoch().count();
}

void Buffer::open(const std::string& filePath, bool isReload/*=false*/)
{
    TIMER_BEGIN_FUNC();

#ifndef TESTING
    glfwSetCursor(g_window, Cursors::busy);
#endif

    m_filePath = filePath;
    m_language = Langs::LangId::Unknown;
    m_isReadOnly = false;
    m_document->clearHistory();
    m_gitRepo.reset();
    m_signs.clear();
    m_lastFileUpdateTime = 0;
    m_version = 0;
    if (isReload)
        Logger::dbg << "Reloading file: " << filePath << Logger::End;
    else
        Logger::dbg << "Opening file: " << filePath << Logger::End;

    try
    {
        m_filePath = std::filesystem::canonical(filePath);
        m_document->setContent(loadUnicodeFile(filePath));
        m_highlightBuffer = std::u8string(m_document->calcCharCount(), Syntax::MARK_NONE);
        m_isHighlightUpdateNeeded = true;
        m_gitRepo = std::make_unique<Git::Repo>(filePath);
        m_lastFileUpdateTime = getFileModTime(m_filePath);
        updateGitDiff();
        m_language = Langs::lookupExtension(std_fs::path(m_filePath).extension().string().substr(1));
        Logger::dbg << "Buffer language: " << Langs::langIdToName(m_language)
            << " (LSP id: " << Langs::langIdToLspId(m_language) << ')' << Logger::End;

        Logger::dbg << "Read "
            << m_document->calcCharCount() << " characters ("
            << m_document->getLineCount() << " lines) from file" << Logger::End;
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
        m_document->clearContent();
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

#ifndef TESTING
        glfwSetCursor(g_window, nullptr);
#endif
        TIMER_END_FUNC();
        return;
    }

    if (isReadOnlyFile(filePath))
    {
        Logger::warn << "File is read only" << Logger::End;
        m_isReadOnly = true;
    }

    { // Regenerate the initial autocomplete list for `buffWordProvid`
        Autocomp::buffWordProvid->clear();

        for (const auto& line : m_document->getAll())
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
                        Autocomp::buffWordProvid->add(m_id, word);
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

    // TODO: This is bad
    const std::string content = utf32To8(m_document->getConcated());
    // TODO: Properly tell reloading to LSP server (close first)
    Autocomp::lspProvider->onFileOpen(m_filePath, m_language, content);

#ifndef TESTING
    glfwSetCursor(g_window, nullptr);
#endif
    TIMER_END_FUNC();
}

int Buffer::saveToFile()
{
    TIMER_BEGIN_FUNC();

    Logger::dbg << "Writing buffer to file: " << m_filePath << Logger::End;
    Autocomp::lspProvider->beforeFileSave(m_filePath, Autocomp::LspProvider::saveReason_t::Manual);

    size_t contentLen{};
    try
    {
        // TODO: Is there a way to write the content line-by-line to the file?
        //       That would be faster.
        const String content = m_document->getConcated();
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
        << m_document->getLineCount() << " lines)" << Logger::End;
    g_statMsg.set("Wrote buffer to file: \""+m_filePath+"\"", StatusMsg::Type::Info);

    if (Autocomp::lspProvider->onFileSaveNeedsContent())
        Autocomp::lspProvider->onFileSave(m_filePath, utf32To8(m_document->getConcated()));
    else
        Autocomp::lspProvider->onFileSave(m_filePath, "");

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

    const auto buffer = m_document->getConcated();

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

std::string Buffer::getCheckedOutObjName(int hashLen/*=-1*/) const
{
    if (m_gitBranchName.isHash && hashLen >= 4 && hashLen <= 40)
    {
        // Hashes can be truncated
        return m_gitBranchName.name.substr(0, hashLen);
    }
    else
    {
        // Names (e.g. branches) shouldn't be truncated
        return m_gitBranchName.name;
    }
}

void Buffer::updateCursor()
{
    TIMER_BEGIN_FUNC();

    m_document->assertPos(m_cursorLine, m_cursorCol, m_cursorCharPos);

    auto getCursorLineLen{[&](){
        return m_document->getLineLen(m_cursorLine);
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
        if ((size_t)m_cursorLine < m_document->getLineCount()-1)
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
        m_cursorLine = m_document->isEmpty() ? 0 : m_document->getLineCount()-1;
        m_cursorCol = getCursorLineLen()-1;
        m_cursorCharPos = m_document->calcCharCount()-1;
        break;

    case CursorMovCmd::SWordEnd:
    {
        const int contentSize = m_document->calcCharCount();

        // Skip spaces
        while ((int)m_cursorCharPos < contentSize)
        {
            int newCCol = m_cursorCol + 1;
            int newCLine = m_cursorLine;
            size_t newCPos = m_cursorCharPos + 1;
            if ((size_t)newCCol == m_document->getLineLen(newCLine)) // If at the end of line
            {
                ++newCLine;
                newCCol = 0;
                if ((size_t)newCLine >= m_document->getLineCount()) // End of bufer?
                    break;
            }

            if (!u_isspace(m_document->getChar({newCLine, newCCol})))
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
            if ((size_t)newCCol == m_document->getLineLen(newCLine)) // If at the end of line
            {
                ++newCLine;
                newCCol = 0;
                if ((size_t)newCLine >= m_document->getLineCount()) // End of bufer?
                    break;
            }

            if (u_isspace(m_document->getChar({newCLine, newCCol})))
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
                newCCol = m_document->getLineLen(newCLine)-1; // Go to line end
            }

            if (!u_isspace(m_document->getChar({newCLine, newCCol})))
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
                newCCol = m_document->getLineLen(newCLine)-1; // Go to line end
            }

            if (u_isspace(m_document->getChar({newCLine, newCCol})))
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

        const int contentSize = m_document->calcCharCount();

        // If inside spaces
        if (u_isspace(m_document->getChar({m_cursorLine, m_cursorCol})))
        {
            // Skip spaces
            while ((int)m_cursorCharPos < contentSize)
            {
                if (!u_isspace(m_document->getChar({m_cursorLine, m_cursorCol})))
                    break;

                int newCCol = m_cursorCol + 1;
                int newCLine = m_cursorLine;
                size_t newCPos = m_cursorCharPos + 1;
                if ((size_t)newCCol == m_document->getLineLen(newCLine)) // If at the end of line
                {
                    ++newCLine;
                    newCCol = 0;
                    if ((size_t)newCLine >= m_document->getLineCount()) // End of bufer?
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
                if (u_isspace(m_document->getChar({m_cursorLine, m_cursorCol})))
                    break;

                int newCCol = m_cursorCol + 1;
                int newCLine = m_cursorLine;
                size_t newCPos = m_cursorCharPos + 1;
                if ((size_t)newCCol == m_document->getLineLen(newCLine)) // If at the end of line
                {
                    ++newCLine;
                    newCCol = 0;
                    if ((size_t)newCLine >= m_document->getLineCount()) // End of bufer?
                        break;
                }

                m_cursorCharPos = newCPos;
                m_cursorLine = newCLine;
                m_cursorCol = newCCol;
            }

            // Skip spaces
            while ((int)m_cursorCharPos < contentSize)
            {
                if (!u_isspace(m_document->getChar({m_cursorLine, m_cursorCol})))
                    break;

                int newCCol = m_cursorCol + 1;
                int newCLine = m_cursorLine;
                size_t newCPos = m_cursorCharPos + 1;
                if ((size_t)newCCol == m_document->getLineLen(newCLine)) // If at the end of line
                {
                    ++newCLine;
                    newCCol = 0;
                    if ((size_t)newCLine >= m_document->getLineCount()) // End of bufer?
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
        for (size_t i{}; i < pageSize && (size_t)m_cursorLine < m_document->getLineCount()-1; ++i)
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

    m_document->assertPos(m_cursorLine, m_cursorCol, m_cursorCharPos);

    // Scroll up when the cursor goes out of the viewport
    if (m_cursorMovCmd != CursorMovCmd::None)
    {
        scrollViewportToCursor();
        m_isCursorShown = true;
        m_cursorMovCmd = CursorMovCmd::None;
        m_cursorHoldTime = 0;
        g_hoverPopup->hideAndClear();
    }

    TIMER_END_FUNC();
}

int Buffer::getNumOfLines() const
{
    return m_document->getLineCount();
}

Char Buffer::getCharAt(size_t pos) const
{
    if (m_document->isEmpty())
        return '\0';

    size_t i{};
    for (const String& line : *m_document)
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

String Buffer::getCursorWord() const
{
    if (isspace(m_document->getChar({m_cursorLine, m_cursorCol})))
        return U"";

    int beginCol = m_cursorCol;
    while (beginCol >= 0 && !u_isspace(m_document->getChar({m_cursorLine, beginCol})))
        --beginCol;
    ++beginCol;

    int endCol = m_cursorCol;
    while (endCol < (int)m_document->getLineLen(m_cursorLine)
        && !u_isspace(m_document->getChar({m_cursorLine, endCol})))
        ++endCol;

    return m_document->getLine(m_cursorLine).substr(beginCol, endCol-beginCol);
}

void Buffer::moveCursorToLineCol(int line, int col)
{
    m_cursorLine = line;
    if (m_cursorLine >= (int)m_document->getLineCount())
        m_cursorLine = (int)m_document->getLineCount()-1;

    m_cursorCol = col;
    if (m_cursorCol >= (int)m_document->getLineLen(m_cursorLine))
        m_cursorCol = m_document->getLineLen(m_cursorLine)-1;

    // Calculate `m_cursorCharPos`
    m_cursorCharPos = 0;
    {
        // Add up the whole line lengths
        for (size_t lineI{}; (int)lineI < m_cursorLine; ++lineI)
            m_cursorCharPos += m_document->getLineLen(lineI);

        // Add the remaining
        m_cursorCharPos += m_cursorCol;
    }

    m_isCursorShown = true;
    m_cursorHoldTime = 0;
#ifndef TESTING
    g_hoverPopup->hideAndClear();
#endif
}

void Buffer::moveCursorToLineCol(const lsPosition& pos)
{
    moveCursorToLineCol(pos.line, pos.character);
}

void Buffer::moveCursorToChar(int pos)
{
    size_t i{};
    for (size_t lineI{}; lineI < m_document->getLineCount(); ++lineI)
    {
        const size_t lineLen = m_document->getLineLen(lineI);
        if (int(i + lineLen - 1) < pos)
        {
            i += lineLen;
        }
        else
        {
            m_cursorCol = pos-i;
            m_cursorLine = lineI;
            m_cursorCharPos = pos;
            break;
        }
    }

    m_isCursorShown = true;
    m_cursorHoldTime = 0;
    g_hoverPopup->hideAndClear();
}

void Buffer::scrollBy(int val)
{
    TIMER_BEGIN_FUNC();

    const int origScroll = m_scrollY;

    m_scrollY += val;
    // Don't scroll above the first line
    if (m_scrollY > 0)
    {
        m_scrollY = 0;
    }
    // Always show the last line when scrolling down
    // FIXME: Line wrapping makes the document longer, so this breaks
    //        NOTE: Line wrapping is broken, so this issue is not valid
    else if (m_scrollY < -(int)(m_document->getLineCount()-1)*g_fontSizePx)
    {
        m_scrollY = -(int)(m_document->getLineCount()-1)*g_fontSizePx;
    }

    // Make the hover popup follow the original origin
    g_hoverPopup->moveYBy(m_scrollY-origScroll);

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
        assert(lineI >= 0 && lineI < (int)m_document->getLineCount());

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
        assert(lineI >= 0 && lineI < (int)m_document->getLineCount());
        assert(colI >= 0 && colI < (int)m_document->getLineLen(lineI));

        // Newlines can't be selected with block selection
        if (m_document->getChar({lineI, colI}) == '\n')
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
    const String clipbText = utf8To32(Clipboard::get());
    if (!clipbText.empty())
    {
        Logger::dbg << "Pasting: " << quoteStr(clipbText) << Logger::End;
        const size_t delCount = deleteSelectedChars(); // Overwrite selected chars if a selection is active
        beginHistoryEntry();
        // Do the insertion and move the cursor to the insertion end
        // TODO: Maybe a parameter to prevent moving the cursor?
        moveCursorToLineCol(applyInsertion({m_cursorLine, m_cursorCol}, clipbText));
        endHistoryEntry();
        scrollViewportToCursor();
        g_statMsg.set("Pasted text from clipboard ("
                +std::to_string(clipbText.size())+" chars)"
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
        for (size_t lineI{}; lineI < m_document->getLineCount(); ++lineI)
        {
            const auto& line = m_document->getLine(lineI);

            for (size_t colI{}; colI < line.length(); ++colI)
            {
                if (isCharSelected(lineI, colI, charI))
                {
                    // We can't select newlines in block selection mode
                    assert(m_selection.mode != Selection::Mode::Block || line[colI] != U'\n');

                    toCopy += m_document->getChar({(int)lineI, (int)colI});

                    // If we are in block selection mode and this is at the right border of the selection
                    if (m_selection.mode == Selection::Mode::Block && colI == (size_t)std::max(m_selection.fromCol, m_cursorCol))
                        toCopy += U'\n';
                }

                ++charI;
            }
        }
    }

    assert(!toCopy.empty());

    Logger::dbg << "Copying: " << quoteStr(toCopy) << Logger::End;
    Clipboard::set(utf32To8(toCopy));

    if (shouldUnselect)
        m_selection.mode = Selection::Mode::None; // Cancel the selection

    g_statMsg.set("Copied text to clipboard ("+std::to_string(toCopy.size())+" chars)",
            StatusMsg::Type::Info);

    scrollViewportToCursor();
    m_isCursorShown = true;
    m_cursorHoldTime = 0;
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

namespace
{

constexpr RGBColor diagnSeverityColors[4] = {
    {1.0f, 0.2f, 0.1f}, // Error
    {0.9f, 0.6f, 0.2f}, // Warning
    {0.5f, 0.5f, 0.8f}, // Information
    {0.3f, 0.3f, 0.3f}, // Hint
};

}

static int getDiagnSevIndex(const lsDiagnostic& diagn)
{
    // Note: We default to Information, maybe this needs to be changed later
    const lsDiagnosticSeverity sev = diagn.severity.get_value_or(lsDiagnosticSeverity::Information);
    const int sevIndex = (int)sev-1;
    return sevIndex;
}


void Buffer::_renderDrawDiagnosticMsg(
        const Autocomp::LspProvider::diagList_t& diags, int lineI, bool isCurrent,
        const glm::ivec2& textPos, int initTextY
        ) const
{
    static constexpr Char severityMarks[4] = {
        'E', // Error
        'W', // Warning
        'I', // Information
        'H', // Hint
    };

    for (const auto& diagn : diags)
    {
        if ((int)diagn.range.start.line == lineI)
        {
            const int sevIndex = getDiagnSevIndex(diagn);
            const RGBColor textColor = diagnSeverityColors[sevIndex];

            // If the diagnostic is in the current line, show the whole message.
            // Otherwise only show the first line.
            std::string msgToDisplay;
            if (isCurrent)
            {
                msgToDisplay = diagn.message;
            }
            else
            {
                LineIterator msgLineIt{diagn.message};
                (void)msgLineIt.next(msgToDisplay);
            }

            auto rendOrMeasure{[&](bool render){
                return g_textRenderer->renderString(
                    charToStr(severityMarks[sevIndex])+U": "+utf8To32(msgToDisplay),
                    {textPos.x+g_fontWidthPx*4, initTextY+textPos.y-m_scrollY-m_position.y},
                    FONT_STYLE_ITALIC|(isCurrent ? FONT_STYLE_BOLD : 0),
                    textColor,
                    true,
                    !render);
            }};

            // Calculate and fill the background of the diagnostic message
            const auto diagnArea = rendOrMeasure(false);
            g_uiRenderer->renderFilledRectangle(
                    diagnArea.first,
                    {diagnArea.second.x, diagnArea.second.y+g_fontSizePx*0.2f},
                    {UNPACK_RGB_COLOR(textColor), 0.1f});

            // Render the message
            rendOrMeasure(true);
            break;
        }
    }

    // Bind the text renderer shader again
    g_textRenderer->prepareForDrawing();
}

static bool isPosInRange(const lsRange& range, const lsPosition& pos)
{
    return !(pos < range.start) && pos < range.end;
}

void Buffer::_renderDrawDiagnosticUnderline(
        const Autocomp::LspProvider::diagList_t& diags, int lineI, int colI,
        const glm::ivec2& textPos, int initTextY
        ) const
{
    static lsDiagnostic foundDiagn{};

    // If `foundDiagn` is outdated
    if (!isPosInRange(foundDiagn.range, {lineI, colI}))
    {
        // Find a new containing diagnostic
        auto found = std::find_if(diags.begin(), diags.end(), [&](const lsDiagnostic& diagn){
                return isPosInRange(diagn.range, {lineI, colI});
        });
        // If there is no containing diagnostic, exit
        if (found == diags.end())
            return;
        foundDiagn = *found;
    }

    const glm::ivec2 pos1 = {textPos.x, initTextY+textPos.y-m_scrollY-m_position.y};
    auto drawSegment = [&](int index){
        static constexpr int segmentW = 1;
        static constexpr int segmentH = 2;
        static constexpr float maxOffs = 2;

        // Draw a wavy line
        const int yoffs1 = g_fontSizePx+std::round(sin(index/g_fontWidthPx*M_PI*2)*maxOffs);
        g_uiRenderer->renderFilledRectangle(
                pos1+glm::ivec2{segmentW*index,     yoffs1},
                pos1+glm::ivec2{segmentW*(index+1), yoffs1+segmentH},
                RGB_COLOR_TO_RGBA(diagnSeverityColors[getDiagnSevIndex(foundDiagn)]));
    };
    for (int i{}; i < g_fontWidthPx; ++i)
        drawSegment(i);
    g_textRenderer->prepareForDrawing();
}

void Buffer::_renderDrawCursorLineCodeAct(int yPos, int initTextY)
{
    if (m_lineCodeAction.forLine == (uint)m_cursorLine && !m_lineCodeAction.actions.empty())
    {
        static auto codeActImg = std::make_unique<Image>(App::getResPath("../img/lightbulb.png"));
        codeActImg->render(
                {m_position.x+g_fontSizePx*(LINEN_BAR_WIDTH_CHAR-1), initTextY+yPos-m_scrollY-m_position.y},
                {g_fontSizePx, g_fontSizePx}
        );

        // Bind the text renderer shader again
        g_textRenderer->prepareForDrawing();
    }
}

void Buffer::_renderDrawBreadcrumbBar()
{
    if (m_breadcBarVal.empty())
        return;

    g_uiRenderer->renderFilledRectangle(m_position, {m_position.x+m_size.x, m_position.y+g_fontSizePx+6},
            RGB_COLOR_TO_RGBA(calcStatLineColor(g_theme->bgColor)));
    g_uiRenderer->renderRectangleOutline(m_position, {m_position.x+m_size.x, m_position.y+g_fontSizePx+6},
            {0.5f, 0.5f, 0.5f, 1.0f}, 1);

    g_textRenderer->renderString(utf8To32(m_breadcBarVal), m_position+glm::ivec2{4, 2});
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

    auto fileDiagsIt = Autocomp::LspProvider::s_diags.find(m_filePath);

    m_charUnderMouseCol = -1;
    m_charUnderMouseRow = -1;
    m_charUnderMouseI = -1;

    bool isLineBeginning = true;
    bool isLeadingSpace = true;
    int lineI{};
    size_t charI{};
    // Chars since last search result character
    int _charFoundOffs = INT_MIN;

    for (const String& line : m_document->getAll())
    {
#ifndef NDEBUG
        if (!line.ends_with('\n'))
        {
            Logger::err << "Line " << lineI << " does not end in a line break" << Logger::End;
            for(;;);
        }
#endif

#if !defined(NDEBUG) && 1
        // NOTE: SLOW
        for (size_t i{}; i < line.length()-1; ++i)
        {
            if (line[i] == '\n')
            {
                Logger::err << "Line " << lineI << " has multiple line breaks"
                    " (found at col " << i << ")" << Logger::End;
                for(;;);
            }
        }
#endif


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

#if 0
                    if (isError)
                    {
                        Logger::fatal << "Cursor char pos: " << m_cursorCharPos
                            << "\nCursor line: " << m_cursorLine << ", expected: " << lineI
                            << "\nCursor col:  " << m_cursorCol << ", expected: " << colI
                            << Logger::End;
                    }
#else
                    (void)isError;
                    m_cursorLine = lineI;
                    m_cursorCol = colI;
#endif

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

            if (fileDiagsIt != Autocomp::LspProvider::s_diags.end())
                _renderDrawDiagnosticUnderline(
                        fileDiagsIt->second, lineI, colI,
                        {textX, textY}, initTextY);

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

        if (fileDiagsIt != Autocomp::LspProvider::s_diags.end())
            _renderDrawDiagnosticMsg(fileDiagsIt->second, lineI, lineI == m_cursorLine, {textX, textY}, initTextY);
        if (m_cursorLine == lineI)
            _renderDrawCursorLineCodeAct(textY, initTextY);

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

    _renderDrawBreadcrumbBar();

    TIMER_END_FUNC();
}

void Buffer::insertCharAtCursor(Char character)
{
    const int oldCol = m_cursorCol;
    const int oldLine = m_cursorLine;

    beginHistoryEntry();
    applyInsertion({m_cursorLine, m_cursorCol}, charToStr(character));
    if (character == '\n')
    {
        ++m_cursorLine;
        m_cursorCol = 0;
    }
    else
    {
        ++m_cursorCol;
    }
    ++m_cursorCharPos;

    if (m_autocompPopup->isRendered())
    {
        m_autocompPopup->appendToFilter(character);
    }
    else
    {
        m_autocompPopup->setVisibility(false);
    }
    endHistoryEntry();

    // If this is the end of a word
    if (u_isspace(character))
    {
        // Find the word
        int wordStart;
        for (wordStart=oldCol-1; wordStart >= 0 && !u_isspace(m_document->getChar({oldLine, wordStart})); wordStart--)
            ;
        const String word = m_document->getLine(oldLine).substr(wordStart+1, oldCol-wordStart-1);

        // Feed the buffer word provider the word
        if (!word.empty())
        {
            //Logger::dbg << "Feeding word to buffer word provider: " << quoteStr(strToAscii(word)) << Logger::End;
            Autocomp::buffWordProvid->add(m_id, word);
        }
    }

    scrollViewportToCursor();
}

void Buffer::replaceCharAtCursor(Char character)
{
    // Disallow replacing a newline or replacing a char by a newline
    if (character == '\n' || m_document->getChar({m_cursorLine, m_cursorCol}) == '\n')
        return;

    beginHistoryEntry();
    applyDeletion({{m_cursorLine, m_cursorCol}, {m_cursorLine, m_cursorCol+1}});
    applyInsertion({m_cursorLine, m_cursorCol}, charToStr(character));
    ++m_cursorCol;
    ++m_cursorCharPos;
    endHistoryEntry();

    scrollViewportToCursor();
}

void Buffer::deleteCharBackwards()
{
    beginHistoryEntry();
    // If deleting at the beginning of the line and we have stuff to delete
    if (m_cursorCol == 0 && m_cursorCharPos > 0)
    {
        applyDeletion({
                {m_cursorLine-1, (int)m_document->getLineLen(m_cursorLine-1)-1},
                {m_cursorLine, 0}});
        --m_cursorLine;
        m_cursorCol = m_document->getLineLen(m_cursorLine);
        --m_cursorCharPos;
    }
    // If deleting in the middle/end of the line and we have stuff to delete
    else if (m_cursorCharPos > 0)
    {
        applyDeletion({{m_cursorLine, m_cursorCol-1}, {m_cursorLine, m_cursorCol}});
        --m_cursorCol;
        --m_cursorCharPos;
    }
    endHistoryEntry();
    scrollViewportToCursor();

    if (m_autocompPopup->isRendered())
        m_autocompPopup->removeLastCharFromFilter();
}

void Buffer::deleteCharForwardOrSelected()
{
    if (m_selection.mode != Selection::Mode::None)
    {
        deleteSelectedChars();
        return;
    }

    beginHistoryEntry();
    applyDeletion({{m_cursorLine, m_cursorCol}, {m_cursorLine, m_cursorCol+1}});
    endHistoryEntry();
}

static inline time_t getElapsedSince(time_t timePoint)
{
    return std::time(nullptr) - timePoint;
}

void Buffer::undo()
{
    if (m_document->getHistory().canGoBack())
    {
        const auto undoInfo = m_document->undo();
        // Restore cursor pos
        m_cursorLine    = undoInfo.oldCursPos.line;
        m_cursorCol     = undoInfo.oldCursPos.col;
        m_cursorCharPos = undoInfo.oldCursPos.index;

        // TODO: Adjust `m_highlightBuffer`

        m_isHighlightUpdateNeeded = true;
        m_version++;
        Autocomp::lspProvider->onFileChange(m_filePath, m_version, utf32To8(m_document->getConcated()));
        scrollViewportToCursor();
        g_statMsg.set("Undid change ("+std::to_string(getElapsedSince(undoInfo.timestamp))+" seconds old)",
                StatusMsg::Type::Info);
        g_isRedrawNeeded = true;
        Logger::dbg << "Went back in history" << Logger::End;
    }
    else
    {
        g_statMsg.set("Already at oldest version", StatusMsg::Type::Info);
        Logger::dbg << "Cannot go back in history" << Logger::End;
    }
}

void Buffer::redo()
{
    if (m_document->getHistory().canGoForward())
    {
        const auto redoInfo = m_document->redo();
        // Restore cursor pos
        m_cursorLine    = redoInfo.newCursPos.line;
        m_cursorCol     = redoInfo.newCursPos.col;
        m_cursorCharPos = redoInfo.newCursPos.index;

        // TODO: Adjust `m_highlightBuffer`

        m_isHighlightUpdateNeeded = true;
        m_version++;
        Autocomp::lspProvider->onFileChange(m_filePath, m_version, utf32To8(m_document->getConcated()));
        g_statMsg.set("Redid change ("+std::to_string(getElapsedSince(redoInfo.timestamp))+" seconds old)",
                StatusMsg::Type::Info);
        scrollViewportToCursor();
        g_isRedrawNeeded = true;
        Logger::dbg << "Went forward in history" << Logger::End;
    }
    else
    {
        g_statMsg.set("Already at newest version", StatusMsg::Type::Info);
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
    regenAutocompList();
    Autocomp::pathProvid->setPrefix(m_id, getPathFromLine(
                m_document->getLine(m_cursorLine).substr(0, m_cursorCol)));
    m_autocompPopup->setVisibility(true);
    m_isCursorShown = true;
    m_cursorHoldTime = 0;
    g_isRedrawNeeded = true;
}

void Buffer::autocompPopupSelectNextItem()
{
    m_autocompPopup->selectNextItem();
    m_isCursorShown = true;
    m_cursorHoldTime = 0;
    g_isRedrawNeeded = true;
}

void Buffer::autocompPopupSelectPrevItem()
{
    m_autocompPopup->selectPrevItem();
    m_isCursorShown = true;
    m_cursorHoldTime = 0;
    g_isRedrawNeeded = true;
}

void Buffer::autocompPopupHide()
{
    m_autocompPopup->setVisibility(false);
    m_isCursorShown = true;
    m_cursorHoldTime = 0;
    g_isRedrawNeeded = true;
}

void Buffer::autocompPopupInsert()
{
    // Hide the popup and insert the current item

    beginHistoryEntry();
    if (m_autocompPopup->isRendered())
    {
        const auto* item = m_autocompPopup->getSelectedItem();
        // Use `textEdit` if available
        if (item->textEdit)
        {
            // TODO: Jump to end
            applyEdit(item->textEdit.get());
        }
        else
        {
            // If there is `insertText`, use it. Otherwise use `label`.
            const std::string toInsert = item->insertText.get_value_or(item->label);
            //    .substr(m_autocompPopup->getFilterLen());
            const lsPosition moveTo = applyInsertion({m_cursorLine, m_cursorCol}, utf8To32(toInsert));
            moveCursorToLineCol(moveTo);
        }

        if (item->additionalTextEdits)
        {
            applyEdits(item->additionalTextEdits.get());
        }
    }
    endHistoryEntry();
    m_autocompPopup->setVisibility(false);
}

void Buffer::regenAutocompList()
{
    Logger::dbg << "Regenerating autocomplete list for buffer " << this << Logger::End;
    m_autocompPopup->clear();
#if 0
    Autocomp::dictProvider->get(m_id, m_autocompPopup.get());
    Autocomp::buffWordProvid->get(m_id, m_autocompPopup.get());
    Autocomp::pathProvid->get(m_id, m_autocompPopup.get());
#endif
    Autocomp::lspProvider->get(m_id, m_autocompPopup.get());
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
    g_editorMode.set(EditorMode::_EditorMode::Normal);
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

    beginHistoryEntry();

    int delCount{};

    switch (m_selection.mode)
    {
    case Selection::Mode::None:
        break;

    case Selection::Mode::Normal:
    {
        lsRange range;
        if (m_selection.fromCharI < m_cursorCharPos)
        {
            range.start.line = m_selection.fromLine;
            range.start.character = m_selection.fromCol;
            range.end.line = m_cursorLine;
            range.end.character = m_cursorCol+1;
        }
        else
        {
            range.start.line = m_cursorLine;
            range.start.character = m_cursorCol;
            range.end.line = m_selection.fromLine;
            range.end.character = m_selection.fromCol+1;
        }
        delCount = applyDeletion(range);
        break;
    }

    case Selection::Mode::Line:
    {
        const int fromLine = std::min(m_selection.fromLine, m_cursorLine);
        const int toLine = std::max(m_selection.fromLine, m_cursorLine);

        lsRange range;
        range.start.line = fromLine;
        range.start.character = 0;
        range.end.line = toLine+1;
        range.end.character = 0;
        delCount = applyDeletion(range);
        break;
    }

    case Selection::Mode::Block:
    {
        const int fromLine = std::min(m_selection.fromLine, m_cursorLine);
        const int toLine = std::max(m_selection.fromLine, m_cursorLine);
        for (int lineI=toLine; lineI >= fromLine; --lineI)
        {
            const int lineLen = m_document->getLineLen(lineI);
            lsRange range;
            range.start.line = lineI;
            range.start.character = std::min(m_selection.fromCol, m_cursorCol);
            if ((int)range.start.character > lineLen-1)
                range.start.character = lineLen-1;
            range.end.line = lineI;
            range.end.character = std::max(m_selection.fromCol, m_cursorCol)+1;
            if ((int)range.end.character > lineLen-1)
                range.end.character = lineLen-1;
            delCount += applyDeletion(range);
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
        assert(m_document->getLineLen(m_cursorLine) != 0);
        const int cursMaxCol = m_document->getLineLen(m_cursorLine)-1;
        if (m_cursorCol > cursMaxCol)
        {
            m_cursorCharPos -= m_cursorCol;
            m_cursorCharPos += cursMaxCol;
            m_cursorCol = cursMaxCol;
        }
    }
    endHistoryEntry();

    m_selection.mode = Selection::Mode::None; // Cancel the selection
    g_statMsg.set("Deleted "+std::to_string(delCount)+" characters", StatusMsg::Type::Info);
    scrollViewportToCursor();

    return delCount;
}

void Buffer::_goToCurrFindResult(bool showStatMsg)
{
    const size_t goTo = m_findResultIs[m_findCurrResultI];
    moveCursorToChar(goTo);
    if (showStatMsg)
    {
        g_statMsg.set("Jumped to search result "+std::to_string(m_findCurrResultI+1)+
                " out of "+std::to_string(m_findResultIs.size()),
                StatusMsg::Type::Info);
    }
    centerCursor();
    m_isCursorShown = true;
    m_cursorHoldTime = 0;
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
        g_statMsg.set("Not found: "+quoteStr(utf32To8(str)), StatusMsg::Type::Error);
        return;
    }

    g_statMsg.set(
            "Found "+std::to_string(m_findResultIs.size())+" occurences of "+quoteStr(utf32To8(m_toFind)),
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
        for (const auto& line : *m_document)
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

    beginHistoryEntry();
    const String toInsert = ((TAB_SPACE_COUNT == 0) ? U"\t" : String(TAB_SPACE_COUNT, ' '));
    for (size_t i{startLine}; i <= endLine; ++i)
    {
        applyInsertion({(int)i, 0}, toInsert);
    }

    // Place the cursor to the upper (less) position
    if (m_selection.fromCharI < m_cursorCharPos)
    {
        m_cursorCharPos = m_selection.fromCharI;
        m_cursorLine = m_selection.fromLine;
        m_cursorCol = m_selection.fromCol;
    }

    endHistoryEntry();
    m_selection.mode = Selection::Mode::None;
    scrollViewportToCursor();
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

    const int startLine = std::min(m_cursorLine, m_selection.fromLine);
    const int endLine = std::max(m_cursorLine, m_selection.fromLine);

    beginHistoryEntry();
    for (int i{startLine}; i <= endLine; ++i)
    {
#if TAB_SPACE_COUNT == 0
        if (m_document->getChar({i, 0}) == U'\t')
            applyDeletion({{i, 0}, {i, 1}});
        (void)countTrailingSpaces;
#else
        const int spaceCount = countTrailingSpaces(m_document->getLine(i));
        applyDeletion({{i, 0}, {i, std::min(spaceCount, TAB_SPACE_COUNT)}});
#endif
    }

    // Place the cursor to the upper (less) position
    if (m_selection.fromCharI < m_cursorCharPos)
    {
        m_cursorCharPos = m_selection.fromCharI;
        m_cursorLine = m_selection.fromLine;
        m_cursorCol = m_selection.fromCol;
    }

    endHistoryEntry();
    m_selection.mode = Selection::Mode::None;
    scrollViewportToCursor();
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
        m_cursorLine = m_document->getLineCount()-1;
        m_cursorCol = m_document->getLineLen(m_cursorLine)-1;
        m_cursorCharPos = m_document->calcCharCount()-1;
    }
    m_isCursorShown = true;
    m_cursorHoldTime = 0;
    g_hoverPopup->hideAndClear();
    g_isRedrawNeeded = true;
}

void Buffer::tickCursorHold(float frameTimeMs)
{
    auto isTriggered{[this, frameTimeMs](int triggerAfter){
        return m_cursorHoldTime < triggerAfter
            && m_cursorHoldTime+frameTimeMs >= triggerAfter;
    }};

    if (isTriggered(CURSOR_HOLD_TIME_CODE_ACTION))
    {
        /*
         * Here we get the code actions for the line.
         * If the user applies one of them, we send a workspace/executeCommand request with the selected command.
         * The server replies with a workspace/applyEdit request. We apply the edits and send back a response.
         */
        auto codeAct = Autocomp::lspProvider->getCodeActionForLine(m_filePath, m_cursorLine);
        m_lineCodeAction.forLine = m_cursorLine;
        m_lineCodeAction.actions = std::move(codeAct);
        // Draw the code action mark
        g_isRedrawNeeded = true;
    }
    if (isTriggered(CURSOR_HOLD_TIME_LOCATION_UPD))
    {
        // TODO: Don't get symbols every time, only if the file was modified.
        //       And then we probably won't even need delay (or only a small amount)
        const auto symbols = Autocomp::lspProvider->getDocSymbols(m_filePath);

        std::function<std::string(const decltype(symbols)&, bool)> _genLocPath
            = [this, &_genLocPath](const decltype(symbols)& syms, bool firstCall){ // -> std::string
            std::string out;
            for (const auto& sym : syms)
            {
                if (isPosInRange(sym.range, {m_cursorLine, m_cursorCol}))
                {
                    if (!firstCall)
                        out += " \033[90m>\033[0m ";
                    out += sym.name;
                    if (sym.children.has_value())
                    {
                        out += _genLocPath(sym.children.get(), false);
                    }
                    break;
                }
            }
            return out;
        };
        auto genLocPath = [&_genLocPath](const decltype(symbols)& symbols){ // -> std::string
            return _genLocPath(symbols, true);
        };

        // Generate the string for the breadcrumb bar.
        // TODO: Use something more sophisticated.
        m_breadcBarVal = genLocPath(symbols);;
        Logger::log << "Location path: " << m_breadcBarVal << Logger::End;

        g_isRedrawNeeded = true;
    }

    m_cursorHoldTime += frameTimeMs;
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

void Buffer::tickGitBranchUpdate(float frameTimeMs)
{
    m_msUntilGitBranchUpdate -= frameTimeMs;
    if (m_msUntilGitBranchUpdate <= 0)
    {
        Logger::dbg << "Updating checked out Git object" << Logger::End;
        m_gitBranchName = m_gitRepo->getCheckedOutObjName();
        Logger::log << "Current checked out Git object: " << m_gitBranchName.name;
        // If this is a branch, show only the branch name
        if (m_gitBranchName.name.starts_with("refs/heads/"))
        {
            m_gitBranchName.name = m_gitBranchName.name.substr(11);
            Logger::log << " aka '" << m_gitBranchName.name << '\'' << Logger::End;
        }
        Logger::log << Logger::End;

        m_msUntilGitBranchUpdate = GIT_BRANCH_CHECK_FREQ_MS;
    }
}

void Buffer::showSymbolHover(bool atMouse/*=false*/)
{
    Autocomp::LspProvider::HoverInfo hoverInfo;
    if (atMouse)
    {
        // Return if there is nothing under the cursor
        if (m_charUnderMouseI == -1)
            return;

        hoverInfo = Autocomp::lspProvider->getHover(
                m_filePath, m_charUnderMouseRow, m_charUnderMouseCol);
    }
    else
    {
        hoverInfo = Autocomp::lspProvider->getHover(
                m_filePath, m_cursorLine, m_cursorCol);
    }

    if (!hoverInfo.text.empty())
    {
        if (hoverInfo.gotRange)
        {
            // TODO: Support multi-line symbol names
            assert(hoverInfo.startLine == hoverInfo.endLine);
            assert(hoverInfo.endCol > hoverInfo.startCol);

            assert(hoverInfo.startLine >= 0 && hoverInfo.startLine < (int)m_document->getLineCount());
            assert(hoverInfo.startCol >= 0 && hoverInfo.startCol < (int)m_document->getLineLen(hoverInfo.startLine));
            assert(hoverInfo.endCol >= 0 && hoverInfo.endCol < (int)m_document->getLineLen(hoverInfo.startLine));
            const String hoveredSymbol = m_document->getLine(hoverInfo.startLine).substr(
                    hoverInfo.startCol, hoverInfo.endCol-hoverInfo.startCol);
            g_hoverPopup->setTitle(hoveredSymbol);
        }
        else
        {
            g_hoverPopup->setTitle(U"Symbol hover");
        }
        g_hoverPopup->setContent(hoverInfo.text);
        if (atMouse)
            g_hoverPopup->setPos({g_cursorX, g_cursorY-g_hoverPopup->calcHeight()});
        else
            g_hoverPopup->setPos({m_cursorXPx+g_fontWidthPx, m_cursorYPx-g_hoverPopup->calcHeight()});
        g_hoverPopup->show();
        g_isRedrawNeeded = true;
    }
    else if (!atMouse)
    {
        g_hoverPopup->hideAndClear();
        g_isRedrawNeeded = true;
    }
}

void Buffer::showSignatureHelp()
{
    auto signHelp = Autocomp::lspProvider->getSignatureHelp(m_filePath, m_cursorLine, m_cursorCol);
    if (signHelp.signatures.empty())
        return;

    // TODO: Show signature documentation
    // TODO: Show parameter documentation
    g_hoverPopup->setTitle(U"");

    size_t actSign = signHelp.activeSignature.get_value_or(0);
    if (actSign >= signHelp.signatures.size()) // Default to 0 when out of range
        actSign = 0;

    size_t actParamI = signHelp.activeParameter.get_value_or(0);

    std::string content;
    for (size_t signI{}; signI < signHelp.signatures.size(); ++signI)
    {
        const auto& sign = signHelp.signatures[signI];
        // Draw the current signature with white, draw all the others with gray
        if (signI == actSign)
        {
            std::string signLabel = sign.label;
            if (!sign.parameters.empty())
            {
                // Highlight the active parameter

                const lsParameterInformation& actParam = sign.parameters[
                    (actParamI < sign.parameters.size()) ? actParamI : 0]; // Default to 0 when out of range

                // The parameters are substrings of the signature
                // Find the bounds
                const size_t paramStart = signLabel.find(actParam.label);
                assert(paramStart != std::string::npos);
                assert(paramStart + actParam.label.length() <= signLabel.length());

                signLabel.insert(paramStart+actParam.label.length(), "\033[0m\033[97m");
                signLabel.insert(paramStart, "\033[1m\033[93m");

            }
            content += "\033[97m"+signLabel+"\033[0m";
        }
        else
        {
            content += "\033[90m"+sign.label+"\033[0m";
        }
        if (signI != signHelp.signatures.size()-1)
            content += '\n';
    }
    g_hoverPopup->setContent(utf8To32(content));
    g_hoverPopup->setPos({m_cursorXPx+g_fontWidthPx, m_cursorYPx-g_hoverPopup->calcHeight()});
    g_hoverPopup->show();
}

void Buffer::_goToDeclOrDefOrImp(const Autocomp::LspProvider::Location& loc)
{
    // If it is in the current file, scroll there
    if (std::filesystem::canonical(loc.path) == std::filesystem::canonical(m_filePath))
    {
        moveCursorToLineCol(loc.line, loc.col);
        centerCursor();
        m_isCursorShown = true;
        m_cursorHoldTime = 0;
        g_hoverPopup->hideAndClear();
    }
    else // Different file, open in a new tab
    {
        Buffer* buff = App::openFileInNewBuffer(loc.path);

        buff->moveCursorToLineCol(loc.line, loc.col);
        buff->centerCursor();
        buff->m_isCursorShown = true;
        buff->m_cursorHoldTime = 0;
        g_hoverPopup->hideAndClear();

        // Insert the buffer next to the current one
        g_tabs.emplace(g_tabs.begin()+g_currTabI+1, std::make_unique<Split>(buff));
        ++g_currTabI; // Go to the current buffer
        g_activeBuff = buff;
    }
    closeSelection();
    g_isRedrawNeeded = true;
}

void Buffer::gotoDef()
{
    const auto loc = Autocomp::lspProvider->getDefinition(m_filePath, m_cursorLine, m_cursorCol);
    if (loc.path.empty())
        return;
    _goToDeclOrDefOrImp(loc);
}

void Buffer::gotoDecl()
{
    const auto loc = Autocomp::lspProvider->getDeclaration(m_filePath, m_cursorLine, m_cursorCol);
    if (loc.path.empty())
        return;
    _goToDeclOrDefOrImp(loc);
}

void Buffer::gotoImp()
{
    const auto loc = Autocomp::lspProvider->getImplementation(m_filePath, m_cursorLine, m_cursorCol);
    if (loc.path.empty())
        return;
    _goToDeclOrDefOrImp(loc);
}

static void applyLineCodeActDialogCb(int btn, Dialog* dlg, void* buff)
{
    MessageDialog* dlg_ = dynamic_cast<MessageDialog*>(dlg);
    assert(dlg_);

    // Exit if cancelled
    if (dlg_->getBtns()[btn].key == GLFW_KEY_Q)
        return;

    // TODO: Show preview

    const Buffer* buff_ = static_cast<const Buffer*>(buff);

    const auto& codeAct = buff_->getLineCodeAct();
    Autocomp::lspProvider->executeCommand(codeAct.actions[btn].command, codeAct.actions[btn].arguments);
}

void Buffer::applyLineCodeAct()
{
    if (m_lineCodeAction.forLine != (uint)m_cursorLine || m_lineCodeAction.actions.empty())
    {
        g_statMsg.set("No code actions for line", StatusMsg::Type::Error);
        return;
    }

    std::vector<MessageDialog::BtnInfo> btns;
    for (size_t i{}; i < m_lineCodeAction.actions.size(); ++i)
    {
        const auto& act = m_lineCodeAction.actions[i];
        btns.push_back({act.title, (i < 9 ? GLFW_KEY_1+(int)i : GLFW_KEY_A+(int)i)});
    }
    btns.push_back({"Cancel", GLFW_KEY_Q});

    MessageDialog::create(
            applyLineCodeActDialogCb, this, "Choose code action to apply", MessageDialog::Type::Information, btns);
}

size_t Buffer::applyDeletion(const lsRange& range)
{
    if (m_isReadOnly)
        return 0;

    //assert(range.start != range.end);
    if (range.start == range.end)
        return 0;

    const size_t deleted = m_document->delete_(range);

    m_isModified = true;
    m_isCursorShown = true;
    m_cursorHoldTime = 0;
    m_isHighlightUpdateNeeded = true;
    m_highlightBuffer.resize(m_document->calcCharCount()); // TODO: Just delete from `range`
    m_version++;
    g_isRedrawNeeded = true;
    // TODO: Only send change
    Autocomp::lspProvider->onFileChange(m_filePath, m_version, utf32To8(m_document->getConcated()));
    return deleted;
}

lsPosition Buffer::applyInsertion(const lsPosition& pos, const String& text)
{
    if (m_isReadOnly)
        return {m_cursorLine, m_cursorCol};

    const lsPosition endPos = m_document->insert(pos, text);

    m_isModified = true;
    m_isCursorShown = true;
    m_cursorHoldTime = 0;
    m_isHighlightUpdateNeeded = true;
    m_highlightBuffer.resize(m_document->calcCharCount()); // TODO: Just insert at `pos`
    m_version++;
    g_isRedrawNeeded = true;
    // TODO: Only send change
    Autocomp::lspProvider->onFileChange(m_filePath, m_version, utf32To8(m_document->getConcated()));
    return endPos;
}

void Buffer::beginHistoryEntry()
{
    m_document->m_history.beginEntry(m_cursorLine, m_cursorCol, m_cursorCharPos);
}

void Buffer::endHistoryEntry()
{
    m_document->m_history.endEntry(m_cursorLine, m_cursorCol, m_cursorCharPos);
}

void Buffer::applyEdit(const lsAnnotatedTextEdit& edit)
{
    Logger::dbg << "Buffer: Applying edit (file=" + m_filePath + ": " << edit.ToString() << Logger::End;

    // If this is a deletion/overwite, do the deletion
    if (edit.range.start != edit.range.end)
        applyDeletion(edit.range);

    // Do the insertion
    if (!edit.newText.empty())
        applyInsertion(edit.range.start, utf8To32(edit.newText));

    if (g_activeBuff == this)
        scrollViewportToCursor();

    moveCursorToLineCol(m_cursorLine, m_cursorCol);
}

void Buffer::applyEdits(const std::vector<lsTextEdit>& edits)
{
    Logger::log << "Applying " << edits.size() << " edits to " << m_filePath << '(' << this << ')' << Logger::End;

    beginHistoryEntry();
    // Sort the edits backwards, so the inserting/deleting lines won't mess up the line indexing
    auto edits_ = edits;
    std::sort(edits_.begin(), edits_.end(), [](const lsTextEdit& first, const lsTextEdit& second){
            const auto& pos1 = first.range.start;
            const auto& pos2 = second.range.start;
            // Note: We don't have to handle overlapping edits(, as it is garanteed that there won't be any),
            // so we only compare the `start`
            if (pos1.line == pos2.line)
                return (pos1.character > pos2.character);
            return (pos1.line > pos2.line);
    });

    for (const lsTextEdit& edit : edits_)
    {
        Logger::dbg << "Edit start: line: " << edit.range.start.line << ", col: " << edit.range.start.character << Logger::End;
        applyEdit(edit);
    }
    endHistoryEntry();
}

void Buffer::renameSymbolAtCursor()
{
    const auto canRename = Autocomp::lspProvider->canRenameSymbolAt(m_filePath, {m_cursorLine, m_cursorCol});
    if (canRename.isError)
    {
        Logger::err << "Can't rename symbol: " << canRename.errorOrSymName << Logger::End;
        g_statMsg.set(canRename.errorOrSymName, StatusMsg::Type::Error);
        return;
    }

    auto cb = [this](int, Dialog* dlg, void*){
        const lsPosition pos{m_cursorLine, m_cursorCol};
        auto dlg_ = dynamic_cast<AskerDialog*>(dlg);
        assert(dlg_);
        Autocomp::lspProvider->renameSymbol(m_filePath, pos, utf32To8(dlg_->getValue()));
    };

    const std::string symName = (!canRename.errorOrSymName.empty()
            ? canRename.errorOrSymName
            : utf32To8(m_document->get(canRename.rangeIfNoName)));
    Logger::dbg << "Will rename symbol: " << symName << Logger::End;
    AskerDialog::create(cb, nullptr, "New name for \033[3m"+symName+"\033[0m:");
}

Buffer::~Buffer()
{
#ifndef TESTING
    glfwSetCursor(g_window, Cursors::busy);
    m_shouldHighlighterLoopRun = false; // Signal the thread that we don't want more syntax updates
    m_isHighlightUpdateNeeded = true; // Exit the current update
#endif
    m_highlighterThread.join(); // Wait for the thread
#ifndef TESTING
    Autocomp::lspProvider->onFileClose(m_filePath);
    Logger::log << "Destroyed a buffer: " << this << " (" << m_filePath << ')' << Logger::End;
    glfwSetCursor(g_window, nullptr);
#endif
}
