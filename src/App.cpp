#include "App.h"
#include "autocomp/DictionaryProvider.h"
#include "autocomp/BufferWordProvider.h"
#include "autocomp/PathProvider.h"
#include "autocomp/LspProvider.h"
#include "Bindings.h"
#include "../external/stb/stb_image.h"
#include "common/file.h"
#include "os.h"
#include "Git.h"
#include <filesystem>
#ifdef OS_LINUX
#   include <unistd.h>
#elif defined(OS_WIN)
#   error "TODO"
#else
#   error "Unsupported OS"
#endif
namespace fs = std::filesystem;

#ifdef OS_LINUX

std::string App::getExePath()
{
    char pathName[PATH_MAX]{};
    realpath("/proc/self/exe", pathName);
    return pathName;
}

#elif defined(OS_WIN)

#   error "TODO"

#else
#   error "Unsupported OS"
#endif

/*
 * Get resource path.
 */
std::string App::getResPath(const std::string& suffix)
{
    assert(!g_exeDirPath.empty());

    if (std::filesystem::path{suffix}.is_absolute())
        return suffix;
    return (std::filesystem::path{g_exeDirPath} / suffix);
}

#define BUFFER_RESIZE_MAX_CURS_DIST 10
#define START_SCRN_INDENT_PX 100

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
    glfwSetWindowSizeLimits(window, 200, 100, GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwSetWindowSizeCallback(window, App::windowResizeCB);
    glfwSetWindowRefreshCallback(window, App::windowRefreshCB);
    glfwSetKeyCallback(window, App::windowKeyCB);
    glfwSetScrollCallback(window, App::windowScrollCB);
    glfwSetCharCallback(window, App::windowCharCB);
    glfwSetWindowCloseCallback(window, App::windowCloseCB);
    glfwSetCursorPosCallback(window, App::cursorPosCB);
    glfwSetMouseButtonCallback(window, App::mouseButtonCB);
    glfwSetDropCallback(window, App::pathDropCB);
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
    auto regularFontPath    = OS::getFontFilePath(FONT_FAMILY_REGULAR, FONT_STYLE_REGULAR);
    auto boldFontPath       = OS::getFontFilePath(FONT_FAMILY_BOLD, FONT_STYLE_BOLD);
    auto italicFontPath     = OS::getFontFilePath(FONT_FAMILY_ITALIC, FONT_STYLE_ITALIC);
    auto boldItalicFontPath = OS::getFontFilePath(FONT_FAMILY_BOLDITALIC, FONT_STYLE_BOLD|FONT_STYLE_ITALIC);
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
    return std::unique_ptr<Image>(new Image{App::getResPath(ICON_FILE_PATH)});
}

#define CURS_PATH_BUSY "../img/busy_cursor.png"

void App::loadCursors()
{
    Logger::dbg << "Loading cursors" << Logger::End;

    GLFWimage image;
    image.pixels = stbi_load(getResPath(CURS_PATH_BUSY).c_str(), &image.width, &image.height, nullptr, 4);
    if (!image.pixels)
        Logger::err << "Failed to load cursor: " CURS_PATH_BUSY ": " << stbi_failure_reason() << Logger::End;
    else
        Cursors::busy = glfwCreateCursor(&image, 0, 0);
    stbi_image_free(image.pixels); // glfwCreateCursor() copies the data, so free the original
}

void App::loadSignImages()
{
    loadSigns();
}

FileTypeHandler* App::createFileTypeHandler()
{
    return new FileTypeHandler{getResPath("../icons/file_icon_index.txt"), getResPath("../icons/folder_icon_index.txt")};
}

void App::createAutocompleteProviders()
{
    Autocomp::dictProvider      = std::make_unique<Autocomp::DictionaryProvider>();
    Autocomp::buffWordProvid    = std::make_unique<Autocomp::BufferWordProvider>();
    Autocomp::pathProvid        = std::make_unique<Autocomp::PathProvider>();
    Autocomp::lspProvider       = std::make_unique<Autocomp::LspProvider>();

    Logger::log << "Autocompletion set up" << Logger::End;
}

void App::loadTheme()
{
    ThemeLoader tl;
    g_theme = std::unique_ptr<Theme>(tl.load(getResPath(THEME_PATH)));
}

