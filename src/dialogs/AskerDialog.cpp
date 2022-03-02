#include "AskerDialog.h"
#include "../Logger.h"
#include "../Buffer.h"
#include "../globals.h"
#include "../common/string.h"
#include <vector>
#include <memory>

AskerDialog::AskerDialog(
        callback_t cb,
        void* cbUserData,
        const std::string& msg
        )
    : Dialog{cb, cbUserData}, m_msg{msg}
{
    Logger::dbg << "Opened an asker dialog with message: " << quoteStr(msg) << Logger::End;
    m_isClosed = false;
}

void AskerDialog::recalculateDimensions()
{
    m_dialogDims.xPos = 160;
    m_dialogDims.width = std::max(g_windowWidth-160*2, 0);

    m_titleDims.xPos = m_dialogDims.xPos+10;
    m_titleDims.width = m_dialogDims.width-20;
    m_titleDims.height = g_fontSizePx*1.5f;

    m_entryRect.xPos = m_titleDims.xPos+10;
    m_entryRect.width = m_dialogDims.width-40;
    m_entryRect.height = g_fontSizePx;

    m_dialogDims.height = m_titleDims.height+m_entryRect.height+40;
    m_dialogDims.yPos = g_windowHeight/2-m_dialogDims.height;
    m_titleDims.yPos = m_dialogDims.yPos+10;
    m_entryRect.yPos = m_titleDims.yPos+m_titleDims.height+10;
}

void AskerDialog::render()
{
    if (m_isClosed)
        return;

    TIMER_BEGIN_FUNC();

    recalculateDimensions();

    renderBase();
    // Render title path
    g_textRenderer->renderString(
            m_msg,
            {m_titleDims.xPos, m_titleDims.yPos});

    // Render entry rectangle
    g_uiRenderer->renderFilledRectangle(
            {m_entryRect.xPos, m_entryRect.yPos},
            {m_entryRect.xPos+m_entryRect.width, m_entryRect.yPos+m_entryRect.height},
            {0.0f, 0.1f, 0.3f, 0.8f});
    // Render entry value
    g_textRenderer->renderString(
            m_buffer,
            {m_entryRect.xPos, m_entryRect.yPos-2});

    TIMER_END_FUNC();
}

void AskerDialog::handleKey(int key, int mods)
{
    if (mods == 0)
    {
        if (key == GLFW_KEY_ESCAPE)
        {
            Logger::dbg << "AskerDialog: Cancelled" << Logger::End;
            m_isClosed = true;
            return;
        }

        switch (key)
        {
        case GLFW_KEY_BACKSPACE:
            m_buffer = m_buffer.substr(0, m_buffer.size()-1);
            break;

        case GLFW_KEY_ENTER:
            m_isClosed = true;
            if (m_callback)
                m_callback(0, this, m_cbUserData);
            break;
        }
    }
    else if (mods == GLFW_MOD_CONTROL)
    {
        if (key == GLFW_KEY_BACKSPACE)
        {
            m_buffer.clear();
        }
    }
}

void AskerDialog::handleChar(uint c)
{
    m_buffer += c;
}
