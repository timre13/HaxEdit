#include "Bindings.h"
#include "App.h"
#include "Buffer.h"
#include "TextRenderer.h"
#include "UiRenderer.h"
#include "dialogs/Dialog.h"
#include "dialogs/FileDialog.h"
#include "dialogs/MessageDialog.h"
#include "dialogs/FindDialog.h"
#include "common/string.h"
#include "globals.h"
#include "config.h"
#include "modes.h"
#include <vector>
#include <memory>
#include <cstring>
#include <ctime>
#include <unicode/uchar.h>

#define BINDINGS_VERBOSE 1

namespace Bindings
{

bindingMap_t nmap = {};
bindingMap_t imap = {};
bindingMap_t* activeBindingMap = nullptr;

BindingKey lastPressedKey{};
long long keyEventDelay{};

Bindings::bindingMap_t* getBindingsForMode(EditorMode::_EditorMode mode)
{
    switch (mode)
    {
    case EditorMode::_EditorMode::Normal:
        return &nmap;

    case EditorMode::_EditorMode::Insert:
        return &imap;

    case EditorMode::_EditorMode::Replace:
        return &imap;
    }
    return nullptr;
}

void registerBinding(EditorMode::_EditorMode mode, const BindingKey::key_t& key, BindingKey::mods_t modifiers, bindingFunc_t func)
{
    assert(func);
    assert(!key.empty());
    auto* bindings = getBindingsForMode(mode);
    BindingKey bkey{modifiers, key};
    const bool overwrote = !bindings->insert_or_assign(bkey, func).second;
    Logger::dbg << (overwrote ? "Remapping: " : "Mapping: ") << (String)bkey << " ==> " << func.target<rawBindingFunc_t*>() << Logger::End;
}

void keyCB(GLFWwindow*, int key, int scancode, int action, int mods)
{
    if (action == GLFW_RELEASE)
        return;

    lastPressedKey.mods = mods;

    switch (key)
    {
    case GLFW_KEY_SPACE: lastPressedKey.key = U"<Space>"; break;
    case GLFW_KEY_ESCAPE: lastPressedKey.key = U"<Escape>"; break;
    case GLFW_KEY_ENTER: lastPressedKey.key = U"<Enter>"; break;
    case GLFW_KEY_TAB: lastPressedKey.key = U"<Tab>"; break;
    case GLFW_KEY_BACKSPACE: lastPressedKey.key = U"<Backspace>"; break;
    case GLFW_KEY_INSERT: lastPressedKey.key = U"<Insert>"; break;
    case GLFW_KEY_DELETE: lastPressedKey.key = U"<Delete>"; break;
    case GLFW_KEY_RIGHT: lastPressedKey.key = U"<Right>"; break;
    case GLFW_KEY_LEFT: lastPressedKey.key = U"<Left>"; break;
    case GLFW_KEY_DOWN: lastPressedKey.key = U"<Down>"; break;
    case GLFW_KEY_UP: lastPressedKey.key = U"<Up>"; break;
    case GLFW_KEY_PAGE_UP: lastPressedKey.key = U"<Page_Up>"; break;
    case GLFW_KEY_PAGE_DOWN: lastPressedKey.key = U"<Page_Down>"; break;
    case GLFW_KEY_HOME: lastPressedKey.key = U"<Home>"; break;
    case GLFW_KEY_END: lastPressedKey.key = U"<End>"; break;
    case GLFW_KEY_CAPS_LOCK: lastPressedKey.key = U"<Caps_Lock>"; break;
    case GLFW_KEY_SCROLL_LOCK: lastPressedKey.key = U"<Scroll_Lock>"; break;
    case GLFW_KEY_NUM_LOCK: lastPressedKey.key = U"<Num_Lock>"; break;
    case GLFW_KEY_PRINT_SCREEN: lastPressedKey.key = U"<Print_Screen>"; break;
    case GLFW_KEY_PAUSE: lastPressedKey.key = U"<Pause>"; break;
    case GLFW_KEY_F1: lastPressedKey.key = U"<F1>"; break;
    case GLFW_KEY_F2: lastPressedKey.key = U"<F2>"; break;
    case GLFW_KEY_F3: lastPressedKey.key = U"<F3>"; break;
    case GLFW_KEY_F4: lastPressedKey.key = U"<F4>"; break;
    case GLFW_KEY_F5: lastPressedKey.key = U"<F5>"; break;
    case GLFW_KEY_F6: lastPressedKey.key = U"<F6>"; break;
    case GLFW_KEY_F7: lastPressedKey.key = U"<F7>"; break;
    case GLFW_KEY_F8: lastPressedKey.key = U"<F8>"; break;
    case GLFW_KEY_F9: lastPressedKey.key = U"<F9>"; break;
    case GLFW_KEY_F10: lastPressedKey.key = U"<F10>"; break;
    case GLFW_KEY_F11: lastPressedKey.key = U"<F11>"; break;
    case GLFW_KEY_F12: lastPressedKey.key = U"<F12>"; break;
    case GLFW_KEY_F13: lastPressedKey.key = U"<F13>"; break;
    case GLFW_KEY_F14: lastPressedKey.key = U"<F14>"; break;
    case GLFW_KEY_F15: lastPressedKey.key = U"<F15>"; break;
    case GLFW_KEY_F16: lastPressedKey.key = U"<F16>"; break;
    case GLFW_KEY_F17: lastPressedKey.key = U"<F17>"; break;
    case GLFW_KEY_F18: lastPressedKey.key = U"<F18>"; break;
    case GLFW_KEY_F19: lastPressedKey.key = U"<F19>"; break;
    case GLFW_KEY_F20: lastPressedKey.key = U"<F20>"; break;
    case GLFW_KEY_F21: lastPressedKey.key = U"<F21>"; break;
    case GLFW_KEY_F22: lastPressedKey.key = U"<F22>"; break;
    case GLFW_KEY_F23: lastPressedKey.key = U"<F23>"; break;
    case GLFW_KEY_F24: lastPressedKey.key = U"<F24>"; break;
    case GLFW_KEY_F25: lastPressedKey.key = U"<F25>"; break;
    case GLFW_KEY_KP_0: lastPressedKey.key = U"<Kp_0>"; break;
    case GLFW_KEY_KP_1: lastPressedKey.key = U"<Kp_1>"; break;
    case GLFW_KEY_KP_2: lastPressedKey.key = U"<Kp_2>"; break;
    case GLFW_KEY_KP_3: lastPressedKey.key = U"<Kp_3>"; break;
    case GLFW_KEY_KP_4: lastPressedKey.key = U"<Kp_4>"; break;
    case GLFW_KEY_KP_5: lastPressedKey.key = U"<Kp_5>"; break;
    case GLFW_KEY_KP_6: lastPressedKey.key = U"<Kp_6>"; break;
    case GLFW_KEY_KP_7: lastPressedKey.key = U"<Kp_7>"; break;
    case GLFW_KEY_KP_8: lastPressedKey.key = U"<Kp_8>"; break;
    case GLFW_KEY_KP_9: lastPressedKey.key = U"<Kp_9>"; break;
    case GLFW_KEY_KP_DECIMAL: lastPressedKey.key = U"<Kp_Decimal>"; break;
    case GLFW_KEY_KP_DIVIDE: lastPressedKey.key = U"<Kp_Divide>"; break;
    case GLFW_KEY_KP_MULTIPLY: lastPressedKey.key = U"<Kp_Multiply>"; break;
    case GLFW_KEY_KP_SUBTRACT: lastPressedKey.key = U"<Kp_Subtract>"; break;
    case GLFW_KEY_KP_ADD: lastPressedKey.key = U"<Kp_Add>"; break;
    case GLFW_KEY_KP_ENTER: lastPressedKey.key = U"<Kp_Enter>"; break;
    case GLFW_KEY_KP_EQUAL: lastPressedKey.key = U"<Kp_Equal>"; break;
    case GLFW_KEY_MENU: lastPressedKey.key = U"<Menu>"; break;
    case GLFW_KEY_LEFT_SHIFT: lastPressedKey.key = U"<Left_Shift>"; break;
    case GLFW_KEY_LEFT_CONTROL: lastPressedKey.key = U"<Left_Control>"; break;
    case GLFW_KEY_LEFT_ALT: lastPressedKey.key = U"<Left_Alt>"; break;
    case GLFW_KEY_LEFT_SUPER: lastPressedKey.key = U"<Left_Super>"; break;
    case GLFW_KEY_RIGHT_SHIFT: lastPressedKey.key = U"<Right_Shift>"; break;
    case GLFW_KEY_RIGHT_CONTROL: lastPressedKey.key = U"<Right_Control>"; break;
    case GLFW_KEY_RIGHT_ALT: lastPressedKey.key = U"<Right_Alt>"; break;
    case GLFW_KEY_RIGHT_SUPER: lastPressedKey.key = U"<Right_Super>"; break;
    default:
        {
            if (const char* keyname = glfwGetKeyName(key, scancode))
            {
                lastPressedKey.key = icuStrToUtf32(icu::UnicodeString{keyname});
            }
            else
            {
                lastPressedKey.key = utf8To32("<"+std::to_string(key)+">");
            }
            break;
        }
    }

    /*
     * The key callback gets an incorrect character in case the key was pressed while holding down AltGr.
     * The char callback gets an incorrect character while holding down Ctrl.
     * The char callback overwrites the value received by the key callback, so both quirks are handled.
     *
     * When using some input methods the char callback is triggered
     * a few frames later than the key callback, so we need to wait.
     * This has been observed to be the case when using Fcitx. It adds 2 frames of delay.
     * If you don't want to disable the input method, add some delay here so you won't get events twice.
     *
     * TODO: Automatically detect delay and apply correction?
     */
    keyEventDelay = 0;
}

void charModsCB(GLFWwindow*, uint codepoint, int mods)
{
    lastPressedKey.mods = mods;

    if (codepoint > ' ' && codepoint != 127)
        lastPressedKey.key = charToStr((Char)codepoint);
    keyEventDelay = 0; // Don't wait any longer
}

static void handleDialogClose()
{
    if (g_dialogs.back()->isClosed())
    {
        g_dialogs.pop_back();
        g_isRedrawNeeded = true;
    }
}

void runBindingForFrame()
{
    --keyEventDelay;
    // If the waiting timed out or we received the char event and there is a key to process
    if (keyEventDelay > 0 || !lastPressedKey)
        return;

    const auto key = std::move(lastPressedKey); // TODO: Add move ctor
    lastPressedKey.clear();
    assert(lastPressedKey.key.empty() && lastPressedKey.mods == 0);

    //Logger::dbg << "PRESSED: " << (String)lastPressedKey << Logger::End;

#if HIDE_MOUSE_WHILE_TYPING
    // Hide cursor while typing
    glfwSetInputMode(g_window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
#endif

    // Debug mode toggle should work even when a dialog is open
    if (key.mods == 0 && key.key == U"<F3>")
    {
        App::toggleDebugDraw();
        return;
    }

    if (Prompt::get()->isOpen())
    {
        // TODO
        //Prompt::get()->handleKey(key, mods);
        //Prompt::get()->handleChar(codePoint);
        g_isRedrawNeeded = true;
        return;
    }

    // If there are dialogs open, pass the event to the top one
    if (!g_dialogs.empty())
    {
        g_dialogs.back()->handleKey(key);
        handleDialogClose();
        g_isRedrawNeeded = true;
        return;
    }

    // If j or the down key is pressed on the startup screen
    if (g_tabs.empty() && (key.key == U"j" || key.key == U"<Down>"))
    {
        g_recentFilePaths->selectNextItem();
        g_isRedrawNeeded = true;
        return;
    }

    // If k or the up key is pressed on the startup screen
    if (g_tabs.empty() && (key.key == U"k" || key.key == U"<Up>"))
    {
        g_recentFilePaths->selectPrevItem();
        g_isRedrawNeeded = true;
        return;
    }

    // If the Enter key is pressed on the startup screen
    if (g_tabs.empty() && key.key == U"<Enter>")
    {
        if (!g_recentFilePaths->isEmpty())
        {
            auto path = g_recentFilePaths->getSelectedItem();
            // Erase the selected recent file item, so it show will be at the end of the list
            // without pushing out other entries
            g_recentFilePaths->removeSelectedItem();

            Buffer* buff = App::openFileInNewBuffer(path);
            g_tabs.push_back(std::make_unique<Split>(buff));
            g_activeBuff = buff;
            g_currTabI = 0;
            g_isRedrawNeeded = true;
        }
        return;
    }

    const auto bindingIt = std::find_if(
            activeBindingMap->begin(), activeBindingMap->end(),
            [&](const auto& a){ return a.first == key; });
    if (bindingIt != activeBindingMap->end())
    {
        bindingIt->second();
        return;
    }

    // If the key was not bound, fall back to the default actions
    switch (g_editorMode.get())
    {
    case EditorMode::_EditorMode::Normal:
        return;

    case EditorMode::_EditorMode::Insert:
        if (g_activeBuff && !key.isFuncKey() && !(key.mods & (GLFW_MOD_CONTROL|GLFW_MOD_ALT)))
        {
            g_activeBuff->insertCharAtCursor(key.getAsChar());
        }
        break;

    case EditorMode::_EditorMode::Replace:
        if (g_activeBuff && !key.isFuncKey() && !(key.mods & (GLFW_MOD_CONTROL|GLFW_MOD_ALT)))
        {
            g_activeBuff->replaceCharAtCursor(key.getAsChar());
        }
        break;
    }
}

namespace Callbacks
{

void switchToNormalMode()
{
    g_editorMode.set(EditorMode::_EditorMode::Normal);
    if (g_activeBuff)
    {
        g_activeBuff->closeSelection();
        g_activeBuff->autocompPopupHide();
        g_activeBuff->setCursorVisibility(true);
        g_isRedrawNeeded = true;
    }
}

void switchToInsertMode()
{
    g_editorMode.set(EditorMode::_EditorMode::Insert);
    if (g_activeBuff)
    {
        g_activeBuff->closeSelection();
        g_activeBuff->autocompPopupHide();
        g_activeBuff->setCursorVisibility(true);
        g_isRedrawNeeded = true;
    }
}

void switchToReplaceMode()
{
    g_editorMode.set(EditorMode::_EditorMode::Replace);
    if (g_activeBuff)
    {
        g_activeBuff->closeSelection();
        g_activeBuff->autocompPopupHide();
        g_activeBuff->setCursorVisibility(true);
        g_isRedrawNeeded = true;
    }
}

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

static std::string genTempFilePath()
{
    const time_t time = std::time(nullptr);
    return (std::filesystem::temp_directory_path()
            / std::filesystem::path{"haxed_tmp_" + std::to_string(time)}
            ).string() + ".txt";
}

void createTempBufferInNewTab()
{
    // TODO: Render temporary buffer filenames differently in tab and status bar
    Buffer* buffer = new Buffer{};
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
    buffer->saveAsToFile(genTempFilePath());
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
            FileDialog::create(saveAsDialogCb, nullptr, FileDialog::Type::Save);
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
        FileDialog::create(saveAsDialogCb, nullptr, FileDialog::Type::Save);
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
    FileDialog::create(openDialogCb, nullptr, FileDialog::Type::Open);
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
                {"Yes", {0, U"y"}}, {"No", {0, U"n"}}, {"Cancel", {0, U"c"}}}
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

void openPathOrUrlAtCursor()
{
    if (!g_activeBuff)
        return;

    const String word = g_activeBuff->getCursorWord();
    std::string aword = utf32To8(word);
    if (aword.empty())
    {
        g_statMsg.set("No word at cursor", StatusMsg::Type::Error);
        return;
    }

    Logger::dbg << "Checking path/url: " << aword << Logger::End;

    if (isFormallyValidUrl(word))
    {
        g_statMsg.set("Opening URL in browser: \""+aword+"\"", StatusMsg::Type::Info);
        OS::openUrlInDefBrowser(aword);
        return;
    }
    else
    {
        if (aword[0] == '"') aword = aword.substr(1);
        if (aword[aword.size()-1] == '"') aword = aword.substr(0, aword.size()-1);

        if (aword[0] != '/')
            aword = getParentPath(g_activeBuff->getFilePath())/std::filesystem::path{aword};

        if (isValidFilePath(aword))
        {
            auto* buffer = App::openFileInNewBuffer(aword);
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
            return;
        }
    }

    g_statMsg.set("Not a valid file path or URL: "+quoteStr(utf32To8(word)), StatusMsg::Type::Error);
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
    if (!g_tabs.empty() && g_tabs[g_currTabI]->getNumOfChildren() > 1)
    {
        if (g_tabs[g_currTabI]->getActiveChildI()+1 < g_tabs[g_currTabI]->getNumOfChildren()) // If not last child
            g_tabs[g_currTabI]->increaseChildWidth(g_tabs[g_currTabI]->getActiveChildI(), 10); // Move right border
        else // If last child
            g_tabs[g_currTabI]->increaseChildWidth(g_tabs[g_currTabI]->getActiveChildI()-1, -10); // Move left border
        g_isRedrawNeeded = true;
    }
}

void decreaseActiveBufferWidth()
{
    if (!g_tabs.empty() && g_tabs[g_currTabI]->getNumOfChildren() > 1)
    {
        if (g_tabs[g_currTabI]->getActiveChildI()+1 < g_tabs[g_currTabI]->getNumOfChildren()) // If not last child
            g_tabs[g_currTabI]->increaseChildWidth(g_tabs[g_currTabI]->getActiveChildI(), -10); // Move right border
        else // If last child
            g_tabs[g_currTabI]->increaseChildWidth(g_tabs[g_currTabI]->getActiveChildI()-1, 10); // Move left border
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

void moveCursorRightAndEnterInsertMode()
{
    moveCursorRight();
    g_editorMode.set(EditorMode::_EditorMode::Insert);
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

void moveCursorToSWordEnd()
{
    if (g_activeBuff)
    {
        g_activeBuff->moveCursor(Buffer::CursorMovCmd::SWordEnd);
        g_isRedrawNeeded = true;
    }
}

void moveCursorToSWordBeginning()
{
    if (g_activeBuff)
    {
        g_activeBuff->moveCursor(Buffer::CursorMovCmd::SWordBeginning);
        g_isRedrawNeeded = true;
    }
}

void moveCursorToNextSWord()
{
    if (g_activeBuff)
    {
        g_activeBuff->moveCursor(Buffer::CursorMovCmd::SNextWord);
        g_isRedrawNeeded = true;
    }
}

void moveCursorToLineBeginningAndEnterInsertMode()
{
    moveCursorToLineBeginning();
    g_editorMode.set(EditorMode::_EditorMode::Insert);
}

void moveCursorToLineEndAndEnterInsertMode()
{
    moveCursorToLineEnd();
    g_editorMode.set(EditorMode::_EditorMode::Insert);
}

void putLineBreakAfterLineAndEnterInsertMode()
{
    if (g_activeBuff)
    {
        g_activeBuff->moveCursor(Buffer::CursorMovCmd::LineEnd);
        g_activeBuff->updateCursor();
        g_activeBuff->insertCharAtCursor('\n');
        g_editorMode.set(EditorMode::_EditorMode::Insert);
    }
}

void putLineBreakBeforeLineAndEnterInsertMode()
{
    if (g_activeBuff)
    {
        g_activeBuff->moveCursor(Buffer::CursorMovCmd::LineBeginning);
        g_activeBuff->updateCursor();
        g_activeBuff->insertCharAtCursor('\n');
        g_activeBuff->moveCursor(Buffer::CursorMovCmd::Up);
        g_activeBuff->updateCursor();
        g_editorMode.set(EditorMode::_EditorMode::Insert);
    }
}

void deleteCharBackwards()
{
    if (g_activeBuff)
    {
        g_activeBuff->deleteCharBackwards();
        g_isRedrawNeeded = true;
    }
}

void deleteCharForwardOrSelection()
{
    if (g_activeBuff)
    {
        g_activeBuff->deleteCharForwardOrSelected();
        g_isRedrawNeeded = true;
    }
}

void insertTabOrSpaces()
{
    if (g_activeBuff)
    {
        if (TAB_SPACE_COUNT < 1)
        {
            g_activeBuff->insertCharAtCursor('\t');
        }
        else
        {
            for (int i{}; i < TAB_SPACE_COUNT; ++i)
            {
                g_activeBuff->insertCharAtCursor(' ');
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

void goPageUp()
{
    if (g_activeBuff)
    {
        g_activeBuff->moveCursor(Buffer::CursorMovCmd::PageUp);
        g_isRedrawNeeded = true;
    }
}

void goPageDown()
{
    if (g_activeBuff)
    {
        g_activeBuff->moveCursor(Buffer::CursorMovCmd::PageDown);
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

void triggerAutocompPopup()
{
    if (!g_activeBuff)
        return;

    g_activeBuff->triggerAutocompPopup({});
    g_isRedrawNeeded = true;
}

void triggerAutocompPopupOrSelectNextItem()
{
    if (!g_activeBuff)
        return;

    if (g_activeBuff->isAutocompPopupShown())
    {
        g_activeBuff->autocompPopupSelectNextItem();
    }
    else
    {
        g_activeBuff->triggerAutocompPopup({});
    }
    g_isRedrawNeeded = true;
}

void triggerAutocompPopupOrSelectPrevItem()
{
    if (!g_activeBuff)
        return;

    if (g_activeBuff->isAutocompPopupShown())
    {
        g_activeBuff->autocompPopupSelectPrevItem();
    }
    else
    {
        g_activeBuff->triggerAutocompPopup({});
    }
    g_isRedrawNeeded = true;
}

void bufferPutEnterOrInsertAutocomplete()
{
    if (!g_activeBuff)
        return;

    if (g_activeBuff->isAutocompPopupShown())
        g_activeBuff->autocompPopupInsert();
    else
        g_activeBuff->insertCharAtCursor(U'\n');
}

void bufferCancelSelection()
{
    if (!g_activeBuff)
        return;

    g_activeBuff->closeSelection();
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

void bufferStartBlockSelection()
{
    if (!g_activeBuff)
        return;

    g_activeBuff->startSelection(Buffer::Selection::Mode::Block);
    g_isRedrawNeeded = true;
}

void bufferDelInsertSelection()
{
    if (!g_activeBuff)
        return;

    if (g_activeBuff->isSelectionInProgress())
    {
        g_activeBuff->deleteCharForwardOrSelected();
        g_editorMode.set(EditorMode::_EditorMode::Insert);
        g_isRedrawNeeded = true;
    }
}

void bufferPasteClipboard()
{
    if (!g_activeBuff)
        return;

    g_activeBuff->pasteFromClipboard();
}

void bufferCopySelectionToClipboard()
{
    if (!g_activeBuff)
        return;

    g_activeBuff->copySelectionToClipboard();
}

void bufferCutSelectionToClipboard()
{
    if (!g_activeBuff)
        return;

    g_activeBuff->cutSelectionToClipboard();
}

static void bufferFindDialogCb(int, Dialog* dlg, void*)
{
    auto dialog = dynamic_cast<FindDialog*>(dlg);
    g_activeBuff->find((dialog->getValue()));
}

void bufferFind()
{
    if (!g_activeBuff)
        return;

    FindDialog::create(bufferFindDialogCb, nullptr);
}

void bufferFindGotoNext()
{
    if (!g_activeBuff)
        return;
    g_activeBuff->findGoToNextResult();
}

void bufferFindGotoPrev()
{
    if (!g_activeBuff)
        return;
    g_activeBuff->findGoToPrevResult();
}

void indentSelectedLines()
{
    if (!g_activeBuff)
        return;

    g_activeBuff->indentSelectedLines();
}

void unindentSelectedLines()
{
    if (!g_activeBuff)
        return;

    g_activeBuff->unindentSelectedLines();
}

void togglePromptAnimated()
{
    Prompt::get()->toggleWithAnim();
}

void showPromptAnimated()
{
    Prompt::get()->showWithAnim();
}

void hidePromptAnimated()
{
    Prompt::get()->hideWithAnim();
}

void bufferShowSymbolHover()
{
    if (!g_activeBuff)
        return;

    g_activeBuff->showSymbolHover();
}

void bufferShowSignHelp()
{
    if (!g_activeBuff)
        return;

    g_activeBuff->showSignatureHelp();
}

void bufferGotoDef()
{
    if (!g_activeBuff)
        return;

    g_activeBuff->gotoDef();
}

void bufferGotoDecl()
{
    if (!g_activeBuff)
        return;

    g_activeBuff->gotoDecl();
}

void bufferGotoImp()
{
    if (!g_activeBuff)
        return;

    g_activeBuff->gotoImp();
}

void bufferApplyCodeAct()
{
    if (!g_activeBuff)
        return;

    g_activeBuff->applyLineCodeAct();
}

void bufferRenameSymbol()
{
    if (!g_activeBuff)
        return;

    g_activeBuff->renameSymbolAtCursor();
}

void bufferInsertCustomSnippet()
{
    if (!g_activeBuff)
        return;

    g_activeBuff->insertCustomSnippet();
}

void bufferGoToNextSnippetTabstop()
{
    if (!g_activeBuff)
        return;

    g_activeBuff->goToNextSnippetTabstop();
}

void bufferFormatDocument()
{
    if (!g_activeBuff)
        return;

    g_activeBuff->formatDocument();
}

void showWorkspaceFindDlg()
{
    App::showFindDlg(App::FindType::WorkspaceSymbol);
}

} // Namespace Callbacks

} // Namespace Bindings