void App::setupKeyBindings()
{
    Logger::dbg << "Setting up keybindings" << Logger::End;

    // `nmap`, 'imap' and `Callbacks` are from the `Bindings` namespace
    using namespace Bindings;

    { // Normal mode mappings

        // We want to set the normal mode mapping
        bindingsP = &nmap;

        // No modifier
        bindPrimKey("i",                    0, Callbacks::switchToInsertMode);
        bindPrimKey("a",                    0, Callbacks::moveCursorRightAndEnterInsertMode);
        bindPrimKey("l",                    0, Callbacks::moveCursorRight);
        bindPrimKey("h",                    0, Callbacks::moveCursorLeft);
        bindPrimKey("j",                    0, Callbacks::moveCursorDown);
        bindPrimKey("k",                    0, Callbacks::moveCursorUp);
        bindPrimKey("0",                    0, Callbacks::moveCursorToLineBeginning);
        bindPrimKey("u",                    0, Callbacks::undoActiveBufferChange);
        bindPrimKey("v",                    0, Callbacks::bufferStartNormalSelection);
        bindPrimKey("x",                    0, Callbacks::deleteCharForwardOrSelection);
        bindPrimKey("o",                    0, Callbacks::putLineBreakAfterLineAndEnterInsertMode);
        bindPrimKey("p",                    0, Callbacks::bufferPasteClipboard);
        bindPrimKey("y",                    0, Callbacks::bufferCopySelectionToClipboard);
        bindPrimKey("n",                    0, Callbacks::bufferFindGotoNext);
        bindPrimKey("-",                    0, Callbacks::zoomOutBufferIfImage);
        bindPrimKey("+",                    0, Callbacks::zoomInBufferIfImage);
        bindNonprimChar(U'$',                  Callbacks::moveCursorToLineEnd);
        bindNonprimChar(U'>',                  Callbacks::indentSelectedLines);
        bindNonprimChar(U'<',                  Callbacks::unindentSelectedLines);
        bindNonprimChar(U':',                  Callbacks::togglePromptAnimated);
        bindNonprimChar(U'á',                  Callbacks::bufferShowSymbolHover);
        bindNonprimChar(U'ő',                  Callbacks::bufferGotoDef);
        bindNonprimChar(U'ú',                  Callbacks::bufferGotoDecl);
        bindNonprimChar(U'ű',                  Callbacks::bufferGotoImp);
        bindNonprimChar(U'ö',                  Callbacks::bufferApplyCodeAct);
        bindNonprimChar(U'ü',                  Callbacks::bufferRenameSymbol);
        bindFuncKey(GLFW_KEY_ESCAPE,        0, Callbacks::bufferCancelSelection);
        bindFuncKey(GLFW_KEY_INSERT,        0, Callbacks::switchToInsertMode);
        bindFuncKey(GLFW_KEY_RIGHT,         0, Callbacks::moveCursorRight);
        bindFuncKey(GLFW_KEY_LEFT,          0, Callbacks::moveCursorLeft);
        bindFuncKey(GLFW_KEY_DOWN,          0, Callbacks::moveCursorDown);
        bindFuncKey(GLFW_KEY_UP,            0, Callbacks::moveCursorUp);
        bindFuncKey(GLFW_KEY_HOME,          0, Callbacks::moveCursorToLineBeginning);
        bindFuncKey(GLFW_KEY_END,           0, Callbacks::moveCursorToLineEnd);
        bindFuncKey(GLFW_KEY_BACKSPACE,     0, Callbacks::deleteCharBackwards);
        bindFuncKey(GLFW_KEY_DELETE,        0, Callbacks::deleteCharForwardOrSelection);
        bindFuncKey(GLFW_KEY_KP_DECIMAL,    0, Callbacks::deleteCharForwardOrSelection);
        bindFuncKey(GLFW_KEY_PAGE_UP,       0, Callbacks::goPageUp);
        bindFuncKey(GLFW_KEY_PAGE_DOWN,     0, Callbacks::goPageDown);

        // Ctrl held down
        bindPrimKey("n",                    GLFW_MOD_CONTROL, Callbacks::createBufferInNewTab);
        bindPrimKey("s",                    GLFW_MOD_CONTROL, Callbacks::saveCurrentBuffer);
        bindPrimKey("o",                    GLFW_MOD_CONTROL, Callbacks::openFile);
        bindPrimKey("q",                    GLFW_MOD_CONTROL, Callbacks::closeActiveBuffer);
        bindPrimKey("v",                    GLFW_MOD_CONTROL, Callbacks::bufferStartBlockSelection);
        bindPrimKey("r",                    GLFW_MOD_CONTROL, Callbacks::redoActiveBufferChange);
        bindPrimKey("x",                    GLFW_MOD_CONTROL, Callbacks::bufferCutSelectionToClipboard);
        bindPrimKey("f",                    GLFW_MOD_CONTROL, Callbacks::bufferFind);
        bindPrimKey("-",                    GLFW_MOD_CONTROL, Callbacks::decreaseFontSize);
        bindPrimKey("+",                    GLFW_MOD_CONTROL, Callbacks::increaseFontSize);
        bindFuncKey(GLFW_KEY_PAGE_UP,       GLFW_MOD_CONTROL, Callbacks::goToPrevTab);
        bindFuncKey(GLFW_KEY_PAGE_DOWN,     GLFW_MOD_CONTROL, Callbacks::goToNextTab);
        bindFuncKey(GLFW_KEY_TAB,           GLFW_MOD_CONTROL, Callbacks::goToNextSplit);
        bindFuncKey(GLFW_KEY_HOME,          GLFW_MOD_CONTROL, Callbacks::goToFirstChar);
        bindFuncKey(GLFW_KEY_END,           GLFW_MOD_CONTROL, Callbacks::goToLastChar);
        bindFuncKey(GLFW_KEY_ENTER,         GLFW_MOD_CONTROL, Callbacks::openPathOrUrlAtCursor);

        // Ctrl-shift held down
        bindPrimKey("s",                    GLFW_MOD_CONTROL|GLFW_MOD_SHIFT, Callbacks::saveCurrentBufferAs);
        bindFuncKey(GLFW_KEY_TAB,           GLFW_MOD_CONTROL|GLFW_MOD_SHIFT, Callbacks::goToPrevSplit);
        bindFuncKey(GLFW_KEY_RIGHT,         GLFW_MOD_CONTROL|GLFW_MOD_SHIFT, Callbacks::increaseActiveBufferWidth);
        bindFuncKey(GLFW_KEY_LEFT,          GLFW_MOD_CONTROL|GLFW_MOD_SHIFT, Callbacks::decreaseActiveBufferWidth);

        // Shift held down
        bindPrimKey("r",                    GLFW_MOD_SHIFT, Callbacks::switchToReplaceMode);
        bindPrimKey("v",                    GLFW_MOD_SHIFT, Callbacks::bufferStartLineSelection);
        bindPrimKey("i",                    GLFW_MOD_SHIFT, Callbacks::moveCursorToLineBeginningAndEnterInsertMode);
        bindPrimKey("a",                    GLFW_MOD_SHIFT, Callbacks::moveCursorToLineEndAndEnterInsertMode);
        bindPrimKey("o",                    GLFW_MOD_SHIFT, Callbacks::putLineBreakBeforeLineAndEnterInsertMode);
        bindPrimKey("n",                    GLFW_MOD_SHIFT, Callbacks::bufferFindGotoPrev);
        bindPrimKey("e",                    GLFW_MOD_SHIFT, Callbacks::moveCursorToSWordEnd);
        bindPrimKey("b",                    GLFW_MOD_SHIFT, Callbacks::moveCursorToSWordBeginning);
        bindPrimKey("w",                    GLFW_MOD_SHIFT, Callbacks::moveCursorToNextSWord);
    }


    { // Insert mode mappings

        // We want to set the normal mode mapping
        bindingsP = &imap;

        // No modifier
        bindFuncKey(GLFW_KEY_ESCAPE,        0, Callbacks::switchToNormalMode);
        bindFuncKey(GLFW_KEY_RIGHT,         0, Callbacks::moveCursorRight);
        bindFuncKey(GLFW_KEY_LEFT,          0, Callbacks::moveCursorLeft);
        bindFuncKey(GLFW_KEY_DOWN,          0, Callbacks::moveCursorDown);
        bindFuncKey(GLFW_KEY_UP,            0, Callbacks::moveCursorUp);
        bindFuncKey(GLFW_KEY_HOME,          0, Callbacks::moveCursorToLineBeginning);
        bindFuncKey(GLFW_KEY_END,           0, Callbacks::moveCursorToLineEnd);
        bindFuncKey(GLFW_KEY_ENTER,         0, Callbacks::bufferPutEnterOrInsertAutocomplete);
        bindFuncKey(GLFW_KEY_KP_ENTER,      0, Callbacks::bufferPutEnterOrInsertAutocomplete);
        bindFuncKey(GLFW_KEY_BACKSPACE,     0, Callbacks::deleteCharBackwards);
        bindFuncKey(GLFW_KEY_DELETE,        0, Callbacks::deleteCharForwardOrSelection);
        bindFuncKey(GLFW_KEY_KP_DECIMAL,    0, Callbacks::deleteCharForwardOrSelection);
        bindFuncKey(GLFW_KEY_TAB,           0, Callbacks::insertTabOrSpaces);
        bindFuncKey(GLFW_KEY_MINUS,         0, Callbacks::zoomOutBufferIfImage);
        bindFuncKey(GLFW_KEY_KP_SUBTRACT,   0, Callbacks::zoomOutBufferIfImage);
        bindFuncKey(GLFW_KEY_EQUAL,         0, Callbacks::zoomInBufferIfImage);
        bindFuncKey(GLFW_KEY_KP_ADD,        0, Callbacks::zoomInBufferIfImage);

        // Ctrl held down
        bindFuncKey(GLFW_KEY_SPACE,         GLFW_MOD_CONTROL, Callbacks::triggerAutocompPopupOrSelectNextItem);

        // Ctrl-Shift held down
        bindFuncKey(GLFW_KEY_SPACE,         GLFW_MOD_CONTROL|GLFW_MOD_SHIFT, Callbacks::triggerAutocompPopupOrSelectPrevItem);
    }

    // Start in normal mode
    g_editorMode.set(EditorMode::_EditorMode::Normal);
}

