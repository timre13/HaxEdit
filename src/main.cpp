#define _DEF_GLOBALS_
#include "globals.h"
#undef _DEF_GLOBALS_
#include "App.h"

int main(int argc, char** argv)
{
    Logger::setLoggerVerbosity(Logger::LoggerVerbosity::Debug);

    if (!glfwInit())
    {
        Logger::fatal << "Failed to initialize GLFW" << Logger::End;
    }

    auto window = App::createWindow();
    Logger::dbg << "Created GLFW window" << Logger::End;

    App::initGlew();
    Logger::dbg << "Initialized GLEW" << Logger::End;

    App::setupGlFeatures();
    Logger::dbg << "OpenGL context configured" << Logger::End;

    App::setupKeyBindings();

    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(UNPACK_RGB_COLOR(BG_COLOR), 1.0f);
    glfwSwapBuffers(window);

    g_textRenderer.reset(App::createTextRenderer());
    g_uiRenderer.reset(App::createUiRenderer());
    g_fileTypeHandler.reset(App::createFileTypeHandler());
    glfwPollEvents();

    for (int i{1}; i < argc; ++i)
    {
        const std::string path = argv[i];
        Buffer* buffer;
        if (isImageExtension(getFileExt(path)))
        {
            buffer = new ImageBuffer;
        }
        else
        {
            buffer = new Buffer;
        }
        if (buffer->open(path))
        {
            g_dialogs.push_back(std::make_unique<MessageDialog>(
                        "Failed to open file: \""+path+'"',
                        MessageDialog::Type::Error));
        }
        g_tabs.push_back(std::make_unique<Split>(buffer));
    }
    if (!g_tabs.empty())
    {
        g_activeBuff = g_tabs[0]->getActiveBufferRecursive();
    }

    //if (g_tabs.empty())
    //{
    //    g_tabs.emplace_back(new Buffer{});
    //    g_tabs.back()->open(__FILE__);
    //}

    auto genTitle{
        [&](){
            if (!g_activeBuff)
                return std::string{"HaxorEdit"};
            return g_activeBuff->getFileName()
                + std::string{" - ["} + std::to_string(g_currTabI+1) + '/'
                + std::to_string(g_tabs.size()) + "]";
        }
    };

    float framesUntilCursorBlinking = CURSOR_BLINK_FRAMES;
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        if (CURSOR_BLINK_FRAMES >= 0 && framesUntilCursorBlinking <= 0 && g_activeBuff)
        {
            g_activeBuff->toggleCursorVisibility();
            g_isRedrawNeeded = true;
            framesUntilCursorBlinking = CURSOR_BLINK_FRAMES;
        }

        if (g_isRedrawNeeded)
        {
            glClear(GL_COLOR_BUFFER_BIT);
            glClearColor(UNPACK_RGB_COLOR(BG_COLOR), 1.0f);

            if (g_tabs.empty())
            {
                App::renderStartupScreen();
            }
            else
            {
                App::renderBuffers();
                App::renderStatusLine();
                App::renderTabLine();
            }
            App::renderDialogs();

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
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

