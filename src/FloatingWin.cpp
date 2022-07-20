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

FloatingWindow::FloatingWindow()
{
}

void FloatingWindow::render()
{
    if (!m_isShown)
        return;

    const uint width = calcWidth();
    const uint height = calcHeight();

    g_uiRenderer->renderFilledRectangle(
            m_pos,
            m_pos + glm::ivec2{width, height},
            {0.5f, 0.5f, 0.5f, 0.8f}
    );

    if (!m_title.empty())
    {
        g_textRenderer->renderString(
                m_title,
                m_pos + glm::ivec2{2, 2},
                FONT_STYLE_BOLD
        );
    }

    if (!m_content.empty())
    {
        g_textRenderer->renderString(
                m_content,
                m_pos + glm::ivec2{2, 2+(m_title.empty() ? 0 : g_fontSizePx)+4}
        );
    }
}
