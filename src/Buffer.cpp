#include "Buffer.h"
#include "config.h"
#include "Logger.h"
#include "Timer.h"
#include "types.h"
#include "syntax.h"
#include <fstream>
#include <sstream>
#include <regex>

Buffer::Buffer()
{
    Logger::dbg << "Created a buffer: " << this << Logger::End;
}

int Buffer::open(const std::string& filePath)
{
    TIMER_BEGIN_FUNC();

    m_filePath = filePath;
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

        m_content = ss.str();
        updateHighlighting();
        m_numOfLines = strCountLines(m_content);

        Logger::dbg << "Read "
            << m_content.length() << " characters ("
            << m_numOfLines << " lines) from file" << Logger::End;
        file.close();
    }
    catch (std::exception& e)
    {
        m_content.clear();
        updateHighlighting();
        m_numOfLines = 0;

        Logger::err << "Failed to open file: \"" << filePath << "\": " << e.what() << Logger::End;
        TIMER_END_FUNC();
        return 1;
    }

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

static int getLineLenAt(const std::string& str, int lineI)
{
    int _lineI{};
    std::string line;
    std::stringstream ss; ss << str;
    while (std::getline(ss, line) && _lineI != lineI) { ++_lineI; }
    return line.length();
}

void Buffer::scrollViewportToCursor()
{
    // Scroll up when the cursor goes out of the viewport
    if (m_cursorLine-5 < -m_scrollY/FONT_SIZE_PX)
    {
        scrollBy(-(m_scrollY+(m_cursorLine-5)*FONT_SIZE_PX));
    }
    // Scroll down when the cursor goes out of the viewport
    // FIXME: Line wrapping breaks this, too
    else if (m_cursorLine+m_scrollY/FONT_SIZE_PX+5 > g_textRenderer->getWindowHeight()/FONT_SIZE_PX)
    {
        scrollBy(-(m_cursorLine*FONT_SIZE_PX+m_scrollY-g_textRenderer->getWindowHeight()+FONT_SIZE_PX*5));
    }
}

void Buffer::updateHighlighting()
{
    if (m_content.empty())
        return;

    //Logger::log << "Updating syntax highlighting" << Logger::End;

    m_highlightBuffer = std::string(m_content.length(), SYNTAX_MARK_NONE);

#if ENABLE_SYNTAX_HIGHLIGHTING
    auto highlightWord{[&](const std::string& word, char colorMark, bool shouldBeWholeWord){
        assert(!word.empty());
        size_t start{};
        while (true)
        {
            size_t found = m_content.find(word, start);
            if (found == std::string::npos)
                break;
            if (!shouldBeWholeWord |
                ((found == 0 || !isalnum(m_content[found-1]))
                 && (found+word.size()-1 == m_content.size()-1 || !isalnum(m_content[found+word.size()]))))
                m_highlightBuffer.replace(found, word.length(), word.length(), colorMark);
            start = found+word.length();
        }
    }};

    auto highlightStrings{[&](char colorMark){
        bool isInsideString = false;
        for (size_t i{}; i < m_content.size(); ++i)
        {
            if (m_content[i] == '"'
                    && (i == 0 || (m_content[i-1] != '\\' || (i >= 2 && m_content[i-2] == '\\'))))
                isInsideString = !isInsideString;
            if (isInsideString || m_content[i] == '"')
                m_highlightBuffer[i] = colorMark;
        }
    }};

    auto highlightPrefixed{[&](const std::string& prefix, char colorMark){
        size_t charI{};
        std::stringstream ss;
        ss << m_content;
        std::string line;
        while (std::getline(ss, line))
        {
            size_t found = line.find(prefix);
            if (found != std::string::npos)
            {
                // FIXME
                m_highlightBuffer.replace(charI+found, line.size()-prefix.size()-1, line.size()-prefix.size()-1, colorMark);
            }
            charI += line.size()+1;
        }
    }};

    timer.reset();
    for (const auto& word : Syntax::keywordList)
        highlightWord(word, SYNTAX_MARK_KEYWORD, true);
    Logger::dbg << "Highlighting of keywords took " << timer.getElapsedTimeMs() << "ms" << Logger::End;

    timer.reset();
    for (const auto& word : Syntax::typeList)
        highlightWord(word, SYNTAX_MARK_TYPE, true);
    Logger::dbg << "Highlighting of types took " << timer.getElapsedTimeMs() << "ms" << Logger::End;

    timer.reset();
    //highlightRegex(Syntax::numberRegex, SYNTAX_MARK_NUMBER);
    Logger::dbg << "Highlighting of numbers took " << timer.getElapsedTimeMs() << "ms" << Logger::End;

    timer.reset();
    for (const auto& word : Syntax::operatorList)
        highlightWord(word, SYNTAX_MARK_OPERATOR, false);
    Logger::dbg << "Highlighting of operators took " << timer.getElapsedTimeMs() << "ms" << Logger::End;

    timer.reset();
    highlightStrings(SYNTAX_MARK_STRING);
    Logger::dbg << "Highlighting of strings took " << timer.getElapsedTimeMs() << "ms" << Logger::End;

    timer.reset();
    highlightPrefixed(Syntax::lineCommentPrefix, SYNTAX_MARK_COMMENT);
    Logger::dbg << "Highlighting of line comments took " << timer.getElapsedTimeMs() << "ms" << Logger::End;

    Logger::log << "Syntax highlighting updated" << Logger::End;
#endif
}

