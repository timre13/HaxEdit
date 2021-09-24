#include "ImageBuffer.h"
#include <cassert>
#include "config.h"
#include "Timer.h"
#include "types.h"
#include "syntax.h"
#include <fstream>
#include <sstream>

int ImageBuffer::open(const std::string& filePath)
{
    TIMER_BEGIN_FUNC();

    assert(isImageExtension(::getFileExt(filePath)));

    m_filePath = filePath;
    m_isReadOnly = true;
    Logger::dbg << "Opening image file: " << filePath << Logger::End;
    m_image = std::make_unique<Image>(filePath, GL_NEAREST, GL_NEAREST);

    TIMER_END_FUNC();
    return 0;
}

void ImageBuffer::updateRStatusLineStr()
{
    m_statusLineStr.str = std::to_string((int)std::round(m_zoom*100.0f))+"%";
    m_statusLineStr.maxLen = std::max((size_t)4, m_statusLineStr.str.length());
}

void ImageBuffer::render()
{
    TIMER_BEGIN_FUNC();

    const float zoomedW = m_image->getPhysicalSize().x*m_zoom;
    const float zoomedH = m_image->getPhysicalSize().y*m_zoom;

    m_image->render(
            {g_windowWidth/2-zoomedW/2, g_windowHeight/2-zoomedH/2},
            {zoomedW, zoomedH});

    TIMER_END_FUNC();
}
