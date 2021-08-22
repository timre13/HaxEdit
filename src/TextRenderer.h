#pragma once

#include "types.h"
#include "Shader.h"
#include <string>
#include <map>
#include <glm/glm.hpp>

enum class FontStyle
{
    Regular,
    Bold,
    Italic,
    BoldItalic,
};

inline std::string fontStyleToStr(FontStyle style)
{
    switch (style)
    {
    case FontStyle::Regular: return "Regular";
    case FontStyle::Bold: return "Bold";
    case FontStyle::Italic: return "Oblique";
    case FontStyle::BoldItalic: return "Bold Oblique";
    }
}

class TextRenderer
{
public:
    struct Glyph
    {
        uint textureId;
        glm::ivec2 dimensions;
        glm::ivec2 bearing;
        uint advance;
    };

private:
    std::map<char, Glyph> m_regularGlyphs;
    std::map<char, Glyph> m_boldGlyphs;
    std::map<char, Glyph> m_italicGlyphs;
    std::map<char, Glyph> m_boldItalicGlyphs;
    uint m_fontVao;
    uint m_fontVbo;

    Shader m_glyphShader;

    int m_windowWidth;
    int m_windowHeight;

public:
    TextRenderer(
            const std::string& regularFontPath,
            const std::string& boldFontPath,
            const std::string& italicFontPath,
            const std::string& boldItalicFontPath);

    inline void onWindowResized(int width, int height) { m_windowWidth = width; m_windowHeight = height; }
    void renderString(
            const std::string& str,
            const glm::ivec2& position,
            FontStyle style=FontStyle::Regular,
            const RGBColor& color={1.0f, 1.0f, 1.0f},
            bool shouldWrap=true,
            bool shouldDrawLineNums=false
    );

    ~TextRenderer();
};
