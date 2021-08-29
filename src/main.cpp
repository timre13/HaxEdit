#include <iostream>
#include <cassert>
#include <vector>
#include <GL/glew.h>
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include "Logger.h"
#include "config.h"
#include "os.h"
#include "Shader.h"
#include "TextRenderer.h"
#include "UiRenderer.h"
#include "Buffer.h"
#include "Dialog.h"
#include "types.h"
#include "Timer.h"

static void GLAPIENTRY glDebugMsgCB(
        GLenum source, GLenum type, GLuint, GLenum severity,
        GLsizei, const GLchar* message, const void*)
{
    std::cout << "\033[93m[OpenGL]\033[0m: " << "Message: type=\"";
    switch (type)
    {
    case GL_DEBUG_TYPE_ERROR: std::cout << "other"; break;
    case GL_DEBUG_TYPE_PERFORMANCE: std::cout << "performance"; break;
    case GL_DEBUG_TYPE_PORTABILITY: std::cout << "portability"; break;
    case GL_DEBUG_TYPE_POP_GROUP: std::cout << "pop group"; break;
    case GL_DEBUG_TYPE_PUSH_GROUP: std::cout << "push group"; break;
    case GL_DEBUG_TYPE_MARKER: std::cout << "marker"; break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: std::cout << "undefined behavior"; break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: std::cout << "deprecated behavior"; break;
    default: std::cout << "unknown"; break;
    }

    std::cout << "\", source=\"";
    switch (source)
    {
    case GL_DEBUG_SOURCE_API: std::cout << "API"; break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM: std::cout << "window system"; break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER: std::cout << "shader compiler"; break;
    case GL_DEBUG_SOURCE_THIRD_PARTY: std::cout << "third party"; break;
    case GL_DEBUG_SOURCE_APPLICATION: std::cout << "application"; break;
    case GL_DEBUG_SOURCE_OTHER: std::cout << "other"; break;
    default: std::cout << "unknown"; break;
    }

    std::cout << "\", severity=\"";
    switch (severity)
    {
    case GL_DEBUG_SEVERITY_HIGH: std::cout << "high"; break;
    case GL_DEBUG_SEVERITY_MEDIUM: std::cout << "medium"; break;
    case GL_DEBUG_SEVERITY_LOW: std::cout << "low"; break;
    case GL_DEBUG_SEVERITY_NOTIFICATION: std::cout << "notification"; break;
    default: std::cout << "unknown"; break;
    }

    std::cout << "\" : " << message << '\n';
    std::cout.flush();
}


static int g_windowWidth = 0;
static int g_windowHeight = 0;

static bool g_isRedrawNeeded = false;
static bool g_isTitleUpdateNeeded = true;
static bool g_isDebugDrawMode = false;

static std::vector<Buffer> g_buffers;
static size_t g_currentBufferI{};

static std::vector<Dialog> g_dialogs;

TextRenderer* g_textRenderer{};
UiRenderer* g_uiRenderer{};


static void windowResizeCB(GLFWwindow*, int width, int height)
{
    glViewport(0, 0, width, height);
    g_windowWidth = width;
    g_windowHeight = height;
    g_textRenderer->onWindowResized(width, height);
    g_uiRenderer->onWindowResized(width, height);
}

static void windowRefreshCB(GLFWwindow*)
{
    g_isRedrawNeeded = true;
}

static void toggleDebugDraw()
{
    g_isDebugDrawMode = !g_isDebugDrawMode;
    glPolygonMode(GL_FRONT_AND_BACK, g_isDebugDrawMode ? GL_LINE : GL_FILL);
    g_isRedrawNeeded = true;
}

static void charCB(GLFWwindow*, uint codePoint)
{
    // If there are dialogs open, don't react to keypresses
    if (!g_dialogs.empty())
    {
        if (codePoint == '\n')
        {
            // Close top dialog if Enter was pressed
            g_dialogs.pop_back();
            g_isRedrawNeeded = true;
        }
        return;
    }

    if (!g_buffers.empty())
    {
        g_buffers[g_currentBufferI].insert((char)codePoint);
        // Show cursor while typing
        g_buffers[g_currentBufferI].setCursorVisibility(true);
    }
    g_isRedrawNeeded = true;
}

static void goToNextTab()
{
    if (!g_buffers.empty() && g_currentBufferI < g_buffers.size()-1)
        ++g_currentBufferI;
    g_isRedrawNeeded = true;
    g_isTitleUpdateNeeded = true;
}

static void goToPrevTab()
{
    if (g_currentBufferI > 0)
        --g_currentBufferI;
    g_isRedrawNeeded = true;
    g_isTitleUpdateNeeded = true;
}

