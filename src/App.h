#pragma once

#include <iostream>
#include <cassert>
#include <vector>
#include <memory>
#include "glstuff.h"
#include "Logger.h"
#include "config.h"
#include "os.h"
#include "Shader.h"
#include "TextRenderer.h"
#include "UiRenderer.h"
#include "FileTypeHandler.h"
#include "Buffer.h"
#include "dialogs/MessageDialog.h"
#include "dialogs/FileDialog.h"
#include "dialogs/AskerDialog.h"
#include "types.h"
#include "Timer.h"
#include "ImageBuffer.h"
#include "Split.h"
#include "globals.h"
#include "autocomp/DictionaryProvider.h"
#include "autocomp/LspProvider.h"
#include "ThemeLoader.h"
#include "Prompt.h"

class RecentFileList
{
private:
    std::vector<std::string> m_list;
    size_t m_selectedItemI{};

public:

    bool isEmpty() const
    {
        return m_list.empty();
    }

    void addItem(const std::string& item)
    {
        assert(m_list.size() <= RECENT_LIST_MAX_SIZE);
        // Remove the last item when the list is full
        if (m_list.size() == RECENT_LIST_MAX_SIZE)
            m_list.pop_back();
        m_list.insert(m_list.begin(), item);
    }

    void removeSelectedItem()
    {
        m_list.erase(m_list.begin()+m_selectedItemI);
        m_selectedItemI = 0;
    }

    const std::string& getItem(size_t i) const
    {
        assert(i < m_list.size());
        return m_list[i];
    }

    const std::string& getSelectedItem() const
    {
        assert(m_selectedItemI < m_list.size());
        return m_list[m_selectedItemI];
    }

    size_t getSelectedItemI() const
    {
        return m_selectedItemI;
    }

    void selectNextItem()
    {
        if (m_selectedItemI < m_list.size()-1)
        {
            ++m_selectedItemI;
        }
    }

    void selectPrevItem()
    {
        if (m_selectedItemI > 0)
        {
            --m_selectedItemI;
        }
    }

    size_t getItemCount() const
    {
        return m_list.size();
    }
};

class App final
{
public:
    App() = delete;

    // ----- Setup functions -----
    static std::string getExePath();
    static std::string getResPath(const std::string& suffix);
    static GLFWwindow* createWindow();
    static void initGlew();
    static void setupGlFeatures();
    static TextRenderer* createTextRenderer();
    static UiRenderer* createUiRenderer();
    static std::unique_ptr<Image> loadProgramIcon();
    static void loadCursors();
    static void loadSignImages();
    static FileTypeHandler* createFileTypeHandler();
    static void createAutocompleteProviders();
    static void loadTheme();
    static void setupKeyBindings();
    static void initGit();

    // ----- Renderer functions -----
    static void renderBuffers();
    static void renderStatusLine();
    static void renderTabLine();
    static void renderDialogs();
    static void renderStartupScreen();
    static void renderPrompt();

    // ----- Helper functions -----
    [[nodiscard]] static Buffer* openFileInNewBuffer(
            const std::string& path, bool addToRecFileList=true);
    static void flashDialog();

private:
    static void GLAPIENTRY glDebugMsgCB(
            GLenum source, GLenum type, GLuint, GLenum severity,
            GLsizei, const GLchar* message, const void*);

    // ----- Callbacks -----
    static void windowRefreshCB(GLFWwindow*);
    static void windowResizeCB(GLFWwindow*, int width, int height);
    static void windowKeyCB(GLFWwindow*, int key, int scancode, int action, int mods);
public:
    static void windowCharCB(GLFWwindow*, uint codePoint);
private:
    static void windowScrollCB(GLFWwindow*, double, double yOffset);
    static void windowCloseCB(GLFWwindow* window);
    static void cursorPosCB(GLFWwindow* window, double x, double y);
    static void mouseButtonCB(GLFWwindow*, int btn, int act, int mods);
    static void pathDropCB(GLFWwindow*, int count, const char** paths);

    static void toggleDebugDraw();
};
