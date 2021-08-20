#include "FontRenderer.h"
#include "Logger.h"
#include <GL/glew.h>
#include <GL/gl.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define FONT_SIZE_PX 18

static void loadGlyphs(FT_Library* library, std::map<char, FontRenderer::Glyph>* glyphs, const std::string& fontPath)
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
    for (int c{}; c < 256; ++c)
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

FontRenderer::FontRenderer(
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

    Logger::dbg << "Font renderer setup done" << Logger::End;
}

void FontRenderer::renderString(
        const std::string& str,
        FontStyle style/*=FontStyle::Regular*/,
        const RGBColor& color/*={1.0f, 1.0f, 1.0f}*/,
        bool shouldWrap/*=true*/)
{
    static constexpr float scale = 1.0f;
    std::map<char, Glyph>* glyphs{};
    switch (style)
    {
    case FontStyle::Regular: glyphs = &m_regularGlyphs; break;
    case FontStyle::Bold: glyphs = &m_boldGlyphs; break;
    case FontStyle::Italic: glyphs = &m_italicGlyphs; break;
    case FontStyle::BoldItalic: glyphs = &m_boldItalicGlyphs; break;
    }

    float textX = 0;
    float textY = -FONT_SIZE_PX*scale;

    m_glyphShader.use();

    glUniform3f(glGetUniformLocation(m_glyphShader.getId(), "textColor"), UNPACK_RGB_COLOR(color));

    const auto matrix = glm::ortho(0.0f, (float)m_windowWidth, (float)m_windowHeight, 0.0f);
    glUniformMatrix4fv(
            glGetUniformLocation(m_glyphShader.getId(), "projectionMat"),
            1,
            GL_FALSE,
            glm::value_ptr(matrix));

    glActiveTexture(GL_TEXTURE0);

    glBindVertexArray(m_fontVao);

    for (char c : str)
    {
        switch (c)
        {
        case '\n': // New line
            textX = 0;
            textY -= FONT_SIZE_PX * scale;
            continue;

        case '\r': // Carriage return
            textX = 0;
            continue;

        case '\t': // Tab
            textX += FONT_SIZE_PX*4;
            continue;

        case '\v': // Vertical tab
            textX = 0;
            textY -= FONT_SIZE_PX * scale * 4;
            continue;
        }

        if (shouldWrap && textX+FONT_SIZE_PX > m_windowWidth)
        {
            textX = 0;
            textY -= FONT_SIZE_PX * scale;
        }
        if (textY < -m_windowHeight)
        {
            return;
        }

        const auto& glyph = glyphs->find(c)->second;
        const float charX = textX + glyph.bearing.x * scale;
        const float charY = textY - (glyph.dimensions.y - glyph.bearing.y) * scale;
        const float charW = glyph.dimensions.x * scale;
        const float charH = glyph.dimensions.y * scale;

        const float vertexData[6][4] = {
            {charX,         charY + charH, 0.0f, 0.0f},
            {charX,         charY,         0.0f, 1.0f},
            {charX + charW, charY,         1.0f, 1.0f},

            {charX,         charY + charH, 0.0f, 0.0f},
            {charX + charW, charY,         1.0f, 1.0f},
            {charX + charW, charY + charH, 1.0f, 0.0f},
        };

        glBindTexture(GL_TEXTURE_2D, glyph.textureId);


        glBindBuffer(GL_ARRAY_BUFFER, m_fontVbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertexData), vertexData);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glDrawArrays(GL_TRIANGLES, 0, 6);

        textX += (glyph.advance/64.0f) * scale;
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

FontRenderer::~FontRenderer()
{
    for (auto& glyph : m_regularGlyphs) { glDeleteTextures(1, &glyph.second.textureId); }
    for (auto& glyph : m_boldGlyphs) { glDeleteTextures(1, &glyph.second.textureId); }
    for (auto& glyph : m_italicGlyphs) { glDeleteTextures(1, &glyph.second.textureId); }
    for (auto& glyph : m_boldItalicGlyphs) { glDeleteTextures(1, &glyph.second.textureId); }
    Logger::dbg << "Cleaned up glyphs" << Logger::End;
}

