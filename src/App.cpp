#include "App.h"
#include "Bindings.h"
#include "../external/stb/stb_image.h"

#define BUFFER_RESIZE_MAX_CURS_DIST 10

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

void App::createAutocompleteProviders()
{
    Autocomp::dictProvider.reset(new Autocomp::DictionaryProvider{});
}

void App::setupKeyBindings()
{
    // `nmap`, 'imap' and `Callbacks` are from the `Bindings` namespace
    using namespace Bindings;

    { // Normal mode mappings
        nmap.noMod[GLFW_KEY_I]          = Callbacks::switchToInsertMode;
        nmap.noMod[GLFW_KEY_A]          = Callbacks::moveCursorRightAndEnterInsertMode;
        nmap.noMod[GLFW_KEY_INSERT]     = Callbacks::switchToInsertMode;
        nmap.noMod[GLFW_KEY_RIGHT]      = Callbacks::moveCursorRight;
        nmap.noMod[GLFW_KEY_L]          = Callbacks::moveCursorRight;
        nmap.noMod[GLFW_KEY_LEFT]       = Callbacks::moveCursorLeft;
        nmap.noMod[GLFW_KEY_H]          = Callbacks::moveCursorLeft;
        nmap.noMod[GLFW_KEY_DOWN]       = Callbacks::moveCursorDown;
        nmap.noMod[GLFW_KEY_J]          = Callbacks::moveCursorDown;
        nmap.noMod[GLFW_KEY_UP]         = Callbacks::moveCursorUp;
        nmap.noMod[GLFW_KEY_K]          = Callbacks::moveCursorUp;
        nmap.noMod[GLFW_KEY_HOME]       = Callbacks::moveCursorToLineBeginning;
        nmap.noMod[GLFW_KEY_0]          = Callbacks::moveCursorToLineBeginning;
        nmap.noMod[GLFW_KEY_END]        = Callbacks::moveCursorToLineEnd;
        nmap.noMod[GLFW_KEY_BACKSPACE]  = Callbacks::deleteCharBackwards;
        nmap.noMod[GLFW_KEY_DELETE]     = Callbacks::deleteCharForwardOrSelection;
        nmap.noMod[GLFW_KEY_KP_DECIMAL] = Callbacks::deleteCharForwardOrSelection;
        nmap.noMod[GLFW_KEY_MINUS]      = Callbacks::zoomOutBufferIfImage;
        nmap.noMod[GLFW_KEY_KP_SUBTRACT]= Callbacks::zoomOutBufferIfImage;
        nmap.noMod[GLFW_KEY_EQUAL]      = Callbacks::zoomInBufferIfImage;
        nmap.noMod[GLFW_KEY_KP_ADD]     = Callbacks::zoomInBufferIfImage;
        nmap.noMod[GLFW_KEY_ESCAPE]     = Callbacks::hideAutocompPopupOrEndSelection;
        nmap.noMod[GLFW_KEY_V]          = Callbacks::bufferStartNormalSelection;

        nmap.ctrl[GLFW_KEY_N]           = Callbacks::createBufferInNewTab;
        nmap.ctrl[GLFW_KEY_S]           = Callbacks::saveCurrentBuffer;
        nmap.ctrl[GLFW_KEY_O]           = Callbacks::openFile;
        nmap.ctrl[GLFW_KEY_Q]           = Callbacks::closeActiveBuffer;
        nmap.ctrl[GLFW_KEY_PAGE_UP]     = Callbacks::goToPrevTab;
        nmap.ctrl[GLFW_KEY_PAGE_DOWN]   = Callbacks::goToNextTab;
        nmap.ctrl[GLFW_KEY_TAB]         = Callbacks::goToNextSplit;
        nmap.ctrl[GLFW_KEY_HOME]        = Callbacks::goToFirstChar;
        nmap.ctrl[GLFW_KEY_END]         = Callbacks::goToLastChar;
        nmap.ctrl[GLFW_KEY_MINUS]       = Callbacks::decreaseFontSize;
        nmap.ctrl[GLFW_KEY_KP_SUBTRACT] = Callbacks::decreaseFontSize;
        nmap.ctrl[GLFW_KEY_EQUAL]       = Callbacks::increaseFontSize;
        nmap.ctrl[GLFW_KEY_KP_ADD]      = Callbacks::increaseFontSize;
        nmap.ctrl[GLFW_KEY_Z]           = Callbacks::undoActiveBufferChange;
        nmap.ctrl[GLFW_KEY_ENTER]       = Callbacks::openPathAtCursor;
        nmap.ctrl[GLFW_KEY_V]           = Callbacks::bufferStartBlockSelection;
        nmap.ctrl[GLFW_KEY_R]           = Callbacks::redoActiveBufferChange;

        nmap.ctrlShift[GLFW_KEY_S]      = Callbacks::saveCurrentBufferAs;
        nmap.ctrlShift[GLFW_KEY_TAB]    = Callbacks::goToPrevSplit;
        nmap.ctrlShift[GLFW_KEY_RIGHT]  = Callbacks::increaseActiveBufferWidth;
        nmap.ctrlShift[GLFW_KEY_LEFT]   = Callbacks::decreaseActiveBufferWidth;

        nmap.shift[GLFW_KEY_V]          = Callbacks::bufferStartLineSelection;
        nmap.shift[GLFW_KEY_I]          = Callbacks::moveCursorToLineBeginningAndEnterInsertMode;
        nmap.shift[GLFW_KEY_A]          = Callbacks::moveCursorToLineEndAndEnterInsertMode;
    }


    { // Insert mode mappings
        imap.noMod[GLFW_KEY_ESCAPE]     = Callbacks::switchToNormalMode;
        imap.noMod[GLFW_KEY_RIGHT]      = Callbacks::moveCursorRight;
        imap.noMod[GLFW_KEY_LEFT]       = Callbacks::moveCursorLeft;
        imap.noMod[GLFW_KEY_DOWN]       = Callbacks::moveCursorDown;
        imap.noMod[GLFW_KEY_UP]         = Callbacks::moveCursorUp;
        imap.noMod[GLFW_KEY_HOME]       = Callbacks::moveCursorToLineBeginning;
        imap.noMod[GLFW_KEY_END]        = Callbacks::moveCursorToLineEnd;
        imap.noMod[GLFW_KEY_ENTER]      = Callbacks::putEnter;
        imap.noMod[GLFW_KEY_KP_ENTER]   = Callbacks::putEnter;
        imap.noMod[GLFW_KEY_BACKSPACE]  = Callbacks::deleteCharBackwards;
        imap.noMod[GLFW_KEY_DELETE]     = Callbacks::deleteCharForwardOrSelection;
        imap.noMod[GLFW_KEY_KP_DECIMAL] = Callbacks::deleteCharForwardOrSelection;
        imap.noMod[GLFW_KEY_TAB]        = Callbacks::insertTabOrSpaces;
        imap.noMod[GLFW_KEY_MINUS]      = Callbacks::zoomOutBufferIfImage;
        imap.noMod[GLFW_KEY_KP_SUBTRACT]= Callbacks::zoomOutBufferIfImage;
        imap.noMod[GLFW_KEY_EQUAL]      = Callbacks::zoomInBufferIfImage;
        imap.noMod[GLFW_KEY_KP_ADD]     = Callbacks::zoomInBufferIfImage;
        //imap.noMod[GLFW_KEY_ESCAPE]     = Callbacks::hideAutocompPopupOrEndSelection;
    }

    // Start in normal mode
    g_editorMode.set(EditorMode::_EditorMode::Normal);
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
             winH-g_fontSizePx-4},
             FontStyle::Regular,
             g_activeBuff->isSelectionInProgress() ? RGBColor{0.3f, 0.9f, 0.6f} : RGBColor{1.f, 1.f, 1.f});
}

