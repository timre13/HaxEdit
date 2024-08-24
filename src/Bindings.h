#pragma once

#include "Logger.h"
#include "glstuff.h"
#include "types.h"
#include "modes.h"
#include <functional>
#include <map>

namespace Bindings
{

class BindingKey
{
public:
    using mods_t = int;
    using key_t = String;

    mods_t mods{};
    key_t key;

    void clear()
    {
        mods = 0;
        key.clear();
    }

    auto operator<=>(const BindingKey&) const = default;

    operator bool() const
    {
        return !key.empty();
    }

    static String modsToStr(int mods)
    {
        String out;
        if (mods & GLFW_MOD_ALT)
            out += U"Alt-";
        if (mods & GLFW_MOD_CONTROL)
            out += U"Ctrl-";
        if (mods & GLFW_MOD_SHIFT)
            out += U"Shift-";

        return out;
    }

    operator String() const
    {
        return modsToStr(mods) + key;
    }

    operator std::string() const
    {
        return utf32To8(modsToStr(mods)+key);
    }
};

using rawBindingFunc_t = void();
using bindingFunc_t = std::function<rawBindingFunc_t>;
using bindingMap_t = std::map<BindingKey, bindingFunc_t>;

extern bindingMap_t nmap; // Normal mode mapping
extern bindingMap_t imap; // Insert mode mapping
extern bindingMap_t* activeBindingMap;

extern BindingKey lastPressedKey;
extern long long keyEventDelay;

Bindings::bindingMap_t* getBindingsForMode(EditorMode::_EditorMode mode);

void registerBinding(EditorMode::_EditorMode mode, const BindingKey::key_t& key, BindingKey::mods_t modifiers, rawBindingFunc_t func);

void keyCB(GLFWwindow*, int key, int scancode, int action, int mods);
void charModsCB(GLFWwindow*, uint codepoint, int mods);
void runBindingForFrame();

namespace Callbacks
{

void switchToNormalMode();
void switchToInsertMode();
void switchToReplaceMode();

void createBufferInNewTab();
void createTempBufferInNewTab();
void saveCurrentBuffer();
void saveCurrentBufferAs();
void openFile();
void closeActiveBuffer();
void openPathOrUrlAtCursor();

void goToNextTab();
void goToPrevTab();
void goToNextSplit();
void goToPrevSplit();

void increaseActiveBufferWidth();
void decreaseActiveBufferWidth();

void moveCursorRight();
void moveCursorRightAndEnterInsertMode();
void moveCursorLeft();
void moveCursorDown();
void moveCursorUp();
void moveCursorToLineBeginning();
void moveCursorToLineEnd();

void moveCursorToSWordEnd();
void moveCursorToSWordBeginning();
void moveCursorToNextSWord();

void moveCursorToLineBeginningAndEnterInsertMode();
void moveCursorToLineEndAndEnterInsertMode();
void putLineBreakAfterLineAndEnterInsertMode();
void putLineBreakBeforeLineAndEnterInsertMode();

void deleteCharBackwards();
void deleteCharForwardOrSelection();
void insertTabOrSpaces();

void goToFirstChar();
void goToLastChar();

void goPageUp();
void goPageDown();

void undoActiveBufferChange();
void redoActiveBufferChange();

void increaseFontSize();
void decreaseFontSize();

void zoomInBufferIfImage();
void zoomOutBufferIfImage();

void triggerAutocompPopupOrSelectNextItem();
void triggerAutocompPopupOrSelectPrevItem();
void bufferPutEnterOrInsertAutocomplete();

void bufferCancelSelection();
void bufferStartNormalSelection();
void bufferStartLineSelection();
void bufferStartBlockSelection();

void bufferDelInsertSelection();

void bufferPasteClipboard();
void bufferCopySelectionToClipboard();
void bufferCutSelectionToClipboard();

void bufferFind();
void bufferFindGotoNext();
void bufferFindGotoPrev();

void indentSelectedLines();
void unindentSelectedLines();

void togglePromptAnimated();
void showPromptAnimated();
void hidePromptAnimated();

void bufferShowSymbolHover();
void bufferShowSignHelp();
void bufferGotoDef();
void bufferGotoDecl();
void bufferGotoImp();
void bufferApplyCodeAct();
void bufferRenameSymbol();

void bufferInsertCustomSnippet();
void bufferGoToNextSnippetTabstop();

void bufferFormatDocument();

void showWorkspaceFindDlg();

}

}
