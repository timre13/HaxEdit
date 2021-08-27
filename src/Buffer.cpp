#include "Buffer.h"
#include "config.h"
#include "Logger.h"
#include "Timer.h"
#include <fstream>
#include <sstream>

Buffer::Buffer(TextRenderer* textRenderer, UiRenderer* uiRenderer)
    : m_textRenderer{textRenderer}, m_uiRenderer{uiRenderer}
{
}

static int countLines(const std::string& str)
{
    int lines{};
    for (auto it=str.begin(); it != str.end(); ++it)
    {
        if (*it == '\n')
            ++lines;
    }
    return lines;
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
        m_numOfLines = countLines(m_content);

        Logger::dbg << "Read "
            << m_content.length() << " characters ("
            << m_numOfLines << " lines) from file" << Logger::End;
        file.close();
    }
    catch (std::exception& e)
    {
        m_content.clear();
        m_numOfLines = 0;

        Logger::err << "Failed to open file: \"" << filePath << "\": " << e.what() << Logger::End;
        return 1;
    }

    TIMER_END_FUNC();
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

void Buffer::updateCursor()
{
    TIMER_BEGIN_FUNC();

    assert(m_cursorCol >= 0);
    assert(m_cursorLine >= 0);
    assert(m_cursorCharPos >= 0);

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
    {
        m_cursorCharPos -= m_cursorCol;
        m_cursorCol = 0;
        break;
    }

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
    assert(m_cursorCharPos >= 0);

    // Scroll up when the cursor goes out of the viewport
    if (m_cursorMovCmd != CursorMovCmd::None &&
        m_cursorLine-5 < -m_scrollY/FONT_SIZE_PX)
    {
        scrollBy(-(m_scrollY+(m_cursorLine-5)*FONT_SIZE_PX));
    }
    // Scroll down when the cursor goes out of the viewport
    // FIXME: Line wrapping breaks this, too
    else if (m_cursorMovCmd != CursorMovCmd::None &&
        m_cursorLine+m_scrollY/FONT_SIZE_PX+5 > m_textRenderer->getWindowHeight()/FONT_SIZE_PX)
    {
        scrollBy(-(m_cursorLine*FONT_SIZE_PX+m_scrollY-m_textRenderer->getWindowHeight()+FONT_SIZE_PX*5));
    }

    m_cursorMovCmd = CursorMovCmd::None;

    TIMER_END_FUNC();
}

static inline void renderStatusLine(
        UiRenderer* uiRenderer, TextRenderer* textRenderer,
        int cursorLine, int cursorCol, int cursorCharPos,
        const std::string& filePath)
{
    uiRenderer->renderRectangleOutline(
            {LINEN_BAR_WIDTH*FONT_SIZE_PX, textRenderer->getWindowHeight()-FONT_SIZE_PX*1.2f},
            {textRenderer->getWindowWidth(), textRenderer->getWindowHeight()},
            {0.5f, 0.5f, 0.5f},
            2);
    uiRenderer->renderFilledRectangle(
            {LINEN_BAR_WIDTH*FONT_SIZE_PX, textRenderer->getWindowHeight()-FONT_SIZE_PX*1.2f},
            {textRenderer->getWindowWidth(), textRenderer->getWindowHeight()},
            RGB_COLOR_TO_RGBA(STATUSBAR_BG_COLOR));

    const std::string cursorPosString
        = std::to_string(cursorLine+1) + ':'
        + std::to_string(cursorCol+1) + " | "
        + std::to_string(cursorCharPos);
    textRenderer->renderString(
            cursorPosString,
            {textRenderer->getWindowWidth()-FONT_SIZE_PX*(4+1+3+3+7)*0.7f,
             textRenderer->getWindowHeight()-FONT_SIZE_PX-4});

    const std::string statusLineString = filePath;
    textRenderer->renderString(
            statusLineString,
            {LINEN_BAR_WIDTH*FONT_SIZE_PX,
             textRenderer->getWindowHeight()-FONT_SIZE_PX-4});
}

