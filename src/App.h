#pragma once

#include <iostream>
#include <cassert>
#include <vector>
#include <memory>
#include "glstuff.h"
#include "Logger.h"
#include "config.h"
#include "os.h"
#include "Shader.h"
#include "TextRenderer.h"
#include "UiRenderer.h"
#include "FileTypeHandler.h"
#include "Buffer.h"
#include "MessageDialog.h"
#include "FileDialog.h"
#include "AskerDialog.h"
#include "types.h"
#include "Timer.h"
#include "ImageBuffer.h"

extern int g_windowWidth;
extern int g_windowHeight;
extern bool g_isRedrawNeeded;
extern bool g_isTitleUpdateNeeded;
extern bool g_isDebugDrawMode;
extern bool g_shouldIgnoreNextChar;
extern std::vector<std::unique_ptr<Buffer>> g_buffers;
extern size_t g_currentBufferI;
extern std::vector<std::unique_ptr<Dialog>> g_dialogs;

class App final
{
public:
    App() = delete;

    // ----- Setup functions -----
    static GLFWwindow* createWindow();
    static void initGlew();
    static void setupGlFeatures();
    static TextRenderer* createTextRenderer();
    static UiRenderer* createUiRenderer();
    static std::unique_ptr<Image> loadProgramIcon();
    static FileTypeHandler* createFileTypeHandler();
    static void setupKeyBindings();

    // ----- Renderer functions -----
    static void renderBuffers();
    static void renderTabLine();
    static void renderDialogs();
    static void renderStartupScreen();

private:
    static void GLAPIENTRY glDebugMsgCB(
            GLenum source, GLenum type, GLuint, GLenum severity,
            GLsizei, const GLchar* message, const void*);

    // ----- Callbacks -----
    static void windowRefreshCB(GLFWwindow*);
    static void windowResizeCB(GLFWwindow*, int width, int height);
    static void windowKeyCB(GLFWwindow*, int key, int scancode, int action, int mods);
public:
    static void windowCharCB(GLFWwindow*, uint codePoint);
private:
    static void windowScrollCB(GLFWwindow*, double, double yOffset);
    static void windowCloseCB(GLFWwindow* window);

    static void toggleDebugDraw();
};
