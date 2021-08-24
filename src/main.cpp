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
#include "types.h"

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
    case GL_DEBUG_SOURCE_APPLICATION: std::cout << "API"; break;
    case GL_DEBUG_SOURCE_OTHER: std::cout << "API"; break;
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

bool g_isRedrawNeeded = false;
int g_windowWidth = 0;
int g_windowHeight = 0;
bool g_isDebugDrawMode = false;

TextRenderer* g_textRenderer{};
UiRenderer* g_uiRenderer{};
std::vector<Buffer> g_buffers;
size_t g_currentBufferI{};

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

static void windowKeyCB(GLFWwindow*, int key, int scancode, int action, int mods)
{
    (void)mods; (void)scancode;

    if (action == GLFW_RELEASE)
    {
        switch (key)
        {
        case GLFW_KEY_F3:
            toggleDebugDraw();
            break;
        }
    }
    else if (action == GLFW_PRESS || action == GLFW_REPEAT)
    {
        switch (key)
        {
        case GLFW_KEY_RIGHT:
            if (!g_buffers.empty())
                g_buffers[g_currentBufferI].moveCursorRight();
            g_isRedrawNeeded = true;
            break;

        case GLFW_KEY_LEFT:
            if (!g_buffers.empty())
                g_buffers[g_currentBufferI].moveCursorLeft();
            g_isRedrawNeeded = true;
            break;

        case GLFW_KEY_DOWN:
            if (!g_buffers.empty())
                g_buffers[g_currentBufferI].moveCursorDown();
            g_isRedrawNeeded = true;
            break;

        case GLFW_KEY_UP:
            if (!g_buffers.empty())
                g_buffers[g_currentBufferI].moveCursorUp();
            g_isRedrawNeeded = true;
            break;
        }
    }
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
        g_buffers.push_back(Buffer{g_textRenderer, g_uiRenderer});
        if (g_buffers.back().open(argv[i]))
        {
            // TODO: Show an error dialog or something
        }
    }
    if (g_buffers.empty())
    {
        g_buffers.push_back(Buffer{g_textRenderer, g_uiRenderer});
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

    glfwSetWindowTitle(window, genTitle().c_str());

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        if (g_isRedrawNeeded)
        {
            glClear(GL_COLOR_BUFFER_BIT);
            glClearColor(UNPACK_RGB_COLOR(BG_COLOR), 1.0f);

            if (!g_buffers.empty())
            {
                g_buffers[g_currentBufferI].render();
            }

            g_isRedrawNeeded = false;
        }

        glfwSwapBuffers(window);
    }

    Logger::log << "Shutting down!" << Logger::End;

    delete g_textRenderer;
    delete g_uiRenderer;
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

