#include "AskerDialog.h"
#include "Logger.h"
#include "Buffer.h"
#include <vector>
#include <memory>

extern int g_windowWidth;
extern int g_windowHeight;
extern bool g_isRedrawNeeded;
extern bool g_isTitleUpdateNeeded;
extern bool g_isDebugDrawMode;
extern bool g_shouldIgnoreNextChar;
extern std::vector<Buffer> g_buffers;
extern size_t g_currentBufferI;
extern std::vector<std::unique_ptr<Dialog>> g_dialogs;
extern int g_fontSizePx;

AskerDialog::AskerDialog(
            const std::string& msg,
            Id id/*=Id::Generic*/
            )
    : m_id{id}, m_msg{msg}
{
    Logger::dbg << "Opened an asker dialog with message \"" << msg << '"' << Logger::End;
    m_isClosed = false;
}

void AskerDialog::recalculateDimensions()
{
    Logger::dbg << "Recalculating asker dialog dimensions (window size: "
        << m_windowWidth << 'x' << m_windowHeight << ')' << Logger::End;

    assert(m_windowWidth > 0);
    assert(m_windowHeight > 0);

    m_dialogDims.xPos = 160;
    m_dialogDims.width = std::max(m_windowWidth-160*2, 0);

    m_titleDims.xPos = m_dialogDims.xPos+10;
    m_titleDims.width = m_dialogDims.width-20;
    m_titleDims.height = g_fontSizePx*1.5f;

    m_entryRect.xPos = m_titleDims.xPos+10;
    m_entryRect.width = m_dialogDims.width-40;
    m_entryRect.height = g_fontSizePx;

    m_dialogDims.height = m_titleDims.height+m_entryRect.height+40;
    m_dialogDims.yPos = m_windowHeight/2-m_dialogDims.height;
    m_titleDims.yPos = m_dialogDims.yPos+10;
    m_entryRect.yPos = m_titleDims.yPos+m_titleDims.height+10;
}

void AskerDialog::render()
{
    if (m_isClosed)
        return;

    TIMER_BEGIN_FUNC();

    // Calls `recalculateDimensions()` if needed
    this->Dialog::render();

    // Render dialog border
    g_uiRenderer->renderFilledRectangle(
            {m_dialogDims.xPos-2, m_dialogDims.yPos-2},
            {m_dialogDims.xPos+m_dialogDims.width+2, m_dialogDims.yPos+m_dialogDims.height+2},
            {1.0f, 1.0f, 1.0f, 0.8f});
    // Render dialog
    g_uiRenderer->renderFilledRectangle(
            {m_dialogDims.xPos, m_dialogDims.yPos},
            {m_dialogDims.xPos+m_dialogDims.width, m_dialogDims.yPos+m_dialogDims.height},
            {0.1f, 0.2f, 0.4f, 0.8f});

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
        switch (key)
        {
        case GLFW_KEY_BACKSPACE:
            m_buffer = m_buffer.substr(0, m_buffer.size()-1);
            break;

        case GLFW_KEY_ENTER:
            m_isClosed = true;
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
