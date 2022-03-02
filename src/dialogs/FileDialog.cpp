#include "FileDialog.h"
#include "../UiRenderer.h"
#include "../FileTypeHandler.h"
#include "../TextRenderer.h"
#include "../Logger.h"
#include "../Timer.h"
#include "../config.h"
#include "../types.h"
#include "../config.h"
#include "../glstuff.h"
#include "../globals.h"
#include "../common/string.h"
#include <filesystem>
#include <algorithm>
#include <chrono>

namespace std_fs = std::filesystem;

std::string FileDialog::s_lastDir = "";

FileDialog::FileDialog(
        callback_t cb,
        void* cbUserData,
        Type type
        )
    : Dialog{cb, cbUserData}, m_type{type}
{
    if (!s_lastDir.empty() && std_fs::is_directory(s_lastDir))
    {
        m_dirPath = s_lastDir;
        Logger::dbg << "Last dir " << quoteStr(s_lastDir) << " is used as current path " << Logger::End;
    }
    else
    {
        m_dirPath = ".";
        Logger::dbg << "Last dir " << quoteStr(s_lastDir) << " won't be used, defaulting to "
            << quoteStr(m_dirPath) << Logger::End;
    }
    Logger::dbg << "Created a " << (type == Type::Open ? "file" : "directory")
        << " chooser dialog inside the dir.: " << quoteStr(m_dirPath) << Logger::End;

    genFileList();
}

void FileDialog::recalculateDimensions()
{
    assert(g_windowWidth > 0);
    assert(g_windowHeight > 0);

    m_dialogDims.xPos = 80;
    m_dialogDims.yPos = 80;
    m_dialogDims.width = std::max(g_windowWidth-160, 0);
    m_dialogDims.height = std::max(g_windowHeight-160, 0);

    m_titleRect.xPos = m_dialogDims.xPos+10;
    m_titleRect.yPos = m_dialogDims.yPos+10;
    m_titleRect.width = m_dialogDims.width-20;
    m_titleRect.height = g_fontSizePx*1.5f;

    m_fileRectDims.clear();
    const int rectHeight = std::max(g_fontSizePx, FILE_DIALOG_ICON_SIZE_PX);
    for (size_t i{}; i < m_fileList.size(); ++i)
    {
        auto rect = std::make_unique<Dimensions>();
        rect->width = m_dialogDims.width-20;
        rect->height = rectHeight;
        rect->xPos = m_dialogDims.xPos+10;
        rect->yPos = m_titleRect.yPos+m_titleRect.height+20+(rect->height+2)*i;
        m_fileRectDims.push_back(std::move(rect));
    }
}

void FileDialog::render()
{
    if (m_isClosed)
        return;

    TIMER_BEGIN_FUNC();

    recalculateDimensions();

    renderBase();
    // Render title rectangle
    g_uiRenderer->renderFilledRectangle(
            {m_titleRect.xPos, m_titleRect.yPos},
            {m_titleRect.xPos+m_titleRect.width, m_titleRect.yPos+m_titleRect.height},
            {0.0f, 0.1f, 0.3f, 0.8f});
    // Render current path
    g_textRenderer->renderString(
            m_dirPath/std_fs::path{!m_fileList.empty() ? m_fileList[m_selectedFileI]->name : ""},
            {m_titleRect.xPos, m_titleRect.yPos});

    if (m_fileList.empty())
    {
        TIMER_END_FUNC();
        return;
    }

    for (size_t i{}; i < m_fileList.size(); ++i)
    {
        auto& file = m_fileList[i];
        Dimensions rect = {
            m_fileRectDims[i]->xPos, m_fileRectDims[i]->yPos-m_scrollPx,
            m_fileRectDims[i]->width, m_fileRectDims[i]->height};

        if (rect.yPos+rect.height > m_dialogDims.yPos+m_dialogDims.height)
            continue;

        if (i == (size_t)m_selectedFileI)
        {
            // Render file rectangle outline
            g_uiRenderer->renderFilledRectangle(
                    {rect.xPos-1, rect.yPos-1},
                    {rect.xPos+rect.width+1, rect.yPos+rect.height+1},
                    {1.0f, 1.0f, 1.0f, 0.7f});
        }
        // Render file rectangle
        g_uiRenderer->renderFilledRectangle(
                {rect.xPos, rect.yPos},
                {rect.xPos+rect.width, rect.yPos+rect.height},
                i == (size_t)m_selectedFileI ?
                    RGBAColor{0.2f, 0.3f, 0.5f, 0.7f} : RGBAColor{0.1f, 0.2f, 0.4f, 0.7f});
        // Render filename
        g_textRenderer->renderString(file->name,
                {rect.xPos+FILE_DIALOG_ICON_SIZE_PX+10, rect.yPos+rect.height/2-g_fontSizePx/2-2},
                file->isDirectory ? FONT_STYLE_ITALIC : FONT_STYLE_REGULAR);
        // Render permissions
        g_textRenderer->renderString(file->permissionStr,
                {m_dialogDims.xPos+m_dialogDims.width-g_fontWidthPx*9-20, rect.yPos-2});
        // Render last mod time
        g_textRenderer->renderString(file->lastModTimeStr,
                {m_dialogDims.xPos+m_dialogDims.width-g_fontWidthPx*(9+DATE_TIME_STR_LEN+4)-20, rect.yPos-2});
        // Render file icon
        g_fileTypeHandler->getIconFromFilename(
                file->name, file->isDirectory)->render(
                    {rect.xPos, rect.yPos}, {FILE_DIALOG_ICON_SIZE_PX, FILE_DIALOG_ICON_SIZE_PX});
    }

    TIMER_END_FUNC();
}

