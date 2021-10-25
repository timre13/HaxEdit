#include "Popup.h"
#include "../globals.h"
#include "../UiRenderer.h"
#include "../TextRenderer.h"
#include <algorithm>

namespace Autocomp
{

void Popup::render()
{
    if (!isRendered())
        return;

    if (m_isItemSortingNeeded)
    {
        std::sort(m_items.begin(), m_items.end(),
                [](const std::unique_ptr<Item>& a, const std::unique_ptr<Item>& b) {
                    return a->value < b->value;
                }
        );
    }

    const int popupW = [&](){ // Note: Nasty lambda trick
        size_t maxItemValLen = 0;
        for (const auto& item : m_items)
        {
            if (item->value.length() > maxItemValLen)
                maxItemValLen = item->value.length();
        }
        return maxItemValLen;
    }()*g_fontSizePx*0.75f;
    const int popupH = std::min(m_items.size()*g_fontSizePx, 400_st);

    g_uiRenderer->renderRectangleOutline(
            {m_position.x-1, m_position.y-1},
            {m_position.x+popupW+1, m_position.y+popupH+1},
            {0.5f, 0.5f, 0.5f},
            2);
    g_uiRenderer->renderFilledRectangle(
            m_position, {m_position.x+popupW, m_position.y+popupH},
            {0.0f, 0.0f, 0.0f});

    for (int i{}; i < (int)m_items.size(); ++i)
    {
        if (i == m_selectedItemI)
        {
            g_uiRenderer->renderFilledRectangle(
                    {m_position.x, m_position.y+i*g_fontSizePx},
                    {m_position.x+popupW, m_position.y+(i+1)*g_fontSizePx},
                    {0.2f, 0.2f, 0.2f});
        }

        // TODO: Support Unicode
        g_textRenderer->renderString(
                strToAscii(m_items[i]->value),
                {m_position.x, m_position.y+i*g_fontSizePx},
                FontStyle::Regular,
                // Color stolen from "Molokai" VIM theme
                // (https://github.com/tomasr/molokai/blob/master/colors/molokai.vim#L69)
                {0.4f, 0.851f, 0.937f});
    }
}

}