void Buffer::render()
{
    TIMER_BEGIN_FUNC();

    // Draw line number background
    m_uiRenderer->renderFilledRectangle(
            m_position,
            {FONT_SIZE_PX*LINEN_BAR_WIDTH, m_textRenderer->getWindowHeight()},
            RGB_COLOR_TO_RGBA(LINEN_BG_COLOR));

    const float initTextX = m_position.x+FONT_SIZE_PX*LINEN_BAR_WIDTH;
    const float initTextY = m_position.y+m_scrollY;
    float textX = initTextX;
    float textY = initTextY;

    m_textRenderer->prepareForDrawing();
    m_textRenderer->setDrawingColor({1.0f, 1.0f, 1.0f});

    char isLineBeginning = true;
    int lineI{};
    int colI{};
    for (char c : m_content)
    {
        if (textY > m_textRenderer->getWindowHeight())
        {
            break;
        }

        // TODO: Don't draw the part of the buffer that is above the viewport

        if (BUFFER_DRAW_LINE_NUMS && isLineBeginning)
        {
            m_textRenderer->setDrawingColor(
                    lineI == m_cursorLine ? LINEN_ACTIVE_FONT_COLOR : LINEN_FONT_COLOR);

            float digitX = m_position.x;
            for (char digit : std::to_string(lineI+1))
            {
                auto dimensions = m_textRenderer->renderChar(digit,
                        {digitX, textY},
                        lineI == m_cursorLine ? FontStyle::Bold : FontStyle::Italic);
                digitX += dimensions.advance/64.0f;
            }

            // Reset color
            m_textRenderer->setDrawingColor({1.0f, 1.0f, 1.0f});
        }

        /*
         * Render a cursor at the current character position if it is the cursor position.
         * Used to draw a cursor when the character is special. (line endings, tabs, etc.)
         */
        auto drawCursorIfNeeded{
            [&]()
            {
                if ((m_cursorMovCmd != CursorMovCmd::None || m_isCursorShown)
                        && lineI == m_cursorLine && colI == m_cursorCol)
                {
#if CURSOR_DRAW_BLOCK
                    m_uiRenderer->renderRectangleOutline(
                            {textX-2, initTextY+textY-m_scrollY-2},
                            {textX+FONT_SIZE_PX*0.75+2, initTextY+textY-m_scrollY+FONT_SIZE_PX+2},
                            {1.0f, 0.0f, 0.0f},
                            2
                    );
                    m_uiRenderer->renderFilledRectangle(
                            {textX-2, initTextY+textY-m_scrollY-2},
                            {textX+FONT_SIZE_PX*0.75+2, initTextY+textY-m_scrollY+FONT_SIZE_PX+2},
                            {1.0f, 0.0f, 0.0f, 0.4f}
                    );
#else
                    m_uiRenderer->renderFilledRectangle(
                            {textX-1, initTextY+textY-m_scrollY-2},
                            {textX+1, initTextY+textY-m_scrollY+FONT_SIZE_PX+2},
                            {1.0f, 0.0f, 0.0f}
                    );
#endif
                    // Bind the text renderer shader again
                    m_textRenderer->prepareForDrawing();
                }
            }
        };


        switch (c)
        {
        case '\n': // New line
        case '\r': // Carriage return
            drawCursorIfNeeded();
            textX = initTextX;
            textY += FONT_SIZE_PX;
            isLineBeginning = true;
            ++lineI;
            colI = 0;
            continue;

        case '\t': // Tab
            drawCursorIfNeeded();
            textX += FONT_SIZE_PX*4;
            ++colI;
            continue;

        case '\v': // Vertical tab
            drawCursorIfNeeded();
            textX = initTextX;
            textY += FONT_SIZE_PX * 4;
            colI = 0;
            ++lineI;
            continue;
        }

        if (BUFFER_WRAP_LINES && textX+FONT_SIZE_PX > m_textRenderer->getWindowWidth())
        {
            textX = initTextX;
            textY += FONT_SIZE_PX;
        }

        auto dimensions = m_textRenderer->renderChar(c, {textX, textY});

        // Draw cursor
        if ((m_cursorMovCmd != CursorMovCmd::None ||
                m_isCursorShown) && lineI == m_cursorLine && colI == m_cursorCol)
        {
#if CURSOR_DRAW_BLOCK
            m_uiRenderer->renderRectangleOutline(
                    {textX-2, initTextY+textY-m_scrollY-2},
                    {textX+dimensions.advance/64.0f+2, initTextY+textY-m_scrollY+FONT_SIZE_PX+2},
                    {1.0f, 0.0f, 0.0f},
                    2
            );
            m_uiRenderer->renderFilledRectangle(
                    {textX-2, initTextY+textY-m_scrollY-2},
                    {textX+dimensions.advance/64.0f+2, initTextY+textY-m_scrollY+FONT_SIZE_PX+2},
                    {1.0f, 0.0f, 0.0f, 0.4f}
            );
#else
            m_uiRenderer->renderFilledRectangle(
                    {textX-1, initTextY+textY-m_scrollY-2},
                    {textX+1, initTextY+textY-m_scrollY+FONT_SIZE_PX+2},
                    {1.0f, 0.0f, 0.0f}
            );
#endif

            // Bind the text renderer shader again
            m_textRenderer->prepareForDrawing();
        }

        textX += (dimensions.advance/64.0f);
        ++colI;
        isLineBeginning = false;
    }

    renderStatusLine(
            m_uiRenderer, m_textRenderer,
            m_cursorLine, m_cursorCol, m_cursorCharPos,
            m_filePath);

    TIMER_END_FUNC();
}

