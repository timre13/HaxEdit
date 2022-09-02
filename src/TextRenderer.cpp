#include "TextRenderer.h"
#include "App.h"
#include "Logger.h"
#include "config.h"
#include "glstuff.h"
#include "globals.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

void TextRenderer::cleanUpGlyphs()
{
    for (auto& glyph : m_regularGlyphs) { glDeleteTextures(1, &glyph.second.textureId); }
    m_regularGlyphs.clear();
    for (auto& glyph : m_boldGlyphs) { glDeleteTextures(1, &glyph.second.textureId); }
    m_boldGlyphs.clear();
    for (auto& glyph : m_italicGlyphs) { glDeleteTextures(1, &glyph.second.textureId); }
    m_italicGlyphs.clear();
    for (auto& glyph : m_boldItalicGlyphs) { glDeleteTextures(1, &glyph.second.textureId); }
    m_boldItalicGlyphs.clear();

    Logger::dbg << "Cleaned up glyphs" << Logger::End;
}

static void loadGlyphs(
        FT_Library* library,
        std::map<Char, TextRenderer::Glyph>* glyphs,
        const std::string& fontPath,
        int fontSize)
{
    Logger::dbg << "Loading font: " << fontPath << Logger::End;
    FT_Face face;
    if (FT_Error error = FT_New_Face(*library, fontPath.c_str(), 0, &face))
    {
        Logger::fatal << "Failed to load font: " << fontPath << ": " << FT_Error_String(error)
            << Logger::End;
    }

    if (FT_Error error = FT_Set_Pixel_Sizes(face, fontSize, 0))
    {
        Logger::fatal << "Failed to set font size: " << FT_Error_String(error) << Logger::End;
    }

    Logger::dbg << "Caching glyphs" << Logger::End;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for (FT_ULong c{}; c < (FT_ULong)face->num_glyphs; ++c)
    {
        //Logger::dbg << "Caching: " << c << Logger::End;

        if (FT_Error error = FT_Load_Char(face, c, FT_LOAD_RENDER))
        {
            Logger::fatal << "Failed to load character: " << c << ": " << FT_Error_String(error)
                << Logger::End;
        }

        uint texId;
        glGenTextures(1, &texId);
        glBindTexture(GL_TEXTURE_2D, texId);
        glTexImage2D(GL_TEXTURE_2D,
                0, GL_RED,
                face->glyph->bitmap.width, face->glyph->bitmap.rows,
                0, GL_RED, GL_UNSIGNED_BYTE, face->glyph->bitmap.buffer);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glyphs->insert({c,
                {
                    texId,
                    {face->glyph->bitmap.width, face->glyph->bitmap.rows}, // Size
                    {face->glyph->bitmap_left, face->glyph->bitmap_top}, // Bearing
                    (uint)face->glyph->advance.x // Advance
                }
        });
    }

    const float width = glyphs->find('A')->second.advance/64.0f;
    Logger::dbg << "Loaded font: " << fontPath
        << "\n\tFamily: " << face->family_name
        << "\n\tStyle: " << face->style_name
        << "\n\tGlyphs: " << face->num_glyphs
        << "\n\tSize: " << g_fontSizePx
        << "\n\tWidth: " << width
        << Logger::End;
    FT_Done_Face(face);
}

TextRenderer::TextRenderer(
        const std::string& regularFontPath,
        const std::string& boldFontPath,
        const std::string& italicFontPath,
        const std::string& boldItalicFontPath)
    :
    m_regularFontPath{regularFontPath},
    m_boldFontPath{boldFontPath},
    m_italicFontPath{italicFontPath},
    m_boldItalicFontPath{boldItalicFontPath},
    m_glyphShader{App::getResPath("../shaders/glyph.vert.glsl"), App::getResPath("../shaders/glyph.frag.glsl")}
{
    setFontSize(DEF_FONT_SIZE_PX);

    glGenVertexArrays(1, &m_fontVao);
    glBindVertexArray(m_fontVao);

    glGenBuffers(1, &m_fontVbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_fontVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float)*6*4, 0, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    setDrawingColor({0.0f, 0.0f, 0.0f});

    Logger::dbg << "Text renderer setup done" << Logger::End;
}

