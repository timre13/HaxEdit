#pragma once

#include <string>
#include <glm/vec2.hpp>
#include "globals.h"
#include "common/string.h"

class FloatingWindow
{
protected:
    glm::ivec2 m_pos{};

    std::string m_title;
    std::string m_content;

    bool m_isShown = false;

    glm::ivec2 calcPos() const;

public:
    FloatingWindow(const glm::ivec2& pos, const std::string& title);
    FloatingWindow(const std::string& title);
    FloatingWindow(const glm::ivec2 pos);
    FloatingWindow();

    virtual inline void setPos(const glm::ivec2& pos) { m_pos = pos; }
    virtual inline void setTitle(const std::string& title) { m_title = title; }
    virtual inline void setContent(const std::string& content) { m_content = content; }
    virtual inline void show() { m_isShown = true; }
    virtual inline void hide() { m_isShown = false; }
    virtual inline void hideAndClear()
    {
        m_isShown = false;
        m_title.clear();
        m_content.clear();
    }

    virtual inline uint calcWidth() const
    {
        return 2 + std::max(getLongestLineLen(m_content), m_title.length())*g_fontWidthPx + 2;
    }

    virtual inline uint calcHeight() const
    {
        return 2 + ((m_title.empty() ? 0 : 1) + strCountLines(m_content)+1)*g_fontSizePx + 4 + 5;
    }

    virtual inline void moveYBy(int amount) { m_pos.y += amount; }
    virtual inline void moveXBy(int amount) { m_pos.y += amount; }

    virtual void render() const;

    virtual ~FloatingWindow() {}
};
