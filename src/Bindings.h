#pragma once

#include "glstuff.h"
#include "types.h"
#include <functional>
#include <map>

namespace Bindings
{

using bindingFunc_t = std::function<void()>;
using bindingMap_t = std::map<String, bindingFunc_t>;

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

extern bindingMap_t nmap; // Normal mode mapping
extern bindingMap_t imap; // Insert mode mapping

extern bindingMap_t* activeBindingMap;

}