void FileDialog::genFileList()
{
    TIMER_BEGIN_FUNC();
    Logger::dbg << "Listing directory: " << quoteStr(m_dirPath) << Logger::End;
    m_dirPath = std_fs::canonical(m_dirPath);
    Logger::dbg << "..., as canonical: " << quoteStr(m_dirPath) << Logger::End;

    m_fileList.clear();

    if (m_dirPath != "/")
    {
        // Add entry to go to parent directory
        auto parentDirEntry = std::make_unique<FileEntry>();
        parentDirEntry->name = "..";
        parentDirEntry->isDirectory = true;
        parentDirEntry->lastModTime = 0;
        parentDirEntry->lastModTimeStr = "";
        parentDirEntry->permissionStr = "";
        m_fileList.push_back(std::move(parentDirEntry));
    }
    if (m_type == Type::Save)
    {
        // Add entry to go the current directory
        auto currentDirEntry = std::make_unique<FileEntry>();
        currentDirEntry->name = ".";
        currentDirEntry->isDirectory = true;
        currentDirEntry->lastModTime = 0;
        currentDirEntry->lastModTimeStr = "";
        currentDirEntry->permissionStr = "";
        m_fileList.push_back(std::move(currentDirEntry));
    }

    try
    {
        auto it = std_fs::directory_iterator{m_dirPath, std_fs::directory_options::skip_permission_denied};
        for (auto& file : it)
        {
            if (!file.is_regular_file() && !file.is_directory())
                continue;

            auto entry = std::make_unique<FileEntry>();
            entry->name = file.path().filename();
            entry->isDirectory = file.is_directory();
            entry->lastModTime = std::chrono::system_clock::to_time_t(std::chrono::file_clock::to_sys(file.last_write_time()));
            entry->lastModTimeStr = dateToStr(entry->lastModTime);
            auto perms = file.status().permissions();
            entry->permissionStr
                = std::string()
                + ((perms & std_fs::perms::owner_read)   != std_fs::perms::none ? 'r' : '-')
                + ((perms & std_fs::perms::owner_write)  != std_fs::perms::none ? 'w' : '-')
                + ((perms & std_fs::perms::owner_exec)   != std_fs::perms::none ? 'x' : '-')
                + ((perms & std_fs::perms::group_read)   != std_fs::perms::none ? 'r' : '-')
                + ((perms & std_fs::perms::group_write)  != std_fs::perms::none ? 'w' : '-')
                + ((perms & std_fs::perms::group_exec)   != std_fs::perms::none ? 'x' : '-')
                + ((perms & std_fs::perms::others_read)  != std_fs::perms::none ? 'r' : '-')
                + ((perms & std_fs::perms::others_write) != std_fs::perms::none ? 'w' : '-')
                + ((perms & std_fs::perms::others_exec)  != std_fs::perms::none ? 'x' : '-');

            m_fileList.push_back(std::move(entry));
        }
    }
    catch (std::exception& e)
    {
        Logger::err << "Failed to list directory: " << e.what() << Logger::End;
    }

    std::sort(m_fileList.begin(), m_fileList.end(),
            [](const std::unique_ptr<FileEntry>& a, const std::unique_ptr<FileEntry>& b){
                return a->name < b->name;
        }
    );

    TIMER_END_FUNC();
}

void FileDialog::handleKey(int key, int mods)
{
    if (mods != 0)
        return;

    if (key == GLFW_KEY_ESCAPE)
    {
        Logger::dbg << "FileDialog: Cancelled" << Logger::End;
        m_isClosed = true;
        return;
    }

    switch (key)
    {
    case GLFW_KEY_UP:
    case GLFW_KEY_K:
        selectPrevFile();
        break;

    case GLFW_KEY_DOWN:
    case GLFW_KEY_J:
        selectNextFile();
        break;

    case GLFW_KEY_ENTER:
        if (!m_fileList.empty() && m_fileList[m_selectedFileI]->name == ".")
        {
            Logger::dbg << "FileDialog: Selected a directory" << Logger::End;
            if (m_callback)
                m_callback(OPENMODE_NEWTAB, this, m_cbUserData);
            m_isClosed = true;
            s_lastDir = m_dirPath;
        }
        else if (m_fileList.empty() || !m_fileList[m_selectedFileI]->isDirectory)
        {
            Logger::dbg << "FileDialog: Opened a file: " <<
                m_fileList[m_selectedFileI]->name << Logger::End;
            // If the list is empty or a file was opened, we are done
            if (m_callback)
                m_callback(OPENMODE_NEWTAB, this, m_cbUserData);
            m_isClosed = true;
            s_lastDir = m_dirPath;
        }
        else
        {
            Logger::dbg << "FileDialog: Opened a directory: "
                << m_fileList[m_selectedFileI]->name << Logger::End;
            // If a directory was opened, go into it
            m_dirPath = std_fs::path{m_dirPath}/m_fileList[m_selectedFileI]->name;
            m_selectedFileI = 0;
            genFileList();
            recalculateDimensions();
        }
        break;

    case GLFW_KEY_S:
        if (!m_fileList.empty() && m_fileList[m_selectedFileI]->name == ".")
        {
            Logger::dbg << "FileDialog: Selected a directory" << Logger::End;
            if (m_callback)
                m_callback(OPENMODE_SPLIT, this, m_cbUserData);
            m_isClosed = true;
            s_lastDir = m_dirPath;
        }
        else if (m_fileList.empty() || !m_fileList[m_selectedFileI]->isDirectory)
        {
            Logger::dbg << "FileDialog: Opened a file: "
                << m_fileList[m_selectedFileI]->name << Logger::End;
            if (m_callback)
                m_callback(OPENMODE_SPLIT, this, m_cbUserData);
            m_isClosed = true;
            s_lastDir = m_dirPath;
        }
        break;
    }
}
