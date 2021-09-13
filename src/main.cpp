#include "App.h"

int g_windowWidth = 0;
int g_windowHeight = 0;

bool g_isRedrawNeeded = false;
bool g_isTitleUpdateNeeded = true;
bool g_isDebugDrawMode = false;

std::vector<Buffer> g_buffers;
size_t g_currentBufferI{};

std::vector<std::unique_ptr<Dialog>> g_dialogs;

std::unique_ptr<TextRenderer> g_textRenderer;
std::unique_ptr<UiRenderer> g_uiRenderer;
std::unique_ptr<FileTypeHandler> g_fileTypeHandler;

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

    auto icon = App::loadProgramIcon();

    for (int i{1}; i < argc; ++i)
    {
        g_buffers.push_back(Buffer{});
        if (g_buffers.back().open(argv[i]))
        {
            g_dialogs.push_back(std::make_unique<MessageDialog>(
                        "Failed to open file: \""+std::string{argv[i]}+'"', MessageDialog::Type::Error));
        }
    }
    if (g_buffers.empty())
    {
        g_buffers.emplace_back();
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

            if (g_buffers.empty())
            {
                std::string welcomeMsg = WELCOME_MSG;
                {
                    size_t buildDatePos = welcomeMsg.find("%BUILD_DATE%");
                    if (buildDatePos != std::string::npos)
                    {
                        welcomeMsg = welcomeMsg.replace(buildDatePos, 12, (__DATE__ " at " __TIME__));
                    }
                }
                {
                    size_t buildTypePos = welcomeMsg.find("%BUILD_TYPE%");
                    if (buildTypePos != std::string::npos)
                    {
                        std::string buildType =
#ifdef NDEBUG
                            "Release";
#else
                            "Debug";
#endif
                        welcomeMsg = welcomeMsg.replace(buildTypePos, 12, buildType);
                    }
                }
                {
                    size_t vendorStrPos = welcomeMsg.find("%GL_VENDOR%");
                    if (vendorStrPos != std::string::npos)
                    {
                        welcomeMsg = welcomeMsg.replace(vendorStrPos, 11, std::string((const char*)glGetString(GL_VENDOR)));
                    }
                }
                {
                    size_t rendererStrPos = welcomeMsg.find("%GL_RENDERER%");
                    if (rendererStrPos != std::string::npos)
                    {
                        welcomeMsg = welcomeMsg.replace(rendererStrPos, 13, std::string((const char*)glGetString(GL_RENDERER)));
                    }
                }
                g_textRenderer->renderString(welcomeMsg, {200, 30}, FontStyle::Bold, {0.8f, 0.8f, 0.8f}, true);

                const int origIconW = icon->getPhysicalSize().x;
                const int origIconH = icon->getPhysicalSize().y;
                const float iconRatio = (float)origIconW/origIconH;
                const int iconSize = std::min(
                        std::min(g_windowWidth, 512),
                        std::min(g_windowHeight, 512));
                icon->render({g_windowWidth/2-iconSize*iconRatio/2, g_windowHeight/2-iconSize/2}, {iconSize*iconRatio, iconSize});
            }
            else
            {
                App::renderBuffers();
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

