#pragma once

#include "Buffer.h"
#include "Image.h"
#include "Logger.h"
#include "types.h"
#include <memory>

extern int g_windowWidth;
extern int g_windowHeight;

inline bool isImageExtension(std::string ext)
{
    ext = strToLower(ext);

    if (ext == "jpg"
     || ext == "jpeg"
     || ext == "png"
     || ext == "bmp"
     || ext == "psd"
     || ext == "tga"
     || ext == "gif"
     || ext == "hdr"
     || ext == "pic"
     || ext == "pnm"
     || ext == "pbm"
     || ext == "pgm"
     || ext == "ppm"
     )
        return true;
    return false;
}

class ImageBuffer final : public Buffer
{
private:
    std::unique_ptr<Image> m_image;
    float m_zoom{1.0f};

public:
    ImageBuffer()
    {
        m_isReadOnly = true;
        Logger::log << "Created an image buffer: " << this << Logger::End;
    }

    ImageBuffer(const ImageBuffer&) = delete;
    ImageBuffer& operator=(const ImageBuffer&) = delete;
    ImageBuffer(ImageBuffer&& other) = default;
    ImageBuffer& operator=(ImageBuffer&& other) = default;

    virtual int open(const std::string& filePath) override;
    virtual int saveToFile() override { return 1; }
    virtual int saveAsToFile(const std::string&) override { return 1; }

    virtual void updateCursor() override {}
    virtual void render() override;

    virtual inline void moveCursor(CursorMovCmd) override {}
    virtual inline void scrollBy(int) override {}
    virtual inline void setReadOnly(bool) override {}

    virtual inline void zoomBy(float val)
    {
        if (m_image->getPhysicalSize().x*(m_zoom+val) >= 1.0f
         && m_image->getPhysicalSize().y*(m_zoom+val) >= 1.0f)
        {
            m_zoom += val;
        }
    }

    virtual inline ~ImageBuffer()
    {
        Logger::log << "Destroyed an image buffer: " << this << " ("<< m_filePath << ')' << Logger::End;
    }
};