void App::initGit()
{
    Git::init();
}

void App::renderBuffers()
{
    if (!g_tabs.empty())
    {
        g_activeBuff->updateCursor();
        g_tabs[g_currTabI]->forEachBufferRecursively([](Buffer* buff) { buff->render(); });
    }
}

static RGBColor calcStatLineColor(RGBColor col)
{
    if ((col.r + col.g + col.b) / 3.f > .5f) // If light color
    {
        col.r *= 0.9f;
        col.g *= 0.9f;
        col.b *= 0.9f;
    }
    else // Dark color
    {
        col.r *= 1.2f;
        col.g *= 1.2f;
        col.b *= 1.2f;
    }
    return col;
}

void App::renderStatusLine()
{
    if (!g_activeBuff && g_statMsg.isEmpty())
        return;

    g_uiRenderer->renderFilledRectangle(
            {0, g_windowHeight-g_fontSizePx*1.2f},
            {g_windowWidth, g_windowHeight},
            RGB_COLOR_TO_RGBA(calcStatLineColor(g_theme->bgColor)));

    std::string leftStr;
    if (g_statMsg.isEmpty())
    {
        const std::string checkedOutObjName = g_activeBuff->getCheckedOutObjName(7);
        const std::string basePath = fs::path(g_activeBuff->m_filePath).remove_filename().string();
        const std::string fileName = fs::path(g_activeBuff->m_filePath).filename().string();
        leftStr = (checkedOutObjName.empty() ? "" : "["+checkedOutObjName+"] ")+basePath+"\033[1m"+fileName+(g_activeBuff->m_isReadOnly ? "\033[0m\033[91m [RO]" : "");
    }

    g_textRenderer->renderString(
            g_statMsg.isEmpty() ? leftStr : g_statMsg.get(),
            {g_statMsg.isEmpty() ? 0 : g_fontSizePx*4, g_windowHeight-g_fontSizePx-4},
            g_statMsg.isEmpty() ? FONT_STYLE_REGULAR : FONT_STYLE_BOLD|FONT_STYLE_ITALIC,
            g_statMsg.isEmpty() ? RGBColor{1.0f, 1.0f, 1.0f} : g_statMsg.getTypeColor());

    if (g_activeBuff)
    {
        g_activeBuff->updateRStatusLineStr();
        assert(g_activeBuff->m_statusLineStr.maxLen > 0);
        g_textRenderer->renderString(
                g_activeBuff->m_statusLineStr.str,
                {g_windowWidth-g_fontWidthPx*(g_activeBuff->m_statusLineStr.maxLen+4),
                 g_windowHeight-g_fontSizePx-4},
                 FONT_STYLE_REGULAR,
                 g_activeBuff->isSelectionInProgress() ? RGBColor{0.3f, 0.9f, 0.6f} : RGBColor{1.f, 1.f, 1.f});

        Autocomp::lspProvider->getStatusIcon()->render(
                {g_windowWidth-g_fontSizePx*2-3, g_windowHeight-g_fontSizePx*1.2f-6},
                {g_fontSizePx*1.2f+12, g_fontSizePx*1.2f+12});
    }
}