void App::renderTabLine()
{
    // Draw tabline background
    g_uiRenderer->renderFilledRectangle({0, 0}, {g_windowWidth, TABLINE_HEIGHT_PX},
            RGB_COLOR_TO_RGBA(TABLINE_BG_COLOR));
    const int tabPageSize = roundf((float)g_windowWidth/TABLINE_TAB_WIDTH_PX);

    // Draw tabs
    for (size_t i{}; i < g_tabs.size(); ++i)
    {
        const auto& tab = g_tabs[i];
        const int tabX = i%tabPageSize*TABLINE_TAB_WIDTH_PX;;
        const auto& buffer = tab->getActiveBufferRecursively();

        // Only render if visible
        if (i/tabPageSize < g_currTabI/tabPageSize)
            continue;
        if (i/tabPageSize > g_currTabI/tabPageSize)
            break;

        // Render a tab, use a differenct color when it is the current tab
        g_uiRenderer->renderFilledRectangle(
                {tabX, 0},
                {tabX+TABLINE_TAB_WIDTH_PX-2, TABLINE_HEIGHT_PX},
                RGB_COLOR_TO_RGBA((
                        i == g_currTabI ?
                        TABLINE_ACTIVE_TAB_COLOR :
                        TABLINE_TAB_COLOR)));

        // Render the filename, use orange color when the buffer is modified since the last save
        g_textRenderer->renderString(buffer->getFileName().substr(0, TABLINE_TAB_MAX_TEXT_LEN),
                {tabX+20, -2},
                i == g_currTabI ? FontStyle::BoldItalic : FontStyle::Regular,
                (buffer->isModified() ? RGBColor{1.0f, 0.5f, 0.0f} : RGBColor{1.0f, 1.0, 1.0f}));

        // Render icon
        g_fileTypeHandler->getIconFromFilename(buffer->getFileName(), false)->render({tabX+2, 2}, {16, 16});
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

Buffer* App::openFileInNewBuffer(const std::string& path)
{
    Buffer* buffer;
    if (isImageExtension(getFileExt(path)))
    {
        buffer = new ImageBuffer;
    }
    else
    {
        buffer = new Buffer;
    }
    buffer->open(path);
    return buffer;
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

static void handleDialogClose()
{
    if (g_dialogs.back()->isClosed())
    {
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

    Bindings::runBinding(mods, key);

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

    if (g_editorMode.get() != EditorMode::_EditorMode::Insert)
    {
        return; // Only insert in insert mode
    }

    if (g_activeBuff)
    {
        g_activeBuff->insert(codePoint);
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
            MessageDialog::create(Dialog::EMPTY_CB, nullptr,
                    "Please save or close all modified buffers",
                    MessageDialog::Type::Information);
            Logger::log << "Window close aborted" << Logger::End;
            return;
        }
    }

    // Don't let the window close when there are open dialogs
    for (const auto& dlg : g_dialogs)
    {
        if (!dlg->isClosed())
        {
            glfwSetWindowShouldClose(window, false);
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

                    // If the current split is being resized
                    if ((int)i == g_mouseHResSplitI)
                    {
                        // Set the size to the new value
                        g_tabs[g_currTabI]->increaseChildWidth(i,
                                g_cursorX-(buffer->getXPos()+buffer->getWidth()));
                        g_isRedrawNeeded = true;
                    }

                    // If the cursor is near the vertical buffer border
                    if (std::abs(g_cursorX - (buffer->getXPos()+buffer->getWidth()))
                            < BUFFER_RESIZE_MAX_CURS_DIST)
                    {
                        glfwSetCursor(window, glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR));
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

    g_mouseHResSplitI = -1;
    if (act == GLFW_PRESS && btn == GLFW_MOUSE_BUTTON_LEFT && !g_tabs.empty())
    {
        // TODO: Nested splits are not well supported
        for (size_t i{}; i < g_tabs[g_currTabI]->getChildren().size()-1; ++i)
        {
            const auto& child = g_tabs[g_currTabI]->getChildren()[i];
            if (std::holds_alternative<std::unique_ptr<Buffer>>(child))
            {
                const auto& buffer = std::get<std::unique_ptr<Buffer>>(child);

                // If the cursor is near the vertical buffer border
                if (std::abs(g_cursorX - (buffer->getXPos()+buffer->getWidth()))
                        < BUFFER_RESIZE_MAX_CURS_DIST)
                {
                    g_mouseHResSplitI = i;
                    break;
                }
            }
        }
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
