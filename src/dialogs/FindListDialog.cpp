#include "FindListDialog.h"
#include "../glstuff.h"
#include "../TextRenderer.h"
#include "../UiRenderer.h"
#include "../common/string.h"

FindListDialog::FindListDialog(
            callback_t enterCb,
            void* enterCbUserData,
            typeCallback_t typeCb,
            void* typeCbUserData,
            String message
)
    : Dialog(enterCb, enterCbUserData)
    , m_msg{message}, m_typeCb{typeCb}, m_typeCbUserData{typeCbUserData}
{
    assert(m_typeCb);
    Logger::dbg << "Created a FindListDialog with message " << message << Logger::End;
    m_typeCb(this, m_buffer, &m_entries, m_typeCbUserData);
}

void FindListDialog::recalculateDimensions()
{
    m_dialogDims.xPos = 80;
    m_dialogDims.yPos = 80;
    m_dialogDims.width = std::max(g_windowWidth-160, 0);
    m_dialogDims.height = std::max(g_windowHeight-160, 0);

    m_titleRect.xPos = m_dialogDims.xPos+10;
    m_titleRect.width = m_dialogDims.width-20;
    m_titleRect.height = g_fontSizePx*1.5f;

    m_entryRect.xPos = m_titleRect.xPos+10;
    m_entryRect.width = m_dialogDims.width-40;
    m_entryRect.height = g_fontSizePx;

    m_titleRect.yPos = m_dialogDims.yPos+10;
    m_entryRect.yPos = m_titleRect.yPos+m_titleRect.height+10;
}

void FindListDialog::render()
{
    if (m_isClosed)
        return;

    recalculateDimensions();

    renderBase();

    // Render title path
    g_textRenderer->renderString(
            m_msg,
            {m_titleRect.xPos, m_titleRect.yPos});

    // Render entry rectangle
    g_uiRenderer->renderFilledRectangle(
            {m_entryRect.xPos, m_entryRect.yPos},
            {m_entryRect.xPos+m_entryRect.width, m_entryRect.yPos+m_entryRect.height},
            {0.0f, 0.1f, 0.3f, 0.8f});
    // Render entry value
    g_textRenderer->renderString(
            m_buffer,
            {m_entryRect.xPos, m_entryRect.yPos-2});

    for (size_t i{}; i < m_entries.size(); ++i)
    {
        const auto& entry = m_entries[i];
        Dimensions rect{};
        rect.width = m_dialogDims.width-20;
        rect.height = g_fontSizePx;
        rect.xPos = m_dialogDims.xPos+10;
        rect.yPos = m_entryRect.yPos+m_entryRect.height+10+(rect.height+2)*i;

        if (rect.yPos+rect.height > m_dialogDims.yPos+m_dialogDims.height)
            continue;

        if (i == (size_t)m_selectedEntryI)
        {
            // Render entry rectangle outline
            g_uiRenderer->renderFilledRectangle(
                    {rect.xPos-1, rect.yPos-1},
                    {rect.xPos+rect.width+1, rect.yPos+rect.height+1},
                    {1.0f, 1.0f, 1.0f, 0.7f});
        }
        // Render entry rectangle
        g_uiRenderer->renderFilledRectangle(
                {rect.xPos, rect.yPos},
                {rect.xPos+rect.width, rect.yPos+rect.height},
                i == (size_t)m_selectedEntryI ?
                    RGBAColor{0.2f, 0.3f, 0.5f, 0.7f} : RGBAColor{0.1f, 0.2f, 0.4f, 0.7f});

        // TODO: Color label / display icon based on kind
        // Render label
        g_textRenderer->renderString(utf8To32(entry.info.name),
                {rect.xPos+FILE_DIALOG_ICON_SIZE_PX+10, rect.yPos+rect.height/2-g_fontSizePx/2-2},
                FONT_STYLE_REGULAR);

        // TODO: Render more info

        //// Render permissions
        //g_textRenderer->renderString(utf8To32(entry->permissionStr),
        //        {m_dialogDims.xPos+m_dialogDims.width-g_fontWidthPx*9-20, rect.yPos-2});
        //// Render last mod time
        //g_textRenderer->renderString(utf8To32(entry->lastModTimeStr),
        //        {m_dialogDims.xPos+m_dialogDims.width-g_fontWidthPx*(9+DATE_TIME_STR_LEN+4)-20, rect.yPos-2});
        //// Render file icon
        //g_fileTypeHandler->getIconFromFilename(
        //        file->name, file->isDirectory)->render(
        //            {rect.xPos, rect.yPos}, {FILE_DIALOG_ICON_SIZE_PX, FILE_DIALOG_ICON_SIZE_PX});
    }
}

void FindListDialog::tick(int elapsedMs)
{
    const int oldVal = m_timeUntilFetch;
    m_timeUntilFetch -= elapsedMs;
    if (oldVal > 0 && m_timeUntilFetch <= 0)
    {
        m_typeCb(this, m_buffer, &m_entries, m_typeCbUserData);
        if (m_entries.empty())
            m_selectedEntryI = 0;
        else
            m_selectedEntryI = std::min(m_selectedEntryI, m_entries.size()-1);
        g_isRedrawNeeded = true;
    }
}

void FindListDialog::handleKey(const Bindings::BindingKey& key)
{
    if (key.mods == 0)
    {
        if (key.key == U"<Escape>")
        {
            Logger::dbg << "AskerDialog: Cancelled" << Logger::End;
            m_isClosed = true;
        }
        else if (key.key == U"<Backspace>")
        {
            m_buffer = m_buffer.substr(0, m_buffer.size()-1);
            m_timeUntilFetch = FIND_LIST_DLG_TYPE_HOLD_TIME_MS;
        }
        else if (key.key == U"<Enter>")
        {
            m_isClosed = true;
            if (m_callback)
                m_callback(0, this, m_cbUserData);
        }
        else if (key.key == U"<Up>")
        {
            selectPrevEntry();
        }
        else if (key.key == U"<Down>")
        {
            selectNextEntry();
        }
        else if (!key.isFuncKey())
        {
            m_buffer += key.getAsChar();
            m_timeUntilFetch = FIND_LIST_DLG_TYPE_HOLD_TIME_MS;
        }
    }
    else if (key.mods == GLFW_MOD_CONTROL)
    {
        if (key.key == U"<Backspace>")
        {
            m_buffer.clear();
            m_timeUntilFetch = FIND_LIST_DLG_TYPE_HOLD_TIME_MS;
        }
        else if (key.key == U"k")
        {
            selectPrevEntry();
        }
        else if (key.key == U"j")
        {
            selectNextEntry();
        }
    }
    else if (key.mods == GLFW_MOD_SHIFT)
    {
        if (!key.isFuncKey())
        {
            m_buffer += key.getAsChar();
        }
    }
}
