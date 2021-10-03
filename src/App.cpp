#include "App.h"
#include "Bindings.h"
#include "../external/stb/stb_image.h"

#define BUFFER_RESIZE_MAX_CURS_DIST 10

static std::string s_selectedSaveDir = "";

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
    glfwSetWindowCloseCallback(window, App::windowCloseCB);
    glfwSetCursorPosCallback(window, App::cursorPosCB);
    glfwSetMouseButtonCallback(window, App::mouseButtonCB);
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

void App::loadCursors()
{
    GLFWimage image;
    image.pixels = stbi_load("../img/busy_cursor.png", &image.width, &image.height, nullptr, 4);
    Cursors::busy = glfwCreateCursor(&image, 0, 0);
    stbi_image_free(image.pixels); // glfwCreateCursor() copies the data, so free the original
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
    Bindings::noModMap[GLFW_KEY_MINUS]      = Bindings::Callbacks::zoomOutBufferIfImage;
    Bindings::noModMap[GLFW_KEY_KP_SUBTRACT]= Bindings::Callbacks::zoomOutBufferIfImage;
    Bindings::noModMap[GLFW_KEY_EQUAL]      = Bindings::Callbacks::zoomInBufferIfImage;
    Bindings::noModMap[GLFW_KEY_KP_ADD]     = Bindings::Callbacks::zoomInBufferIfImage;

    Bindings::ctrlMap[GLFW_KEY_N]           = Bindings::Callbacks::createBufferInNewTab;
    Bindings::ctrlMap[GLFW_KEY_S]           = Bindings::Callbacks::saveCurrentBuffer;
    Bindings::ctrlMap[GLFW_KEY_O]           = Bindings::Callbacks::openFile;
    Bindings::ctrlMap[GLFW_KEY_Q]           = Bindings::Callbacks::closeActiveBuffer;
    Bindings::ctrlMap[GLFW_KEY_PAGE_UP]     = Bindings::Callbacks::goToPrevTab;
    Bindings::ctrlMap[GLFW_KEY_PAGE_DOWN]   = Bindings::Callbacks::goToNextTab;
    Bindings::ctrlMap[GLFW_KEY_TAB]         = Bindings::Callbacks::goToNextSplit;
    Bindings::ctrlMap[GLFW_KEY_HOME]        = Bindings::Callbacks::goToFirstChar;
    Bindings::ctrlMap[GLFW_KEY_END]         = Bindings::Callbacks::goToLastChar;
    Bindings::ctrlMap[GLFW_KEY_MINUS]       = Bindings::Callbacks::decreaseFontSize;
    Bindings::ctrlMap[GLFW_KEY_KP_SUBTRACT] = Bindings::Callbacks::decreaseFontSize;
    Bindings::ctrlMap[GLFW_KEY_EQUAL]       = Bindings::Callbacks::increaseFontSize;
    Bindings::ctrlMap[GLFW_KEY_KP_ADD]      = Bindings::Callbacks::increaseFontSize;

    Bindings::ctrlShiftMap[GLFW_KEY_S]      = Bindings::Callbacks::saveCurrentBufferAs;
    Bindings::ctrlShiftMap[GLFW_KEY_TAB]    = Bindings::Callbacks::goToPrevSplit;
}

void App::renderBuffers()
{
    if (!g_tabs.empty())
    {
        g_activeBuff->updateCursor();
        g_tabs[g_currTabI]->forEachBufferRecursively([](Buffer* buff) { buff->render(); });
    }
}

void App::renderStatusLine()
{
    if (!g_activeBuff)
        return;

    const int winW = g_textRenderer->getWindowWidth();
    const int winH = g_textRenderer->getWindowHeight();

    g_uiRenderer->renderFilledRectangle(
            {0, winH-g_fontSizePx*1.2f},
            {winW, winH},
            RGB_COLOR_TO_RGBA(STATUSBAR_BG_COLOR));


    const std::string leftStr = g_activeBuff->m_filePath+(g_activeBuff->m_isReadOnly ? " [RO]" : "");
    g_textRenderer->renderString(
            leftStr,
            {0, winH-g_fontSizePx-4});

    g_activeBuff->updateRStatusLineStr();
    assert(g_activeBuff->m_statusLineStr.maxLen > 0);
    g_textRenderer->renderString(
            g_activeBuff->m_statusLineStr.str,
            {winW-g_fontSizePx*g_activeBuff->m_statusLineStr.maxLen*0.7f,
             winH-g_fontSizePx-4});
}

