#include "Dialog.h"
#include "UiRenderer.h"
#include "TextRenderer.h"
#include "Logger.h"
#include "Timer.h"
#include "config.h"
#include "types.h"

Dialog::Dialog(const std::string& msg, Type type, const std::string& btnText/*="OK"*/)
    : m_message{msg+'\n'}, m_type{type}, m_buttonText{btnText+'\n'}
{
}

void Dialog::recalculateDimensions()
{
    Logger::dbg << "Recalculating dialog dimensions (window size: "
        << m_windowWidth << 'x' << m_windowHeight << ')' << Logger::End;

    assert(m_windowWidth > 0);
    assert(m_windowHeight > 0);

    m_btnTextDims.width = getLongestLineLen(m_buttonText)*FONT_SIZE_PX*0.7;
    m_btnTextDims.height = FONT_SIZE_PX*strCountLines(m_buttonText);
    m_btnDims.width = m_btnTextDims.width+10+10;
    m_btnDims.height = m_btnTextDims.height+10+10;
    m_btnDims.xPos = m_windowWidth/2-m_btnDims.width/2;
    m_btnTextDims.xPos = m_btnDims.xPos+m_btnDims.width/2-m_btnTextDims.width/2;

    m_msgTextDims.width = getLongestLineLen(m_message)*FONT_SIZE_PX*0.7;
    m_msgTextDims.height = strCountLines(m_message)*FONT_SIZE_PX;

    m_dialogDims.width = std::max(m_msgTextDims.width, m_btnDims.width)+10+10;
    m_dialogDims.height = 10+m_btnDims.height+10+m_msgTextDims.height+10;
    m_dialogDims.xPos = m_windowWidth/2-m_dialogDims.width/2;
    m_dialogDims.yPos = m_windowHeight/2-m_dialogDims.height/2;

    m_msgTextDims.xPos = m_dialogDims.xPos+m_dialogDims.width/2-m_msgTextDims.width/2;
    m_msgTextDims.yPos = m_dialogDims.yPos+10;

    m_btnDims.yPos = m_dialogDims.yPos+m_dialogDims.height-10-m_btnDims.height;
    m_btnTextDims.yPos = m_btnDims.yPos+m_btnDims.height/2-m_btnTextDims.height/2;
}

void Dialog::render()
{
    TIMER_BEGIN_FUNC();

    // If the window has been resized since last time, recalculate dimensions
    if (g_textRenderer->getWindowWidth() != m_windowWidth
     || g_textRenderer->getWindowHeight() != m_windowHeight)
    {
        m_windowWidth = g_textRenderer->getWindowWidth();
        m_windowHeight = g_textRenderer->getWindowHeight();
        recalculateDimensions();
    }

    RGBAColor dialogColor;
    RGBAColor buttonColor;
    switch (m_type)
    {
    case Type::Information:
        dialogColor = {0.5f, 0.7f, 1.0f};
        buttonColor = {0.6f, 0.8f, 0.9f};
        break;

    case Type::Warning:
        dialogColor = {0.8f, 0.5f, 0.0f};
        buttonColor = {0.9f, 0.6f, 0.4f};
        break;

    case Type::Error:
        dialogColor = {1.0f, 0.3f, 0.2f};
        buttonColor = {0.9f, 0.3f, 0.3f};
        break;
    }

    // Render dialog border
    g_uiRenderer->renderFilledRectangle(
            {m_dialogDims.xPos-2, m_dialogDims.yPos-2},
            {m_dialogDims.xPos+m_dialogDims.width+2, m_dialogDims.yPos+m_dialogDims.height+2},
            {1.0f, 1.0f, 1.0f});
    // Render dialog
    g_uiRenderer->renderFilledRectangle(
            {m_dialogDims.xPos, m_dialogDims.yPos},
            {m_dialogDims.xPos+m_dialogDims.width, m_dialogDims.yPos+m_dialogDims.height},
            dialogColor);
    // Render dialog text
    g_textRenderer->renderString(m_message, {m_msgTextDims.xPos, m_msgTextDims.yPos});


    // Render button border
    g_uiRenderer->renderFilledRectangle(
            {m_btnDims.xPos-1, m_btnDims.yPos-1},
            {m_btnDims.xPos+m_btnDims.width+1, m_btnDims.yPos+m_btnDims.height+1},
            {1.0f, 1.0f, 1.0f});
    // Render button
    g_uiRenderer->renderFilledRectangle(
            {m_btnDims.xPos, m_btnDims.yPos},
            {m_btnDims.xPos+m_btnDims.width, m_btnDims.yPos+m_btnDims.height},
            buttonColor);
    // Render button text
    g_textRenderer->renderString(m_buttonText, {m_btnTextDims.xPos, m_btnTextDims.yPos});

    TIMER_END_FUNC();
}