void TextRenderer::setFontSize(int size)
{
    cleanUpGlyphs();

    Logger::dbg << "Initializing FreeType" << Logger::End;
    FT_Library library;
    if (FT_Error error = FT_Init_FreeType(&library))
    {
        Logger::fatal << "Failed to initialize FreeType: " << FT_Error_String(error) << Logger::End;
    }

    loadGlyphs(&library, &m_regularGlyphs,    m_regularFontPath, size);
    loadGlyphs(&library, &m_boldGlyphs,       m_boldFontPath, size);
    loadGlyphs(&library, &m_italicGlyphs,     m_italicFontPath, size);
    loadGlyphs(&library, &m_boldItalicGlyphs, m_boldItalicFontPath, size);

    FT_Done_FreeType(library);
    g_fontSizePx = size;
    g_fontWidthPx = m_regularGlyphs.find('A')->second.advance/64.0f;
}

void TextRenderer::prepareForDrawing()
{
    m_glyphShader.use();

    const auto matrix = glm::ortho(0.0f, (float)g_windowWidth, (float)g_windowHeight, 0.0f);
    glUniformMatrix4fv(
            glGetUniformLocation(m_glyphShader.getId(), "projectionMat"),
            1,
            GL_FALSE,
            glm::value_ptr(matrix));

    glBindVertexArray(m_fontVao);
}

void TextRenderer::setDrawingColor(const RGBColor& color)
{
    // TODO: Support alpha

    // Don't set the color if it is already set
    if (color.r == m_currentTextColor.r
     && color.g == m_currentTextColor.g
     && color.b == m_currentTextColor.b)
    {
        return;
    }
    m_currentTextColor = color;

    glUniform3f(glGetUniformLocation(m_glyphShader.getId(), "textColor"), UNPACK_RGB_COLOR(color));
}

static inline TextRenderer::GlyphDimensions renderGlyph(
        const TextRenderer::Glyph& glyph,
        int textX, int textY, float scale,
        uint fontVbo)
{
    const float charX = textX + glyph.bearing.x * scale;
    const float charY = textY - (glyph.bearing.y) + g_fontSizePx;
    const float charW = glyph.size.x * scale;
    const float charH = glyph.size.y * scale;

    const float vertexData[6][4] = {
        {charX,         charY + charH, 0.0f, 0.0f},
        {charX,         charY,         0.0f, 1.0f},
        {charX + charW, charY,         1.0f, 1.0f},

        {charX,         charY + charH, 0.0f, 0.0f},
        {charX + charW, charY,         1.0f, 1.0f},
        {charX + charW, charY + charH, 1.0f, 0.0f},
    };

    glBindTexture(GL_TEXTURE_2D, glyph.textureId);


    glBindBuffer(GL_ARRAY_BUFFER, fontVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertexData), vertexData);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    return {glyph.advance};
}


std::map<Char, TextRenderer::Glyph>* TextRenderer::getGlyphListFromStyle(FontStyle style)
{
    switch (style)
    {
    case FONT_STYLE_REGULAR:                return &m_regularGlyphs;
    case FONT_STYLE_BOLD:                   return &m_boldGlyphs;
    case FONT_STYLE_ITALIC:                 return &m_italicGlyphs;
    case FONT_STYLE_BOLD|FONT_STYLE_ITALIC: return &m_boldItalicGlyphs;
    default: assert(false);
    }
    return {}; // Never reached
}

TextRenderer::GlyphDimensions TextRenderer::renderChar(
        Char c,
        const glm::ivec2& position,
        FontStyle style/*=FONT_STYLE_REGULAR*/
    )
{
    auto glyphs = getGlyphListFromStyle(style);
    const auto& glyphIt = glyphs->find(c);
    return renderGlyph(
            glyphIt == glyphs->end() ? glyphs->find(0)->second : glyphIt->second,
            position.x, position.y, 1.0f, m_fontVbo);
}