void Buffer::insert(char character)
{
    TIMER_BEGIN_FUNC();

    assert(m_cursorCol >= 0);
    assert(m_cursorLine >= 0);
    assert(m_cursorCharPos >= 0);

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

    TIMER_END_FUNC();
}

void Buffer::deleteCharBackwards()
{
    TIMER_BEGIN_FUNC();

    assert(m_cursorCol >= 0);
    assert(m_cursorLine >= 0);
    assert(m_cursorCharPos >= 0);

    // If deleting at the beginning of the line and we have stuff to delete
    if (m_cursorCol == 0 && m_cursorLine != 0)
    {
        --m_cursorLine;
        m_cursorCol = getLineLenAt(m_content, m_cursorLine);
        m_content.erase(m_cursorCharPos-1, 1);
        --m_cursorCharPos;
    }
    // If deleting in the middle/end of the line and we have stuff to delete
    else if (m_cursorCharPos > 0)
    {
        m_content.erase(m_cursorCharPos-1, 1);
        --m_cursorCol;
        --m_cursorCharPos;
    }

    assert(m_cursorCol >= 0);
    assert(m_cursorLine >= 0);
    assert(m_cursorCharPos >= 0);

    TIMER_END_FUNC();
}

void Buffer::deleteCharForward()
{
    TIMER_BEGIN_FUNC();

    assert(m_cursorCol >= 0);
    assert(m_cursorLine >= 0);
    assert(m_cursorCharPos >= 0);

    auto lineLen = getLineLenAt(m_content, m_cursorLine);

    // If deleting at the end of the line and we have stuff to delete
    if (m_cursorCol == lineLen && m_cursorLine < m_numOfLines)
    {
        m_content.erase(m_cursorCharPos, 1);
        --m_numOfLines;
    }
    // If deleting in the middle/beginning of the line and we have stuff to delete
    else if (m_cursorCharPos != lineLen && m_cursorCharPos < m_content.size())
    {
        m_content.erase(m_cursorCharPos, 1);
    }

    assert(m_cursorCol >= 0);
    assert(m_cursorLine >= 0);
    assert(m_cursorCharPos >= 0);

    TIMER_END_FUNC();
}
