#include "Image.h"
#include "Logger.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG // Get better error messages
#include "../external/stb/stb_image.h"
#include "glstuff.h"

Image::Image(
        const std::string& filePath,
        uint upscaleFilt/*=UPSCALE_FILT_DEF*/, uint downscaleFilt/*=DOWNSCALE_FILT_DEF*/,
        bool preventLogging/*=false*/)
{
    m_filePath = filePath;
    m_preventLogging = preventLogging;

    stbi_set_flip_vertically_on_load(1);
    int channelCount;
    m_data = stbi_load(filePath.c_str(), &m_physicalSize.x, &m_physicalSize.y, &channelCount, 4);
    if (!m_data)
    {
        Logger::err << "Failed to load image: " << filePath << ": " << stbi_failure_reason() << Logger::End;
        m_isOpenFailed = true;
        m_data = (unsigned char*)malloc(4*4*4);
        static constexpr const uint32_t imgData[] = {
            0xffffffff, 0xff000000, 0xffffffff, 0xff000000,
            0xff000000, 0xffffffff, 0xff000000, 0xffffffff,
            0xffffffff, 0xff000000, 0xffffffff, 0xff000000,
            0xff000000, 0xffffffff, 0xff000000, 0xffffffff,
        };
        memcpy(m_data, imgData, 4*4*4);
        m_physicalSize.x = 4;
        m_physicalSize.y = 4;
    }
    else
    {
        if (!m_preventLogging)
            Logger::dbg << "Loaded image file: " << filePath << Logger::End;
        m_isOpenFailed = false;
    }

    glGenTextures(1, &m_sampler);
    glBindTexture(GL_TEXTURE_2D, m_sampler);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_physicalSize.x, m_physicalSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_data);
    if (m_isOpenFailed) // If failed to open, force "nearest" filter
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); // Downscale filter
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); // Upscale filter
    }
    else
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, downscaleFilt); // Downscale filter
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, upscaleFilt); // Upscale filter
    }
}

void Image::render(const glm::ivec2& pos, const glm::ivec2& size/*={0, 0}*/) const
{
    g_uiRenderer->renderImage(this,
            pos,
            {size.x > 0 && size.y > 0 ? size : m_physicalSize});
}

RGBAColor Image::getColorAt(glm::ivec2 pos)
{
    // Flip the coordinate vertically
    pos.y = m_physicalSize.y-pos.y-1;

    assert(pos.x >= 0 && pos.x < m_physicalSize.x && pos.y >= 0 && pos.y < m_physicalSize.y);
    if (pos.x < 0 || pos.x >= m_physicalSize.x || pos.y < 0 || pos.y >= m_physicalSize.y)
        return {};

    const int baseI = (pos.y*m_physicalSize.x + pos.x) * 4;
    RGBAColor color;
    color.r = m_data[baseI+0] / 255.0f;
    color.g = m_data[baseI+1] / 255.0f;
    color.b = m_data[baseI+2] / 255.0f;
    color.a = m_data[baseI+3] / 255.0f;
    return color;
}

Image::~Image()
{
    stbi_image_free(m_data);
    glDeleteTextures(1, &m_sampler);
    if (!m_preventLogging)
        Logger::dbg << "Cleaned up texture data for image " << this << " (" << m_filePath << ')' << Logger::End;
}