#define ANSI_ESC_CHAR_0                 '\033'
#define ANSI_ESC_CHAR_1                 '['
#define ANSI_ESC_END_CHAR               'm'
#define ANSI_ESC_RESET_CHAR             '0'
#define ANSI_ESC_BOLD_CHAR              '1'
#define ANSI_ESC_FG_OR_ITALIC_CHAR      '3'
#define ANSI_ESC_BRFG_CHAR              '9'
#define ANSI_ESC_NOT_CHAR_0             '2'
#define ANSI_ESC_NOT_BOLD_CHAR_1_VAR1   '1'
#define ANSI_ESC_NOT_BOLD_CHAR_1_VAR2   '2'
#define ANSI_ESC_NOT_ITALIC_CHAR_1      '3'

namespace AnsiColors
{

// Note: These are XTerm's default colors

static constexpr RGBColor fgColors[8]{
    {  0/255.f,   0/255.f,   0/255.f}, // Black
    {205/255.f,   0/255.f,   0/255.f}, // Red
    {  0/255.f, 205/255.f,   0/255.f}, // Green
    {205/255.f, 205/255.f,   0/255.f}, // Yellow
    {  0/255.f,   0/255.f, 238/255.f}, // Blue
    {205/255.f,   0/255.f, 205/255.f}, // Magenta
    {  0/255.f, 205/255.f, 205/255.f}, // Cyan
    {229/255.f, 229/255.f, 229/255.f}, // White
};

static constexpr RGBColor brightFgColors[8]{
    {127/255.f, 127/255.f, 127/255.f}, // Bright black (gray)
    {255/255.f,   0/255.f,   0/255.f}, // Bright red
    {  0/255.f, 255/255.f,   0/255.f}, // Bright green
    {255/255.f, 255/255.f,   0/255.f}, // Bright yellow
    { 92/255.f,  92/255.f, 255/255.f}, // Bright blue
    {255/255.f,   0/255.f, 255/255.f}, // Bright magenta
    {  0/255.f, 255/255.f,   0/255.f}, // Bright cyan
    {255/255.f, 255/255.f, 255/255.f}, // Bright white
};

}

