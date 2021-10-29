#include "Bindings.h"
#include "App.h"
#include "Buffer.h"
#include "TextRenderer.h"
#include "UiRenderer.h"
#include "Dialog.h"
#include "FileDialog.h"
#include "MessageDialog.h"
#include "globals.h"
#include "config.h"
#include <vector>
#include <memory>
#include <cstring>

namespace Bindings
{

void runBinding(BindingMapSet& map, int mods, int key)
{
    BindingMapSet::bindingFunc_t func;
    switch (mods)
    {
    case 0: /* No mod */                    func = map.noMod[key]; break;
    case GLFW_MOD_CONTROL:                  func = map.ctrl[key]; break;
    case GLFW_MOD_CONTROL | GLFW_MOD_SHIFT: func = map.ctrlShift[key]; break;
    default: break;
    }
    if (func)
    {
        func();
    }
}

namespace Callbacks
{

void createBufferInNewTab()
{
    if (g_tabs.empty())
    {
        g_tabs.push_back(std::make_unique<Split>(new Buffer));
        g_currTabI = 0;
        g_activeBuff = g_tabs[g_currTabI]->getActiveBufferRecursively();
    }
    else
    {
        // Insert the buffer next to the current one
        g_tabs.emplace(g_tabs.begin()+g_currTabI+1, std::make_unique<Split>(new Buffer));
        ++g_currTabI; // Go to the current buffer
        g_activeBuff = g_tabs[g_currTabI]->getActiveBufferRecursively();
    }
    g_isRedrawNeeded = true;
    g_isTitleUpdateNeeded = true;
}

static void askSaveFilenameDialogCb(int, Dialog* dlg, void* filePath)
{
    char* _filePath = (char*)filePath;
    auto askerDialog = dynamic_cast<AskerDialog*>(dlg);
    if (g_activeBuff->saveAsToFile(
                std_fs::path{_filePath}/std_fs::path{askerDialog->getValue()}))
    {
        MessageDialog::create(MessageDialog::EMPTY_CB, nullptr,
                "Failed to save file: \""+std::string(_filePath)+'"',
                MessageDialog::Type::Error);
    }
    delete[] _filePath;
    g_isTitleUpdateNeeded = true;
}

static void saveAsDialogCb(int, Dialog* dlg, void*)
{
    auto fileDialog = dynamic_cast<FileDialog*>(dlg);
    const std::string path = fileDialog->getSelectedFilePath();
    if (fileDialog->isDirSelected()) // ask filename
    {
        char* _path = new char[path.size()+1]{};
        strncpy(_path, path.data(), path.size());
        AskerDialog::create(askSaveFilenameDialogCb, (void*)_path, "Filename:");
    }
    else // Save to an existing file
    {
        if (g_activeBuff->saveAsToFile(path))
        {
            MessageDialog::create(MessageDialog::EMPTY_CB, nullptr,
                    "Failed to save file: \""+fileDialog->getSelectedFilePath()+'"',
                    MessageDialog::Type::Error);
        }
    }
    g_isTitleUpdateNeeded = true;
}

void saveCurrentBuffer()
{
    if (g_activeBuff)
    {
        if (g_activeBuff->isNewFile())
        {
            // Open a save as dialog
            FileDialog::create(saveAsDialogCb, nullptr, ".", FileDialog::Type::Save);
        }
        else
        {
            if (g_activeBuff->saveToFile())
            {
                MessageDialog::create(MessageDialog::EMPTY_CB, nullptr,
                        "Failed to save file",
                        MessageDialog::Type::Error);
            }
        }
        g_isRedrawNeeded = true;
    }
}

void saveCurrentBufferAs()
{
    if (g_activeBuff)
    {
        // Open a save as dialog
        FileDialog::create(saveAsDialogCb, nullptr, ".", FileDialog::Type::Save);
        g_isRedrawNeeded = true;
    }
}

static void openDialogCb(int openMode, Dialog* dlg, void*)
{
    auto fileDialog = dynamic_cast<FileDialog*>(dlg);
    auto path = fileDialog->getSelectedFilePath();
    Buffer* buffer = App::openFileInNewBuffer(path);

    if (openMode == FileDialog::OPENMODE_NEWTAB || g_tabs.empty()) // Open in new tab
    {
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
    }
    else // Open in split
    {
        g_tabs[g_currTabI]->addChild(buffer);
        g_activeBuff = g_tabs[g_currTabI]->getActiveBufferRecursively();
    }
    g_isTitleUpdateNeeded = true;
}

void openFile()
{
    FileDialog::create(openDialogCb, nullptr, ".", FileDialog::Type::Open);
    g_isRedrawNeeded = true;
}

static void askSaveCloseDialogCb(int btn, Dialog*, void*)
{
    if (btn == 0) // If pressed "Yes"
    {
        Bindings::Callbacks::saveCurrentBuffer();
        Bindings::Callbacks::closeActiveBuffer();
    }
    else if (btn == 1) // Pressed "No"
    {
        g_activeBuff->setModified(false); // Drop changes
        Bindings::Callbacks::closeActiveBuffer();
    }
}

void closeActiveBuffer()
{
    if (!g_activeBuff)
        return;

    if (g_activeBuff->isModified())
    {
        MessageDialog::create(
                askSaveCloseDialogCb, nullptr,
                "Save?",
                MessageDialog::Type::Information,
                std::vector<MessageDialog::BtnInfo>{
                {"Yes", GLFW_KEY_Y}, {"No", GLFW_KEY_N}, {"Cancel", GLFW_KEY_C}}
                );
    }
    else
    {
        g_tabs[g_currTabI]->closeActiveBufferRecursively();
        if (g_tabs.empty())
        {
            g_currTabI = 0;
            g_activeBuff = nullptr;
        }
        else
        {
            if (g_currTabI >= g_tabs.size())
            {
                g_currTabI = g_tabs.size()-1;
            }
            g_activeBuff = g_tabs[g_currTabI]->getActiveBufferRecursively();
            if (!g_activeBuff)
            {
                g_tabs.erase(g_tabs.begin()+g_currTabI);
                g_currTabI = g_tabs.empty() ? 0 : std::min(g_currTabI, g_tabs.size()-1);
                if (!g_tabs.empty() && g_tabs[g_currTabI])
                {
                    g_activeBuff = g_tabs[g_currTabI]->getActiveBufferRecursively();
                }
                else
                {
                    g_activeBuff = nullptr;
                }
            }
        }
        g_isTitleUpdateNeeded = true;
    }
    g_isRedrawNeeded = true;
}

void openPathAtCursor()
{
    if (g_activeBuff)
    {
        std::string path = strToAscii(g_activeBuff->getCursorWord());
        if (path.empty())
            return;
        if (path[0] == '"') path = path.substr(1);
        if (path[path.size()-1] == '"') path = path.substr(0, path.size()-1);

        if (path[0] != '/')
            path = getParentPath(g_activeBuff->getFilePath())/std::filesystem::path{path};

        Logger::dbg << "Checking path: " << path << Logger::End;
        if (!isValidFilePath(path))
            return;

        auto* buffer = App::openFileInNewBuffer(path);
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
        g_isRedrawNeeded = true;
    }
}

void goToNextTab()
{
    if (!g_tabs.empty() && g_currTabI < g_tabs.size()-1)
    {
        ++g_currTabI;
        g_activeBuff = g_tabs[g_currTabI]->getActiveBufferRecursively();
    }
    g_isRedrawNeeded = true;
    g_isTitleUpdateNeeded = true;
}

void goToPrevTab()
{
    if (g_currTabI > 0)
    {
        --g_currTabI;
        g_activeBuff = g_tabs[g_currTabI]->getActiveBufferRecursively();
    }
    g_isRedrawNeeded = true;
    g_isTitleUpdateNeeded = true;
}

void increaseActiveBufferWidth()
{
    if (g_tabs.size() > 1
        && g_tabs[g_currTabI]->getActiveChildI()+1 < g_tabs[g_currTabI]->getNumOfChildren())
    {
        g_tabs[g_currTabI]->increaseChildWidth(g_tabs[g_currTabI]->getActiveChildI(), 10);
        g_isRedrawNeeded = true;
    }
}

void decreaseActiveBufferWidth()
{
    if (g_tabs.size() > 1
        && g_tabs[g_currTabI]->getActiveChildI()+1 < g_tabs[g_currTabI]->getNumOfChildren())
    {
        g_tabs[g_currTabI]->increaseChildWidth(g_tabs[g_currTabI]->getActiveChildI(), -10);
        g_isRedrawNeeded = true;
    }
}

void goToNextSplit()
{
    if (!g_activeBuff)
        return;

    auto& tab = g_tabs[g_currTabI];
    if (tab->getNumOfChildren() < 2)
        return;
    // If focused on the last child, focus the first one
    if (tab->getActiveChildI() == tab->getNumOfChildren()-1)
    {
        tab->setActiveChildI(0);
    }
    // Else: focus on the next one
    else
    {
        tab->setActiveChildI(tab->getActiveChildI()+1);
    }
    g_activeBuff = tab->getActiveBufferRecursively();
    g_isRedrawNeeded = true;
    g_isTitleUpdateNeeded = true;
}

void goToPrevSplit()
{
    if (!g_activeBuff)
        return;

    auto& tab = g_tabs[g_currTabI];
    if (tab->getNumOfChildren() < 2)
        return;
    // If focused on the first child, focus the last one
    if (tab->getActiveChildI() == 0)
    {
        tab->setActiveChildI(tab->getNumOfChildren()-1);
    }
    // Else: focus on the prev. one
    else
    {
        tab->setActiveChildI(tab->getActiveChildI()-1);
    }
    g_activeBuff = tab->getActiveBufferRecursively();
    g_isRedrawNeeded = true;
    g_isTitleUpdateNeeded = true;
}

void moveCursorRight()
{
    if (g_activeBuff)
    {
        g_activeBuff->moveCursor(Buffer::CursorMovCmd::Right);
        g_isRedrawNeeded = true;
    }
}

void moveCursorLeft()
{
    if (g_activeBuff)
    {
        g_activeBuff->moveCursor(Buffer::CursorMovCmd::Left);
        g_isRedrawNeeded = true;
    }
}

void moveCursorDown()
{
    if (g_activeBuff)
    {
        g_activeBuff->moveCursor(Buffer::CursorMovCmd::Down);
        g_isRedrawNeeded = true;
    }
}

void moveCursorUp()
{
    if (g_activeBuff)
    {
        g_activeBuff->moveCursor(Buffer::CursorMovCmd::Up);
        g_isRedrawNeeded = true;
    }
}

void moveCursorToLineBeginning()
{
    if (g_activeBuff)
    {
        g_activeBuff->moveCursor(Buffer::CursorMovCmd::LineBeginning);
        g_isRedrawNeeded = true;
    }
}

void moveCursorToLineEnd()
{
    if (g_activeBuff)
    {
        g_activeBuff->moveCursor(Buffer::CursorMovCmd::LineEnd);
        g_isRedrawNeeded = true;
    }
}

void putEnter()
{
    App::windowCharCB(nullptr, '\n');
}

void deleteCharBackwards()
{
    if (g_activeBuff)
    {
        g_activeBuff->deleteCharBackwards();
        g_isRedrawNeeded = true;
    }
}

void deleteCharForward()
{
    if (g_activeBuff)
    {
        g_activeBuff->deleteCharForward();
        g_isRedrawNeeded = true;
    }
}

void insertTabOrSpaces()
{
    if (g_activeBuff)
    {
        if (TAB_SPACE_COUNT < 1)
        {
            g_activeBuff->insert('\t');
        }
        else
        {
            for (int i{}; i < TAB_SPACE_COUNT; ++i)
            {
                g_activeBuff->insert(' ');
            }
        }
        g_isRedrawNeeded = true;
    }
}

void goToFirstChar()
{
    if (g_activeBuff)
    {
        g_activeBuff->moveCursor(Buffer::CursorMovCmd::FirstChar);
        g_isRedrawNeeded = true;
    }
}

void goToLastChar()
{
    if (g_activeBuff)
    {
        g_activeBuff->moveCursor(Buffer::CursorMovCmd::LastChar);
        g_isRedrawNeeded = true;
    }
}

void undoActiveBufferChange()
{
    if (g_activeBuff)
    {
        g_activeBuff->undo();
    }
}

void redoActiveBufferChange()
{
    if (g_activeBuff)
    {
        g_activeBuff->redo();
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

void zoomInBufferIfImage()
{
    if (!g_activeBuff)
        return;

    if (auto* img = dynamic_cast<ImageBuffer*>(g_activeBuff))
    {
        img->zoomBy(IMG_BUF_ZOOM_STEP);
    }
    g_isRedrawNeeded = true;
}

void zoomOutBufferIfImage()
{
    if (!g_activeBuff)
        return;

    if (auto* img = dynamic_cast<ImageBuffer*>(g_activeBuff))
    {
        img->zoomBy(-IMG_BUF_ZOOM_STEP);
    }
    g_isRedrawNeeded = true;
}

void triggerAutocompPopupOrSelectNextItem()
{
    if (!g_activeBuff)
        return;

    g_activeBuff->triggerAutocompPopupOrSelectNextItem();
    g_isRedrawNeeded = true;
}

void triggerAutocompPopupOrSelectPrevItem()
{
    if (!g_activeBuff)
        return;

    g_activeBuff->triggerAutocompPopupOrSelectPrevItem();
    g_isRedrawNeeded = true;
}

void hideAutocompPopupOrEndSelection()
{
    if (!g_activeBuff)
        return;

    /*
     * This function is bound to <Escape>, which is used to cancel
     * different things based on the editor state.
     * If the autocomplete popup is shown, close it. Else, end selection.
     *
     * Maybe more things will be added in this function in the future.
     */

    if (g_activeBuff->isAutocompPopupShown())
    {
        g_activeBuff->hideAutocompPopup();
    }
    else
    {
        g_activeBuff->closeSelection();
    }
    g_isRedrawNeeded = true;
}

void bufferStartNormalSelection()
{
    if (!g_activeBuff)
        return;

    g_activeBuff->startSelection(Buffer::Selection::Mode::Normal);
    g_isRedrawNeeded = true;
}

void bufferStartLineSelection()
{
    if (!g_activeBuff)
        return;

    g_activeBuff->startSelection(Buffer::Selection::Mode::Line);
    g_isRedrawNeeded = true;
}

} // Namespace Callbacks

BindingMapSet mappings = {};

} // Namespace Bindings