#define BUFFCLOSE_BTN_SIZE_PX TABLINE_HEIGHT_PX

void App::renderTabLine()
{
    // Draw tabline background
    g_uiRenderer->renderFilledRectangle({0, 0}, {g_windowWidth, TABLINE_HEIGHT_PX},
            RGB_COLOR_TO_RGBA(TABLINE_BG_COLOR));
    const int tabPageSize = roundf(float(g_windowWidth-BUFFCLOSE_BTN_SIZE_PX)/TABLINE_TAB_WIDTH_PX);


    static const Image closeImg = Image(getResPath("../img/buff_close.png"));
    // Draw buffer close button
    g_uiRenderer->renderFilledRectangle(
            {g_windowWidth-BUFFCLOSE_BTN_SIZE_PX, 0}, {g_windowWidth, BUFFCLOSE_BTN_SIZE_PX},
            {0.8f, 0.0f, 0.0f}
    );
    if (!closeImg.isOpenFailed())
        closeImg.render({g_windowWidth-BUFFCLOSE_BTN_SIZE_PX, 0}, {BUFFCLOSE_BTN_SIZE_PX, BUFFCLOSE_BTN_SIZE_PX});

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
                i == g_currTabI ? FONT_STYLE_BOLD|FONT_STYLE_ITALIC : FONT_STYLE_REGULAR,
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
                {UNPACK_RGB_COLOR(g_theme->bgColor), 0.7f});

        // Render the top dialog
        g_dialogs.back()->render();
    }
}