std::pair<glm::ivec2, glm::ivec2> TextRenderer::renderString(
        const std::string& str,
        const glm::ivec2& position,
        FontStyle initStyle/*=FONT_STYLE_REGULAR*/,
        const RGBColor& initColor/*={1.0f, 1.0f, 1.0f}*/,
        bool shouldWrap/*=false*/,
        bool onlyMeasure/*=false*/
    )
{
    static constexpr float scale = 1.0f;
    FontStyle currStyle = initStyle;
    std::map<Char, Glyph>* glyphs;
    std::map<Char, Glyph>::iterator glyphUnkIt;

    auto setStyle{[&currStyle, &glyphs, &glyphUnkIt, this](FontStyle style){
        currStyle = style;
        glyphs = getGlyphListFromStyle(currStyle);
        glyphUnkIt = glyphs->find(0);
        assert(glyphUnkIt != glyphs->end());
    }};

    std::pair<glm::ivec2, glm::ivec2> area;
    area.first = position;
    area.second = position;

    const float initTextX = position.x;
    const float initTextY = position.y;
    float textX = initTextX;
    float textY = initTextY;

    if (!onlyMeasure)
    {
        prepareForDrawing();
        setDrawingColor(initColor);
    }
    setStyle(initStyle);

    std::string ansiSeq;
    for (char c : str)
    {
        switch (c)
        {
        case '\n': // New line
            textX = initTextX;
            textY += g_fontSizePx * scale;
            continue;

        case '\r': // Carriage return
            textX = initTextX;
            continue;

        case '\t': // Tab
            textX += g_fontSizePx*4;
            area.second.x = std::max(area.second.x, (int)std::ceil(textX));
            continue;

        case '\v': // Vertical tab
            textX = initTextX;
            textY += g_fontSizePx * scale * 4;
            continue;

        case ANSI_ESC_CHAR_0: // Escape seqence introducer 1
            ansiSeq += c;
            continue;

        case ANSI_ESC_END_CHAR: // End of escape sequence
            if (!ansiSeq.empty())
            {
#if 0
                Logger::dbg << "ANSI sequence: ";
                for (char k : ansiSeq)
                    Logger::dbg << +k << '/' << k << " | ";
                Logger::dbg << +c << '/' << c << Logger::End;
#endif

                assert(ansiSeq[1] == ANSI_ESC_CHAR_1); // Check the second escape introducer character

                assert(ansiSeq.size() >= 3);
                switch (ansiSeq[2])
                {
                case ANSI_ESC_FG_OR_ITALIC_CHAR: // Italic or foregound color
                    if (ansiSeq.size() == 4) // Foregound seqence identifier
                    {
                        assert(ansiSeq[3] >= '0' && ansiSeq[3] <= '7');
                        if (!onlyMeasure)
                            setDrawingColor(AnsiColors::fgColors[ansiSeq[3]-'0']);
                    }
                    else if (ansiSeq.size() == 3) // Italic style sequence
                    {
                        setStyle(currStyle|FONT_STYLE_ITALIC);
                    }
                    else
                    {
                        assert(false);
                    }
                    break;

                case ANSI_ESC_BRFG_CHAR: // Bright foregound seqence identifier
                    assert(ansiSeq.size() == 4);
                    assert(ansiSeq[3] >= '0' && ansiSeq[3] <= '7');
                    if (!onlyMeasure)
                        setDrawingColor(AnsiColors::brightFgColors[ansiSeq[3]-'0']);
                    break;

                case ANSI_ESC_BOLD_CHAR: // Bold style sequence
                    setStyle(currStyle|FONT_STYLE_BOLD);
                    break;

                case ANSI_ESC_RESET_CHAR: // Reset sequence identifier
                    if (!onlyMeasure)
                        setDrawingColor(initColor); // Reset color
                    setStyle(initStyle); // Reset style
                    break;

                case ANSI_ESC_NOT_CHAR_0: // Not X (disables an attribute)
                    assert(ansiSeq.size() == 4);
                    switch (ansiSeq[3])
                    {
                    case ANSI_ESC_NOT_BOLD_CHAR_1_VAR1: // Not bold
                    case ANSI_ESC_NOT_BOLD_CHAR_1_VAR2:
                        setStyle(currStyle & (~FONT_STYLE_BOLD));
                        break;

                    case ANSI_ESC_NOT_ITALIC_CHAR_1: // Not italic
                        setStyle(currStyle & (~FONT_STYLE_ITALIC));
                        break;

                    default: // Invalid 2X code
                        assert(false);
                    }
                    break;

                default: // Invalid escape sequence
                    assert(false);
                }
                ansiSeq.clear();
                continue;
            }
            break;

        default:
            if (!ansiSeq.empty())
            {
                ansiSeq += c;
                continue;
            }
            break;
        }

        if (shouldWrap && textX+g_fontSizePx > g_windowWidth)
        {
            textX = initTextX;
            textY += g_fontSizePx * scale;
        }
        if (textY > g_windowHeight)
        {
            area.second.y = std::ceil(textY+g_fontSizePx*scale);
            return area;
        }

        auto glyphIt = glyphs->find(c);
        if (glyphIt == glyphs->end()) // Glyph not found
            glyphIt = glyphUnkIt; // Use the unknown glyph glyph
        if (!onlyMeasure)
            renderGlyph(glyphIt->second, textX, textY, scale, m_fontVbo);
        textX += (glyphIt->second.advance/64.0f) * scale;
        area.second.x = std::max(area.second.x, (int)std::ceil(textX));
    }

    area.second.y = std::ceil(textY+g_fontSizePx*scale);
    return area;
}

TextRenderer::~TextRenderer()
{
    cleanUpGlyphs();
    glDeleteBuffers(1, &m_fontVbo);
    glDeleteVertexArrays(1, &m_fontVao);
    Logger::dbg << "Cleaned up font vertex data" << Logger::End;
}
