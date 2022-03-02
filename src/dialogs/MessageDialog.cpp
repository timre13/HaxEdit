#include "MessageDialog.h"
#include "../UiRenderer.h"
#include "../TextRenderer.h"
#include "../Logger.h"
#include "../Timer.h"
#include "../config.h"
#include "../common/string.h"
#include "../globals.h"
#include <algorithm>

MessageDialog::MessageDialog(
        callback_t cb,
        void* cbUserData,
        const std::string& msg,
        Type type,
        const std::vector<BtnInfo>& btns
        )
    : Dialog{cb, cbUserData}, m_message{msg+'\n'}, m_type{type}, m_btnInfo{btns}
{
    Logger::dbg << "Opened a message dialog with message: " << quoteStr(msg) << Logger::End;
    for (const auto& btn : m_btnInfo)
    {
        (void)btn; // Fix unused `btn` warning in release build
        assert(btn.key != GLFW_KEY_UNKNOWN);
        assert(btn.key != GLFW_KEY_ESCAPE); // Escape is a special key that cancels the dialog
    }
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
    const int widestBtnW = std::max_element(m_btnInfo.begin(), m_btnInfo.end(),
            [](const BtnInfo& a, const BtnInfo& b){ return getBtnText(a).size() < getBtnText(b).size(); }
            )->label.size()*g_fontWidthPx+20;

    m_msgTextDims.width = getLongestLineLen(m_message)*g_fontWidthPx;
    m_msgTextDims.height = strCountLines(m_message)*g_fontSizePx;

    m_dialogDims.width = std::max(std::max(m_msgTextDims.width, widestBtnW)+10+10, 200);
    m_dialogDims.height = 10+m_msgTextDims.height+10+(g_fontSizePx+30)*m_btnInfo.size();
    m_dialogDims.xPos = g_windowWidth/2-m_dialogDims.width/2;
    m_dialogDims.yPos = g_windowHeight/2-m_dialogDims.height/2;

    m_msgTextDims.xPos = m_dialogDims.xPos+m_dialogDims.width/2-m_msgTextDims.width/2;
    m_msgTextDims.yPos = m_dialogDims.yPos+10;

    m_btnDims.clear();
    m_btnTxtDims.clear();
    for (const auto& btn : m_btnInfo)
    {
        m_btnDims.emplace_back();
        m_btnTxtDims.emplace_back();

        m_btnTxtDims.back().width = getBtnText(btn).size()*g_fontWidthPx;
        m_btnTxtDims.back().height = g_fontSizePx;
        m_btnDims.back().width = m_btnTxtDims.back().width+10+10;
        m_btnDims.back().height = m_btnTxtDims.back().height+10+10;
        m_btnDims.back().xPos = g_windowWidth/2-m_btnDims.back().width/2;
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

    recalculateDimensions();

    RGBColor dialogColor;
    RGBAColor buttonColor;
    switch (m_type)
    {
    case Type::Information:
        dialogColor = {0.5f, 0.7f, 1.0f};
        buttonColor = {0.6f, 0.8f, 0.9f, 0.8f};
        break;

    case Type::Warning:
        dialogColor = {0.8f, 0.5f, 0.0f};
        buttonColor = {0.9f, 0.6f, 0.4f, 0.8f};
        break;

    case Type::Error:
        dialogColor = {1.0f, 0.3f, 0.2f};
        buttonColor = {0.9f, 0.3f, 0.3f, 0.8f};
        break;
    }

    renderBase(dialogColor);
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

    if (key == GLFW_KEY_ESCAPE)
    {
        Logger::dbg << "MessageDialog: Cancelled" << Logger::End;
        m_isClosed = true;
        return;
    }

    for (size_t i{}; i < m_btnInfo.size(); ++i)
    {
        if (key == m_btnInfo[i].key)
        {
            m_isClosed = true;
            if (m_callback)
                m_callback(i, this, m_cbUserData);
            break;
        }
    }
}

bool MessageDialog::isInsideButton(const glm::ivec2& pos) const
{
    for (const auto& btn : m_btnDims)
    {
        if (pos.x >= btn.xPos
         && pos.x < btn.xPos+btn.width
         && pos.y >= btn.yPos
         && pos.y < btn.yPos+btn.height)
        {
            return true;
        }
    }
    return false;
}

void MessageDialog::pressButtonAt(const glm::ivec2 pos)
{
    for (size_t i{}; i < m_btnInfo.size(); ++i)
    {
        const auto& btn = m_btnDims[i];
        if (pos.x >= btn.xPos
         && pos.x < btn.xPos+btn.width
         && pos.y >= btn.yPos
         && pos.y < btn.yPos+btn.height)
        {
            m_isClosed = true;
            if (m_callback)
                m_callback(i, this, m_cbUserData);
            break;
        }
    }
}
