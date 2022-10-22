#define _DEF_GLOBALS_
#include "globals.h"
#undef _DEF_GLOBALS_
#include "App.h"
#include "Bindings.h"
#include "SessionHandler.h"
#include <filesystem>
#include "dialogs/FindDialog.h"
#include "dialogs/FindListDialog.h"

int main(int argc, char** argv)
{
    Logger::setLoggerVerbosity(Logger::LoggerVerbosity::Debug);

    g_exePath = App::getExePath();
    Logger::log << "Exe path: " << g_exePath << Logger::End;
    g_exeDirPath = std::filesystem::path{g_exePath}.parent_path();
    Logger::log << "Exe dir: " << g_exeDirPath << Logger::End;

    Logger::dbg << "Initializing GLFW" << Logger::End;
    if (!glfwInit())
    {
        Logger::fatal << "Failed to initialize GLFW" << Logger::End;
    }

    double appStartTime = glfwGetTime();

    Logger::dbg << "Creating GLFW window" << Logger::End;
    g_window = App::createWindow();
    Logger::dbg << "Created GLFW window" << Logger::End;

    Logger::dbg << "Initializing GLEW" << Logger::End;
    App::initGlew();
    Logger::dbg << "Initialized GLEW" << Logger::End;

    Logger::dbg << "Configurign OpenGL context" << Logger::End;
    App::setupGlFeatures();
    Logger::dbg << "OpenGL context configured" << Logger::End;

    App::setupKeyBindings();

    App::initGit();
    App::loadCursors();
    App::loadSignImages();

    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(UNPACK_RGB_COLOR(DEF_BG), 1.0f);
    glfwSwapBuffers(g_window);

    g_textRenderer.reset(App::createTextRenderer());
    g_uiRenderer.reset(App::createUiRenderer());
    g_hoverPopup.reset(new FloatingWindow);
    g_progressPopup.reset(new ProgressFloatingWin);
    g_lspInfoPopup.reset(new FloatingWindow);
    g_fileTypeHandler.reset(App::createFileTypeHandler());
    g_recentFilePaths = std::make_unique<RecentFileList>();
    App::createAutocompleteProviders();
    App::loadTheme();
    glfwPollEvents();

    for (int i{1}; i < argc; ++i)
    {
        const std::string path = argv[i];
        if (std::filesystem::is_directory(path))
            continue;
        g_tabs.push_back(std::make_unique<Split>(App::openFileInNewBuffer(path)));
    }
    if (!g_tabs.empty())
    {
        g_activeBuff = g_tabs[0]->getActiveBufferRecursively();
    }

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
            ", loaded "+std::to_string(g_tabs.size())+" files)", StatusMsg::Type::Info);

    int msUntilCursorBlinking = CURSOR_BLINK_MS;
    while (!glfwWindowShouldClose(g_window))
    {
        const double startTime = glfwGetTime();

        glfwPollEvents();

        if (CURSOR_BLINK_MS >= 0 && msUntilCursorBlinking <= 0 && g_activeBuff)
        {
            g_activeBuff->toggleCursorVisibility();
            g_isRedrawNeeded = true;
            msUntilCursorBlinking = CURSOR_BLINK_MS;
        }

        if (g_hasBindingToCall)
        {
            Bindings::runFetchedBinding();
            g_hasBindingToCall = false;
        }

        if (g_isRedrawNeeded || g_dialogFlashTime > 0)
        {
            glClearColor(UNPACK_RGB_COLOR(g_theme->bgColor), 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

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
            App::renderPopups();
            App::renderDialogs();
            App::renderPrompt();

            g_isRedrawNeeded = false;
        }

        if (g_isTitleUpdateNeeded)
        {
            glfwSetWindowTitle(g_window, genTitle().c_str());
            g_isTitleUpdateNeeded = false;
        }

        glfwSwapBuffers(g_window);

        const double frameTimeSec = glfwGetTime()-startTime;

        msUntilCursorBlinking -= frameTimeSec*1000;
        g_statMsg.tick(frameTimeSec);
        if (g_dialogFlashTime > 0)
            g_dialogFlashTime -= frameTimeSec*1000;
        Prompt::get()->update(frameTimeSec*1000);
        if (g_activeBuff)
        {
            g_activeBuff->tickCursorHold(frameTimeSec*1000);
            g_activeBuff->tickAutoReload(frameTimeSec*1000);
            g_activeBuff->tickGitBranchUpdate(frameTimeSec*1000);
        }
        if (!g_dialogs.empty())
        {
            if (auto fldlg = dynamic_cast<FindListDialog*>(g_dialogs.back().get()))
            {
                fldlg->tick(frameTimeSec*1000);
            }
        }
        App::tickMouseHold(frameTimeSec*1000);
    }

    Logger::log << "Shutting down!" << Logger::End;
    sessHndlr.writeToFile();
    g_tabs.clear();
    // Last thing to do, we keep alive the window while cleaning up buffers
    glfwDestroyWindow(g_window);
    glfwTerminate();
    Git::shutdown();
    return 0;
}