void Buffer::updateCursor()
{
    TIMER_BEGIN_FUNC();

    assert(m_cursorCol >= 0);
    assert(m_cursorLine >= 0);

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

    case CursorMovCmd::None:
        break;
    }

    assert(m_cursorCol >= 0);
    assert(m_cursorLine >= 0);

    // Scroll up when the cursor goes out of the viewport
    if (m_cursorMovCmd != CursorMovCmd::None)
    {
        scrollViewportToCursor();
        m_cursorMovCmd = CursorMovCmd::None;
    }

    TIMER_END_FUNC();
}

static inline void renderStatusLine(
        int cursorLine, int cursorCol, int cursorCharPos,
        const std::string& filePath)
{
    const int winW = g_textRenderer->getWindowWidth();
    const int winH = g_textRenderer->getWindowHeight();

    g_uiRenderer->renderRectangleOutline(
            {LINEN_BAR_WIDTH*FONT_SIZE_PX, winH-FONT_SIZE_PX*1.2f},
            {winW, winH},
            {0.5f, 0.5f, 0.5f},
            2);
    g_uiRenderer->renderFilledRectangle(
            {LINEN_BAR_WIDTH*FONT_SIZE_PX, winH-FONT_SIZE_PX*1.2f},
            {winW, winH},
            RGB_COLOR_TO_RGBA(STATUSBAR_BG_COLOR));

    const std::string cursorPosString
        = std::to_string(cursorLine+1) + ':'
        + std::to_string(cursorCol+1) + " | "
        + std::to_string(cursorCharPos);
    g_textRenderer->renderString(
            cursorPosString,
            {winW-FONT_SIZE_PX*(4+1+3+3+7)*0.7f,
             winH-FONT_SIZE_PX-4});

    const std::string statusLineString = filePath;
    g_textRenderer->renderString(
            statusLineString,
            {LINEN_BAR_WIDTH*FONT_SIZE_PX,
             winH-FONT_SIZE_PX-4});
}

