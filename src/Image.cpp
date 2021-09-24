#include "Image.h"
#include "Logger.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG // Get better error messages
#include "../external/stb/stb_image.h"
#include "glstuff.h"

Image::Image(
        const std::string& filePath,
        uint upscaleFilt/*=GL_LINEAR*/, uint downscaleFilt/*=GL_LINEAR*/)
{
    m_filePath = filePath;

    stbi_set_flip_vertically_on_load(1);
    int channelCount;
    unsigned char* imageData = stbi_load(filePath.c_str(), &m_physicalSize.x, &m_physicalSize.y, &channelCount, 4);
    if (!imageData)
    {
        Logger::err << "Failed to load image: " << filePath << ": " << stbi_failure_reason() << Logger::End;
        m_isOpenFailed = true;
        imageData = (unsigned char*)malloc(4*4*4);
        static constexpr const uint32_t imgData[] = {
            0xffffffff, 0xff000000, 0xffffffff, 0xff000000,
            0xff000000, 0xffffffff, 0xff000000, 0xffffffff,
            0xffffffff, 0xff000000, 0xffffffff, 0xff000000,
            0xff000000, 0xffffffff, 0xff000000, 0xffffffff,
        };
        memcpy(imageData, imgData, 4*4*4);
        m_physicalSize.x = 4;
        m_physicalSize.y = 4;
    }
    else
    {
        Logger::dbg << "Loaded image file: " << filePath << Logger::End;
        m_isOpenFailed = false;
    }

    glGenTextures(1, &m_sampler);
    glBindTexture(GL_TEXTURE_2D, m_sampler);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_physicalSize.x, m_physicalSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, imageData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, downscaleFilt); // Downscale filter
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, upscaleFilt); // Upscale filter

    stbi_image_free(imageData);
}

void Image::render(const glm::ivec2& pos, const glm::ivec2& size/*={0, 0}*/) const
{
    g_uiRenderer->renderImage(this,
            pos,
            {size.x > 0 && size.y > 0 ? size : m_physicalSize});
}

Image::~Image()
{
    glDeleteTextures(1, &m_sampler);
    Logger::dbg << "Cleaned up texture data for image " << this << " (" << m_filePath << ')' << Logger::End;
}
