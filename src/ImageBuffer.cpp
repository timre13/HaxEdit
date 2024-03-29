#include "ImageBuffer.h"
#include "dialogs/MessageDialog.h"
#include <cassert>
#include "config.h"
#include "Timer.h"
#include "types.h"
#include "ThemeLoader.h"
#include "globals.h"
#include <fstream>
#include <sstream>

void ImageBuffer::open(const std::string& filePath, bool isReload/*=false*/)
{
    TIMER_BEGIN_FUNC();
    glfwSetCursor(g_window, Cursors::busy);

    assert(isImageExtension(::getFileExt(filePath)));

    m_filePath = filePath;
    m_isReadOnly = true;
    if (isReload)
        Logger::dbg << "Reloading image file: " << filePath << Logger::End;
    else
        Logger::dbg << "Opening image file: " << filePath << Logger::End;
    m_image = std::make_unique<Image>(filePath, GL_NEAREST, GL_NEAREST);

    if (m_image->isOpenFailed())
    {
        // TODO: Show reason
        if (isReload)
        {
            Logger::err << "Failed to reload image file: " << quoteStr(filePath) << Logger::End;
            g_statMsg.set("Failed to reload image file: "+quoteStr(filePath), StatusMsg::Type::Error);
            MessageDialog::create(Dialog::EMPTY_CB, nullptr,
                    "Failed to reload image file: "+quoteStr(filePath),
                    MessageDialog::Type::Error);
        }
        else
        {
            Logger::err << "Failed to open image file: " << quoteStr(filePath) << Logger::End;
            g_statMsg.set("Failed to open image file: "+quoteStr(filePath), StatusMsg::Type::Error);
            MessageDialog::create(Dialog::EMPTY_CB, nullptr,
                    "Failed to open image file: "+quoteStr(filePath),
                    MessageDialog::Type::Error);
        }
    }

    if (isReload)
        g_statMsg.set("Reloaded image file: "+quoteStr(filePath), StatusMsg::Type::Info);
    else
        g_statMsg.set("Opened image file: "+quoteStr(filePath), StatusMsg::Type::Info);

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
    const bool isCursorOutsideImg = (imgCursorX < 0 || imgCursorX >= imgPW || imgCursorY < 0 || imgCursorY >= imgPW);

    m_statusLineStr.str
        = "W: "+std::to_string(m_image->getPhysicalSize().x)
        + "  H: "+std::to_string(m_image->getPhysicalSize().y)
        + " | "
        + "Z: "+std::to_string((int)std::round(m_zoom*100.0f))+"%"
        + " | "
        + (isCursorOutsideImg ? "\033[90m" : "")+"Cur: "+std::to_string(imgCursorX)+", "+std::to_string(imgCursorY);
    if (!isCursorOutsideImg)
    {
        const RGBAColor colorAtCurs = m_image->getColorAt({imgCursorX, imgCursorY});
        m_statusLineStr.str += " | " + colorAtCurs.asStrFloat() + " | " + colorAtCurs.asStrHex();
    }

    m_statusLineStr.maxLen = std::max((size_t)88, strPLen(m_statusLineStr.str));
}

void ImageBuffer::render()
{
    TIMER_BEGIN_FUNC();

    // Fill background
    g_uiRenderer->renderFilledRectangle(
            m_position,
            {m_position.x+m_size.x, m_position.y+m_size.y},
            RGB_COLOR_TO_RGBA(g_theme->bgColor)
    );
    // Draw border
    g_uiRenderer->renderRectangleOutline(
            m_position,
            {m_position.x+m_size.x, m_position.y+m_size.y},
            {0.6f, 0.6f, 0.6f},
            1
    );

    const int imgPW = m_image->getPhysicalSize().x;
    const float zoomedW = imgPW*m_zoom;
    const int imgX = m_position.x+m_size.x/2-zoomedW/2;
    const int imgCursorX = (g_cursorX-imgX)/m_zoom;

    const int imgPH = m_image->getPhysicalSize().y;
    const float zoomedH = imgPH*m_zoom;
    const int imgY = m_position.y+m_size.y/2-zoomedH/2;
    const int imgCursorY = (g_cursorY-imgY)/m_zoom;

    // Fill transparent part of image
    g_uiRenderer->renderFilledRectangle(
            {m_position.x+m_size.x/2-zoomedW/2,
             m_position.y+m_size.y/2-zoomedH/2},
            {m_position.x+m_size.x/2-zoomedW/2+zoomedW,
             m_position.y+m_size.y/2-zoomedH/2+zoomedH},
            {0, 0, 0, 1}
    );

    glEnable(GL_SCISSOR_TEST);
    // Only draw inside the bounding box,
    // don't draw over the neighbouring buffer (when there are multiple splits)
    glScissor(m_position.x, m_position.y, m_size.x, m_size.y);
    m_image->render(
            {m_position.x+m_size.x/2-zoomedW/2,
             m_position.y+m_size.y/2-zoomedH/2},
            {zoomedW, zoomedH});
    glDisable(GL_SCISSOR_TEST);

    // If the cursor is inside the image
    if (imgCursorX >= 0 && imgCursorX < imgPW && imgCursorY >= 0 && imgCursorY < imgPW)
    {
        // Render a border around the hovered pixel
        g_uiRenderer->renderRectangleOutline(
                {imgX+int(imgCursorX)*m_zoom,        imgY+int(imgCursorY)*m_zoom},
                {imgX+int(imgCursorX)*m_zoom+m_zoom, imgY+int(imgCursorY)*m_zoom+m_zoom},
                {1, 1, 1, 1},
                1);
        g_uiRenderer->renderRectangleOutline(
                {imgX+int(imgCursorX)*m_zoom+1,        imgY+int(imgCursorY)*m_zoom+1},
                {imgX+int(imgCursorX)*m_zoom+1+m_zoom, imgY+int(imgCursorY)*m_zoom+1+m_zoom},
                {0, 0, 0, 1},
                1);
    }

    TIMER_END_FUNC();
}
