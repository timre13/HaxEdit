#pragma once

#include "glstuff.h"
#include <functional>

namespace Bindings
{

struct BindingMapSet
{
    using bindingFunc_t = std::function<void()>;
    using bindingMap_t = bindingFunc_t[GLFW_KEY_LAST];

    bindingMap_t noMod;
    bindingMap_t ctrl;
    bindingMap_t ctrlShift;
    bindingMap_t shift;
};

void runBinding(int mods, int key);

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
void moveCursorToLineBeginningAndEnterInsertMode();
void moveCursorToLineEndAndEnterInsertMode();
void putLineBreakAfterLineAndEnterInsertMode();
void putLineBreakBeforeLineAndEnterInsertMode();

void putEnter();
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
void hideAutocompPopupOrEndSelection();

void bufferStartNormalSelection();
void bufferStartLineSelection();
void bufferStartBlockSelection();

void bufferPasteClipboard();
void bufferCopySelectionToClipboard();

}

extern BindingMapSet nmap; // Normal mode mapping
extern BindingMapSet imap; // Insert mode mapping

extern Bindings::BindingMapSet* bindingsP;

}
