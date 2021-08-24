#include "Buffer.h"
#include "config.h"
#include "Logger.h"
#include <fstream>
#include <sstream>

Buffer::Buffer(TextRenderer* textRenderer, UiRenderer* uiRenderer)
    : m_textRenderer{textRenderer}, m_uiRenderer{uiRenderer}
{
}

static size_t countLines(const std::string& str)
{
    size_t lines{};
    for (auto it=str.begin(); it != str.end(); ++it)
    {
        if (*it == '\n')
            ++lines;
    }
    return lines;
}

int Buffer::open(const std::string& filePath)
{
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
        return 0;
    }
    catch (std::exception& e)
    {
        m_content.clear();
        m_numOfLines = 0;

        Logger::err << "Failed to open file: \"" << filePath << "\": " << e.what() << Logger::End;
        return 1;
    }
}

void Buffer::updateCursor()
{
    auto getCursorLineStr{
        [&]()
        {
            size_t lineI{};
            std::string line;
            std::stringstream ss; ss << m_content;
            while (std::getline(ss, line) && lineI != m_cursorLine) { ++lineI; }
            return line;
        }
    };
    std::string cursorLine = getCursorLineStr();

    switch (m_cursorMovCmd)
    {
    case CursorMovCmd::Left:
        if (m_cursorCol > 0)
        {
            --m_cursorCol;
        }
        break;

    case CursorMovCmd::Right:
        if (!cursorLine.empty() && m_cursorCol < cursorLine.size())
        {
            ++m_cursorCol;
        }
        break;

    case CursorMovCmd::Up:
        if (m_cursorLine > 0)
        {
            --m_cursorLine;
            // If the new line is smaller than the current cursor column, step back to the end of the line
            m_cursorCol = std::min(getCursorLineStr().length(), m_cursorCol);
        }
        break;

    case CursorMovCmd::Down:
        if (m_cursorLine < m_numOfLines-1)
        {
            ++m_cursorLine;
            // If the new line is smaller than the current cursor column, step back to the end of the line
            m_cursorCol = std::min(getCursorLineStr().length(), m_cursorCol);
        }
        break;

    case CursorMovCmd::None:
        break; // Already handled
    }

    m_cursorMovCmd = CursorMovCmd::None;
}

void Buffer::render()
{
    // Draw line number background
    m_uiRenderer->renderFilledRectangle(
            m_position,
            {FONT_SIZE_PX*LINEN_BAR_WIDTH, m_textRenderer->getWindowHeight()},
            RGB_COLOR_TO_RGBA(LINEN_BG_COLOR));

    const float initTextX = m_position.x+FONT_SIZE_PX*LINEN_BAR_WIDTH;
    const float initTextY = m_position.y;
    float textX = initTextX;
    float textY = initTextY;

    m_textRenderer->prepareForDrawing();
    m_textRenderer->setDrawingColor({1.0f, 1.0f, 1.0f});

    char isLineBeginning = true;
    size_t lineI{};
    size_t colI{};
    for (char c : m_content)
    {
        if (BUFFER_DRAW_LINE_NUMS && isLineBeginning)
        {
            m_textRenderer->setDrawingColor(LINEN_FONT_COLOR);

            float digitX = m_position.x;
            for (char digit : std::to_string(lineI+1))
            {
                auto dimensions = m_textRenderer->renderChar(digit, {digitX, textY}, FontStyle::Italic);
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
                if (lineI == m_cursorLine && colI == m_cursorCol)
                {
#if CURSOR_DRAW_BLOCK
                    m_uiRenderer->renderRectangleOutline(
                            {textX-2, initTextY+textY-2},
                            {textX+FONT_SIZE_PX*0.75+2, initTextY+textY+FONT_SIZE_PX+2},
                            {1.0f, 0.0f, 0.0f},
                            2
                    );
                    m_uiRenderer->renderFilledRectangle(
                            {textX-2, initTextY+textY-2},
                            {textX+FONT_SIZE_PX*0.75+2, initTextY+textY+FONT_SIZE_PX+2},
                            {1.0f, 0.0f, 0.0f, 0.4f}
                    );
#else
                    m_uiRenderer->renderFilledRectangle(
                            {textX-1, initTextY+textY-2},
                            {textX+1, initTextY+textY+FONT_SIZE_PX+2},
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
        if (textY < -m_textRenderer->getWindowHeight())
        {
            return;
        }

        auto dimensions = m_textRenderer->renderChar(c, {textX, textY});

        // Draw cursor
        if (lineI == m_cursorLine && colI == m_cursorCol)
        {
#if CURSOR_DRAW_BLOCK
            m_uiRenderer->renderRectangleOutline(
                    {textX-2, initTextY+textY-2},
                    {textX+dimensions.advance/64.0f+2, initTextY+textY+FONT_SIZE_PX+2},
                    {1.0f, 0.0f, 0.0f},
                    2
            );
            m_uiRenderer->renderFilledRectangle(
                    {textX-2, initTextY+textY-2},
                    {textX+dimensions.advance/64.0f+2, initTextY+textY+FONT_SIZE_PX+2},
                    {1.0f, 0.0f, 0.0f, 0.4f}
            );
#else
            m_uiRenderer->renderFilledRectangle(
                    {textX-1, initTextY+textY-2},
                    {textX+1, initTextY+textY+FONT_SIZE_PX+2},
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
}
