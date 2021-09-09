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

            App::renderBuffers();
            App::renderTabLine();
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

