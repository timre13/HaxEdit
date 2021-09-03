#pragma once

#include "types.h"
#include "Shader.h"
#include "Image.h"
#include <memory>
#include <glm/glm.hpp>

class Image;

class UiRenderer
{
private:
    uint m_vao;
    uint m_vbo;

    uint m_imgVao;
    uint m_imgVbo;

    Shader m_shader;
    Shader m_imgShader;

    int m_windowWidth;
    int m_windowHeight;

    friend class Image;
    // Used by Image::render()
    void renderImage(const Image* image);

public:
    UiRenderer();

    inline void onWindowResized(int width, int height)
    {
        m_windowWidth = width;
        m_windowHeight = height;
    }

    void renderFilledRectangle(
        const glm::ivec2& position1,
        const glm::ivec2& position2,
        const RGBAColor& fillColor
        );

    void renderRectangleOutline(
        const glm::ivec2& position1,
        const glm::ivec2& position2,
        const RGBAColor& color,
        uint borderThickness
        );

    ~UiRenderer();
};

extern std::unique_ptr<UiRenderer> g_uiRenderer;
