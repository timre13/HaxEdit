#pragma once

#include "common.h"
#include "Shader.h"
#include <string>
#include <map>
#include <glm/glm.hpp>

class FontRenderer
{
private:
    struct Glyph
    {
        uint textureId;
        glm::ivec2 dimensions;
        glm::ivec2 bearing;
        uint advance;
    };
    std::map<char, Glyph> m_glyphs;
    uint m_fontVao;
    uint m_fontVbo;

    Shader m_glyphShader;

    int m_windowWidth;
    int m_windowHeight;

public:
    FontRenderer(const std::string& fontPath);

    inline void onWindowResized(int width, int height) { m_windowWidth = width; m_windowHeight = height; }
    void renderString(const std::string& str);

    ~FontRenderer();
};
