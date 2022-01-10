#pragma once

#include "types.h"
#include "Shader.h"
#include "globals.h"
#include <memory>
#include <string>
#include <map>
#include <glm/glm.hpp>

using FontStyle = int;
#define FONT_STYLE_REGULAR  (FontStyle)(0b00)
#define FONT_STYLE_BOLD     (FontStyle)(0b01)
#define FONT_STYLE_ITALIC   (FontStyle)(0b10)

inline std::string fontStyleToStr(FontStyle style)
{
    // Note: DO NOT CHANGE THE RETURN VALUES. They are used to query the font paths.
    switch (style)
    {
    case 0:                                 return "Regular";
    case FONT_STYLE_BOLD:                   return "Bold";
    case FONT_STYLE_ITALIC:                 return "Oblique";
    case FONT_STYLE_BOLD|FONT_STYLE_ITALIC: return "Bold Oblique";
    default: assert(false);
    }
    return ""; // Never reached
}

class TextRenderer
{
public:
    struct Glyph
    {
        uint textureId;
        glm::ivec2 size;
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
    std::string m_regularFontPath;
    std::string m_boldFontPath;
    std::string m_italicFontPath;
    std::string m_boldItalicFontPath;

    std::map<Char, Glyph> m_regularGlyphs;
    std::map<Char, Glyph> m_boldGlyphs;
    std::map<Char, Glyph> m_italicGlyphs;
    std::map<Char, Glyph> m_boldItalicGlyphs;
    uint m_fontVao;
    uint m_fontVbo;

    Shader m_glyphShader;

    RGBColor m_currentTextColor{1, 1, 1};

    // ---------------------------------------------
    friend class Buffer;
    // Buffer drawing functions
    // Step 1
    void prepareForDrawing();
    // Step 2
    void setDrawingColor(const RGBColor& color);
    // Step 3
    GlyphDimensions renderChar(
            Char c,
            const glm::ivec2& position,
            FontStyle style=FONT_STYLE_REGULAR
    );
    // ---------------------------------------------

    std::map<Char, Glyph>* getGlyphListFromStyle(FontStyle style);

    void cleanUpGlyphs();

public:
    TextRenderer(
            const std::string& regularFontPath,
            const std::string& boldFontPath,
            const std::string& italicFontPath,
            const std::string& boldItalicFontPath);

    void setFontSize(int size);

    /*
     * Render a string to the window.
     *
     * Some ANSI escape sequences are supported to change the current style and color.
     * The settings are not preserved across function calls.
     * If a wrong sequence is received, the behaviour is undefined.
     * Supported ANSI escape sequences:
     *  + 4-bit foreground color modifiers (3?, 9?)
     *
     * Args:
     *  + str: The string to render. It can contain ANSI escape sequences.
     *  + position: Textbox position.
     *  + initStyle: The initial style, can be changed with ANSI sequences.
     *  + initColor: The initial foreground color, can be changed with ANSI sequences.
     *  + shouldWrap: If true, a new line is started when reaching the end of the screen.
     */
    void renderString(
            const std::string& str,
            const glm::ivec2& position,
            FontStyle initStyle=FONT_STYLE_REGULAR,
            const RGBColor& initColor={1.0f, 1.0f, 1.0f},
            bool shouldWrap=false
    );

    ~TextRenderer();
};

extern std::unique_ptr<TextRenderer> g_textRenderer;