void App::renderPopups()
{
    if (g_activeBuff)
        g_hoverPopup->render();
}

static const std::string genWelcomeMsg(std::string templ)
{
    static std::map<std::string, std::string> map;
    map["BUILD_DATE"] = __DATE__ " at " __TIME__;
    map["BUILD_TYPE"] =
#ifdef NDEBUG
                "\033[32mRelease\033[0m";
#else
                "\033[33mDebug\033[0m";
#endif
    map["BUILD_IS_OPTIMIZED"] =
#ifdef __OPTIMIZE__
                "\033[32mON\033[0m";
#else
                "\033[31mOFF\033[0m";
#endif
    map["BUILD_IS_ASAN_ON"] =
#ifdef __has_feature
#   if __has_feature(address_sanitizer)
                "\033[32mON\033[0m";
#   else
                "\033[31mOFF\033[0m";
#   endif
#else
                "???";
#endif
    map["COMPILER_NAME"]            = __VERSION__;
    map["GL_VENDOR"]                = (const char*)glGetString(GL_VENDOR);
    map["GL_RENDERER"]              = (const char*)glGetString(GL_RENDERER);
    map["FONT_FAMILY_REGULAR"]      = FONT_FAMILY_REGULAR;
    map["FONT_FAMILY_BOLD"]         = FONT_FAMILY_BOLD;
    map["FONT_FAMILY_ITALIC"]       = FONT_FAMILY_ITALIC;
    map["FONT_FAMILY_BOLDITALIC"]   = FONT_FAMILY_BOLDITALIC;
    map["FORTUNE"]                  = OS::runExternalCommand(FORTUNE_CMD);

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
    static const auto welcomeMsg = genWelcomeMsg(WELCOME_MSG);
    g_textRenderer->renderString(welcomeMsg, {START_SCRN_INDENT_PX, 30},
            FONT_STYLE_REGULAR, g_theme->values[Syntax::MARK_NONE].color, true);

    const int origIconW = icon->getPhysicalSize().x;
    const int origIconH = icon->getPhysicalSize().y;
    const float iconRatio = (float)origIconW/origIconH;
    const int iconSize = std::min(
            std::min(g_windowWidth, 512),
            std::min(g_windowHeight, 512));
    icon->render(
            {g_windowWidth/2-iconSize*iconRatio/2, g_windowHeight/2-iconSize/2},
            {iconSize*iconRatio, iconSize});

    // Render recent file list if not empty
    if (!g_recentFilePaths->isEmpty())
    {
        g_textRenderer->renderString(
                "\033[90mRecent files:",
                {START_SCRN_INDENT_PX, (strCountLines(welcomeMsg)+4)*g_fontSizePx},
                FONT_STYLE_BOLD);

        for (size_t i{}; i < g_recentFilePaths->getItemCount(); ++i)
        {
            g_textRenderer->renderString(
                    "\033[90m["+std::string(i == g_recentFilePaths->getSelectedItemI() ? "\033[97m" : "")
                    +std::to_string(i+1)+"\033[0m\033[90m]: \033[3m"+g_recentFilePaths->getItem(i),
                    {START_SCRN_INDENT_PX, (strCountLines(welcomeMsg)+5+i)*g_fontSizePx});
        }
    }
}

