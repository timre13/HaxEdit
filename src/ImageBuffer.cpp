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
    m_image = std::make_unique<Image>(filePath);

    TIMER_END_FUNC();
    return 0;
}

void ImageBuffer::render()
{
    TIMER_BEGIN_FUNC();

    m_image->render({
            g_windowWidth/2-m_image->getPhysicalSize().x/2,
            g_windowHeight/2-m_image->getPhysicalSize().y/2,
    });

    renderStatusLine();

    TIMER_END_FUNC();
}
