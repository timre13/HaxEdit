#include "Prompt.h"
#include "common/string.h"
#include "dialogs/Dialog.h"
#include "UiRenderer.h"
#include "TextRenderer.h"
#include "App.h"
#include "Bindings.h"
#include "globals.h"

#define PROMPT_ANIM_LEN_MS (100)

void Prompt::runCommand()
{
    auto getWord{[](const String& str, size_t start=0){
        String out;
        for (size_t i=start; i < str.size(); ++i)
        {
            Char c = str[i];
            if (c == U' ')
                break;
            out += c;
        }
        return out;
    }};

    const String cmd = getWord(m_buffer);
    const String args = (m_buffer.length() <= cmd.length()+1) ? U"" : m_buffer.substr(cmd.length()+1);
    if (cmd == U"tabe")
    {
        if (args.empty())
            goto err_arg_req;

        auto buffer = App::openFileInNewBuffer(strToAscii(args));
        if (g_tabs.empty())
        {
            g_tabs.push_back(std::make_unique<Split>(buffer));
            g_activeBuff = buffer;
            g_currTabI = 0;
        }
        else
        {
            // Insert the buffer next to the current one
            g_tabs.insert(g_tabs.begin()+g_currTabI+1, std::make_unique<Split>(buffer));
            g_activeBuff = buffer;
            ++g_currTabI; // Go to the current buffer
        }
        g_isTitleUpdateNeeded = true;
        g_isRedrawNeeded = true;
    }
    else if (cmd == U"q" || cmd == U"quit")
    {
        if (!args.empty())
            goto err_arg_not_req;

        Bindings::Callbacks::closeActiveBuffer();
    }
    else if (cmd == U"w" || cmd == U"write")
    {
        if (!args.empty())
            goto err_arg_not_req;

        Bindings::Callbacks::saveCurrentBuffer();
    }
    else if (cmd == U"o" || cmd == U"open")
    {
        if (!args.empty())
            goto err_arg_not_req;

        Bindings::Callbacks::openFile();
    }
    else
    {
        g_statMsg.set("Unknown command", StatusMsg::Type::Error);
    }

    goto end;

err_arg_req:
    g_statMsg.set("Argument required", StatusMsg::Type::Error);
    goto end;

err_arg_not_req:
    g_statMsg.set("Argument NOT required", StatusMsg::Type::Error);
    goto end;

end:;
}

void Prompt::render() const
{
    if (!isOpen())
        return;

    const glm::ivec2 pos1 = {g_windowWidth/5, 0};
    const glm::ivec2 pos2 = {g_windowWidth/5.f*4, (g_fontSizePx+40)*m_slideVal};
    g_uiRenderer->renderFilledRectangle(
            pos1, pos2,
            {0.2f, 0.3f, 0.5f, 0.6f}
    );

    g_textRenderer->renderString(
            ">", {pos1.x, (pos1.y+pos2.y)/2-g_fontSizePx/2}, FONT_STYLE_BOLD);

    g_textRenderer->renderString(
            strToAscii(m_buffer), {pos1.x+g_fontWidthPx*1.5f, (pos1.y+pos2.y)/2-g_fontSizePx/2});

}

void Prompt::update(float frameTimeMs)
{
    const float valChange = frameTimeMs/PROMPT_ANIM_LEN_MS;

    if (m_isSlideDirDown)
    {
        if (m_slideVal >= 1)
            return;

        m_slideVal += valChange;
        g_isRedrawNeeded = true;
    }
    else
    {
        if (m_slideVal <= 0)
            return;

        m_slideVal -= valChange;
        g_isRedrawNeeded = true;
    }
}

void Prompt::toggleWithAnim()
{
    m_isSlideDirDown = !m_isSlideDirDown;
    m_buffer.clear();
}

void Prompt::hideWithAnim()
{
    m_isSlideDirDown = false;
    m_buffer.clear();
}

void Prompt::showWithAnim()
{
    m_isSlideDirDown = true;
    m_buffer.clear();
}

void Prompt::handleKey(int key, int mods)
{
    if (mods == 0 && key == GLFW_KEY_ESCAPE)
    {
        hideWithAnim();
    }
    else if (mods == 0 && (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER))
    {
        m_isSlideDirDown = false; // Hide without clearing the buffer
        if (!m_buffer.empty())
            runCommand();
    }
    else if (mods == 0 && key == GLFW_KEY_BACKSPACE)
    {
        if (!m_buffer.empty())
            m_buffer.pop_back();
    }

    g_isRedrawNeeded = true;
}

void Prompt::handleChar(uint codePoint)
{
    if (m_buffer.empty() && codePoint == U':')
    {
        hideWithAnim();
        return;
    }

    m_buffer.push_back((char32_t)codePoint);
    g_isRedrawNeeded = true;
}
