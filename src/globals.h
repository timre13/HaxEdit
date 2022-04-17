#pragma once

#include <vector>
#include <memory>
#include "glstuff.h"
#include "config.h"
#include "modes.h"
#include "StatusMsg.h"
#include "autocomp/DictionaryProvider.h"

class Dialog;
class Split;
class Buffer;
class TextRenderer;
class UiRenderer;
class FileTypeHandler;
struct Theme;
class RecentFileList;

#ifdef _DEF_GLOBALS_

std::string g_exePath;
std::string g_exeDirPath;

GLFWwindow* g_window = nullptr;
int g_windowWidth = 0;
int g_windowHeight = 0;
int g_cursorX = 0;
int g_cursorY = 0;
int g_mouseHResSplitI = -1;
bool g_isMouseBtnLPressed = false;
bool g_isMouseBtnRPressed = false;
bool g_isMouseBtnMPressed = false;

bool g_isRedrawNeeded = false;
bool g_isTitleUpdateNeeded = true;
bool g_isDebugDrawMode = false;

std::vector<std::unique_ptr<Split>> g_tabs;
Buffer* g_activeBuff = nullptr;
size_t g_currTabI{};
std::unique_ptr<RecentFileList> g_recentFilePaths;

std::vector<std::unique_ptr<Dialog>> g_dialogs;
int g_dialogFlashTime = 0;

std::unique_ptr<TextRenderer> g_textRenderer;
std::unique_ptr<UiRenderer> g_uiRenderer;
std::unique_ptr<FileTypeHandler> g_fileTypeHandler;

std::unique_ptr<Theme> g_theme;

int g_fontSizePx = DEF_FONT_SIZE_PX;
float g_fontWidthPx = 0;

// Loaded by App::loadCursors()
namespace Cursors
{
GLFWcursor* busy{};
}

namespace Autocomp
{
std::unique_ptr<Autocomp::DictionaryProvider> dictProvider;
}

EditorMode g_editorMode;
StatusMsg g_statMsg;

bool g_hasBindingToCall = false;

#else // ----------------------------------------------------------------------

extern std::string g_exePath;
extern std::string g_exeDirPath;

extern GLFWwindow* g_window;
extern int g_windowWidth;
extern int g_windowHeight;
extern int g_cursorX;
extern int g_cursorY;
extern int g_mouseHResSplitI;
extern bool g_isMouseBtnLPressed;
extern bool g_isMouseBtnRPressed;
extern bool g_isMouseBtnMPressed;

extern bool g_isRedrawNeeded;
extern bool g_isTitleUpdateNeeded;
extern bool g_isDebugDrawMode;

extern std::vector<std::unique_ptr<Dialog>> g_dialogs;
extern int g_dialogFlashTime;

extern std::unique_ptr<Theme> g_theme;

extern std::vector<std::unique_ptr<Split>> g_tabs;
extern Buffer* g_activeBuff;
extern size_t g_currTabI;
extern std::unique_ptr<RecentFileList> g_recentFilePaths;

extern int g_fontSizePx;
extern float g_fontWidthPx;

namespace Cursors
{
extern GLFWcursor* busy;
}

namespace Autocomp
{
extern std::unique_ptr<Autocomp::DictionaryProvider> dictProvider;
}

extern EditorMode g_editorMode;
extern StatusMsg g_statMsg;

extern bool g_hasBindingToCall;

#endif
