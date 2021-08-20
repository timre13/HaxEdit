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
#include "FontRenderer.h"
#include "Buffer.h"

#define DEF_WIN_W 1500
#define DEF_WIN_H 1000


FontRenderer* g_fontRendererPtr{};
bool g_isRedrawNeeded = false;

void windowResizeCB(GLFWwindow*, int width, int height)
{
    glViewport(0, 0, width, height);
    g_fontRendererPtr->onWindowResized(width, height);
}

void windowRefreshCB(GLFWwindow*)
{
    g_isRedrawNeeded = true;
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

    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
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
    FontRenderer fontRenderer = {regularFontPath, boldFontPath, italicFontPath, boldItalicFontPath};
    g_fontRendererPtr = &fontRenderer;

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
            glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
            glfwSwapBuffers(window);

            if (!buffers.empty())
            {
                fontRenderer.renderString(buffers[currentBufferI].getContent());
            }

            g_isRedrawNeeded = false;
        }

        glfwSwapBuffers(window);
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

