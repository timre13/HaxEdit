#include "Popup.h"
#include "../globals.h"
#include "../UiRenderer.h"
#include "../TextRenderer.h"
#include "../Timer.h"
#include "../Logger.h"
#include "../common/string.h"
#include "../markdown.h"
#include "../ThemeLoader.h"
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

static RGBColor getItemColorFromKind(lsCompletionItemKind kind)
{
    Syntax::SyntaxMarks mark{};
    switch (kind)
    {
    case lsCompletionItemKind::Text:
        mark = Syntax::SyntaxMarks::MARK_NONE;
        break;
    case lsCompletionItemKind::Method:
        mark = Syntax::SyntaxMarks::MARK_OPERATOR;
        break;
    case lsCompletionItemKind::Function:
        mark = Syntax::SyntaxMarks::MARK_OPERATOR;
        break;
    case lsCompletionItemKind::Constructor:
        mark = Syntax::SyntaxMarks::MARK_NONE;
        break;
    case lsCompletionItemKind::Field:
        mark = Syntax::SyntaxMarks::MARK_NONE;
        break;
    case lsCompletionItemKind::Variable:
        mark = Syntax::SyntaxMarks::MARK_NONE;
        break;
    case lsCompletionItemKind::Class:
        mark = Syntax::SyntaxMarks::MARK_TYPE;
        break;
    case lsCompletionItemKind::Interface:
        mark = Syntax::SyntaxMarks::MARK_NONE;
        break;
    case lsCompletionItemKind::Module:
        mark = Syntax::SyntaxMarks::MARK_TYPE;
        break;
    case lsCompletionItemKind::Property:
        mark = Syntax::SyntaxMarks::MARK_NONE;
        break;
    case lsCompletionItemKind::Unit:
        mark = Syntax::SyntaxMarks::MARK_TYPE;
        break;
    case lsCompletionItemKind::Value:
        mark = Syntax::SyntaxMarks::MARK_NUMBER;
        break;
    case lsCompletionItemKind::Enum:
        mark = Syntax::SyntaxMarks::MARK_TYPE;
        break;
    case lsCompletionItemKind::Keyword:
        mark = Syntax::SyntaxMarks::MARK_KEYWORD;
        break;
    case lsCompletionItemKind::Snippet:
        mark = Syntax::SyntaxMarks::MARK_STRING;
        break;
    case lsCompletionItemKind::Color:
        mark = Syntax::SyntaxMarks::MARK_NONE;
        break;
    case lsCompletionItemKind::File:
        mark = Syntax::SyntaxMarks::MARK_FILEPATH;
        break;
    case lsCompletionItemKind::Reference:
        mark = Syntax::SyntaxMarks::MARK_NONE;
        break;
    case lsCompletionItemKind::Folder:
        mark = Syntax::SyntaxMarks::MARK_FILEPATH;
        break;
    case lsCompletionItemKind::EnumMember:
        mark = Syntax::SyntaxMarks::MARK_NUMBER;
        break;
    case lsCompletionItemKind::Constant:
        mark = Syntax::SyntaxMarks::MARK_PREPRO;
        break;
    case lsCompletionItemKind::Struct:
        mark = Syntax::SyntaxMarks::MARK_TYPE;
        break;
    case lsCompletionItemKind::Event:
        mark = Syntax::SyntaxMarks::MARK_NONE;
        break;
    case lsCompletionItemKind::Operator:
        mark = Syntax::SyntaxMarks::MARK_OPERATOR;
        break;
    case lsCompletionItemKind::TypeParameter:
        mark = Syntax::SyntaxMarks::MARK_TYPE;
        break;
    }

    return g_theme->values[mark].color;
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

        const auto& item = m_filteredItems[i];

        const bool isSnippet = item->kind.get_value_or(
                lsCompletionItemKind::Text) == lsCompletionItemKind::Snippet;
        // FIXME: Take into account when calculating popup width (or just truncate)
        std::string fullLabel = item->label;
        if (item->labelDetails && (item->labelDetails->detail || item->labelDetails->description))
        {
            fullLabel += "\033[90m";
            if (item->labelDetails->detail)
                fullLabel += item->labelDetails->detail.value();
            if (item->labelDetails->description)
                fullLabel += " | "+item->labelDetails->description.value();
        }
        g_textRenderer->renderString(
                utf8To32(fullLabel),
                {m_position.x, m_position.y+m_scrollByItems*g_fontSizePx+i*g_fontSizePx},
                (isSnippet ? FONT_STYLE_ITALIC : FONT_STYLE_REGULAR),
                getItemColorFromKind(item->kind.get_value_or(
                            lsCompletionItemKind::Text)));
    }

    m_docWin.render();

    TIMER_END_FUNC();
}

void Popup::updateDocWin()
{
    String itemTitle;
    String itemDoc;
    // Show detail in the first line
    if (m_selectedItemI != -1 && m_filteredItems[m_selectedItemI]->detail)
    {
        itemTitle = utf8To32(m_filteredItems[m_selectedItemI]->detail.get());
    }

    // Show documentation in the other lines
    if (m_selectedItemI != -1 && m_filteredItems[m_selectedItemI]->documentation)
    {
        const auto doc = m_filteredItems[m_selectedItemI]->documentation;
        if (doc->first)
        {
            itemDoc = utf8To32(doc->first.get());
        }
        else if (doc->second)
        {
            if (doc->second->kind == "markdown")
            {
                itemDoc = Markdown::markdownToAnsiEscaped(doc->second->value);
            }
            else
            {
                if (doc->second->kind != "plaintext")
                    Logger::warn << "MarkupContent with kind " << doc->second->kind
                        << " is not supported" << Logger::End;
                itemDoc = utf8To32(doc->second->value);
            }
        }
        else
        {
            assert(false);
        }
    }

    if (!itemDoc.empty() || !itemTitle.empty())
    {
        m_docWin.setContent(itemDoc);
        m_docWin.setTitle(itemTitle);
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

