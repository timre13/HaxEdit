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
#include <vector>
#include <memory>
#include <cstring>
#include <unicode/uchar.h>

#define BINDINGS_VERBOSE 1

namespace Bindings
{

static std::string modsToStr(int mods)
{
    switch (mods)
    {
    case 0:
        return "";

    case GLFW_MOD_CONTROL:
        return "Ctrl-";

    case GLFW_MOD_CONTROL|GLFW_MOD_SHIFT:
        return "Ctrl-Shift-";

    case GLFW_MOD_SHIFT:
        return "Shift-";

    default:
        return "???";
    }
}

BindingMapSet::bindingFunc_t _foundCharCB{};
BindingMapSet::bindingFunc_t _foundKeyCB{};

void fetchKeyBinding(int key, int mods)
{
    assert(!_foundKeyCB && "_foundKeyCB hasn't been cleared");

    switch (key)
    {
    case GLFW_KEY_CAPS_LOCK:
    case GLFW_KEY_SCROLL_LOCK:
    case GLFW_KEY_NUM_LOCK:
    case GLFW_KEY_LEFT_SHIFT:
    case GLFW_KEY_LEFT_CONTROL:
    case GLFW_KEY_LEFT_ALT:
    case GLFW_KEY_LEFT_SUPER:
    case GLFW_KEY_RIGHT_SHIFT:
    case GLFW_KEY_RIGHT_CONTROL:
    case GLFW_KEY_RIGHT_ALT:
    case GLFW_KEY_RIGHT_SUPER:
        return; // Ignore modifier presses
    }

    const char* _keyName = glfwGetKeyName(key, 0);
    try // `std::map::at()` throws if not found
    {
        if (_keyName) // If normal key
        {
            const std::string keyName = _keyName;
#if BINDINGS_VERBOSE
            Logger::dbg << "Fetching binding for primary key: " << modsToStr(mods) << keyName << Logger::End;
#endif

            switch (mods)
            {
            case 0: /* No mod */                  _foundKeyCB = bindingsP->primNoMod.at(keyName); break;
            case GLFW_MOD_CONTROL:                _foundKeyCB = bindingsP->primCtrl.at(keyName); break;
            case GLFW_MOD_CONTROL|GLFW_MOD_SHIFT: _foundKeyCB = bindingsP->primCtrlShift.at(keyName); break;
            case GLFW_MOD_SHIFT:                  _foundKeyCB = bindingsP->primShift.at(keyName); break;
            default: break;
            }
        }
        else // If function key (including space)
        {
#if BINDINGS_VERBOSE
            Logger::dbg << "Fetching binding for function key: " << modsToStr(mods) << key << Logger::End;
#endif

            switch (mods)
            {
            case 0: /* No mod */                  _foundKeyCB = bindingsP->funcNoMod.at(key); break;
            case GLFW_MOD_CONTROL:                _foundKeyCB = bindingsP->funcCtrl.at(key); break;
            case GLFW_MOD_CONTROL|GLFW_MOD_SHIFT: _foundKeyCB = bindingsP->funcCtrlShift.at(key); break;
            case GLFW_MOD_SHIFT:                  _foundKeyCB = bindingsP->funcShift.at(key); break;
            default: break;
            }
        }
    }
    catch (...)
    {
    // TODO
    //if (!((g_editorMode.get() == EditorMode::_EditorMode::Insert
    //                || g_editorMode.get() == EditorMode::_EditorMode::Replace)
    //            && (mods == GLFW_MOD_SHIFT || mods == 0)))
    //{
    //    g_statMsg.set("Not bound", StatusMsg::Type::Error);
    //    g_isRedrawNeeded = true;
    //}
    }
}

void fetchCharBinding(uint codePoint)
{
    assert(!_foundCharCB && "_foundCharCB hasn't been cleared");

#if BINDINGS_VERBOSE
    Logger::dbg << "Fetching binding for character: ";
    if (isascii(codePoint)) Logger::dbg << (char)codePoint;
    else Logger::dbg << '<' << codePoint << '>';
    Logger::dbg << Logger::End;
#endif

    try // `std::map::at()` throws if not found
    {
        _foundCharCB = bindingsP->nonprimNoMod.at(codePoint);
    }
    catch (...)
    {
    // TODO
    //if (!((g_editorMode.get() == EditorMode::_EditorMode::Insert
    //                || g_editorMode.get() == EditorMode::_EditorMode::Replace)
    //            && (mods == GLFW_MOD_SHIFT || mods == 0)))
    //{
    //    g_statMsg.set("Not bound", StatusMsg::Type::Error);
    //    g_isRedrawNeeded = true;
    //}
    }
}

void runFetchedBinding()
{
    // Char callbacks have priority
    if (_foundCharCB)
    {
#if BINDINGS_VERBOSE
        Logger::dbg << "Running char callback" << Logger::End;
#endif
        _foundCharCB();
    }
    else if (_foundKeyCB)
    {
#if BINDINGS_VERBOSE
        Logger::dbg << "Running key callback" << Logger::End;
#endif
        _foundKeyCB();
    }
    else
    {
    }

    _foundKeyCB = 0;
    _foundCharCB = 0;
}

void bindPrimKey(const std::string& keyName, int glfwMods, BindingMapSet::rawBindingFunc_t func)
{
    assert(func && "Called `bindPrimKey()` with a null function");
    assert(bindingsP && "Called `bindPrimKey()` without any editor mode set");

    switch (glfwMods)
    {
    case 0: /* No mod */                  bindingsP->primNoMod.emplace(keyName, func); break;
    case GLFW_MOD_CONTROL:                bindingsP->primCtrl.emplace(keyName, func); break;
    case GLFW_MOD_CONTROL|GLFW_MOD_SHIFT: bindingsP->primCtrlShift.emplace(keyName, func); break;
    case GLFW_MOD_SHIFT:                  bindingsP->primShift.emplace(keyName, func); break;
    default: assert(0 && "Unimplemented/Invalid mod combination"); return;
    }

    Logger::dbg << "Primary key " << modsToStr(glfwMods) << keyName << " ==> " << reinterpret_cast<void*>(func) << Logger::End;
}

void bindFuncKey(int glfwKey, int glfwMods, BindingMapSet::rawBindingFunc_t func)
{
    assert(func && "Called `bindFuncKey()` with a null function");
    assert(bindingsP && "Called `bindPrimKey()` without any editor mode set");

    switch (glfwMods)
    {
    case 0: /* No mod */                  bindingsP->funcNoMod.emplace(glfwKey, func); break;
    case GLFW_MOD_CONTROL:                bindingsP->funcCtrl.emplace(glfwKey, func); break;
    case GLFW_MOD_CONTROL|GLFW_MOD_SHIFT: bindingsP->funcCtrlShift.emplace(glfwKey, func); break;
    case GLFW_MOD_SHIFT:                  bindingsP->funcShift.emplace(glfwKey, func); break;
    default: assert(0 && "Unimplemented/Invalid mod combination"); return;
    }

    Logger::dbg << "Function key " << modsToStr(glfwMods) << '<' << glfwKey << "> ==> "
        << reinterpret_cast<void*>(func) << Logger::End;
}

void bindNonprimChar(uint codePoint, BindingMapSet::rawBindingFunc_t func)
{
    assert(func && "Called `bindNonprimChar()` with a null function");
    assert(codePoint >= UCHAR_MIN_VALUE && codePoint <= UCHAR_MAX_VALUE && "Called `bindNonprimChar()` with invalid code point");
    assert(bindingsP && "Called `bindPrimKey()` without any editor mode set");

    bindingsP->nonprimNoMod.emplace(codePoint, func);

    Logger::dbg << "Character ";
    if (isascii(codePoint)) Logger::dbg << (char)codePoint;
    else Logger::dbg << '<' << codePoint << '>';
    Logger::dbg << " ==> " << reinterpret_cast<void*>(func) << Logger::End;
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
        g_activeBuff->regenAutocompList();
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
        {
            g_statMsg.set("Not a valid file path: "+quoteStr(path), StatusMsg::Type::Error);
            return;
        }

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
        g_isTitleUpdateNeeded = true;
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
        g_activeBuff->insert('\n');
        g_editorMode.set(EditorMode::_EditorMode::Insert);
    }
}

void putLineBreakBeforeLineAndEnterInsertMode()
{
    if (g_activeBuff)
    {
        g_activeBuff->moveCursor(Buffer::CursorMovCmd::LineBeginning);
        g_activeBuff->updateCursor();
        g_activeBuff->insert('\n');
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

    if (g_activeBuff->isAutocompPopupShown())
    {
        g_activeBuff->autocompPopupSelectNextItem();
    }
    else
    {
        g_activeBuff->triggerAutocompPopup();
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
        g_activeBuff->triggerAutocompPopup();
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
        App::windowCharCB(nullptr, '\n');
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
    g_activeBuff->find(strToUtf32(dialog->getValue()));
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

} // Namespace Callbacks

BindingMapSet nmap = {};
BindingMapSet imap = {};
Bindings::BindingMapSet* bindingsP = nullptr;

} // Namespace Bindings
