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

void Buffer::render()
{
    // Draw line number background
    m_uiRenderer->renderFilledRectangle(m_position, {FONT_SIZE_PX*LINEN_BAR_WIDTH, m_textRenderer->getWindowHeight()}, LINEN_BG_COLOR);

    const float initTextX = m_position.x+FONT_SIZE_PX*LINEN_BAR_WIDTH;
    const float initTextY = m_position.y-FONT_SIZE_PX;
    float textX = initTextX;
    float textY = initTextY;

    m_textRenderer->prepareForDrawing();
    m_textRenderer->setDrawingColor({1.0f, 1.0f, 1.0f});

    char isLineBeginning = true;
    size_t lineI{1};
    for (char c : m_content)
    {
        if (BUFFER_DRAW_LINE_NUMS && isLineBeginning)
        {
            m_textRenderer->setDrawingColor(LINEN_FONT_COLOR);

            float digitX = m_position.x;
            for (char digit : std::to_string(lineI))
            {
                auto dimensions = m_textRenderer->renderChar(digit, {digitX, textY}, FontStyle::Italic);
                digitX += dimensions.advance/64.0f;
            }
            ++lineI;

            // Reset color
            m_textRenderer->setDrawingColor({1.0f, 1.0f, 1.0f});
        }

        switch (c)
        {
        case '\n': // New line
            textX = initTextX;
            textY -= FONT_SIZE_PX;
            isLineBeginning = true;
            continue;

        case '\r': // Carriage return
            textX = initTextX;
            continue;

        case '\t': // Tab
            textX += FONT_SIZE_PX*4;
            continue;

        case '\v': // Vertical tab
            textX = initTextX;
            textY -= FONT_SIZE_PX * 4;
            continue;
        }

        if (BUFFER_WRAP_LINES && textX+FONT_SIZE_PX > m_textRenderer->getWindowWidth())
        {
            textX = initTextX;
            textY -= FONT_SIZE_PX;
        }
        if (textY < -m_textRenderer->getWindowHeight())
        {
            return;
        }

        auto dimsensions = m_textRenderer->renderChar(c, {textX, textY});
        textX += (dimsensions.advance/64.0f);

        isLineBeginning = false;
    }
}
