#include "Buffer.h"
#include "config.h"
#include "Timer.h"
#include "types.h"
#include "Syntax.h"
#include "MessageDialog.h"
#include "unicode/ustdio.h"
#include "common.h"
#include <fstream>
#include <sstream>
#include <chrono>
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

    m_autocompPopup = std::make_unique<Autocomp::Popup>();
    Autocomp::dictProvider->get(m_autocompPopup.get());

    Logger::log << "Created a buffer: " << this << Logger::End;
}

void Buffer::open(const std::string& filePath)
{
    TIMER_BEGIN_FUNC();

    glfwSetCursor(g_window, Cursors::busy);

    m_filePath = filePath;
    m_isReadOnly = false;
    m_history.clear();
    Logger::dbg << "Opening file: " << filePath << Logger::End;

    try
    {
        m_content = loadUnicodeFile(filePath);
        m_highlightBuffer = std::u8string(m_content.length(), Syntax::MARK_NONE);
        m_isHighlightUpdateNeeded = true;
        m_numOfLines = strCountLines(m_content);

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

        Logger::err << "Failed to open file: " << quoteStr(filePath) << ": " << e.what() << Logger::End;
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
        TIMER_END_FUNC();
        return 1;
    }
    Logger::log << "Wrote " << m_content.size() << " characters" << Logger::End;

    m_isModified = false;
    m_isReadOnly = false;

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
            if (path[0] == '/')
                // Absolute path
                return isValidFilePath(path);
            else
                // Relative path
                return isValidFilePath(getParentPath(m_filePath)/std::filesystem::path{path});
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

void Buffer::updateRStatusLineStr()
{
    if (m_selection.mode == Selection::Mode::None)
    {
        m_statusLineStr.str
            = std::to_string(m_cursorLine+1)
            +':'+std::to_string(m_cursorCol+1)
            +" | "+std::to_string(m_cursorCharPos);
        m_statusLineStr.maxLen = std::max(size_t(4+1+3+3+7), m_statusLineStr.str.size());
    }
    else
    {
        bool showSizeInLines = (m_selection.mode == Selection::Mode::Line)
                            || (m_selection.mode == Selection::Mode::Block);
        bool showSizeInCols = m_selection.mode == Selection::Mode::Block;
        bool showSizeInChars = m_selection.mode == Selection::Mode::Normal;

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

        bool isCharSelected = false;
        switch (m_selection.mode)
        {
        case Selection::Mode::None:
            isCharSelected = false;
            break;

        case Selection::Mode::Normal:
            if (m_selection.fromCharI < m_cursorCharPos)
            {
                isCharSelected = charI >= m_selection.fromCharI && charI <= m_cursorCharPos;
            }
            else
            {
                isCharSelected = charI <= m_selection.fromCharI && charI >= m_cursorCharPos;
            }
            break;

        case Selection::Mode::Line:
            if (m_selection.fromLine < m_cursorLine)
            {
                isCharSelected = lineI >= m_selection.fromLine && lineI <= m_cursorLine;
            }
            else
            {
                isCharSelected = lineI <= m_selection.fromLine && lineI >= m_cursorLine;
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

            isCharSelected = isLineOk && isColOk;
            break;
        }
        }

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
#if CURSOR_DRAW_BLOCK
                    g_uiRenderer->renderRectangleOutline(
                            {textX-2, initTextY+textY-m_scrollY-m_position.y-2},
                            {textX+width+2, initTextY+textY-m_scrollY-m_position.y+g_fontSizePx+2},
                            {1.0f, 0.0f, 0.0f},
                            2
                    );
                    g_uiRenderer->renderFilledRectangle(
                            {textX-2, initTextY+textY-m_scrollY-m_position.y-2},
                            {textX+width+2, initTextY+textY-m_scrollY-m_position.y+g_fontSizePx+2},
                            {1.0f, 0.0f, 0.0f, 0.4f}
                    );
#else
                    (void)width;
                    g_uiRenderer->renderFilledRectangle(
                            {textX-1, initTextY+textY-m_scrollY-m_position.y-2},
                            {textX+1, initTextY+textY-m_scrollY-m_position.y+g_fontSizePx+2},
                            {1.0f, 0.0f, 0.0f}
                    );
#endif

                    // Bind the text renderer shader again
                    g_textRenderer->prepareForDrawing();
                }
            }
        };

        /*
         * Draws the selection rectangle around the current character if it is selected.
         */
        auto drawCharSelectionMarkIfNeeded{[&](int width){
            if (isCharSelected)
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
            .pos=m_cursorCharPos,
            .arg=character});
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
                .pos=m_cursorCharPos-1,
                .arg=m_content[m_cursorCharPos-1]});
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
                .pos=m_cursorCharPos-1,
                .arg=m_content[m_cursorCharPos-1]});
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

void Buffer::deleteCharForward()
{
    if (m_isReadOnly)
        return;

    TIMER_BEGIN_FUNC();

    assert(m_cursorCol >= 0);
    assert(m_cursorLine >= 0);

    int lineLen = getLineLenAt(m_content, m_cursorLine);

    // If deleting at the end of the line and we have stuff to delete
    if (m_cursorCol == lineLen && m_cursorLine < m_numOfLines)
    {
        m_history.add(BufferHistory::Entry{
                .action=BufferHistory::Entry::Action::Delete,
                .pos=m_cursorCharPos,
                .arg=m_content[m_cursorCharPos]});
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
                .pos=m_cursorCharPos,
                .arg=m_content[m_cursorCharPos]});
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
            break;

        case BufferHistory::Entry::Action::Insert:
            m_content.erase(entry.pos, 1);
            m_highlightBuffer.erase(entry.pos, 1);
            break;

        case BufferHistory::Entry::Action::Delete:
            m_content.insert(entry.pos, 1, entry.arg);
            m_highlightBuffer.insert(entry.pos, 1, entry.arg);
            break;
        }

        m_isHighlightUpdateNeeded = true;
        scrollViewportToCursor();
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
            break;

        case BufferHistory::Entry::Action::Insert:
            m_content.insert(entry.pos, 1, entry.arg);
            m_highlightBuffer.insert(entry.pos, 1, entry.arg);
            break;

        case BufferHistory::Entry::Action::Delete:
            m_content.erase(entry.pos, 1);
            m_highlightBuffer.erase(entry.pos, 1);
            break;
        }

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

void Buffer::triggerAutocompPopupOrSelectNextItem()
{
    if (m_autocompPopup->isVisible())
    {
        m_autocompPopup->selectNextItem();
    }
    else
    {
        m_autocompPopup->setVisibility(true);
    }
}

void Buffer::triggerAutocompPopupOrSelectPrevItem()
{
    if (m_autocompPopup->isVisible())
    {
        m_autocompPopup->selectPrevItem();
    }
    else
    {
        m_autocompPopup->setVisibility(true);
    }
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

Buffer::~Buffer()
{
    glfwSetCursor(g_window, Cursors::busy);
    m_isHighlightUpdateNeeded = true; // Exit the current update
    m_shouldHighlighterLoopRun = false; // Signal the thread that we don't want more syntax updates
    m_highlighterThread.join(); // Wait for the thread
    Logger::log << "Destroyed a buffer: " << this << " (" << m_filePath << ')' << Logger::End;
    glfwSetCursor(g_window, nullptr);
}
