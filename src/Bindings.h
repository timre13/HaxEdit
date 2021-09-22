#pragma once

#include "glstuff.h"
#include <functional>
#include <vector>
#include <memory>
#include "App.h"
#include "Buffer.h"
#include "TextRenderer.h"
#include "UiRenderer.h"
#include "Dialog.h"
#include "FileDialog.h"
#include "MessageDialog.h"

extern int g_windowWidth;
extern int g_windowHeight;
extern bool g_isRedrawNeeded;
extern bool g_isTitleUpdateNeeded;
extern bool g_isDebugDrawMode;
extern std::vector<std::unique_ptr<Buffer>> g_buffers;
extern size_t g_currentBufferI;
extern std::vector<std::unique_ptr<Dialog>> g_dialogs;

namespace Bindings
{

using bindingMap_t = std::function<void()>[GLFW_KEY_LAST];

void runBinding(bindingMap_t& map, int binding)
{
    auto found = map[binding];
    if (found)
    {
        found();
    }
}

namespace Callbacks
{

void createNewBuffer()
{
    if (g_buffers.empty())
    {
        g_buffers.emplace_back(new Buffer{});
        g_currentBufferI = 0;
    }
    else
    {
        // Insert the buffer next to the current one
        g_buffers.emplace(g_buffers.begin()+g_currentBufferI+1, new Buffer{});
        ++g_currentBufferI; // Go to the current buffer
    }
    g_isRedrawNeeded = true;
    g_isTitleUpdateNeeded = true;
}

void saveCurrentBuffer()
{
    if (!g_buffers.empty())
    {
        if (g_buffers[g_currentBufferI]->isNewFile())
        {
            // Open a save as dialog
            g_dialogs.push_back(std::make_unique<FileDialog>(".", FileDialog::Type::SaveAs));
        }
        else
        {
            if (g_buffers[g_currentBufferI]->saveToFile())
            {
                g_dialogs.push_back(std::make_unique<MessageDialog>(
                            "Failed to save file",
                            MessageDialog::Type::Error));
            }
        }
        g_isRedrawNeeded = true;
    }
}

void saveCurrentBufferAs()
{
    if (!g_buffers.empty())
    {
        // Open a save as dialog
        g_dialogs.push_back(std::make_unique<FileDialog>(".", FileDialog::Type::SaveAs));
        g_isRedrawNeeded = true;
    }
}

void openFile()
{
    g_dialogs.push_back(std::make_unique<FileDialog>(".", FileDialog::Type::Open));
    g_isRedrawNeeded = true;
}

void closeCurrentBuffer()
{
    if (g_buffers.empty())
        return;

    if (g_buffers[g_currentBufferI]->isModified())
    {
        g_dialogs.push_back(std::make_unique<MessageDialog>(
                    "Save?",
                    MessageDialog::Type::Information,
                    MessageDialog::Id::AskSaveCloseCurrentBuffer,
                    std::vector<MessageDialog::BtnInfo>{{"Yes", GLFW_KEY_Y}, {"No", GLFW_KEY_N}, {"Cancel", GLFW_KEY_C}}
        ));
    }
    else
    {
        g_buffers.erase(g_buffers.begin()+g_currentBufferI);
        if (g_buffers.size() == 0)
        {
            g_currentBufferI = 0;
        }
        else if (g_currentBufferI >= g_buffers.size())
        {
            g_currentBufferI = g_buffers.size()-1;
        }
        g_isTitleUpdateNeeded = true;
    }
    g_isRedrawNeeded = true;
}

void goToNextTab()
{
    if (!g_buffers.empty() && g_currentBufferI < g_buffers.size()-1)
        ++g_currentBufferI;
    g_isRedrawNeeded = true;
    g_isTitleUpdateNeeded = true;
}

void goToPrevTab()
{
    if (g_currentBufferI > 0)
        --g_currentBufferI;
    g_isRedrawNeeded = true;
    g_isTitleUpdateNeeded = true;
}

void moveCursorRight()
{
    if (!g_buffers.empty())
    {
        g_buffers[g_currentBufferI]->moveCursor(Buffer::CursorMovCmd::Right);
        g_isRedrawNeeded = true;
    }
}

void moveCursorLeft()
{
    if (!g_buffers.empty())
    {
        g_buffers[g_currentBufferI]->moveCursor(Buffer::CursorMovCmd::Left);
        g_isRedrawNeeded = true;
    }
}

void moveCursorDown()
{
    if (!g_buffers.empty())
    {
        g_buffers[g_currentBufferI]->moveCursor(Buffer::CursorMovCmd::Down);
        g_isRedrawNeeded = true;
    }
}

void moveCursorUp()
{
    if (!g_buffers.empty())
    {
        g_buffers[g_currentBufferI]->moveCursor(Buffer::CursorMovCmd::Up);
        g_isRedrawNeeded = true;
    }
}

void moveCursorToLineBeginning()
{
    if (!g_buffers.empty())
    {
        g_buffers[g_currentBufferI]->moveCursor(Buffer::CursorMovCmd::LineBeginning);
        g_isRedrawNeeded = true;
    }
}

void moveCursorToLineEnd()
{
    if (!g_buffers.empty())
    {
        g_buffers[g_currentBufferI]->moveCursor(Buffer::CursorMovCmd::LineEnd);
        g_isRedrawNeeded = true;
    }
}

void putEnter()
{
    App::windowCharCB(nullptr, '\n');
}

void deleteCharBackwards()
{
    if (!g_buffers.empty())
    {
        g_buffers[g_currentBufferI]->deleteCharBackwards();
        g_isRedrawNeeded = true;
    }
}

void deleteCharForward()
{
    if (!g_buffers.empty())
    {
        g_buffers[g_currentBufferI]->deleteCharForward();
        g_isRedrawNeeded = true;
    }
}

void insertTabOrSpaces()
{
    if (!g_buffers.empty())
    {
        if (TAB_SPACE_COUNT < 1)
        {
            g_buffers[g_currentBufferI]->insert('\t');
        }
        else
        {
            for (int i{}; i < TAB_SPACE_COUNT; ++i)
            {
                g_buffers[g_currentBufferI]->insert(' ');
            }
        }
        g_isRedrawNeeded = true;
    }
}

void goToFirstChar()
{
    if (!g_buffers.empty())
    {
        g_buffers[g_currentBufferI]->moveCursor(Buffer::CursorMovCmd::FirstChar);
        g_isRedrawNeeded = true;
    }
}

void goToLastChar()
{
    if (!g_buffers.empty())
    {
        g_buffers[g_currentBufferI]->moveCursor(Buffer::CursorMovCmd::LastChar);
        g_isRedrawNeeded = true;
    }
}

void increaseFontSize()
{
    ++g_fontSizePx;
    g_textRenderer->setFontSize(g_fontSizePx);
    g_isRedrawNeeded = true;
}

void decreaseFontSize()
{
    --g_fontSizePx;
    if (g_fontSizePx <= 1)
        g_fontSizePx = 1;
    g_textRenderer->setFontSize(g_fontSizePx);
    g_isRedrawNeeded = true;
}

}

bindingMap_t noModMap = {};
bindingMap_t ctrlMap = {};
bindingMap_t ctrlShiftMap = {};

}
