#include "Popup.h"
#include "../globals.h"
#include "../UiRenderer.h"
#include "../TextRenderer.h"
#include "../Timer.h"
#include "../Logger.h"
#include <algorithm>

namespace Autocomp
{

void Popup::recalcSize()
{
    size_t maxItemValLen = 0;
    for (const auto& item : m_items)
    {
        if (item->value.length() > maxItemValLen)
            maxItemValLen = item->value.length();
    }
    m_size.x = maxItemValLen*g_fontSizePx*0.75f;
    m_size.y = std::min(m_items.size()*g_fontSizePx, 800_st);
}

void Popup::render()
{
    if (!isRendered())
        return;

    TIMER_BEGIN_FUNC();

    recalcSize();

    if (m_isItemSortingNeeded)
    {
        std::sort(m_items.begin(), m_items.end(),
                [](const std::unique_ptr<Item>& a, const std::unique_ptr<Item>& b) {
                    auto la = strToLower(a->value);
                    auto lb = strToLower(b->value);
                    // Generally case is ignored
                    if (la < lb)
                        return true;
                    // If the two strings only differ in the case, don't ignore it
                    else if (la == lb)
                        return a->value < b->value;
                    else
                        return false;
                }
        );
        m_isItemSortingNeeded = false;
    }

    g_uiRenderer->renderRectangleOutline(
            {m_position.x-1, m_position.y-1},
            {m_position.x+m_size.x+1, m_position.y+m_size.y+1},
            {0.5f, 0.5f, 0.5f},
            2);
    g_uiRenderer->renderFilledRectangle(
            m_position, {m_position.x+m_size.x, m_position.y+m_size.y},
            {0.0f, 0.0f, 0.0f});

    for (int i{}; i < (int)m_items.size(); ++i)
    {
        // Don't render if not visible yet
        if (m_position.y+m_scrollByItems*g_fontSizePx+(i+1)*g_fontSizePx < 0
         || (i+1)*g_fontSizePx+m_scrollByItems*g_fontSizePx < g_fontSizePx)
            continue;

        // Stop if out of screen or popup
        if (m_position.y+m_scrollByItems*g_fontSizePx+(i+1)*g_fontSizePx > g_windowHeight
         || (i+1)*g_fontSizePx+m_scrollByItems*g_fontSizePx > m_size.y)
            break;

        if (i == m_selectedItemI)
        {
            g_uiRenderer->renderFilledRectangle(
                    {m_position.x,          m_position.y+m_scrollByItems*g_fontSizePx+i*g_fontSizePx},
                    {m_position.x+m_size.x, m_position.y+m_scrollByItems*g_fontSizePx+(i+1)*g_fontSizePx},
                    {0.2f, 0.2f, 0.2f});
        }

        // TODO: Support Unicode
        g_textRenderer->renderString(
                strToAscii(m_items[i]->value),
                {m_position.x, m_position.y+m_scrollByItems*g_fontSizePx+i*g_fontSizePx},
                FontStyle::Regular,
                {0.400f, 0.851f, 0.937f});
    }

    TIMER_END_FUNC();
}

}

