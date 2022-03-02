#include "Dialog.h"
#include "../Timer.h"
#include "../TextRenderer.h"
#include "../UiRenderer.h"

Dialog::callback_t Dialog::EMPTY_CB = [](int, Dialog*, void*){};

Dialog::Dialog(callback_t cb, void* cbUserData)
    : m_callback{cb}, m_cbUserData{cbUserData}
{
}

void Dialog::renderBase(const RGBColor& color/*=DIALOG_DEF_BG*/)
{
    // Render dialog border
    g_uiRenderer->renderFilledRectangle(
            {m_dialogDims.xPos-2, m_dialogDims.yPos-2},
            {m_dialogDims.xPos+m_dialogDims.width+2, m_dialogDims.yPos+m_dialogDims.height+2},
            {1.0f, 1.0f, 1.0f, 0.8f});

    // Render dialog
    g_uiRenderer->renderFilledRectangle(
            {m_dialogDims.xPos, m_dialogDims.yPos},
            {m_dialogDims.xPos+m_dialogDims.width, m_dialogDims.yPos+m_dialogDims.height},
            {UNPACK_RGB_COLOR(color), 0.8f});

}
