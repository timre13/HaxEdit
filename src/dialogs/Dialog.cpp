#include "Dialog.h"
#include "../Timer.h"
#include "../TextRenderer.h"
#include "../UiRenderer.h"

Dialog::callback_t Dialog::EMPTY_CB = [](int, Dialog*, void*){};

Dialog::Dialog(callback_t cb, void* cbUserData)
    : m_callback{cb}, m_cbUserData{cbUserData}
{
}

void Dialog::render()
{
    assert(!m_isClosed);
    // If the window has been resized since last time, recalculate dimensions
    if (g_textRenderer->getWindowWidth() != m_windowWidth
     || g_textRenderer->getWindowHeight() != m_windowHeight)
    {
        m_windowWidth = g_textRenderer->getWindowWidth();
        m_windowHeight = g_textRenderer->getWindowHeight();
        recalculateDimensions();
    }
}