static void windowKeyCB(GLFWwindow*, int key, int scancode, int action, int mods)
{
    TIMER_BEGIN_FUNC();

    (void)scancode;

    if (action == GLFW_RELEASE && key == GLFW_KEY_F3)
    {
        toggleDebugDraw();
        return;
    }

    // If there are dialogs open, don't react to keypresses
    if ((action == GLFW_PRESS) && !g_dialogs.empty())
    {
        if (key == GLFW_KEY_ENTER)
        {
            // Close top dialog if Enter was pressed
            g_dialogs.pop_back();
            g_isRedrawNeeded = true;
        }
        return;
    }

    if (action == GLFW_PRESS || action == GLFW_REPEAT)
    {
        switch (key)
        {
        case GLFW_KEY_RIGHT:
            if (!g_buffers.empty())
            {
                g_buffers[g_currentBufferI].moveCursor(Buffer::CursorMovCmd::Right);
                // Show cursor while moving
                g_buffers[g_currentBufferI].setCursorVisibility(true);
                g_isRedrawNeeded = true;
            }
            break;

        case GLFW_KEY_LEFT:
            if (!g_buffers.empty())
            {
                g_buffers[g_currentBufferI].moveCursor(Buffer::CursorMovCmd::Left);
                // Show cursor while moving
                g_buffers[g_currentBufferI].setCursorVisibility(true);
                g_isRedrawNeeded = true;
            }
            break;

        case GLFW_KEY_DOWN:
            if (!g_buffers.empty())
            {
                g_buffers[g_currentBufferI].moveCursor(Buffer::CursorMovCmd::Down);
                // Show cursor while moving
                g_buffers[g_currentBufferI].setCursorVisibility(true);
                g_isRedrawNeeded = true;
            }
            break;

        case GLFW_KEY_UP:
            if (!g_buffers.empty())
            {
                g_buffers[g_currentBufferI].moveCursor(Buffer::CursorMovCmd::Up);
                // Show cursor while moving
                g_buffers[g_currentBufferI].setCursorVisibility(true);
                g_isRedrawNeeded = true;
            }
            break;

        case GLFW_KEY_HOME:
            if (!g_buffers.empty())
            {
                g_buffers[g_currentBufferI].moveCursor(Buffer::CursorMovCmd::LineBeginning);
                // Show cursor while moving
                g_buffers[g_currentBufferI].setCursorVisibility(true);
                g_isRedrawNeeded = true;
            }
            break;

        case GLFW_KEY_END:
            if (!g_buffers.empty())
            {
                g_buffers[g_currentBufferI].moveCursor(Buffer::CursorMovCmd::LineEnd);
                // Show cursor while moving
                g_buffers[g_currentBufferI].setCursorVisibility(true);
                g_isRedrawNeeded = true;
            }
            break;

        case GLFW_KEY_ENTER:
            charCB(nullptr, '\n');
            break;

        case GLFW_KEY_BACKSPACE:
            if (!g_buffers.empty())
            {
                g_buffers[g_currentBufferI].deleteCharBackwards();
                // Show cursor while typing
                g_buffers[g_currentBufferI].setCursorVisibility(true);
                g_isRedrawNeeded = true;
            }
            break;

        case GLFW_KEY_DELETE:
            if (!g_buffers.empty())
            {
                g_buffers[g_currentBufferI].deleteCharForward();
                // Show cursor while typing
                g_buffers[g_currentBufferI].setCursorVisibility(true);
                g_isRedrawNeeded = true;
            }
            break;

        case GLFW_KEY_TAB:
            if (!g_buffers.empty())
            {
                if (TAB_SPACE_COUNT < 1)
                {
                    g_buffers[g_currentBufferI].insert('\t');
                }
                else
                {
                    for (int i{}; i < TAB_SPACE_COUNT; ++i)
                    {
                        g_buffers[g_currentBufferI].insert(' ');
                    }
                }
                // Show cursor while typing
                g_buffers[g_currentBufferI].setCursorVisibility(true);
                g_isRedrawNeeded = true;
            }
            break;

        case GLFW_KEY_PAGE_UP:
            if (mods & GLFW_MOD_CONTROL)
            {
                goToPrevTab();
            }
            else
            {
                // TODO: Implement stepping in buffers by a page
            }
            break;

        case GLFW_KEY_PAGE_DOWN:
            if (mods & GLFW_MOD_CONTROL)
            {
                goToNextTab();
            }
            else
            {
                // TODO: Implement stepping in buffers by a page
            }
            break;
        }
    }

    TIMER_END_FUNC();
}

static void windowScrollCB(GLFWwindow*, double, double yOffset)
{
    if (!g_buffers.empty())
    {
        g_buffers[g_currentBufferI].scrollBy(yOffset*SCROLL_SPEED_MULTIPLIER);
    }
    g_isRedrawNeeded = true;
}

