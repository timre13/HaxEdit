#pragma once

#include <GLFW/glfw3.h>
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
extern std::vector<Buffer> g_buffers;
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
    g_buffers.emplace_back();
    g_currentBufferI = g_buffers.size()-1;
    g_isRedrawNeeded = true;
    g_isTitleUpdateNeeded = true;
}

void saveCurrentBuffer()
{
    if (!g_buffers.empty())
    {
        if (g_buffers[g_currentBufferI].isNewFile())
        {
            // Open a save as dialog
            g_dialogs.push_back(std::make_unique<FileDialog>(".", FileDialog::Type::SaveAs));
        }
        else
        {
            if (g_buffers[g_currentBufferI].saveToFile())
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
    // Open a save as dialog
    g_dialogs.push_back(std::make_unique<FileDialog>(".", FileDialog::Type::SaveAs));
}

void openFile()
{
    g_dialogs.push_back(std::make_unique<FileDialog>(".", FileDialog::Type::Open));
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
        g_buffers[g_currentBufferI].moveCursor(Buffer::CursorMovCmd::Right);
        // Show cursor while moving
        g_buffers[g_currentBufferI].setCursorVisibility(true);
        g_isRedrawNeeded = true;
    }
}

void moveCursorLeft()
{
    if (!g_buffers.empty())
    {
        g_buffers[g_currentBufferI].moveCursor(Buffer::CursorMovCmd::Left);
        // Show cursor while moving
        g_buffers[g_currentBufferI].setCursorVisibility(true);
        g_isRedrawNeeded = true;
    }
}

void moveCursorDown()
{
    if (!g_buffers.empty())
    {
        g_buffers[g_currentBufferI].moveCursor(Buffer::CursorMovCmd::Down);
        // Show cursor while moving
        g_buffers[g_currentBufferI].setCursorVisibility(true);
        g_isRedrawNeeded = true;
    }
}

void moveCursorUp()
{
    if (!g_buffers.empty())
    {
        g_buffers[g_currentBufferI].moveCursor(Buffer::CursorMovCmd::Up);
        // Show cursor while moving
        g_buffers[g_currentBufferI].setCursorVisibility(true);
        g_isRedrawNeeded = true;
    }
}

void moveCursorToLineBeginning()
{
    if (!g_buffers.empty())
    {
        g_buffers[g_currentBufferI].moveCursor(Buffer::CursorMovCmd::LineBeginning);
        // Show cursor while moving
        g_buffers[g_currentBufferI].setCursorVisibility(true);
        g_isRedrawNeeded = true;
    }
}

void moveCursorToLineEnd()
{
    if (!g_buffers.empty())
    {
        g_buffers[g_currentBufferI].moveCursor(Buffer::CursorMovCmd::LineEnd);
        // Show cursor while moving
        g_buffers[g_currentBufferI].setCursorVisibility(true);
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
        g_buffers[g_currentBufferI].deleteCharBackwards();
        // Show cursor while typing
        g_buffers[g_currentBufferI].setCursorVisibility(true);
        g_isRedrawNeeded = true;
    }
}

void deleteCharForward()
{
    if (!g_buffers.empty())
    {
        g_buffers[g_currentBufferI].deleteCharForward();
        // Show cursor while typing
        g_buffers[g_currentBufferI].setCursorVisibility(true);
        g_isRedrawNeeded = true;
    }
}

void insertTabOrSpaces()
{
    if (!g_buffers.empty())
    {
        if (TAB_SPACE_COUNT < 1)
        {
            g_buffers[g_currentBufferI].insert('\t');
        }
        else
        {
            for (int i{}; i < TAB_SPACE_COUNT; ++i)
            {
                g_buffers[g_currentBufferI].insert(' ');
            }
        }
        // Show cursor while typing
        g_buffers[g_currentBufferI].setCursorVisibility(true);
        g_isRedrawNeeded = true;
    }
}

void goToFirstChar()
{
    if (!g_buffers.empty())
    {
        g_buffers[g_currentBufferI].moveCursor(Buffer::CursorMovCmd::FirstChar);
        g_buffers[g_currentBufferI].setCursorVisibility(true);
        g_isRedrawNeeded = true;
    }
}

void goToLastChar()
{
    if (!g_buffers.empty())
    {
        g_buffers[g_currentBufferI].moveCursor(Buffer::CursorMovCmd::LastChar);
        g_buffers[g_currentBufferI].setCursorVisibility(true);
        g_isRedrawNeeded = true;
    }
}

}

bindingMap_t noModMap = {};
bindingMap_t ctrlMap = {};
bindingMap_t ctrlShiftMap = {};

}
