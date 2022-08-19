#include "ProgressFloatingWin.h"

void ProgressFloatingWin::render() const
{
    if (!m_isShown)
        return;

    FloatingWindow::render();

    const int width = calcWidth();
    const int height = calcHeight();
    const glm::ivec2 pos = calcPos();

    const glm::ivec2 pos1 = pos + glm::ivec2{0, height-8};
    const glm::ivec2 pos2 = pos + glm::ivec2{width*(m_perc/100.f), height};
    const float percVal = m_perc/100.0f;
    const RGBAColor color = lerpColors<RGBAColor>({1.0f, 0.3f, 0.3f}, {0.2f, 1.0f, 0.3f}, percVal*percVal);
    g_uiRenderer->renderFilledRectangle(pos1, pos2, color);
}
