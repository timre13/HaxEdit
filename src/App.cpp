#include "App.h"
#include "Bindings.h"

GLFWwindow* App::createWindow()
{
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    auto window = glfwCreateWindow(DEF_WIN_W, DEF_WIN_H, "HaxorEdit", nullptr, nullptr);
    if (!window)
    {
        Logger::fatal << "Failed to create window" << Logger::End;
    }
    glfwSetWindowSizeCallback(window, App::windowResizeCB);
    glfwSetWindowRefreshCallback(window, App::windowRefreshCB);
    glfwSetKeyCallback(window, App::windowKeyCB);
    glfwSetScrollCallback(window, App::windowScrollCB);
    glfwSetCharCallback(window, App::windowCharCB);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    return window;
}

void App::initGlew()
{
    glewExperimental = GL_TRUE;
    uint initStatus = glewInit();
    if (initStatus != GLEW_OK)
    {
        Logger::fatal << "Failed to initialize GLEW: " << glewGetErrorString(initStatus) << Logger::End;
    }
}

void App::setupGlFeatures()
{
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(glDebugMsgCB, 0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

TextRenderer* App::createTextRenderer()
{
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
    assert(regularFontPath.length()
        && boldFontPath.length()
        && italicFontPath.length()
        && boldItalicFontPath.length());
    return new TextRenderer{
        regularFontPath,
        boldFontPath,
        italicFontPath,
        boldItalicFontPath};
}

UiRenderer* App::createUiRenderer()
{
    return new UiRenderer{};
}

std::unique_ptr<Image> App::loadProgramIcon()
{
    return std::unique_ptr<Image>(new Image{ICON_FILE_PATH});
}

FileTypeHandler* App::createFileTypeHandler()
{
    return new FileTypeHandler{"../icons/file_icon_index.txt", "../icons/folder_icon_index.txt"};
}

void App::setupKeyBindings()
{
    Bindings::noModMap[GLFW_KEY_RIGHT]      = Bindings::Callbacks::moveCursorRight;
    Bindings::noModMap[GLFW_KEY_LEFT]       = Bindings::Callbacks::moveCursorLeft;
    Bindings::noModMap[GLFW_KEY_DOWN]       = Bindings::Callbacks::moveCursorDown;
    Bindings::noModMap[GLFW_KEY_UP]         = Bindings::Callbacks::moveCursorUp;
    Bindings::noModMap[GLFW_KEY_HOME]       = Bindings::Callbacks::moveCursorToLineBeginning;
    Bindings::noModMap[GLFW_KEY_END]        = Bindings::Callbacks::moveCursorToLineEnd;
    Bindings::noModMap[GLFW_KEY_ENTER]      = Bindings::Callbacks::putEnter;
    Bindings::noModMap[GLFW_KEY_KP_ENTER]   = Bindings::Callbacks::putEnter;
    Bindings::noModMap[GLFW_KEY_BACKSPACE]  = Bindings::Callbacks::deleteCharBackwards;
    Bindings::noModMap[GLFW_KEY_DELETE]     = Bindings::Callbacks::deleteCharForward;
    Bindings::noModMap[GLFW_KEY_KP_DECIMAL] = Bindings::Callbacks::deleteCharForward;
    Bindings::noModMap[GLFW_KEY_TAB]        = Bindings::Callbacks::insertTabOrSpaces;

    Bindings::ctrlMap[GLFW_KEY_N]           = Bindings::Callbacks::createNewBuffer;
    Bindings::ctrlMap[GLFW_KEY_S]           = Bindings::Callbacks::saveCurrentBuffer;
    Bindings::ctrlMap[GLFW_KEY_O]           = Bindings::Callbacks::openFile;
    Bindings::ctrlMap[GLFW_KEY_Q]           = Bindings::Callbacks::closeCurrentBuffer;
    Bindings::ctrlMap[GLFW_KEY_PAGE_UP]     = Bindings::Callbacks::goToPrevTab;
    Bindings::ctrlMap[GLFW_KEY_PAGE_DOWN]   = Bindings::Callbacks::goToNextTab;
    Bindings::ctrlMap[GLFW_KEY_HOME]        = Bindings::Callbacks::goToFirstChar;
    Bindings::ctrlMap[GLFW_KEY_END]         = Bindings::Callbacks::goToLastChar;
    Bindings::ctrlMap[GLFW_KEY_MINUS]       = Bindings::Callbacks::decreaseFontSize;
    Bindings::ctrlMap[GLFW_KEY_KP_SUBTRACT] = Bindings::Callbacks::decreaseFontSize;
    Bindings::ctrlMap[GLFW_KEY_EQUAL]       = Bindings::Callbacks::increaseFontSize;
    Bindings::ctrlMap[GLFW_KEY_KP_ADD]      = Bindings::Callbacks::increaseFontSize;

    Bindings::ctrlShiftMap[GLFW_KEY_S]      = Bindings::Callbacks::saveCurrentBufferAs;
}

void App::renderBuffers()
{
    if (!g_buffers.empty())
    {
        g_buffers[g_currentBufferI].updateCursor();
        g_buffers[g_currentBufferI].render();
    }
}

void App::renderTabLine()
{
    // Draw tabline background
    g_uiRenderer->renderFilledRectangle({0, 0}, {g_windowWidth, TABLINE_HEIGHT_PX},
            RGB_COLOR_TO_RGBA(TABLINE_BG_COLOR));
    uint tabI{};
    // Draw tabs
    for (const auto& buffer : g_buffers)
    {
        const int tabX = TABLINE_TAB_WIDTH_PX*tabI;

        // Stop rendering if we go out of the screen
        if (tabX > g_windowWidth)
            break;

        // Render a tab, use a differenct color when it is the current tab
        g_uiRenderer->renderFilledRectangle(
                {tabX, 0},
                {TABLINE_TAB_WIDTH_PX*(tabI+1)-2, TABLINE_HEIGHT_PX},
                RGB_COLOR_TO_RGBA((
                        tabI == g_currentBufferI ?
                        TABLINE_ACTIVE_TAB_COLOR :
                        TABLINE_TAB_COLOR)));
        // Render the filename, use orange color when the buffer is modified since the last save
        g_textRenderer->renderString(buffer.getFileName().substr(0, TABLINE_TAB_MAX_TEXT_LEN),
                {tabX+20, -2},
                tabI == g_currentBufferI ? FontStyle::BoldItalic : FontStyle::Regular,
                (buffer.isModified() ? RGBColor{1.0f, 0.5f, 0.0f} : RGBColor{1.0f, 1.0, 1.0f}));

        g_fileTypeHandler->getIconFromFilename(buffer.getFileName(), false)->render({tabX+2, 2}, {16, 16});
        ++tabI;
    }
}

void App::renderDialogs()
{
    if (!g_dialogs.empty())
    {
        // Render the top dialog
        g_dialogs.back()->render();
    }
}

static const std::string genWelcomeMsg()
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
            welcomeMsg = welcomeMsg.replace(vendorStrPos, 11,
                    std::string((const char*)glGetString(GL_VENDOR)));
        }
    }
    {
        size_t rendererStrPos = welcomeMsg.find("%GL_RENDERER%");
        if (rendererStrPos != std::string::npos)
        {
            welcomeMsg = welcomeMsg.replace(rendererStrPos, 13,
                    std::string((const char*)glGetString(GL_RENDERER)));
        }
    }
    return welcomeMsg;
}

