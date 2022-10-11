#include "TextRenderer.h"
#include "App.h"
#include "Logger.h"
#include "config.h"
#include "glstuff.h"
#include "globals.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

void Face::_load(
        FT_Library library,
        const std::string& path,
        int size)
{
    cleanUp();

    Logger::dbg << "Loading font: " << path << Logger::End;
    if (FT_Error error = FT_New_Face(library, path.c_str(), 0, &m_face))
    {
        Logger::fatal << "Failed to load font: " << path << ": " << FT_Error_String(error)
            << Logger::End;
    }

    if (FT_Error error = FT_Set_Pixel_Sizes(m_face, size, 0))
    {
        Logger::fatal << "Failed to set font size: " << FT_Error_String(error) << Logger::End;
    }

    Logger::dbg << "Caching glyphs..." << Logger::End;
    assert(m_contextWin);
    glfwMakeContextCurrent(m_contextWin);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for (FT_Long i{}; i < m_face->num_glyphs; ++i)
    {
        // Note: FT_Load_Glyph needs the gylph index, not the character code
        // Use FT_Get_Char_Index to get the glyph index for the char code
        if (FT_Error error = FT_Load_Glyph(m_face, i, FT_LOAD_RENDER))
        {
            Logger::fatal << "Failed to load glyph " << +i << ": " << FT_Error_String(error)
                << Logger::End;
        }

        uint texId;
        glGenTextures(1, &texId);
        glBindTexture(GL_TEXTURE_2D, texId);
        glTexImage2D(GL_TEXTURE_2D,
                0, GL_RED,
                m_face->glyph->bitmap.width, m_face->glyph->bitmap.rows,
                0, GL_RED, GL_UNSIGNED_BYTE, m_face->glyph->bitmap.buffer);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        assert(texId);
        m_glyphs.push_back(Face::Glyph{
                texId,
                {m_face->glyph->bitmap.width, m_face->glyph->bitmap.rows}, // Size
                {m_face->glyph->bitmap_left, m_face->glyph->bitmap_top}, // Bearing
                (uint)m_face->glyph->advance.x // Advance
        });
    }

    const float width = m_glyphs[FT_Get_Char_Index(m_face, 'A')].advance/64.0f;
    Logger::dbg << "Loaded font: " << path
        << "\n\tFamily: " << m_face->family_name
        << "\n\tStyle: " << m_face->style_name
        << "\n\tGlyphs: " << m_face->num_glyphs
        << "\n\tSize: " << g_fontSizePx
        << "\n\tWidth: " << width
        << Logger::End;
}

void Face::load(
        FT_Library library,
        const std::string& path,
        int size,
        bool isRegularFont)
{
    m_isLoading = true;
    m_loaderThread = std::thread{[this, library, &path, size, isRegularFont](){
        _load(library, path, size);
        if (isRegularFont)
        {
            g_fontSizePx = size;
            g_fontWidthPx = getGlyphByIndex(getGlyphIndex('A'))->advance/64.0f;
        }
        m_isLoading = false;
    }};
}

void Face::cleanUp()
{
    if (!m_face)
        return;

    Logger::dbg << "Freeing glyphs.." << Logger::End;

    m_loaderThread.join();

    const size_t count = m_glyphs.size();
    for (auto& glyph : m_glyphs)
    {
        glDeleteTextures(1, &glyph.textureId);
    }
    m_glyphs.clear();
    FT_Done_Face(m_face);
    glfwDestroyWindow(m_contextWin);

    Logger::dbg << "Freed " << count << " glyphs" << Logger::End;
}

FT_UInt Face::getGlyphIndex(FT_ULong charcode)
{
    return FT_Get_Char_Index(m_face, charcode);
}

Face::Glyph* Face::getGlyphByIndex(FT_UInt index)
{
    return &m_glyphs[index];
}

Face::Glyph* TextRenderer::getGlyph(Face* face, FT_ULong charcode)
{
    const FT_UInt index = face->getGlyphIndex(charcode);
    if (index == 0) // If not found, use the fallback font
    {
        return m_fallbackFace->getGlyphByIndex(m_fallbackFace->getGlyphIndex(charcode));
    }
    return face->getGlyphByIndex(index);
}

