#include "TextRenderer.h"
#include "Logger.h"
#include "config.h"
#include <GL/glew.h>
#include <GL/gl.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

static void loadGlyphs(FT_Library* library, std::map<char, TextRenderer::Glyph>* glyphs, const std::string& fontPath)
{
    Logger::dbg << "Loading font: " << fontPath << Logger::End;
    FT_Face face;
    if (FT_Error error = FT_New_Face(*library, fontPath.c_str(), 0, &face))
    {
        Logger::fatal << "Failed to load font: " << fontPath << ": " << FT_Error_String(error)
            << Logger::End;
    }
    Logger::dbg << "Loaded font: " << fontPath
        << "\n\tFamily: " << face->family_name
        << "\n\tStyle: " << face->style_name
        << "\n\tGlyphs: " << face->num_glyphs
        << Logger::End;

    if (FT_Error error = FT_Set_Pixel_Sizes(face, FONT_SIZE_PX, 0))
    {
        Logger::fatal << "Failed to set font size: " << FT_Error_String(error) << Logger::End;
    }

    Logger::dbg << "Caching glyphs" << Logger::End;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for (int c{}; c < 128; ++c)
    {
        //Logger::dbg << "Caching: " << c << Logger::End;

        if (FT_Error error = FT_Load_Char(face, (char)c, FT_LOAD_RENDER))
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
    FT_Done_Face(face);
}

TextRenderer::TextRenderer(
        const std::string& regularFontPath,
        const std::string& boldFontPath,
        const std::string& italicFontPath,
        const std::string& boldItalicFontPath)
    : m_glyphShader{"../shaders/glyph.vert.glsl", "../shaders/glyph.frag.glsl"}
{
    auto fontPath = regularFontPath;

    Logger::dbg << "Initializing FreeType" << Logger::End;
    FT_Library library;
    if (FT_Error error = FT_Init_FreeType(&library))
    {
        Logger::fatal << "Failed to initialize FreeType: " << FT_Error_String(error)
            << Logger::End;
    }

    loadGlyphs(&library, &m_regularGlyphs,    regularFontPath);
    loadGlyphs(&library, &m_boldGlyphs,       boldFontPath);
    loadGlyphs(&library, &m_italicGlyphs,     italicFontPath);
    loadGlyphs(&library, &m_boldItalicGlyphs, boldItalicFontPath);

    FT_Done_FreeType(library);


    glGenVertexArrays(1, &m_fontVao);
    glBindVertexArray(m_fontVao);

    glGenBuffers(1, &m_fontVbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_fontVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float)*6*4, 0, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    Logger::dbg << "Text renderer setup done" << Logger::End;
}

void TextRenderer::prepareForDrawing()
{
    m_glyphShader.use();

    const auto matrix = glm::ortho(0.0f, (float)m_windowWidth, (float)m_windowHeight, 0.0f);
    glUniformMatrix4fv(
            glGetUniformLocation(m_glyphShader.getId(), "projectionMat"),
            1,
            GL_FALSE,
            glm::value_ptr(matrix));

    glBindVertexArray(m_fontVao);
}

void TextRenderer::setDrawingColor(const RGBColor& color)
{
    glUniform3f(glGetUniformLocation(m_glyphShader.getId(), "textColor"), UNPACK_RGB_COLOR(color));
}

static inline TextRenderer::GlyphDimensions renderGlyph(
        const TextRenderer::Glyph& glyph,
        int textX, int textY, float scale,
        uint fontVbo)
{
    const float charX = textX + glyph.bearing.x * scale;
    const float charY = textY - (glyph.bearing.y) + FONT_SIZE_PX;
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

    return {glyph.size, glyph.size, glyph.advance};
}


std::map<char, TextRenderer::Glyph>* TextRenderer::getGlyphListFromStyle(FontStyle style)
{
    switch (style)
    {
    case FontStyle::Regular:    return &m_regularGlyphs;
    case FontStyle::Bold:       return &m_boldGlyphs;
    case FontStyle::Italic:     return &m_italicGlyphs;
    case FontStyle::BoldItalic: return &m_boldItalicGlyphs;
    }
}

TextRenderer::GlyphDimensions TextRenderer::renderChar(
        char c,
        const glm::ivec2& position,
        FontStyle style/*=FontStyle::Regular*/
    )
{
    auto glyphs = getGlyphListFromStyle(style);

    const auto& glyphIt = glyphs->find(c);
    if (glyphIt == glyphs->end())
        return {};

    return renderGlyph(glyphIt->second, position.x, position.y, 1.0f, m_fontVbo);
}

void TextRenderer::renderString(
        const std::string& str,
        const glm::ivec2& position,
        FontStyle style/*=FontStyle::Regular*/,
        const RGBColor& color/*={1.0f, 1.0f, 1.0f}*/,
        bool shouldWrap/*=false*/
    )
{
    static constexpr float scale = 1.0f;
    auto glyphs = getGlyphListFromStyle(style);

    const float initTextX = position.x;
    const float initTextY = position.y;
    float textX = initTextX;
    float textY = initTextY;

    prepareForDrawing();
    setDrawingColor(color);

    for (char c : str)
    {
        switch (c)
        {
        case '\n': // New line
            textX = initTextX;
            textY += FONT_SIZE_PX * scale;
            continue;

        case '\r': // Carriage return
            textX = initTextX;
            continue;

        case '\t': // Tab
            textX += FONT_SIZE_PX*4;
            continue;

        case '\v': // Vertical tab
            textX = initTextX;
            textY += FONT_SIZE_PX * scale * 4;
            continue;
        }

        if (shouldWrap && textX+FONT_SIZE_PX > m_windowWidth)
        {
            textX = initTextX;
            textY -= FONT_SIZE_PX * scale;
        }
        if (textY < -m_windowHeight)
        {
            return;
        }

        const auto& glyphIt = glyphs->find(c);
        if (glyphIt == glyphs->end())
        {
            continue; // No such glyph, skip :(
        }
        renderGlyph(glyphIt->second, textX, textY, scale, m_fontVbo);

        textX += (glyphIt->second.advance/64.0f) * scale;
    }
}

uint TextRenderer::getCharGlyphAdvance(char c, FontStyle style/*=FontStyle::Regular*/)
{
    auto glyphs = getGlyphListFromStyle(style);
    auto glyph = glyphs->find(c);
    return glyph == glyphs->end() ? 0 : glyph->second.advance;
}

TextRenderer::~TextRenderer()
{
    for (auto& glyph : m_regularGlyphs) { glDeleteTextures(1, &glyph.second.textureId); }
    for (auto& glyph : m_boldGlyphs) { glDeleteTextures(1, &glyph.second.textureId); }
    for (auto& glyph : m_italicGlyphs) { glDeleteTextures(1, &glyph.second.textureId); }
    for (auto& glyph : m_boldItalicGlyphs) { glDeleteTextures(1, &glyph.second.textureId); }
    Logger::dbg << "Cleaned up glyphs" << Logger::End;

    glDeleteBuffers(1, &m_fontVbo);
    glDeleteVertexArrays(1, &m_fontVao);
    Logger::dbg << "Cleaned up font vertex data" << Logger::End;
}

