#pragma once

#include "glstuff.h"
#include <functional>

namespace Bindings
{

using bindingMap_t = std::function<void()>[GLFW_KEY_LAST];

void runBinding(bindingMap_t& map, int binding);

namespace Callbacks
{

void createBufferInNewTab();
void saveCurrentBuffer();
void saveCurrentBufferAs();
void openFile();
void closeActiveBuffer();

void goToNextTab();
void goToPrevTab();

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

void increaseFontSize();
void decreaseFontSize();

void zoomInBufferIfImage();
void zoomOutBufferIfImage();

}

extern bindingMap_t noModMap;
extern bindingMap_t ctrlMap;
extern bindingMap_t ctrlShiftMap;

}