void Buffer::render()
{
    TIMER_BEGIN_FUNC();

    // Draw line number background
    g_uiRenderer->renderFilledRectangle(
            m_position,
            {FONT_SIZE_PX*LINEN_BAR_WIDTH, g_textRenderer->getWindowHeight()},
            RGB_COLOR_TO_RGBA(LINEN_BG_COLOR));

    const float initTextX = m_position.x+FONT_SIZE_PX*LINEN_BAR_WIDTH;
    const float initTextY = m_position.y+m_scrollY;
    float textX = initTextX;
    float textY = initTextY;

    g_textRenderer->prepareForDrawing();
    g_textRenderer->setDrawingColor({1.0f, 1.0f, 1.0f});

    char isLineBeginning = true;
    int lineI{};
    int colI{};
    size_t charI{(size_t)-1};
    for (char c : m_content)
    {
        ++charI;

        // Don't draw the part of the buffer that is below the viewport
        if (textY > g_textRenderer->getWindowHeight())
        {
            break;
        }
        const bool isCharInsideViewport = textY > -FONT_SIZE_PX;

        if (isCharInsideViewport && BUFFER_DRAW_LINE_NUMS && isLineBeginning)
        {
            g_textRenderer->setDrawingColor(
                    lineI == m_cursorLine ? LINEN_ACTIVE_FONT_COLOR : LINEN_FONT_COLOR);

            float digitX = m_position.x;
            for (char digit : std::to_string(lineI+1))
            {
                auto dimensions = g_textRenderer->renderChar(digit,
                        {digitX, textY},
                        lineI == m_cursorLine ? FontStyle::Bold : FontStyle::Italic);
                digitX += dimensions.advance/64.0f;
            }

            // Reset color
            g_textRenderer->setDrawingColor({1.0f, 1.0f, 1.0f});
        }

        if (isCharInsideViewport && isLineBeginning && m_cursorLine == lineI)
        {
            g_uiRenderer->renderFilledRectangle(
                    {textX, initTextY+textY-m_scrollY-m_position.y+2},
                    {textX+g_textRenderer->getWindowWidth(), initTextY+textY-m_scrollY-m_position.y+2+FONT_SIZE_PX},
                    CURSOR_LINE_COLOR
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
                if ((m_cursorMovCmd != CursorMovCmd::None || m_isCursorShown)
                        && lineI == m_cursorLine && colI == m_cursorCol)
                {
#if CURSOR_DRAW_BLOCK
                    g_uiRenderer->renderRectangleOutline(
                            {textX-2, initTextY+textY-m_scrollY-m_position.y-2},
                            {textX+width+2, initTextY+textY-m_scrollY-m_position.y+FONT_SIZE_PX+2},
                            {1.0f, 0.0f, 0.0f},
                            2
                    );
                    g_uiRenderer->renderFilledRectangle(
                            {textX-2, initTextY+textY-m_scrollY-m_position.y-2},
                            {textX+width+2, initTextY+textY-m_scrollY-m_position.y+FONT_SIZE_PX+2},
                            {1.0f, 0.0f, 0.0f, 0.4f}
                    );
#else
                    (void)width;
                    g_uiRenderer->renderFilledRectangle(
                            {textX-1, initTextY+textY-m_scrollY-m_position.y-2},
                            {textX+1, initTextY+textY-m_scrollY-m_position.y+FONT_SIZE_PX+2},
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
        case '\r': // Carriage return
            drawCursorIfNeeded(FONT_SIZE_PX*0.7f);
            textX = initTextX;
            textY += FONT_SIZE_PX;
            isLineBeginning = true;
            ++lineI;
            colI = 0;
            continue;

        case '\t': // Tab
            drawCursorIfNeeded(FONT_SIZE_PX*0.7f);
            textX += FONT_SIZE_PX*4;
            ++colI;
            continue;

        case '\v': // Vertical tab
            drawCursorIfNeeded(FONT_SIZE_PX*0.7f);
            textX = initTextX;
            textY += FONT_SIZE_PX * 4;
            colI = 0;
            ++lineI;
            continue;
        }

        if (BUFFER_WRAP_LINES && textX+FONT_SIZE_PX > g_textRenderer->getWindowWidth())
        {
            textX = initTextX;
            textY += FONT_SIZE_PX;
        }

        uint advance{};
        if (isCharInsideViewport)
        {
            FontStyle charStyle = FontStyle::Regular;
            RGBColor charColor;
            switch (m_highlightBuffer[charI])
            {
                case SYNTAX_MARK_NONE:     charColor = SYNTAX_COLOR_NONE;     charStyle = SYNTAX_STYLE_NONE; break;
                case SYNTAX_MARK_KEYWORD:  charColor = SYNTAX_COLOR_KEYWORD;  charStyle = SYNTAX_STYLE_KEYWORD; break;
                case SYNTAX_MARK_TYPE:     charColor = SYNTAX_COLOR_TYPE;     charStyle = SYNTAX_STYLE_TYPE; break;
                case SYNTAX_MARK_NUMBER:   charColor = SYNTAX_COLOR_NUMBER;   charStyle = SYNTAX_STYLE_NUMBER; break;
                case SYNTAX_MARK_OPERATOR: charColor = SYNTAX_COLOR_OPERATOR; charStyle = SYNTAX_STYLE_OPERATOR; break;
                case SYNTAX_MARK_STRING:   charColor = SYNTAX_COLOR_STRING;   charStyle = SYNTAX_STYLE_STRING; break;
                case SYNTAX_MARK_COMMENT:  charColor = SYNTAX_COLOR_COMMENT;  charStyle = SYNTAX_STYLE_COMMENT; break;
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

    renderStatusLine(
            m_cursorLine, m_cursorCol, m_cursorCharPos,
            m_filePath);

    TIMER_END_FUNC();
}

void Buffer::insert(char character)
{
    TIMER_BEGIN_FUNC();

    assert(m_cursorCol >= 0);
    assert(m_cursorLine >= 0);

    m_content = m_content.insert(m_cursorCharPos, 1, character);

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

    updateHighlighting();
    scrollViewportToCursor();

    TIMER_END_FUNC();
}

void Buffer::deleteCharBackwards()
{
    TIMER_BEGIN_FUNC();

    assert(m_cursorCol >= 0);
    assert(m_cursorLine >= 0);

    // If deleting at the beginning of the line and we have stuff to delete
    if (m_cursorCol == 0 && m_cursorLine != 0)
    {
        --m_cursorLine;
        m_cursorCol = getLineLenAt(m_content, m_cursorLine);
        m_content.erase(m_cursorCharPos-1, 1);
        --m_cursorCharPos;
        m_isModified = true;
    }
    // If deleting in the middle/end of the line and we have stuff to delete
    else if (m_cursorCharPos > 0)
    {
        m_content.erase(m_cursorCharPos-1, 1);
        --m_cursorCol;
        --m_cursorCharPos;
        m_isModified = true;
    }

    assert(m_cursorCol >= 0);
    assert(m_cursorLine >= 0);

    updateHighlighting();
    scrollViewportToCursor();

    TIMER_END_FUNC();
}

void Buffer::deleteCharForward()
{
    TIMER_BEGIN_FUNC();

    assert(m_cursorCol >= 0);
    assert(m_cursorLine >= 0);

    int lineLen = getLineLenAt(m_content, m_cursorLine);

    // If deleting at the end of the line and we have stuff to delete
    if (m_cursorCol == lineLen && m_cursorLine < m_numOfLines)
    {
        m_content.erase(m_cursorCharPos, 1);
        --m_numOfLines;
        m_isModified = true;
    }
    // If deleting in the middle/beginning of the line and we have stuff to delete
    else if (m_cursorCharPos != (size_t)lineLen && m_cursorCharPos < m_content.size())
    {
        m_content.erase(m_cursorCharPos, 1);
        m_isModified = true;
    }

    assert(m_cursorCol >= 0);
    assert(m_cursorLine >= 0);

    scrollViewportToCursor();
    updateHighlighting();

    TIMER_END_FUNC();
}

Buffer::~Buffer()
{
    Logger::dbg << "Destroyed a buffer: " << this << Logger::End;
}
