#include "Buffer.h"
#include "config.h"
#include "Timer.h"
#include "types.h"
#include "Syntax.h"
#include "MessageDialog.h"
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

    Logger::log << "Created a buffer: " << this << Logger::End;
}

int Buffer::open(const std::string& filePath)
{
    TIMER_BEGIN_FUNC();

    glfwSetCursor(g_window, Cursors::busy);

    m_filePath = filePath;
    m_isReadOnly = false;
    m_history.clear();
    Logger::dbg << "Opening file: " << filePath << Logger::End;

    try
    {
        std::fstream file;
        file.open(filePath, std::ios::in);
        if (file.fail())
        {
            throw std::runtime_error{"Open failed"};
        }
        std::stringstream ss;
        ss << file.rdbuf();

        for (char c : ss.str())
        {
            if (c & 0b10000000)
            {
                // This is an unicode file, we don't support writing yet, so set file to read-only
                const std::string msg = "Unicode/binary file detected: "+m_filePath+"\n"
                    "Writing of Unicode/binary files is not yet supported.\n"
                    "Buffer is set to read-only.";
                Logger::log << msg << Logger::End;
                MessageDialog::create(Dialog::EMPTY_CB, nullptr,
                        msg,
                        MessageDialog::Type::Warning);
                m_isReadOnly = true;
                break;
            }
        }
        m_content = strToUtf32(ss.str().c_str());
        //Logger::dbg << "Loaded:\n" << strToAscii(m_content) << Logger::End;
        m_highlightBuffer = std::u8string(m_content.length(), Syntax::MARK_NONE);
        m_isHighlightUpdateNeeded = true;
        m_numOfLines = strCountLines(m_content);

        Logger::dbg << "Read "
            << m_content.length() << " characters ("
            << m_numOfLines << " lines) from file" << Logger::End;
        file.close();
    }
    catch (std::exception& e)
    {
        m_content.clear();
        m_highlightBuffer.clear();
        m_isHighlightUpdateNeeded = true;
        m_numOfLines = 0;

        Logger::err << "Failed to open file: \"" << filePath << "\": " << e.what() << Logger::End;
        glfwSetCursor(g_window, nullptr);
        TIMER_END_FUNC();
        return 1;
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
    return 0;
}

int Buffer::saveToFile()
{
    TIMER_BEGIN_FUNC();

    Logger::dbg << "Writing buffer to file: " << m_filePath << Logger::End;

    try
    {
        std::fstream file;
        file.open(m_filePath, std::ios::out);
        if (file.fail())
        {
            throw std::runtime_error{"Open failed"};
        }
        file.write(m_content.c_str(), m_content.length());
        if (file.fail())
        {
            throw std::runtime_error{"Call to write() failed"};
        }
        file.close();
        Logger::dbg << "Wrote "
            << m_content.length() << " characters ("
            << m_numOfLines << " lines) to file" << Logger::End;
    }
    catch (std::exception& e)
    {
        Logger::err << "Failed to write to file: \"" << m_filePath << "\": " << e.what() << Logger::End;
        TIMER_END_FUNC();
        return 1;
    }

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
    m_statusLineStr.str
        = std::to_string(m_cursorLine+1)
        +':'+std::to_string(m_cursorCol+1)
        +" | "+std::to_string(m_cursorCharPos)
        + " L"+std::to_string(getLineLenAt(m_content, m_cursorLine));
    m_statusLineStr.maxLen = std::max(size_t(4+1+3+3+7), m_statusLineStr.str.size());
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


        switch (c)
        {
        case '\n': // New line
            drawCursorIfNeeded(g_fontSizePx*0.7f);
            textX = initTextX;
            textY += g_fontSizePx;
            isLineBeginning = true;
            isLeadingSpace = true;
            ++lineI;
            colI = 0;
            continue;

        case '\r': // Carriage return
            // Don't render them or anything
            continue;

        case '\t': // Tab
            drawCursorIfNeeded(g_fontSizePx*0.7f);
            textX += g_fontSizePx*4;
            ++colI;
            continue;

        case '\v': // Vertical tab
            drawCursorIfNeeded(g_fontSizePx*0.7f);
            textX = initTextX;
            textY += g_fontSizePx * 4;
            colI = 0;
            ++lineI;
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

Buffer::~Buffer()
{
    glfwSetCursor(g_window, Cursors::busy);
    m_isHighlightUpdateNeeded = true; // Exit the current update
    m_shouldHighlighterLoopRun = false; // Signal the thread that we don't want more syntax updates
    m_highlighterThread.join(); // Wait for the thread
    Logger::log << "Destroyed a buffer: " << this << " (" << m_filePath << ')' << Logger::End;
    glfwSetCursor(g_window, nullptr);
}
