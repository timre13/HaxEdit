#include "modes.h"
#include "types.h"
#include "Bindings.h"
#include "globals.h"

EditorMode::_EditorMode EditorMode::get()
{
    return m_editorMode;
}

void EditorMode::set(EditorMode::_EditorMode mode)
{
    m_editorMode = mode;
    switch (mode)
    {
    case _EditorMode::Normal:
        Bindings::bindingsP = &Bindings::nmap;
        break;

    case _EditorMode::Insert:
        Bindings::bindingsP = &Bindings::imap;
        g_shouldIgnoreNextChar = true;
        break;

    case _EditorMode::Replace:
        Bindings::bindingsP = &Bindings::imap;
        g_shouldIgnoreNextChar = true;
        break;
    }
    Logger::dbg << "Switched to editor mode: " << quoteStr(asString()) << Logger::End;
}

char EditorMode::asChar()
{
    switch (m_editorMode)
    {
    case _EditorMode::Normal: return 'N';
    case _EditorMode::Insert: return 'I';
    case _EditorMode::Replace: return 'R';
    }
}

std::string EditorMode::asString()
{
    switch (m_editorMode)
    {
    case _EditorMode::Normal: return "Normal";
    case _EditorMode::Insert: return "Insert";
    case _EditorMode::Replace: return "Replace";
    }
}

std::string EditorMode::asStatLineStr()
{
    // Note: Adjust `EDITMODE_STATLINE_STR_LEN` if needed

    switch (m_editorMode)
    {
    case _EditorMode::Normal: return "--- NORMAL --- ";
    case _EditorMode::Insert: return "--- INSERT --- ";
    case _EditorMode::Replace: return "--- REPLACE ---";
    }
}
