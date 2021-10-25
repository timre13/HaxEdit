#pragma once

#include "../types.h"
#include <vector>
#include <memory>
#include <glm/glm.hpp>

namespace Autocomp
{

class Popup final
{
public:
    struct Item
    {
        enum class Type
        {
            Misc,
            Word,
            Path,
        } type;

        String value;
    };

private:
    bool m_isVisible = false;
    bool m_isItemSortingNeeded{};

    std::vector<std::unique_ptr<Item>> m_items;
    int m_selectedItemI{};

    glm::ivec2 m_position{};

public:
    //Popup();

    void render();

    inline void setVisibility(bool val) { m_isVisible = val; }
    inline bool isVisible() const { return m_isVisible; }
    inline bool isRendered() const { return m_isVisible && !m_items.empty(); }

    inline void addItem(Item* item)
    {
        m_items.push_back(std::unique_ptr<Item>{item});
        m_isItemSortingNeeded = true;
    }

    inline void selectNextItem()
    {
        m_selectedItemI = std::min((int)m_items.size()-1, m_selectedItemI+1);
    }

    inline void selectPrevItem()
    {
        m_selectedItemI = std::max(0, m_selectedItemI-1);
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
