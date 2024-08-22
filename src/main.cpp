#define _DEF_GLOBALS_
#include "globals.h"
#undef _DEF_GLOBALS_
#include "App.h"
#include "Bindings.h"
#include "SessionHandler.h"
#include <filesystem>
#include "dialogs/FindDialog.h"
#include "dialogs/FindListDialog.h"

int g_pressedMods{};
String g_pressedChar{};
//unsigned long long g_frameI{};
long long g_keyEventDelay{};

static std::string modsToStr(int mods)
{
    std::string out;

    if (mods & GLFW_MOD_ALT)
        out += "Alt-";
    if (mods & GLFW_MOD_CONTROL)
        out += "Ctrl-";
    if (mods & GLFW_MOD_SHIFT)
        out += "Shift-";

    return out;
}

static void keyCB(GLFWwindow*, int key, int scancode, int action, int mods)
{
    if (action == GLFW_RELEASE)
    {
        return;
    }

    //Logger::dbg << "KEY:  " << g_frameI << Logger::End;

    g_pressedMods = mods;

    //Logger::dbg << "Key " << (action == GLFW_RELEASE ? "(UP  ): " : "(DOWN): ") << (keyname ? keyname : "???") << Logger::End;
    
    switch (key)
    {
    case GLFW_KEY_SPACE: g_pressedChar = U"<Space>"; break;
    case GLFW_KEY_ESCAPE: g_pressedChar = U"<Escape>"; break;
    case GLFW_KEY_ENTER: g_pressedChar = U"<Enter>"; break;
    case GLFW_KEY_TAB: g_pressedChar = U"<Tab>"; break;
    case GLFW_KEY_BACKSPACE: g_pressedChar = U"<Backspace>"; break;
    case GLFW_KEY_INSERT: g_pressedChar = U"<Insert>"; break;
    case GLFW_KEY_DELETE: g_pressedChar = U"<Delete>"; break;
    case GLFW_KEY_RIGHT: g_pressedChar = U"<Right>"; break;
    case GLFW_KEY_LEFT: g_pressedChar = U"<Left>"; break;
    case GLFW_KEY_DOWN: g_pressedChar = U"<Down>"; break;
    case GLFW_KEY_UP: g_pressedChar = U"<Up>"; break;
    case GLFW_KEY_PAGE_UP: g_pressedChar = U"<Page_Up>"; break;
    case GLFW_KEY_PAGE_DOWN: g_pressedChar = U"<Page_Down>"; break;
    case GLFW_KEY_HOME: g_pressedChar = U"<Home>"; break;
    case GLFW_KEY_END: g_pressedChar = U"<End>"; break;
    case GLFW_KEY_CAPS_LOCK: g_pressedChar = U"<Caps_Lock>"; break;
    case GLFW_KEY_SCROLL_LOCK: g_pressedChar = U"<Scroll_Lock>"; break;
    case GLFW_KEY_NUM_LOCK: g_pressedChar = U"<Num_Lock>"; break;
    case GLFW_KEY_PRINT_SCREEN: g_pressedChar = U"<Print_Screen>"; break;
    case GLFW_KEY_PAUSE: g_pressedChar = U"<Pause>"; break;
    case GLFW_KEY_F1: g_pressedChar = U"<F1>"; break;
    case GLFW_KEY_F2: g_pressedChar = U"<F2>"; break;
    case GLFW_KEY_F3: g_pressedChar = U"<F3>"; break;
    case GLFW_KEY_F4: g_pressedChar = U"<F4>"; break;
    case GLFW_KEY_F5: g_pressedChar = U"<F5>"; break;
    case GLFW_KEY_F6: g_pressedChar = U"<F6>"; break;
    case GLFW_KEY_F7: g_pressedChar = U"<F7>"; break;
    case GLFW_KEY_F8: g_pressedChar = U"<F8>"; break;
    case GLFW_KEY_F9: g_pressedChar = U"<F9>"; break;
    case GLFW_KEY_F10: g_pressedChar = U"<F10>"; break;
    case GLFW_KEY_F11: g_pressedChar = U"<F11>"; break;
    case GLFW_KEY_F12: g_pressedChar = U"<F12>"; break;
    case GLFW_KEY_F13: g_pressedChar = U"<F13>"; break;
    case GLFW_KEY_F14: g_pressedChar = U"<F14>"; break;
    case GLFW_KEY_F15: g_pressedChar = U"<F15>"; break;
    case GLFW_KEY_F16: g_pressedChar = U"<F16>"; break;
    case GLFW_KEY_F17: g_pressedChar = U"<F17>"; break;
    case GLFW_KEY_F18: g_pressedChar = U"<F18>"; break;
    case GLFW_KEY_F19: g_pressedChar = U"<F19>"; break;
    case GLFW_KEY_F20: g_pressedChar = U"<F20>"; break;
    case GLFW_KEY_F21: g_pressedChar = U"<F21>"; break;
    case GLFW_KEY_F22: g_pressedChar = U"<F22>"; break;
    case GLFW_KEY_F23: g_pressedChar = U"<F23>"; break;
    case GLFW_KEY_F24: g_pressedChar = U"<F24>"; break;
    case GLFW_KEY_F25: g_pressedChar = U"<F25>"; break;
    case GLFW_KEY_KP_0: g_pressedChar = U"<Kp_0>"; break;
    case GLFW_KEY_KP_1: g_pressedChar = U"<Kp_1>"; break;
    case GLFW_KEY_KP_2: g_pressedChar = U"<Kp_2>"; break;
    case GLFW_KEY_KP_3: g_pressedChar = U"<Kp_3>"; break;
    case GLFW_KEY_KP_4: g_pressedChar = U"<Kp_4>"; break;
    case GLFW_KEY_KP_5: g_pressedChar = U"<Kp_5>"; break;
    case GLFW_KEY_KP_6: g_pressedChar = U"<Kp_6>"; break;
    case GLFW_KEY_KP_7: g_pressedChar = U"<Kp_7>"; break;
    case GLFW_KEY_KP_8: g_pressedChar = U"<Kp_8>"; break;
    case GLFW_KEY_KP_9: g_pressedChar = U"<Kp_9>"; break;
    case GLFW_KEY_KP_DECIMAL: g_pressedChar = U"<Kp_Decimal>"; break;
    case GLFW_KEY_KP_DIVIDE: g_pressedChar = U"<Kp_Divide>"; break;
    case GLFW_KEY_KP_MULTIPLY: g_pressedChar = U"<Kp_Multiply>"; break;
    case GLFW_KEY_KP_SUBTRACT: g_pressedChar = U"<Kp_Subtract>"; break;
    case GLFW_KEY_KP_ADD: g_pressedChar = U"<Kp_Add>"; break;
    case GLFW_KEY_KP_ENTER: g_pressedChar = U"<Kp_Enter>"; break;
    case GLFW_KEY_KP_EQUAL: g_pressedChar = U"<Kp_Equal>"; break;
    case GLFW_KEY_MENU: g_pressedChar = U"<Menu>"; break;
    case GLFW_KEY_LEFT_SHIFT: g_pressedChar = U"<Left_Shift>"; break;
    case GLFW_KEY_LEFT_CONTROL: g_pressedChar = U"<Left_Control>"; break;
    case GLFW_KEY_LEFT_ALT: g_pressedChar = U"<Left_Alt>"; break;
    case GLFW_KEY_LEFT_SUPER: g_pressedChar = U"<Left_Super>"; break;
    case GLFW_KEY_RIGHT_SHIFT: g_pressedChar = U"<Right_Shift>"; break;
    case GLFW_KEY_RIGHT_CONTROL: g_pressedChar = U"<Right_Control>"; break;
    case GLFW_KEY_RIGHT_ALT: g_pressedChar = U"<Right_Alt>"; break;
    case GLFW_KEY_RIGHT_SUPER: g_pressedChar = U"<Right_Super>"; break;
    default:
        {
            if (const char* keyname = glfwGetKeyName(key, scancode))
            {
                g_pressedChar = icuStrToUtf32(icu::UnicodeString{keyname});
            }
            else
            {
                g_pressedChar = utf8To32("<"+std::to_string(key)+">");
            }
            break;
        }
    }

    /*
     * The key callback gets an incorrect character in case the key was pressed while holding down AltGr.
     * The char callback gets an incorrect character while holding down Ctrl.
     * The char callback overwrites the value received by the key callback, so both quirks are handled.
     * The char callback is triggered a few (usually 2) frames later than the key callback (thanks GLFW!), so we need to wait.
     */
    g_keyEventDelay = 3;
}

