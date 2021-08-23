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

TextRenderer* g_fontRendererPtr{};
UiRenderer* g_uiRendererPtr{};
bool g_isRedrawNeeded = false;
int g_windowWidth = 0;
int g_windowHeight = 0;
bool g_isDebugDrawMode = false;

static void windowResizeCB(GLFWwindow*, int width, int height)
{
    glViewport(0, 0, width, height);
    g_windowWidth = width;
    g_windowHeight = height;
    g_fontRendererPtr->onWindowResized(width, height);
    g_uiRendererPtr->onWindowResized(width, height);
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
}

int main(int argc, char** argv)
{
    Logger::setLoggerVerbosity(Logger::LoggerVerbosity::Debug);

    std::vector<Buffer> buffers;
    size_t currentBufferI{};
    for (int i{1}; i < argc; ++i)
    {
        buffers.push_back(Buffer{});
        if (buffers.back().open(argv[i]))
        {
            // TODO: Show an error dialog or something
        }
    }

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
    TextRenderer fontRenderer = {regularFontPath, boldFontPath, italicFontPath, boldItalicFontPath};
    g_fontRendererPtr = &fontRenderer;

    UiRenderer uiRenderer;
    g_uiRendererPtr = &uiRenderer;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    auto genTitle{
        [&](){
            if (buffers.empty())
                return std::string{"HaxorEdit"};
            return buffers[currentBufferI].getFileName()
                + std::string{" - ["} + std::to_string(currentBufferI+1) + '/' + std::to_string(buffers.size()) + "]";
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
            glfwSwapBuffers(window);

            if (!buffers.empty())
            {
                buffers[currentBufferI].render();
            }

            g_isRedrawNeeded = false;
        }

        glfwSwapBuffers(window);
    }

    Logger::log << "Shutting down!" << Logger::End;
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