TextRenderer::TextRenderer(
        const std::string& regularFontPath,
        const std::string& boldFontPath,
        const std::string& italicFontPath,
        const std::string& boldItalicFontPath,
        const std::string& fallbackFontPath)
    :
    m_regularFontPath{regularFontPath},
    m_boldFontPath{boldFontPath},
    m_italicFontPath{italicFontPath},
    m_boldItalicFontPath{boldItalicFontPath},
    m_fbFontPath{fallbackFontPath},
    m_glyphShader{App::getResPath("../shaders/glyph.vert.glsl"), App::getResPath("../shaders/glyph.frag.glsl")}
{
    Logger::dbg << "Initializing FreeType" << Logger::End;
    if (FT_Error error = FT_Init_FreeType(&m_library))
    {
        Logger::fatal << "Failed to initialize FreeType: " << FT_Error_String(error) << Logger::End;
    }

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
    m_regularFace->load(m_library, m_regularFontPath, size, true);
    m_boldFace->load(m_library, m_boldFontPath, size, false);
    m_italicFace->load(m_library, m_italicFontPath, size, false);
    m_boldItalicFace->load(m_library, m_boldItalicFontPath, size, false);
    m_fallbackFace->load(m_library, m_fbFontPath, size, false);
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

static inline Face::GlyphDimensions renderGlyph(
        const Face::Glyph& glyph,
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


Face* TextRenderer::getFaceFromStyle(FontStyle style)
{
    switch (style)
    {
    case FONT_STYLE_REGULAR:                return m_regularFace.get();
    case FONT_STYLE_BOLD:                   return m_boldFace.get();
    case FONT_STYLE_ITALIC:                 return m_italicFace.get();
    case FONT_STYLE_BOLD|FONT_STYLE_ITALIC: return m_boldItalicFace.get();
    default: assert(false);
    }
    return {}; // Never reached
}

Face::GlyphDimensions TextRenderer::renderChar(
        Char c,
        const glm::ivec2& position,
        FontStyle style/*=FONT_STYLE_REGULAR*/
    )
{
    auto face = getFaceFromStyle(style);
    if (face->isLoading())
    {
        g_isRedrawNeeded = true; // Try again later
        return {};
    }
    return renderGlyph(
            *getGlyph(face, c),
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
        const String& str,
        const glm::ivec2& position,
        FontStyle initStyle/*=FONT_STYLE_REGULAR*/,
        const RGBColor& initColor/*={1.0f, 1.0f, 1.0f}*/,
        bool shouldWrap/*=false*/,
        bool onlyMeasure/*=false*/
    )
{
    static constexpr float scale = 1.0f;
    FontStyle currStyle = initStyle;
    Face* face;

    auto setStyle{[&currStyle, &face, this](FontStyle style){
        currStyle = style;
        face = getFaceFromStyle(currStyle);
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
    for (Char c : str)
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
                for (Char k : ansiSeq)
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

        if (face->isLoading())
        {
            g_isRedrawNeeded = true; // Try again later
        }
        else
        {
            if (!onlyMeasure)
                renderGlyph(*getGlyph(face, c), textX, textY, scale, m_fontVbo);
            textX += (getGlyph(face, c)->advance/64.0f) * scale;
            area.second.x = std::max(area.second.x, (int)std::ceil(textX));
        }
    }

    area.second.y = std::ceil(textY+g_fontSizePx*scale);
    return area;
}

TextRenderer::~TextRenderer()
{
    glDeleteBuffers(1, &m_fontVbo);
    glDeleteVertexArrays(1, &m_fontVao);
    m_regularFace.reset();
    m_boldFace.reset();
    m_italicFace.reset();
    m_boldItalicFace.reset();
    m_fallbackFace.reset();
    FT_Done_FreeType(m_library);
    Logger::dbg << "Cleaned up TextRenderer data" << Logger::End;
}
