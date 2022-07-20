#pragma once

#include <string>
#include <glm/vec2.hpp>
#include "globals.h"
#include "common/string.h"

class FloatingWindow final
{
private:
    glm::ivec2 m_pos{};

    std::string m_title;
    std::string m_content;

    bool m_isShown = false;

public:
    FloatingWindow(const glm::ivec2& pos, const std::string& title);
    FloatingWindow(const std::string& title);
    FloatingWindow();

    inline void setPos(const glm::ivec2& pos) { m_pos = pos; }
    inline void setTitle(const std::string& title) { m_title = title; }
    inline void setContent(const std::string& content) { m_content = content; }
    inline void show() { m_isShown = true; }
    inline void hide() { m_isShown = false; }
    inline void hideAndClear()
    {
        m_isShown = false;
        m_title.clear();
        m_content.clear();
    }

    inline uint calcWidth() const
    {
        return 2 + std::max(getLongestLineLen(m_content), m_title.length())*g_fontWidthPx + 2;
    }

    inline uint calcHeight() const
    {
        return 2 + ((m_title.empty() ? 0 : 1) + strCountLines(m_content)+1)*g_fontSizePx + 4 + 5;
    }

    void render();
};
