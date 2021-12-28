#pragma once

#include "Buffer.h"
#include "Image.h"
#include "Logger.h"
#include "common/string.h"
#include "globals.h"
#include <memory>

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

    /*
     * Do not call this. Use `App::openFileInNewBuffer`.
     */
    virtual void open(const std::string& filePath) override;

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

    virtual int saveToFile() override { return 1; }
    virtual int saveAsToFile(const std::string&) override { return 1; }

    virtual void updateRStatusLineStr() override;
    virtual void updateCursor() override {}
    virtual void render() override;

    virtual inline void moveCursor(CursorMovCmd) override {}
    virtual inline void scrollBy(int val) override { zoomBy(val/100.0); }
    virtual inline void setReadOnly(bool) override {}

    virtual inline void zoomBy(float val)
    {
        Logger::log << "Zooming image buffer by " << val*100 << '%' << Logger::End;
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
