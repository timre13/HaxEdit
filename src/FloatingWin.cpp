#include "FloatingWin.h"
#include "TextRenderer.h"
#include "UiRenderer.h"

FloatingWindow::FloatingWindow(const glm::ivec2& pos, const std::string& title)
    : m_pos{pos}, m_title{title}
{
}

FloatingWindow::FloatingWindow(const std::string& title)
    : m_title{title}
{
}

FloatingWindow::FloatingWindow(const glm::ivec2 pos)
    : m_pos{pos}
{
}

FloatingWindow::FloatingWindow()
{
}

glm::ivec2 FloatingWindow::calcPos() const
{
    const int width = calcWidth();
    const int height = calcHeight();

    auto pos = m_pos;
    if (pos.x+width > g_windowWidth)
        pos.x = g_windowWidth-width;
    if (pos.y+height > g_windowHeight)
        pos.y = g_windowHeight-height;
    if (pos.x < 0)
        pos.x = 0;
    if (pos.y < 0)
        pos.y = 0;
    return pos;
}

void FloatingWindow::render() const
{
    if (!m_isShown)
        return;

    const int width = calcWidth();
    const int height = calcHeight();
    const glm::ivec2 pos = calcPos();

    g_uiRenderer->renderFilledRectangle(
            pos,
            pos + glm::ivec2{width, height},
            {0.5f, 0.5f, 0.5f, 0.8f}
    );

    if (!m_title.empty())
    {
        g_textRenderer->renderString(
                m_title,
                pos + glm::ivec2{2, 2},
                FONT_STYLE_BOLD
        );
    }

    if (!m_content.empty())
    {
        g_textRenderer->renderString(
                m_content,
                pos + glm::ivec2{2, 2+(m_title.empty() ? 0 : g_fontSizePx)+4}
        );
    }
}