void App::renderTabLine()
{
    // Draw tabline background
    g_uiRenderer->renderFilledRectangle({0, 0}, {g_windowWidth, TABLINE_HEIGHT_PX},
            RGB_COLOR_TO_RGBA(TABLINE_BG_COLOR));
    uint tabI{};
    // Draw tabs
    for (const auto& tab : g_tabs)
    {
        const int tabX = TABLINE_TAB_WIDTH_PX*tabI;
        const auto& buffer = tab->getActiveBufferRecursively();

        // Stop rendering if we go out of the screen
        if (tabX > g_windowWidth)
            break;

        // Render a tab, use a differenct color when it is the current tab
        g_uiRenderer->renderFilledRectangle(
                {tabX, 0},
                {TABLINE_TAB_WIDTH_PX*(tabI+1)-2, TABLINE_HEIGHT_PX},
                RGB_COLOR_TO_RGBA((
                        tabI == g_currTabI ?
                        TABLINE_ACTIVE_TAB_COLOR :
                        TABLINE_TAB_COLOR)));
        // Render the filename, use orange color when the buffer is modified since the last save
        g_textRenderer->renderString(buffer->getFileName().substr(0, TABLINE_TAB_MAX_TEXT_LEN),
                {tabX+20, -2},
                tabI == g_currTabI ? FontStyle::BoldItalic : FontStyle::Regular,
                (buffer->isModified() ? RGBColor{1.0f, 0.5f, 0.0f} : RGBColor{1.0f, 1.0, 1.0f}));

        g_fileTypeHandler->getIconFromFilename(buffer->getFileName(), false)->render({tabX+2, 2}, {16, 16});
        ++tabI;
    }
}

void App::renderDialogs()
{
    if (!g_dialogs.empty())
    {
        g_uiRenderer->renderFilledRectangle(
                {0, 0},
                {g_windowWidth, g_windowHeight},
                {UNPACK_RGB_COLOR(BG_COLOR), 0.7f});

        // Render the top dialog
        g_dialogs.back()->render();
    }
}

static const std::string genWelcomeMsg(std::string templ)
{
    static std::map<std::string, std::string> map;
    map["BUILD_DATE"] = __DATE__ " at " __TIME__;
    map["BUILD_TYPE"] =
#ifdef NDEBUG
                "Release";
#else
                "Debug";
#endif
    map["BUILD_IS_OPTIMIZED"] =
#ifdef __OPTIMIZE__
                "ON";
#else
                "OFF";
#endif
    map["BUILD_IS_ASAN_ON"] =
#ifdef __has_feature
#   if __has_feature(address_sanitizer)
                "ON";
#   else
                "OFF";
#   endif
#else
                "???";
#endif
    map["COMPILER_NAME"] = __VERSION__;
    map["GL_VENDOR"] = (const char*)glGetString(GL_VENDOR);
    map["GL_RENDERER"] = (const char*)glGetString(GL_RENDERER);

    for (const auto& pair : map)
    {
        const size_t pos = templ.find('%'+pair.first+'%');
        if (pos != std::string::npos)
        {
            templ = templ.replace(pos, pair.first.length()+2, pair.second);
        }
    }
    return templ;
}

