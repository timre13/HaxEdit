#include <vector>
#include <memory>
#include "glstuff.h"
#include "config.h"

class Dialog;
class Split;
class Buffer;
class TextRenderer;
class UiRenderer;
class FileTypeHandler;

#ifdef _DEF_GLOBALS_

GLFWwindow* g_window = nullptr;
int g_windowWidth = 0;
int g_windowHeight = 0;
int g_cursorX = 0;
int g_cursorY = 0;
int g_mouseBtnLPressX = 0;
int g_mouseBtnLPressY = 0;
bool g_isMouseBtnLPressed = false;
bool g_isMouseBtnRPressed = false;
bool g_isMouseBtnMPressed = false;

bool g_isRedrawNeeded = false;
bool g_isTitleUpdateNeeded = true;
bool g_isDebugDrawMode = false;
bool g_shouldIgnoreNextChar = false;

std::vector<std::unique_ptr<Split>> g_tabs;
Buffer* g_activeBuff = nullptr;
size_t g_currTabI{};

std::vector<std::unique_ptr<Dialog>> g_dialogs;

std::unique_ptr<TextRenderer> g_textRenderer;
std::unique_ptr<UiRenderer> g_uiRenderer;
std::unique_ptr<FileTypeHandler> g_fileTypeHandler;

int g_fontSizePx = DEF_FONT_SIZE_PX;

// Loaded by App::loadCursors()
namespace Cursors
{
GLFWcursor* busy{};
}

#else // ----------------------------------------------------------------------

extern GLFWwindow* g_window;
extern int g_windowWidth;
extern int g_windowHeight;
extern int g_cursorX;
extern int g_cursorY;
extern int g_mouseBtnLPressX;
extern int g_mouseBtnLPressY;
extern bool g_isMouseBtnLPressed;
extern bool g_isMouseBtnRPressed;
extern bool g_isMouseBtnMPressed;

extern bool g_isRedrawNeeded;
extern bool g_isTitleUpdateNeeded;
extern bool g_isDebugDrawMode;
extern bool g_shouldIgnoreNextChar;

extern std::vector<std::unique_ptr<Dialog>> g_dialogs;

extern std::vector<std::unique_ptr<Split>> g_tabs;
extern Buffer* g_activeBuff;
extern size_t g_currTabI;

extern int g_fontSizePx;

namespace Cursors
{
extern GLFWcursor* busy;
}

#endif
