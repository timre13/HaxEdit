#pragma once

#include "glstuff.h"
#include <functional>
#include <map>

namespace Bindings
{

struct BindingMapSet
{
    typedef void (*rawBindingFunc_t)(void);
    using bindingFunc_t = std::function<void()>;
    using primMap_t    = std::map<std::string, bindingFunc_t>;
    using funcMap_t    = std::map<int, bindingFunc_t>;
    using nonprimMap_t = std::map<uint, bindingFunc_t>;

    /*
     * For characters that can be typed without AltGr.
     * Layout dependent, logical keys.
     *
     * Uses lower case characters (unicode characters as multiple chars).
     * TODO: We should use code points
     */
    primMap_t primNoMod;
    primMap_t primCtrl;
    primMap_t primCtrlShift;
    primMap_t primShift;

    /*
     * For Enter, Space and other non-alphanumeric keys.
     * NOT layout dependent, physical keys.
     *
     * Uses `GLFW_KEY_` values.
     */
    funcMap_t funcNoMod;
    funcMap_t funcCtrl;
    funcMap_t funcCtrlShift;
    funcMap_t funcShift;

    /*
     * For characters that can only be typed with AltGr.
     * Layout dependent, characters.
     *
     * Uses unicode code points.
     */
    funcMap_t nonprimNoMod;
};

extern BindingMapSet::bindingFunc_t _foundCharCB;
extern BindingMapSet::bindingFunc_t _foundKeyCB;

void fetchKeyBinding(int key, int mods);
void fetchCharBinding(uint codePoint);
void runFetchedBinding();

void bindPrimKey(const std::string& keyName, int glfwMods, BindingMapSet::rawBindingFunc_t func);
void bindFuncKey(int glfwKey, int glfwMods, BindingMapSet::rawBindingFunc_t func);
void bindNonprimChar(uint codePoint, BindingMapSet::rawBindingFunc_t func);

namespace Callbacks
{

void switchToNormalMode();
void switchToInsertMode();
void switchToReplaceMode();

void createBufferInNewTab();
void saveCurrentBuffer();
void saveCurrentBufferAs();
void openFile();
void closeActiveBuffer();
void openPathAtCursor();

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

void bufferPasteClipboard();
void bufferCopySelectionToClipboard();
void bufferCutSelectionToClipboard();

void bufferFind();
void bufferFindGotoNext();
void bufferFindGotoPrev();

}

extern BindingMapSet nmap; // Normal mode mapping
extern BindingMapSet imap; // Insert mode mapping

extern Bindings::BindingMapSet* bindingsP;

}