void App::renderStartupScreen()
{
    static const auto icon = loadProgramIcon();
    static const auto welcomeMsg = genWelcomeMsg();
    g_textRenderer->renderString(welcomeMsg, {200, 30}, FontStyle::Bold, {0.8f, 0.8f, 0.8f}, true);

    const int origIconW = icon->getPhysicalSize().x;
    const int origIconH = icon->getPhysicalSize().y;
    const float iconRatio = (float)origIconW/origIconH;
    const int iconSize = std::min(
            std::min(g_windowWidth, 512),
            std::min(g_windowHeight, 512));
    icon->render({g_windowWidth/2-iconSize*iconRatio/2, g_windowHeight/2-iconSize/2}, {iconSize*iconRatio, iconSize});
}

void GLAPIENTRY App::glDebugMsgCB(
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

void App::windowRefreshCB(GLFWwindow*)
{
    g_isRedrawNeeded = true;
}

void App::windowResizeCB(GLFWwindow*, int width, int height)
{
    glViewport(0, 0, width, height);
    g_windowWidth = width;
    g_windowHeight = height;
    g_textRenderer->onWindowResized(width, height);
    g_uiRenderer->onWindowResized(width, height);
}

static void handleSaveAsDialog(FileDialog* fileDialog)
{
    if (g_buffers[g_currentBufferI].saveAsToFile(fileDialog->getSelectedFilePath()))
    {
        g_dialogs.push_back(std::make_unique<MessageDialog>(
                    "Failed to save file: \""+fileDialog->getSelectedFilePath()+'"',
                    MessageDialog::Type::Error));
    }
}

static void handleOpenDialog(FileDialog* fileDialog)
{
    Buffer buffer;
    if (buffer.open(fileDialog->getSelectedFilePath()))
    {
        g_dialogs.push_back(std::make_unique<MessageDialog>(
                    "Failed to open file: \""+fileDialog->getSelectedFilePath()+'"',
                    MessageDialog::Type::Error));
    }
    if (g_buffers.empty())
    {
        g_buffers.push_back(std::move(buffer));
        g_currentBufferI = 0;
    }
    else
    {
        // Insert the buffer next to the current one
        g_buffers.insert(g_buffers.begin()+g_currentBufferI+1, std::move(buffer));
        ++g_currentBufferI; // Go to the current buffer
    }
}

void App::windowKeyCB(GLFWwindow*, int key, int scancode, int action, int mods)
{
    (void)scancode;

    TIMER_BEGIN_FUNC();

    // We don't care about key releases
    if (action == GLFW_RELEASE)
    {
        TIMER_END_FUNC();
        return;
    }

    // Debug mode toggle should work even when a dialog is open
    if (mods == 0 && key == GLFW_KEY_F3)
    {
        toggleDebugDraw();
        TIMER_END_FUNC();
        return;
    }

    // If there are dialogs open, pass the event to the top one
    if (!g_dialogs.empty())
    {
        g_dialogs.back()->handleKey(key, mods);
        // Dialog closed? Destroy it
        if (g_dialogs.back()->isClosed())
        {
            if (auto* fileDialog = dynamic_cast<FileDialog*>(g_dialogs.back().get()))
            {
                if (fileDialog->getType() == FileDialog::Type::SaveAs) // Save as dialog
                {
                    handleSaveAsDialog(fileDialog);
                }
                else if (fileDialog->getType() == FileDialog::Type::Open) // Open dialog
                {
                    handleOpenDialog(fileDialog);
                }
                g_isTitleUpdateNeeded = true;
            }
            else if (auto* msgDialog = dynamic_cast<MessageDialog*>(g_dialogs.back().get()))
            {
                if (msgDialog->getId() == MessageDialog::Id::AskSaveCloseCurrentBuffer)
                {
                    if (msgDialog->getPressedBtnI() == 0) // If pressed "Yes"
                    {
                        Bindings::Callbacks::saveCurrentBuffer();
                        Bindings::Callbacks::closeCurrentBuffer();
                    }
                    else if (msgDialog->getPressedBtnI() == 1) // Pressed "No"
                    {
                        g_buffers.erase(g_buffers.begin()+g_currentBufferI);
                        g_currentBufferI = g_buffers.size() < 2 ? 0 : g_currentBufferI-1;
                        g_isTitleUpdateNeeded = true;
                    }
                    else
                    {
                        // Do nothing
                    }
                }
            }
            g_dialogs.pop_back();
            g_shouldIgnoreNextChar = true;
        }
        g_isRedrawNeeded = true;
        TIMER_END_FUNC();
        return;
    }

    if (mods == GLFW_MOD_CONTROL)
    {
        Bindings::runBinding(Bindings::ctrlMap, key);
    }
    else if (mods == 0)
    {
        Bindings::runBinding(Bindings::noModMap, key);
    }
    else if (mods == (GLFW_MOD_CONTROL | GLFW_MOD_SHIFT))
    {
        Bindings::runBinding(Bindings::ctrlShiftMap, key);
    }

    TIMER_END_FUNC();
}

void App::windowCharCB(GLFWwindow*, uint codePoint)
{
    // If there are dialogs open, don't react to keypresses
    if (!g_dialogs.empty())
    {
        return;
    }

    if (g_shouldIgnoreNextChar)
    {
        g_shouldIgnoreNextChar = false;
        return;
    }

    if (!g_buffers.empty())
    {
        g_buffers[g_currentBufferI].insert((char)codePoint);
    }
    g_isRedrawNeeded = true;
}

void App::windowScrollCB(GLFWwindow*, double, double yOffset)
{
    if (!g_dialogs.empty())
        return;

    if (!g_buffers.empty())
    {
        g_buffers[g_currentBufferI].scrollBy(yOffset*SCROLL_SPEED_MULTIPLIER);
    }
    g_isRedrawNeeded = true;
}

void App::toggleDebugDraw()
{
    g_isDebugDrawMode = !g_isDebugDrawMode;
    glPolygonMode(GL_FRONT_AND_BACK, g_isDebugDrawMode ? GL_LINE : GL_FILL);
    if (g_isDebugDrawMode)
        glDisable(GL_BLEND);
    else
        glEnable(GL_BLEND);
    g_isRedrawNeeded = true;
}

