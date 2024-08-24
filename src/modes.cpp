#include "modes.h"
#include "common/string.h"
#include "Bindings.h"
#include "globals.h"

EditorMode::_EditorMode EditorMode::get()
{
    return m_editorMode;
}

void EditorMode::set(EditorMode::_EditorMode mode)
{
    m_editorMode = mode;
    Bindings::activeBindingMap = Bindings::getBindingsForMode(mode);
    assert(Bindings::activeBindingMap);
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
    assert(false);
    return ' ';
}

std::string EditorMode::asString()
{
    switch (m_editorMode)
    {
    case _EditorMode::Normal: return "Normal";
    case _EditorMode::Insert: return "Insert";
    case _EditorMode::Replace: return "Replace";
    }
    assert(false);
    return " ";
}

std::string EditorMode::asStatLineStr()
{
    // Note: Adjust `EDITMODE_STATLINE_STR_PWIDTH` if needed

    switch (m_editorMode)
    {
    case _EditorMode::Normal:  return "\033[36mNORMAL \033[0m";
    case _EditorMode::Insert:  return "\033[32mINSERT \033[0m";
    case _EditorMode::Replace: return "\033[31mREPLACE\033[0m";
    }
    assert(false);
    return " ";
}
