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

    struct GlyphDimensions
    {
        glm::ivec2 position;
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

    // ---------------------------------------------
    friend class Buffer;
    // Buffer drawing functions
    // Step 1
    void prepareForDrawing();
    // Step 2
    void setDrawingColor(const RGBColor& color);
    // Step 3
    GlyphDimensions renderChar(
            char c,
            const glm::ivec2& position,
            FontStyle style=FontStyle::Regular
    );
    // ---------------------------------------------

public:
    TextRenderer(
            const std::string& regularFontPath,
            const std::string& boldFontPath,
            const std::string& italicFontPath,
            const std::string& boldItalicFontPath);

    inline void onWindowResized(int width, int height)
    {
        m_windowWidth = width;
        m_windowHeight = height;
    }

    /*
     * Render a string to the window.
     */
    void renderString(
            const std::string& str,
            const glm::ivec2& position,
            FontStyle style=FontStyle::Regular,
            const RGBColor& color={1.0f, 1.0f, 1.0f},
            bool shouldWrap=false
    );

    inline int getWindowWidth() const { return m_windowWidth; }
    inline int getWindowHeight() const { return m_windowHeight; }

    ~TextRenderer();
};
