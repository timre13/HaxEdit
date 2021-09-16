#include "MessageDialog.h"
#include "UiRenderer.h"
#include "TextRenderer.h"
#include "Logger.h"
#include "Timer.h"
#include "config.h"
#include "types.h"
#include <algorithm>

MessageDialog::MessageDialog(
        const std::string& msg,
        Type type,
        Id id,
        const std::vector<BtnInfo>& btns/*={{"OK", GLFW_KEY_ENTER}}*/
        )
    : m_id{id}, m_message{msg+'\n'}, m_type{type}, m_btnInfo{btns}
{
    for (const auto& btn : m_btnInfo)
        assert(btn.key != GLFW_KEY_UNKNOWN);
}

static std::string getBtnText(const MessageDialog::BtnInfo& btn)
{
    const char* keyName = glfwGetKeyName(btn.key, 0);
    if (keyName)
        return '['+std::string(keyName)+"] "+btn.label;
    return btn.label;
}

void MessageDialog::recalculateDimensions()
{
    Logger::dbg << "Recalculating message dialog dimensions (window size: "
        << m_windowWidth << 'x' << m_windowHeight << ')' << Logger::End;

    assert(m_windowWidth > 0);
    assert(m_windowHeight > 0);

    const int widestBtnW = std::max_element(m_btnInfo.begin(), m_btnInfo.end(),
            [](const BtnInfo& a, const BtnInfo& b){ return getBtnText(a).size() < getBtnText(b).size(); }
            )->label.size()*FONT_SIZE_PX*0.7f+20;

    m_dialogDims.width = std::max(std::max(m_msgTextDims.width, widestBtnW)+10+10, 200);
    m_dialogDims.height = 10+m_msgTextDims.height+10+(FONT_SIZE_PX+30)*m_btnInfo.size();
    m_dialogDims.xPos = m_windowWidth/2-m_dialogDims.width/2;
    m_dialogDims.yPos = m_windowHeight/2-m_dialogDims.height/2;

    m_msgTextDims.width = getLongestLineLen(m_message)*FONT_SIZE_PX*0.7;
    m_msgTextDims.height = strCountLines(m_message)*FONT_SIZE_PX;
    m_msgTextDims.xPos = m_dialogDims.xPos+m_dialogDims.width/2-m_msgTextDims.width/2;
    m_msgTextDims.yPos = m_dialogDims.yPos+10;

    m_btnDims.clear();
    m_btnTxtDims.clear();
    for (const auto& btn : m_btnInfo)
    {
        m_btnDims.emplace_back();
        m_btnTxtDims.emplace_back();

        m_btnTxtDims.back().width = getBtnText(btn).size()*FONT_SIZE_PX*0.7;
        m_btnTxtDims.back().height = FONT_SIZE_PX;
        m_btnDims.back().width = m_btnTxtDims.back().width+10+10;
        m_btnDims.back().height = m_btnTxtDims.back().height+10+10;
        m_btnDims.back().xPos = m_windowWidth/2-m_btnDims.back().width/2;
        m_btnTxtDims.back().xPos
            = m_btnDims.back().xPos+m_btnDims.back().width/2-m_btnTxtDims.back().width/2;
        m_btnDims.back().yPos
            = (m_btnDims.size() > 1
            ? m_btnDims[m_btnDims.size()-2].yPos+m_btnDims[m_btnDims.size()-2].height
            : m_msgTextDims.yPos+m_msgTextDims.height)+10;
        m_btnTxtDims.back().yPos
            = m_btnDims.back().yPos+m_btnDims.back().height/2-m_btnTxtDims.back().height/2;
    }
}

void MessageDialog::render()
{
    if (m_isClosed)
        return;

    TIMER_BEGIN_FUNC();

    // Calls `recalculateDimensions()` if needed
    this->Dialog::render();
    recalculateDimensions(); // FIXME: Why do we need to call this?

    RGBAColor dialogColor;
    RGBAColor buttonColor;
    switch (m_type)
    {
    case Type::Information:
        dialogColor = {0.5f, 0.7f, 1.0f, 0.8f};
        buttonColor = {0.6f, 0.8f, 0.9f, 0.8f};
        break;

    case Type::Warning:
        dialogColor = {0.8f, 0.5f, 0.0f, 0.8f};
        buttonColor = {0.9f, 0.6f, 0.4f, 0.8f};
        break;

    case Type::Error:
        dialogColor = {1.0f, 0.3f, 0.2f, 0.8f};
        buttonColor = {0.9f, 0.3f, 0.3f, 0.8f};
        break;
    }

    // Render dialog border
    g_uiRenderer->renderFilledRectangle(
            {m_dialogDims.xPos-2, m_dialogDims.yPos-2},
            {m_dialogDims.xPos+m_dialogDims.width+2, m_dialogDims.yPos+m_dialogDims.height+2},
            {1.0f, 1.0f, 1.0f, 0.8f});
    // Render dialog
    g_uiRenderer->renderFilledRectangle(
            {m_dialogDims.xPos, m_dialogDims.yPos},
            {m_dialogDims.xPos+m_dialogDims.width, m_dialogDims.yPos+m_dialogDims.height},
            dialogColor);
    // Render dialog text
    g_textRenderer->renderString(m_message, {m_msgTextDims.xPos, m_msgTextDims.yPos});


    for (size_t i{}; i < m_btnDims.size(); ++i)
    {
        const auto& dims = m_btnDims[i];

        // Render button border
        g_uiRenderer->renderFilledRectangle(
                {dims.xPos-1, dims.yPos-1},
                {dims.xPos+dims.width+1, dims.yPos+dims.height+1},
                {1.0f, 1.0f, 1.0f, 0.8f});
        // Render button
        g_uiRenderer->renderFilledRectangle(
                {dims.xPos, dims.yPos},
                {dims.xPos+dims.width, dims.yPos+dims.height},
                buttonColor);
        // Render button text
        g_textRenderer->renderString(getBtnText(m_btnInfo[i]), {m_btnTxtDims[i].xPos, m_btnTxtDims[i].yPos});
    }

    TIMER_END_FUNC();
}

void MessageDialog::handleKey(int key, int mods)
{
    if (mods != 0)
        return;

    for (size_t i{}; i < m_btnInfo.size(); ++i)
    {
        if (key == m_btnInfo[i].key)
        {
            m_pressedBtnI = i;
            m_isClosed = true;
            break;
        }
    }
}
