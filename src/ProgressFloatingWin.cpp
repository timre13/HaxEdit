#include "ProgressFloatingWin.h"

void ProgressFloatingWin::render() const
{
    if (!m_isShown)
        return;

    FloatingWindow::render();

    const int width = calcWidth();
    const int height = calcHeight();
    const glm::ivec2 pos = calcPos();

    if (m_perc.has_value())
    {
        const glm::ivec2 pos1 = pos + glm::ivec2{0, height-8};
        const glm::ivec2 pos2 = pos + glm::ivec2{width*(m_perc.value()/100.f), height};
        const float percVal = m_perc.value()/100.0f;
        const RGBAColor color = lerpColors<RGBAColor>({1.0f, 0.3f, 0.3f}, {0.2f, 1.0f, 0.3f}, percVal*percVal);
        g_uiRenderer->renderFilledRectangle(pos1, pos2, color);
    }
    else
    {
        //const float posVal = (std::sin(m_tick/20.f)+1.f)/2.f;
        float posVal = (m_tick%200);
        if (posVal > 100)
            posVal = 100-(posVal-100);
        posVal /= 100.f;

        const glm::ivec2 pos1 = pos + glm::ivec2{(width-30)*posVal, height-8};
        const glm::ivec2 pos2 = pos + glm::ivec2{(width-30)*posVal+30, height};
        const RGBAColor color = {0.2f, 0.3f, 1.0f, 1.0f};
        g_uiRenderer->renderFilledRectangle(pos1, pos2, color);
    }
}