void App::renderPrompt()
{
    Prompt::get()->render();
}

Buffer* App::openFileInNewBuffer(
        const std::string& path, bool addToRecFileList/*=true*/)
{
    g_statMsg.set("Opening file: "+path, StatusMsg::Type::Info);

    if (addToRecFileList)
        g_recentFilePaths->addItem(path);

    App::renderStatusLine();
    glfwSwapBuffers(g_window);

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

void App::flashDialog()
{
    g_dialogFlashTime = DIALOG_FLASH_TIME_MS;
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
    if (g_isDebugDrawMode)
        Logger::log << "Resized window to " << width << 'x' << height << Logger::End;
    glViewport(0, 0, width, height);
    g_windowWidth = width;
    g_windowHeight = height;
    for (auto& tab : g_tabs)
    {
        tab->makeChildrenSizesEqual();
    }
    g_isRedrawNeeded = true;
}

static void handleDialogClose()
{
    if (g_dialogs.back()->isClosed())
    {
        g_dialogs.pop_back();
        g_isRedrawNeeded = true;
    }
}

void App::windowKeyCB(GLFWwindow*, int key, int scancode, int action, int mods)
{
    (void)scancode;
    // We don't care about key releases
    if (action == GLFW_RELEASE)
        return;

#if HIDE_MOUSE_WHILE_TYPING
    // Hide cursor while typing
    glfwSetInputMode(g_window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
#endif

    // Debug mode toggle should work even when a dialog is open
    if (mods == 0 && key == GLFW_KEY_F3)
    {
        toggleDebugDraw();
        return;
    }

    if (Prompt::get()->isOpen())
    {
        Prompt::get()->handleKey(key, mods);
        return;
    }

    // If there are dialogs open, pass the event to the top one
    if (!g_dialogs.empty())
    {
        g_dialogs.back()->handleKey(key, mods);
        handleDialogClose();
        g_isRedrawNeeded = true;
        return;
    }

    // If j or the down key is pressed on the startup screen
    if (g_tabs.empty() && (key == GLFW_KEY_J || key == GLFW_KEY_DOWN))
    {
        g_recentFilePaths->selectNextItem();
        g_isRedrawNeeded = true;
    }
    // If k or the up key is pressed on the startup screen
    else if (g_tabs.empty() && (key == GLFW_KEY_K || key == GLFW_KEY_UP))
    {
        g_recentFilePaths->selectPrevItem();
        g_isRedrawNeeded = true;
    }
    // If the Enter key is pressed on the startup screen
    else if (g_tabs.empty() && key == GLFW_KEY_ENTER)
    {
        if (!g_recentFilePaths->isEmpty())
        {
            auto path = g_recentFilePaths->getSelectedItem();
            // Erase the selected recent file item, so it show will be at the end of the list
            // without pushing out other entries
            g_recentFilePaths->removeSelectedItem();

            Buffer* buff = App::openFileInNewBuffer(path);
            g_tabs.push_back(std::make_unique<Split>(buff));
            g_activeBuff = buff;
            g_currTabI = 0;
            g_isRedrawNeeded = true;
        }
    }
    else
    {
        Bindings::fetchKeyBinding(key, mods);
        // Key callback is always called, but char callback is not
        // Make sure to run the fetched binding from outside, after the char callback (if called)
        g_hasBindingToCall = true;
    }
}

void App::windowCharCB(GLFWwindow*, uint codePoint)
{
#if HIDE_MOUSE_WHILE_TYPING
    // Hide cursor while typing
    glfwSetInputMode(g_window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
#endif

    // If there are dialogs open, don't react to keypresses
    if (!g_dialogs.empty())
    {
        g_dialogs.back()->handleChar(codePoint);
        return;
    }

    if (Prompt::get()->isOpen())
    {
        Prompt::get()->handleChar(codePoint);
        return;
    }

    switch (g_editorMode.get())
    {
        case EditorMode::_EditorMode::Normal:
            Bindings::fetchCharBinding(codePoint);
            return;

        case EditorMode::_EditorMode::Insert:
            if (g_activeBuff)
            {
                g_activeBuff->insert(codePoint);
            }
            break;

        case EditorMode::_EditorMode::Replace:
            if (g_activeBuff)
            {
                g_activeBuff->replaceChar(codePoint);
            }
            break;
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
            flashDialog();
            Logger::log << "Window close request ignored, there are open dialogs" << Logger::End;
            return;
        }
    }
}

void App::cursorPosCB(GLFWwindow* window, double x, double y)
{
#if HIDE_MOUSE_WHILE_TYPING
    // Show cursor when moved
    glfwSetInputMode(g_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
#endif

    g_cursorX = (int)x;
    g_cursorY = (int)y;
    g_isRedrawNeeded = true;

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

                    // Redraw on move when we have a visible `ImageBuffer`, so the cursor
                    // position gets updated on the status bar
                    if (dynamic_cast<ImageBuffer*>(buffer.get()))
                    {
                        g_isRedrawNeeded = true;
                    }

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

                        // If the button is being held down, start a selection
                        if (g_isMouseBtnLPressed)
                        {
                            buffer->goToMousePos();
                            buffer->startSelection(Buffer::Selection::Mode::Normal);
                        }
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
        if (act == GLFW_PRESS)
        {
            // If pressed inside the dialog
            if (g_dialogs.back()->isInsideBody({g_cursorX, g_cursorY}))
            {
                // If pressed a dialog button
                if (g_dialogs.back()->isInsideButton({g_cursorX, g_cursorY}))
                {
                    g_dialogs.back()->pressButtonAt({g_cursorX, g_cursorY});
                    handleDialogClose();
                    g_isRedrawNeeded = true;
                }
            }
            else // If pressed outside the dialog
            {
                flashDialog();
            }
        }
        return;
    }

    // If the cursor was pressed on the close buffer button
    if (act == GLFW_PRESS
     && g_cursorX > g_windowWidth-BUFFCLOSE_BTN_SIZE_PX
     && g_cursorY < BUFFCLOSE_BTN_SIZE_PX)
    {
        Bindings::Callbacks::closeActiveBuffer();
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

                    buffer->goToMousePos();

                    // Cancel selection when going to a new position after releasing the button
                    if (act == GLFW_PRESS)
                        buffer->closeSelection();

                    g_isTitleUpdateNeeded = true;
                    g_isRedrawNeeded = true;
                    break;
                }
            }
        }
    }
}

void App::pathDropCB(GLFWwindow*, int count, const char** paths)
{
    Logger::log << "Dropped " << count << " paths into window" << Logger::End;
    Logger::dbg << "Paths:";
    for (int i{}; i < count; ++i)
        Logger::dbg << '\n' << '\t' << paths[i];
    Logger::dbg << Logger::End;

    for (int i{}; i < count; ++i)
    {
        if (!std_fs::is_regular_file(paths[i]))
            continue;

        auto* buffer = App::openFileInNewBuffer(paths[i]);
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
    g_isTitleUpdateNeeded = true;
}
