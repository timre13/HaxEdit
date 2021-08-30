#include "Dialog.h"
#include "Timer.h"
#include "TextRenderer.h"
#include "UiRenderer.h"

void Dialog::render()
{
    // If the window has been resized since last time, recalculate dimensions
    if (g_textRenderer->getWindowWidth() != m_windowWidth
     || g_textRenderer->getWindowHeight() != m_windowHeight)
    {
        m_windowWidth = g_textRenderer->getWindowWidth();
        m_windowHeight = g_textRenderer->getWindowHeight();
        recalculateDimensions();
    }
}
