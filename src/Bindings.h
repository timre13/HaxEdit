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
};

void runBinding(BindingMapSet& map, int mods, int key);

namespace Callbacks
{

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
void moveCursorLeft();
void moveCursorDown();
void moveCursorUp();
void moveCursorToLineBeginning();
void moveCursorToLineEnd();

void putEnter();
void deleteCharBackwards();
void deleteCharForward();
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

}

extern BindingMapSet mappings;

}