void App::renderStartupScreen()
{
    static const auto icon = loadProgramIcon();
    static const auto welcomeMsg1 = genWelcomeMsg(WELCOME_MSG_PRIMARY);
    static const auto welcomeMsg2 = genWelcomeMsg(WELCOME_MSG_SECONDARY);
    g_textRenderer->renderString(welcomeMsg1, {200, 30}, FontStyle::Bold, {0.8f, 0.8f, 0.8f}, true);
    g_textRenderer->renderString(welcomeMsg2,
            {200, 30+(strCountLines(welcomeMsg1)+1)*g_fontSizePx},
            FontStyle::Regular, {0.8f, 0.8f, 0.8f}, true);

    const int origIconW = icon->getPhysicalSize().x;
    const int origIconH = icon->getPhysicalSize().y;
    const float iconRatio = (float)origIconW/origIconH;
    const int iconSize = std::min(
            std::min(g_windowWidth, 512),
            std::min(g_windowHeight, 512));
    icon->render({g_windowWidth/2-iconSize*iconRatio/2, g_windowHeight/2-iconSize/2},
            {iconSize*iconRatio, iconSize});
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
    Logger::log << "Resized window to " << width << 'x' << height << Logger::End;
    glViewport(0, 0, width, height);
    g_windowWidth = width;
    g_windowHeight = height;
    g_textRenderer->onWindowResized(width, height);
    g_uiRenderer->onWindowResized(width, height);
    for (auto& tab : g_tabs)
    {
        tab->makeChildrenSizesEqual();
    }
}

static void handleSaveAsDialog(FileDialog* fileDialog)
{
    const std::string path = fileDialog->getSelectedFilePath();
    if (fileDialog->isDirSelected()) // ask filename
    {
        s_selectedSaveDir = path;
        g_dialogs.insert(g_dialogs.begin()+g_dialogs.size()-1, std::make_unique<AskerDialog>(
                    "Filename:",
                    AskerDialog::Id::AskSaveFileName));
    }
    else // Save to an existing file
    {
        if (g_activeBuff->saveAsToFile(path))
        {
            g_dialogs.push_back(std::make_unique<MessageDialog>(
                        "Failed to save file: \""+fileDialog->getSelectedFilePath()+'"',
                        MessageDialog::Type::Error));
        }
    }
}

static void handleOpenDialog(FileDialog* fileDialog)
{
    auto path = fileDialog->getSelectedFilePath();

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

    if (fileDialog->getOpenMode() == FileDialog::OpenMode::NewTab || g_tabs.empty())
    {
        if (g_tabs.empty())
        {
            g_tabs.push_back(std::make_unique<Split>(buffer));
            g_activeBuff = buffer;
            g_currTabI = 0;
        }
        else
        {
            // Insert the buffer next to the current one
            g_tabs.insert(g_tabs.begin()+g_currTabI+1, std::make_unique<Split>(buffer));
            g_activeBuff = buffer;
            ++g_currTabI; // Go to the current buffer
        }
    }
    else
    {
        g_tabs[g_currTabI]->addChild(buffer);
        g_activeBuff = g_tabs[g_currTabI]->getActiveBufferRecursively();
    }
}

static void handleDialogClose()
{
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
            if (msgDialog->getId() == MessageDialog::Id::AskSaveCloseActiveBuffer)
            {
                if (msgDialog->getPressedBtnI() == 0) // If pressed "Yes"
                {
                    Bindings::Callbacks::saveCurrentBuffer();
                    Bindings::Callbacks::closeActiveBuffer();
                }
                else if (msgDialog->getPressedBtnI() == 1) // Pressed "No"
                {
                    g_activeBuff->setModified(false); // Drop changes
                    Bindings::Callbacks::closeActiveBuffer();
                }
                else
                {
                    // Do nothing
                }
            }
        }
        else if (auto* askerDialog = dynamic_cast<AskerDialog*>(g_dialogs.back().get()))
        {
            if (askerDialog->getId() == AskerDialog::Id::AskSaveFileName)
            {
                assert(g_activeBuff);
                if (g_activeBuff->saveAsToFile(
                            std_fs::path{s_selectedSaveDir}/std_fs::path{askerDialog->getValue()}))
                {
                    g_dialogs.insert(g_dialogs.begin()+g_dialogs.size()-1, std::make_unique<MessageDialog>(
                                "Failed to save file: \""+fileDialog->getSelectedFilePath()+'"',
                                MessageDialog::Type::Error));
                }
            }
        }
        g_dialogs.pop_back();
        g_shouldIgnoreNextChar = true;
        g_isRedrawNeeded = true;
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
        handleDialogClose();
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
        g_dialogs.back()->handleChar(codePoint);
        return;
    }

    if (g_shouldIgnoreNextChar)
    {
        g_shouldIgnoreNextChar = false;
        return;
    }

    if (g_activeBuff)
    {
        g_activeBuff->insert((char)codePoint);
    }
    g_isRedrawNeeded = true;
}

