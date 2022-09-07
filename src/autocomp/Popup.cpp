#include "Popup.h"
#include "../globals.h"
#include "../UiRenderer.h"
#include "../TextRenderer.h"
#include "../Timer.h"
#include "../Logger.h"
#include "../common/string.h"
#include "../markdown.h"
#include <algorithm>

namespace Autocomp
{

void Popup::recalcSize()
{
    size_t maxItemValLen = 0;
    for (const auto& item : m_filteredItems)
    {
        if (item->label.length() > maxItemValLen)
            maxItemValLen = item->label.length();
    }
    m_size.x = maxItemValLen*g_fontWidthPx;
    m_size.y = std::min(m_filteredItems.size()*g_fontSizePx, 800_st);
}

void Popup::sortItems()
{
    std::sort(m_items.begin(), m_items.end(),
            [](const std::unique_ptr<Item>& a, const std::unique_ptr<Item>& b) {
                // Sort by `sortText` if exists, otherwise use `label`
                const auto& textA = a->sortText.get_value_or(a->label);
                const auto& textB = b->sortText.get_value_or(b->label);
                return textA < textB;
            }
    );

    // Remove duplicates
    //m_items.erase(std::unique(m_items.begin(), m_items.end(),
    //            [](const std::unique_ptr<Item>& a, const std::unique_ptr<Item>& b){
    //                return a->type == b->type && a->label == b->label;
    //            }),
    //        m_items.end());

    m_isItemSortingNeeded = false;
}

void Popup::filterItems()
{
    m_filteredItems.clear();
    for (size_t i{}; i < m_items.size(); ++i)
    {
        if (m_items[i]->label.rfind(utf32To8(m_filterBuffer), 0) == 0)
        {
            m_filteredItems.push_back(m_items[i].get());
        }
    }
    m_isFilteringNeeded = false;
}

void Popup::render()
{
    if (!m_isEnabled)
        return;

    TIMER_BEGIN_FUNC();

    if (m_isItemSortingNeeded)
        sortItems();
    if (m_isFilteringNeeded)
        filterItems();

    if (m_filteredItems.empty())
        return;

    recalcSize();

    g_uiRenderer->renderRectangleOutline(
            {m_position.x-1, m_position.y-1},
            {m_position.x+m_size.x+1, m_position.y+m_size.y+1},
            {0.5f, 0.5f, 0.5f},
            2);
    g_uiRenderer->renderFilledRectangle(
            m_position, {m_position.x+m_size.x, m_position.y+m_size.y},
            {0.0f, 0.0f, 0.0f});

    for (int i{}; i < (int)m_filteredItems.size(); ++i)
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
            const int y1 = m_position.y+m_scrollByItems*g_fontSizePx+i*g_fontSizePx;
            g_uiRenderer->renderFilledRectangle(
                    {m_position.x,          y1},
                    {m_position.x+m_size.x, m_position.y+m_scrollByItems*g_fontSizePx+(i+1)*g_fontSizePx},
                    {0.2f, 0.2f, 0.2f});
            m_docWin.setPos({m_position.x+m_size.x, y1-2});
        }

        g_textRenderer->renderString(
                utf8To32(m_filteredItems[i]->label),
                {m_position.x, m_position.y+m_scrollByItems*g_fontSizePx+i*g_fontSizePx},
                FONT_STYLE_REGULAR,
                {0.400f, 0.851f, 0.937f});
    }

    m_docWin.render();

    TIMER_END_FUNC();
}

void Popup::updateDocWin()
{
    String itemDoc;
    // Show detail in the first line
    if (m_filteredItems[m_selectedItemI]->detail)
    {
        itemDoc = utf8To32(m_filteredItems[m_selectedItemI]->detail.get());
    }

    // Show documentation in the other lines
    if (m_filteredItems[m_selectedItemI]->documentation)
    {
        const auto doc = m_filteredItems[m_selectedItemI]->documentation;
        if (doc->first)
        {
            itemDoc += U'\n'+utf8To32(doc->first.get());
        }
        else if (doc->second)
        {
            if (doc->second->kind == "markdown")
            {
                itemDoc += U'\n'+Markdown::markdownToAnsiEscaped(doc->second->value);
            }
            else
            {
                if (doc->second->kind != "plaintext")
                    Logger::warn << "MarkupContent with kind " << doc->second->kind
                        << " is not supported" << Logger::End;
                itemDoc += U'\n'+utf8To32(doc->second->value);
            }
        }
        else
        {
            assert(false);
        }
    }

    if (!itemDoc.empty())
    {
        m_docWin.setContent(itemDoc);
        m_docWin.show();
    }
    else
    {
        m_docWin.hideAndClear();
    }
}

void Popup::clear()
{
    m_items.clear();
    m_filteredItems.clear();
    m_filterBuffer.clear();
    m_scrollByItems = 0;
    m_isItemSortingNeeded = true;
    m_isFilteringNeeded = true;
}

}

