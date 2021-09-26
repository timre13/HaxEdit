#include <vector>
#include <memory>
#include "config.h"

class Dialog;
class Split;
class Buffer;
class TextRenderer;
class UiRenderer;
class FileTypeHandler;

#ifdef _DEF_GLOBALS_

int g_windowWidth = 0;
int g_windowHeight = 0;

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

#else // ----------------------------------------

extern int g_windowWidth;
extern int g_windowHeight;

extern bool g_isRedrawNeeded;
extern bool g_isTitleUpdateNeeded;
extern bool g_isDebugDrawMode;
extern bool g_shouldIgnoreNextChar;

extern std::vector<std::unique_ptr<Dialog>> g_dialogs;

extern std::vector<std::unique_ptr<Split>> g_tabs;
extern Buffer* g_activeBuff;
extern size_t g_currTabI;

extern int g_fontSizePx;

#endif