void App::windowScrollCB(GLFWwindow*, double, double yOffset)
{
    if (!g_dialogs.empty())
        return;

    if (g_activeBuff)
    {
        g_activeBuff->scrollBy(yOffset*SCROLL_SPEED_MULTIPLIER);
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

static bool hasModifiedBuffer(Split* parent)
{
    for (const auto& child : parent->getChildren())
    {
        if (std::holds_alternative<std::unique_ptr<Split>>(child))
        {
            hasModifiedBuffer(std::get<std::unique_ptr<Split>>(child).get());
        }
        else if (std::get<std::unique_ptr<Buffer>>(child)->isModified())
        {
            return true;
        }
    }
    return false;
}

void App::windowCloseCB(GLFWwindow* window)
{
    Logger::log << "Window close requested!" << Logger::End;

    for (const auto& tab : g_tabs)
    {
        if (hasModifiedBuffer(tab.get()))
        {
            glfwSetWindowShouldClose(window, false);
            g_dialogs.push_back(std::make_unique<MessageDialog>(
                        "Please save or close all modified buffers",
                        MessageDialog::Type::Information));
            Logger::log << "Window close aborted" << Logger::End;
            return;
        }
    }
}

void App::cursorPosCB(GLFWwindow* window, double x, double y)
{
    g_cursorX = (int)x;
    g_cursorY = (int)y;

    if (!g_dialogs.empty() && !g_dialogs.back()->isClosed())
    {
        if (g_dialogs.back()->isInsideButton({g_cursorX, g_cursorY}))
        {
            glfwSetCursor(window, glfwCreateStandardCursor(GLFW_HAND_CURSOR));
        }
        else
        {
            glfwSetCursor(window, NULL);
        }
    }
    else
    {
        // If the cursor is inside the tab bar
        if (g_cursorY < TABLINE_HEIGHT_PX)
        {
            glfwSetCursor(window, NULL);
        }

        if (!g_tabs.empty())
        {
            // TODO: Nested splits are not well supported
            for (size_t i{}; i < g_tabs[g_currTabI]->getChildren().size(); ++i)
            {
                const auto& child = g_tabs[g_currTabI]->getChildren()[i];
                if (std::holds_alternative<std::unique_ptr<Buffer>>(child))
                {
                    const auto& buffer = std::get<std::unique_ptr<Buffer>>(child);

                    // If the cursor is near the vertical buffer border
                    if (i != g_tabs.size()-1
                        && std::abs(g_cursorX -
                            (buffer->getXPos()+buffer->getWidth())) < BUFFER_RESIZE_MAX_CURS_DIST)
                    {
                        glfwSetCursor(window, glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR));
                    }
                    // If the cursor is near the horizontal buffer border
                    else if (i != g_tabs.size()-1
                        && std::abs(g_cursorY -
                            (buffer->getYPos()+buffer->getHeight())) < BUFFER_RESIZE_MAX_CURS_DIST)
                    {
                        glfwSetCursor(window, glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR));
                        break;
                    }
                    // If the pointer is inside a buffer
                    else if (g_cursorX >= buffer->getXPos()
                          && g_cursorX < buffer->getXPos()+buffer->getWidth()
                          && g_cursorY >= buffer->getYPos()
                          && g_cursorY < buffer->getYPos()+buffer->getHeight())
                    {
                        glfwSetCursor(window, glfwCreateStandardCursor(
                                    dynamic_cast<ImageBuffer*>(buffer.get()) == nullptr
                                    ? GLFW_IBEAM_CURSOR : GLFW_CROSSHAIR_CURSOR
                        ));
                        break;
                    }
                }
            }
        }
        else
        {
            glfwSetCursor(window, NULL);
        }
    }
}

