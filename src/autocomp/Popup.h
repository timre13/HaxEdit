#pragma once

#include "../types.h"
#include "../globals.h"
#include <vector>
#include <memory>
#include <algorithm>
#include <glm/glm.hpp>

namespace Autocomp
{

class Popup final
{
public:
    class Item
    {
    public:
        enum class Type
        {
            Misc,
            DictionaryWord,
            BufferWord,
            Path,
        } type;

        String value;

        friend inline bool operator==(const Item& a, const Item& b)
        {
            return a.type == b.type && a.value == b.value;
        }

        std::string getTypeAsStr()
        {
            switch (type)
            {
            case Type::Misc:                return "Misc";
            case Type::DictionaryWord:      return "Dict";
            case Type::BufferWord:          return "Buff";
            case Type::Path:                return "Path";
            }
        }
    };

private:
    bool m_isEnabled = false;

    glm::ivec2 m_position{};
    glm::ivec2 m_size{};
    int m_scrollByItems{};

    std::vector<std::unique_ptr<Item>> m_items;
    bool m_isItemSortingNeeded{};
    int m_selectedItemI{};

    String m_filterBuffer;
    std::vector<Item*> m_filteredItems;
    bool m_isFilteringNeeded{};

    void recalcSize();
    void sortItems();
    void filterItems();

public:
    void render();

    inline void setVisibility(bool val)
    {
        m_isEnabled = val;
        m_selectedItemI = 0; // Reset selected item
        m_filterBuffer.clear();
        m_isFilteringNeeded = true;

        if (val)
        {
            if (m_isItemSortingNeeded)
                sortItems();
            if (m_isFilteringNeeded)
                filterItems();
        }
    }
    inline bool isEnabled() const { return m_isEnabled; }
    inline bool isRendered() const { return m_isEnabled && !m_filteredItems.empty(); }

    inline void addItem(Item item)
    {
        m_items.push_back(std::make_unique<Item>(item));
        m_isItemSortingNeeded = true;
        m_isFilteringNeeded = true;
    }

    inline void selectNextItem()
    {
        if (m_isItemSortingNeeded)
            sortItems();
        if (m_isFilteringNeeded)
            filterItems();

        ++m_selectedItemI;
        if (m_selectedItemI >= (int)m_filteredItems.size())
        {
            m_selectedItemI = m_filteredItems.size()-1;
        }

        recalcSize();
        // Scroll to reveal more items when reaching the bottom
        if ((m_selectedItemI+m_scrollByItems+1)*g_fontSizePx > m_size.y)
        {
            m_scrollByItems -= m_size.y/g_fontSizePx-5;
        }

        if (m_scrollByItems < -int(m_filteredItems.size()-m_size.y/g_fontSizePx))
        {
            m_scrollByItems = -int(m_filteredItems.size()-m_size.y/g_fontSizePx);
        }
    }

    inline void selectPrevItem()
    {
        if (m_isItemSortingNeeded)
            sortItems();
        if (m_isFilteringNeeded)
            filterItems();

        --m_selectedItemI;
        if (m_selectedItemI < 0)
        {
            m_selectedItemI = 0;
        }

        recalcSize();
        // Scroll to reveal more items when reaching the top
        if ((m_selectedItemI+m_scrollByItems)*g_fontSizePx < 0)
        {
            m_scrollByItems += m_size.y/g_fontSizePx+5;
        }

        if (m_scrollByItems > 0)
        {
            m_scrollByItems = 0;
        }
    }

    inline int getSelectedItemI() const { return m_selectedItemI; }

    inline const Item* getSelectedItem() const
    {
        if (m_selectedItemI < 0 || m_filteredItems.empty())
            return nullptr;
        return m_filteredItems[m_selectedItemI];
    }

    inline void setPos(const glm::vec2& pos)
    {
        m_position = pos;
    }

    inline void appendToFilter(Char c)
    {
        m_selectedItemI = 0;
        m_filterBuffer += c;
        m_isFilteringNeeded = true;
    }
    inline void removeLastCharFromFilter()
    {
        if (!m_filterBuffer.empty())
            m_filterBuffer.pop_back();
        m_selectedItemI = 0;
        m_isFilteringNeeded = true;
    }
    inline size_t getFilterLen()
    {
        return m_filterBuffer.length();
    }

    void clear();
};

}