int main(int argc, char** argv)
{
    Logger::setLoggerVerbosity(Logger::LoggerVerbosity::Debug);

    if (!glfwInit())
    {
        Logger::fatal << "Failed to initialize GLFW" << Logger::End;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    auto window = glfwCreateWindow(DEF_WIN_W, DEF_WIN_H, "HaxorEdit", nullptr, nullptr);
    if (!window)
    {
        Logger::fatal << "Failed to create window" << Logger::End;
    }
    glfwSetWindowSizeCallback(window, windowResizeCB);
    glfwSetWindowRefreshCallback(window, windowRefreshCB);
    glfwSetKeyCallback(window, windowKeyCB);
    glfwSetScrollCallback(window, windowScrollCB);
    glfwSetCharCallback(window, charCB);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    Logger::dbg << "Created GLFW window" << Logger::End;

    glewExperimental = GL_TRUE;
    uint initStatus = glewInit();
    if (initStatus != GLEW_OK)
    {
        Logger::fatal << "Failed to initialize GLEW: " << glewGetErrorString(initStatus) << Logger::End;
    }
    Logger::dbg << "Initialized GLEW" << Logger::End;

    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(glDebugMsgCB, 0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(UNPACK_RGB_COLOR(BG_COLOR), 1.0f);
    glfwSwapBuffers(window);

    auto regularFontPath    = OS::getFontFilePath(FONT_FAMILY_REGULAR, FontStyle::Regular);
    auto boldFontPath       = OS::getFontFilePath(FONT_FAMILY_BOLD, FontStyle::Bold);
    auto italicFontPath     = OS::getFontFilePath(FONT_FAMILY_ITALIC, FontStyle::Italic);
    auto boldItalicFontPath = OS::getFontFilePath(FONT_FAMILY_BOLDITALIC, FontStyle::BoldItalic);
    Logger::dbg
        << "Regular font: " << regularFontPath
        << "\n       Bold font: " << boldFontPath
        << "\n       Italic font: " << italicFontPath
        << "\n       Bold italic font: " << boldItalicFontPath
        << Logger::End;
    assert(regularFontPath.length() && boldFontPath.length() && italicFontPath.length() && boldItalicFontPath.length());
    g_textRenderer = new TextRenderer{regularFontPath, boldFontPath, italicFontPath, boldItalicFontPath};
    g_uiRenderer = new UiRenderer{};

    // Update TextRenderer's and UiRenderer's variables by triggering the resize callback
    glfwPollEvents();

    for (int i{1}; i < argc; ++i)
    {
        g_buffers.push_back(Buffer{});
        if (g_buffers.back().open(argv[i]))
        {
            g_dialogs.push_back(
                    Dialog{"Failed to open file: \""+std::string{argv[i]}+'"', Dialog::Type::Error}
            );
        }
    }
    if (g_buffers.empty())
    {
        g_buffers.push_back(Buffer{});
        g_buffers.back().open(__FILE__);
    }

    auto genTitle{
        [&](){
            if (g_buffers.empty())
                return std::string{"HaxorEdit"};
            return g_buffers[g_currentBufferI].getFileName()
                + std::string{" - ["} + std::to_string(g_currentBufferI+1) + '/'
                + std::to_string(g_buffers.size()) + "]";
        }
    };

    float framesUntilCursorBlinking = CURSOR_BLINK_FRAMES;
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        if (CURSOR_BLINK_FRAMES >= 0 && framesUntilCursorBlinking <= 0 && !g_buffers.empty())
        {
            g_buffers[g_currentBufferI].toggleCursorVisibility();
            g_isRedrawNeeded = true;
            framesUntilCursorBlinking = CURSOR_BLINK_FRAMES;
        }

        if (g_isRedrawNeeded)
        {
            glClear(GL_COLOR_BUFFER_BIT);
            glClearColor(UNPACK_RGB_COLOR(BG_COLOR), 1.0f);

            if (!g_buffers.empty())
            {
                g_buffers[g_currentBufferI].updateCursor();
                g_buffers[g_currentBufferI].render();
            }

            // Draw tabline background
            g_uiRenderer->renderFilledRectangle({0, 0}, {g_windowWidth, TABLINE_HEIGHT_PX}, RGB_COLOR_TO_RGBA(TABLINE_BG_COLOR));
            uint tabI{};
            // Draw tabs
            for (const auto& buffer : g_buffers)
            {
                const int tabX = TABLINE_TAB_WIDTH_PX*tabI;

                // Stop rendering if we go out of the screen
                if (tabX > g_windowWidth)
                    break;

                g_uiRenderer->renderFilledRectangle(
                        {tabX, 0},
                        {TABLINE_TAB_WIDTH_PX*(tabI+1)-2, TABLINE_HEIGHT_PX},
                        RGB_COLOR_TO_RGBA((tabI == g_currentBufferI ? TABLINE_ACTIVE_TAB_COLOR : TABLINE_TAB_COLOR)));
                g_textRenderer->renderString(buffer.getFileName().substr(0, TABLINE_TAB_MAX_TEXT_LEN),
                        {tabX, -2},
                        tabI == g_currentBufferI ? FontStyle::BoldItalic : FontStyle::Regular);
                ++tabI;
            }

            if (!g_dialogs.empty())
            {
                // Render the top dialog
                g_dialogs.back().render();
            }

            g_isRedrawNeeded = false;
        }

        if (g_isTitleUpdateNeeded)
        {
            glfwSetWindowTitle(window, genTitle().c_str());
            g_isTitleUpdateNeeded = false;
        }

        glfwSwapBuffers(window);
        --framesUntilCursorBlinking;
    }

    Logger::log << "Shutting down!" << Logger::End;

    delete g_textRenderer;
    delete g_uiRenderer;
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