void App::mouseButtonCB(GLFWwindow*, int btn, int act, int mods)
{
    (void)mods;
    Logger::dbg << "Mouse button " << btn << " (" << GlfwHelpers::mouseBtnToStr(btn) << ") "
        << GlfwHelpers::keyOrBtnActionToStr(act) << " at {"
        << g_cursorX << ", " << g_cursorY << '}' << Logger::End;

    switch (btn)
    {
    case GLFW_MOUSE_BUTTON_LEFT:   g_isMouseBtnLPressed = (act == GLFW_PRESS); break;
    case GLFW_MOUSE_BUTTON_RIGHT:  g_isMouseBtnRPressed = (act == GLFW_PRESS); break;
    case GLFW_MOUSE_BUTTON_MIDDLE: g_isMouseBtnMPressed = (act == GLFW_PRESS); break;
    }
    if (btn == GLFW_MOUSE_BUTTON_LEFT && act == GLFW_PRESS)
    {
        g_mouseBtnLPressX = g_cursorX;
        g_mouseBtnLPressY = g_cursorY;
    }

    if (!g_dialogs.empty())
    {
        if (act == GLFW_PRESS && g_dialogs.back()->isInsideButton({g_cursorX, g_cursorY}))
        {
            g_dialogs.back()->pressButtonAt({g_cursorX, g_cursorY});
            handleDialogClose();
            g_isRedrawNeeded = true;
        }
        return;
    }

    // If the cursor has been pressed on the tab line
    if (act == GLFW_PRESS
     && g_cursorX < (int)g_tabs.size()*TABLINE_TAB_WIDTH_PX
     && g_cursorY < TABLINE_HEIGHT_PX)
    {
        // Switch to the clicked tab
        g_currTabI = g_cursorX/TABLINE_TAB_WIDTH_PX;
        assert(g_currTabI < g_tabs.size());

        g_activeBuff = g_tabs[g_currTabI]->getActiveBufferRecursively();
        g_isTitleUpdateNeeded = true;
        g_isRedrawNeeded = true;
    }

    if (!g_tabs.empty())
    {
        // TODO: Nested splits are not well supported
        for (size_t i{}; i < g_tabs[g_currTabI]->getChildren().size(); ++i)
        {
            const auto& child = g_tabs[g_currTabI]->getChildren()[i];
            if (std::holds_alternative<std::unique_ptr<Buffer>>(child))
            {
                const auto& buffer = std::get<std::unique_ptr<Buffer>>(child);

                // If the buffer border has been dragged, resize the buffer
                if (i != g_tabs.size()-1
                    && act == GLFW_RELEASE && btn == GLFW_MOUSE_BUTTON_LEFT
                    && std::abs(g_mouseBtnLPressX -
                        (buffer->getXPos()+buffer->getWidth())) < BUFFER_RESIZE_MAX_CURS_DIST)
                {
                    g_tabs[g_currTabI]->setChildWidth(i,
                            buffer->getWidth()+g_cursorX-(buffer->getXPos()+buffer->getWidth()));
                    g_isRedrawNeeded = true;
                    break;
                }

                // If the pointer is inside the buffer
                if (g_cursorX >= buffer->getXPos()
                      && g_cursorX < buffer->getXPos()+buffer->getWidth()
                      && g_cursorY >= buffer->getYPos()
                      && g_cursorY < buffer->getYPos()+buffer->getHeight())
                {
                    // Activate the clicked buffer
                    g_tabs[g_currTabI]->setActiveChildI(i);
                    g_activeBuff = g_tabs[g_currTabI]->getActiveBufferRecursively();
                    g_isTitleUpdateNeeded = true;
                    g_isRedrawNeeded = true;
                    break;
                }
            }
        }
    }
}
