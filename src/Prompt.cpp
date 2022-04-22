#include "Prompt.h"
#include "dialogs/Dialog.h"
#include "UiRenderer.h"
#include "TextRenderer.h"
#include "globals.h"

#define PROMPT_ANIM_LEN_MS (100)

void Prompt::runCommand()
{
    // TODO
    Logger::log << "TODO" << Logger::End;
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

void Prompt::handleKey(int key, int mods)
{
    if (mods == 0 && key == GLFW_KEY_ESCAPE)
    {
        hideWithAnim();
    }
    else if (mods == 0 && (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER))
    {
        runCommand();
    }
}

void Prompt::handleChar(uint codePoint)
{
    // TODO
    Logger::log << "TODO" << Logger::End;
}
