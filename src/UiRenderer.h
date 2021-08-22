#pragma once

#include "types.h"
#include "Shader.h"
#include <glm/glm.hpp>

class UiRenderer
{
private:
    uint m_vao;
    uint m_vbo;

    Shader m_shader;

    int m_windowWidth;
    int m_windowHeight;

public:
    UiRenderer();

    inline void onWindowResized(int width, int height)
        { m_windowWidth = width; m_windowHeight = height; }

    void renderRectangle(
        const glm::ivec2& position1,
        const glm::ivec2& position2,
        const RGBColor& fillColor/*={1.0f, 1.0f, 1.0f}*/);

    ~UiRenderer();
};
