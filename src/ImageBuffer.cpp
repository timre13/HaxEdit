#include "ImageBuffer.h"
#include "dialogs/MessageDialog.h"
#include <cassert>
#include "config.h"
#include "Timer.h"
#include "types.h"
#include <fstream>
#include <sstream>

void ImageBuffer::open(const std::string& filePath)
{
    TIMER_BEGIN_FUNC();
    glfwSetCursor(g_window, Cursors::busy);

    assert(isImageExtension(::getFileExt(filePath)));

    m_filePath = filePath;
    m_isReadOnly = true;
    Logger::dbg << "Opening image file: " << filePath << Logger::End;
    m_image = std::make_unique<Image>(filePath, GL_NEAREST, GL_NEAREST);

    if (m_image->isOpenFailed())
    {
        // TODO: Show reason
        Logger::err << "Failed to open image file: " << quoteStr(filePath) << Logger::End;
        MessageDialog::create(Dialog::EMPTY_CB, nullptr,
                "Failed to open image file: "+quoteStr(filePath),
                MessageDialog::Type::Error);
    }

    glfwSetCursor(g_window, nullptr);
    TIMER_END_FUNC();
}

void ImageBuffer::updateRStatusLineStr()
{
    const int imgPW = m_image->getPhysicalSize().x;
    const float zoomedW = imgPW*m_zoom;
    const int imgX = m_position.x+m_size.x/2-zoomedW/2;
    const int imgCursorX = (g_cursorX-imgX)/m_zoom;

    const int imgPH = m_image->getPhysicalSize().y;
    const float zoomedH = imgPH*m_zoom;
    const int imgY = m_position.y+m_size.y/2-zoomedH/2;
    const int imgCursorY = (g_cursorY-imgY)/m_zoom;

    m_statusLineStr.str
        = "W: "+std::to_string(m_image->getPhysicalSize().x)
        + "  H: "+std::to_string(m_image->getPhysicalSize().y)
        + " | "
        + "Z: "+std::to_string((int)std::round(m_zoom*100.0f))+"%"
        + " | "
        + (imgCursorX < 0 || imgCursorX > imgPW || imgCursorY < 0 || imgCursorY > imgPW
                ? ""
                : "Cur: "+std::to_string(imgCursorX)+'x'+std::to_string(imgCursorY));
    m_statusLineStr.maxLen = std::max((size_t)43, m_statusLineStr.str.length());
}

void ImageBuffer::render()
{
    TIMER_BEGIN_FUNC();

    // Fill background
    g_uiRenderer->renderFilledRectangle(
            m_position,
            {m_position.x+m_size.x, m_position.y+m_size.y},
            RGB_COLOR_TO_RGBA(BG_COLOR)
    );
    // Draw border
    g_uiRenderer->renderRectangleOutline(
            m_position,
            {m_position.x+m_size.x, m_position.y+m_size.y},
            {0.6f, 0.6f, 0.6f},
            1
    );

    const float zoomedW = m_image->getPhysicalSize().x*m_zoom;
    const float zoomedH = m_image->getPhysicalSize().y*m_zoom;

    glEnable(GL_SCISSOR_TEST);
    // Only draw inside the bounding box,
    // don't draw over the neighbouring buffer (when there are multiple splits)
    glScissor(m_position.x, m_position.y, m_size.x, m_size.y);
    m_image->render(
            {m_position.x+m_size.x/2-zoomedW/2,
             m_position.y+m_size.y/2-zoomedH/2},
            {zoomedW, zoomedH});
    glDisable(GL_SCISSOR_TEST);

    TIMER_END_FUNC();
}
