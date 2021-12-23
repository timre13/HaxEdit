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
            Word,
            Path,
        } type;

        String value;

        friend inline bool operator==(const Item& a, const Item& b)
        {
            return a.type == b.type && a.value == b.value;
        }
    };

private:
    bool m_isVisible = false;
    bool m_isItemSortingNeeded{};

    std::vector<std::unique_ptr<Item>> m_items;
    int m_selectedItemI{};

    glm::ivec2 m_position{};
    glm::ivec2 m_size{};
    int m_scrollByItems{};

    void recalcSize();

public:
    void render();

    inline void setVisibility(bool val) { m_isVisible = val; }
    inline bool isVisible() const { return m_isVisible; }
    inline bool isRendered() const { return m_isVisible && !m_items.empty(); }

    inline void addItem(Item item)
    {
        m_items.push_back(std::make_unique<Item>(item));
        m_isItemSortingNeeded = true;
    }

    inline void selectNextItem()
    {
        ++m_selectedItemI;
        if (m_selectedItemI >= (int)m_items.size())
        {
            m_selectedItemI = m_items.size()-1;
        }

        recalcSize();
        // Scroll to reveal more items when reaching the bottom
        if ((m_selectedItemI+m_scrollByItems+1)*g_fontSizePx > m_size.y)
        {
            m_scrollByItems -= m_size.y/g_fontSizePx-5;
        }

        if (m_scrollByItems < -int(m_items.size()-m_size.y/g_fontSizePx))
        {
            m_scrollByItems = -int(m_items.size()-m_size.y/g_fontSizePx);
        }
    }

    inline void selectPrevItem()
    {
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

    inline const Item* getSelectedItem() const
    {
        if (m_selectedItemI < 0)
            return nullptr;
        return m_items[m_selectedItemI].get();
    }

    inline void setPos(const glm::vec2& pos)
    {
        m_position = pos;
    }
};

}