static void charModsCB(GLFWwindow*, uint codepoint, int mods)
{
    g_pressedMods = mods;

    //Logger::dbg << "CHAR:  " << g_frameI << Logger::End;

    //Logger::dbg << "Char mod:   " << charToStr((Char)codepoint) << " / " << codepoint << Logger::End;
    if (codepoint > ' ' && codepoint != 127)
        g_pressedChar = charToStr((Char)codepoint);
    g_keyEventDelay = 0; // Don't wait any longer
}


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

    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(UNPACK_RGB_COLOR(DEF_BG), 1.0f);
    App::renderStartupScreen();
    glfwSwapBuffers(g_window);

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


    glfwSetKeyCallback(g_window, keyCB);
    glfwSetCharModsCallback(g_window, charModsCB);


    int msUntilCursorBlinking = CURSOR_BLINK_MS;
    while (!glfwWindowShouldClose(g_window))
    {
        const double startTime = glfwGetTime();

        //Logger::dbg << "----------" << Logger::End;

        if (CURSOR_BLINK_MS >= 0 && msUntilCursorBlinking <= 0 && g_activeBuff)
        {
            g_activeBuff->toggleCursorVisibility();
            g_isRedrawNeeded = true;
            msUntilCursorBlinking = CURSOR_BLINK_MS;
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

        glfwPollEvents();
        //++g_frameI;
        --g_keyEventDelay;
        // If the waiting timed out or we received the char event
        if (g_keyEventDelay <= 0 && !g_pressedChar.empty())
        {
            Logger::dbg << "PRESSED: " << modsToStr(g_pressedMods) << '\'' << g_pressedChar << '\'' << Logger::End;
            auto mapKey = utf8To32(modsToStr(g_pressedMods))+g_pressedChar;
            auto bindingIt = Bindings::activeBindingMap->find(mapKey);
            if (bindingIt != Bindings::activeBindingMap->end())
                bindingIt->second();
            g_pressedChar.clear();
            g_pressedMods = 0;
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
        App::tickPopups();
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

