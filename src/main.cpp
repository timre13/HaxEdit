#define _DEF_GLOBALS_
#include "globals.h"
#undef _DEF_GLOBALS_
#include "App.h"
#include "SessionHandler.h"

int main(int argc, char** argv)
{
    Logger::setLoggerVerbosity(Logger::LoggerVerbosity::Debug);

    if (!glfwInit())
    {
        Logger::fatal << "Failed to initialize GLFW" << Logger::End;
    }

    double appStartTime = glfwGetTime();

    g_window = App::createWindow();
    Logger::dbg << "Created GLFW window" << Logger::End;

    App::initGlew();
    Logger::dbg << "Initialized GLEW" << Logger::End;

    App::setupGlFeatures();
    Logger::dbg << "OpenGL context configured" << Logger::End;

    App::setupKeyBindings();

    App::loadCursors();
    App::loadSignImages();

    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(UNPACK_RGB_COLOR(BG_COLOR), 1.0f);
    glfwSwapBuffers(g_window);

    g_textRenderer.reset(App::createTextRenderer());
    g_uiRenderer.reset(App::createUiRenderer());
    g_fileTypeHandler.reset(App::createFileTypeHandler());
    App::createAutocompleteProviders();
    glfwPollEvents();

    for (int i{1}; i < argc; ++i)
    {
        const std::string path = argv[i];
        g_tabs.push_back(std::make_unique<Split>(App::openFileInNewBuffer(path)));
    }
    if (!g_tabs.empty())
    {
        g_activeBuff = g_tabs[0]->getActiveBufferRecursively();
    }

    //if (g_tabs.empty())
    //{
    //    g_tabs.emplace_back(new Buffer{});
    //    g_tabs.back()->open(__FILE__);
    //}

    SessionHandler sessHndlr{"Session.haxedsess"};
    // If we didn't get files to open, load the last session
    if (g_tabs.empty())
    {
        sessHndlr.loadFromFile();
    }

    auto genTitle{
        [&](){
            if (!g_activeBuff)
                return std::string{"HaxorEdit"};
            return g_activeBuff->getFileName()
                + std::string{" - ["} + std::to_string(g_currTabI+1) + '/'
                + std::to_string(g_tabs.size()) + "]";
        }
    };

    g_statMsg.set("Welcome to HaxorEdit!\t"
            "(Launched in "+std::to_string(glfwGetTime()-appStartTime)+"s"
            ", loaded "+std::to_string(g_tabs.size())+" files)");

    float framesUntilCursorBlinking = CURSOR_BLINK_FRAMES;
    while (!glfwWindowShouldClose(g_window))
    {
        double startTime = glfwGetTime();

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
                App::renderTabLine();
            }
            App::renderStatusLine();
            App::renderDialogs();

            g_isRedrawNeeded = false;
        }

        if (g_isTitleUpdateNeeded)
        {
            glfwSetWindowTitle(g_window, genTitle().c_str());
            g_isTitleUpdateNeeded = false;
        }

        glfwSwapBuffers(g_window);
        double frameTime = glfwGetTime()-startTime;
        --framesUntilCursorBlinking;
        g_statMsg.tick(frameTime);
    }

    Logger::log << "Shutting down!" << Logger::End;
    sessHndlr.writeToFile();
    g_tabs.clear();
    // Last thing to do, we keep alive the window while cleaning up buffers
    glfwDestroyWindow(g_window);
    glfwTerminate();
    return 0;
}

