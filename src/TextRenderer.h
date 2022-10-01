#pragma once

#include "types.h"
#include "Shader.h"
#include "globals.h"
#include <memory>
#include <string>
#include <map>
#include <glm/glm.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H

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

class Face
{
public:
    struct Glyph
    {
        uint textureId{};
        glm::ivec2 size;
        glm::ivec2 bearing;
        uint advance{};
    };

    struct GlyphDimensions
    {
        uint advance{};
    };

private:
    FT_Face m_face{};
    std::vector<Glyph> m_glyphs;
    bool m_isLoading{};
    std::thread m_loaderThread;
    GLFWwindow* m_contextWin;

    void _load(FT_Library library, const std::string& path, int size);

public:
    Face()
    {
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        m_contextWin = glfwCreateWindow(100, 100, "", nullptr, g_window);
    }

    void load(FT_Library library, const std::string& path, int size, bool isRegularFont);
    inline bool isLoading() const { return m_isLoading; }

    void cleanUp();

    FT_UInt getGlyphIndex(FT_ULong charcode);
    Glyph* getGlyphByIndex(FT_UInt index);

    ~Face()
    {
        cleanUp();
    }
};

class TextRenderer
{
private:
    std::string m_regularFontPath;
    std::string m_boldFontPath;
    std::string m_italicFontPath;
    std::string m_boldItalicFontPath;
    // Fallback font
    std::string m_fbFontPath;

    FT_Library m_library{};
    std::unique_ptr<Face> m_regularFace = std::make_unique<Face>();
    std::unique_ptr<Face> m_boldFace = std::make_unique<Face>();
    std::unique_ptr<Face> m_italicFace = std::make_unique<Face>();
    std::unique_ptr<Face> m_boldItalicFace = std::make_unique<Face>();
    std::unique_ptr<Face> m_fallbackFace = std::make_unique<Face>();
    uint m_fontVao;
    uint m_fontVbo;

    Shader m_glyphShader;

    RGBColor m_currentTextColor{1, 1, 1};

    Face::Glyph* getGlyph(Face* face, FT_ULong charcode);

    // ---------------------------------------------
    friend class Buffer;
    // Buffer drawing functions
    // Step 1
    void prepareForDrawing();
    // Step 2
    void setDrawingColor(const RGBColor& color);
    // Step 3
    Face::GlyphDimensions renderChar(
            Char c,
            const glm::ivec2& position,
            FontStyle style=FONT_STYLE_REGULAR
    );
    // ---------------------------------------------

    Face* getFaceFromStyle(FontStyle style);

public:
    TextRenderer(
            const std::string& regularFontPath,
            const std::string& boldFontPath,
            const std::string& italicFontPath,
            const std::string& boldItalicFontPath,
            const std::string& fallbackFontPath);

    void setFontSize(int size);

    /*
     * Render a string to the window.
     *
     * Some ANSI escape sequences are supported to change the current style and color.
     * The settings are not preserved across function calls.
     * If a wrong sequence is received, the behaviour is undefined.
     * Supported ANSI escape sequences:
     *  + 4-bit foreground color modifiers (3?, 9?)
     *  + Reset (0)
     *  + Bold (1)
     *  + Italic (3)
     *  + Not bold (21/22)
     *  + Not italic (23)
     *
     * Args:
     *  + str: The string to render. It can contain ANSI escape sequences.
     *  + position: Textbox position.
     *  + initStyle: The initial style, can be changed with ANSI sequences.
     *  + initColor: The initial foreground color, can be changed with ANSI sequences.
     *  + shouldWrap: If true, a new line is started when reaching the end of the screen.
     *  + onlyMeasure: If true, nothing will be rendered, but the used area will be calculated as usual.
     *                 Useful to draw background for some text.
     *
     * Returns:
     *  + the enclosing area as {{x1, y1}, {x2, y2}}.
     */
    std::pair<glm::ivec2, glm::ivec2> renderString(
            const String& str,
            const glm::ivec2& position,
            FontStyle initStyle=FONT_STYLE_REGULAR,
            const RGBColor& initColor={1.0f, 1.0f, 1.0f},
            bool shouldWrap=false,
            bool onlyMeasure=false
    );

    ~TextRenderer();
};

extern std::unique_ptr<TextRenderer> g_textRenderer;
