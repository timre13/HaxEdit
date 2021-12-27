#pragma once

#include "Logger.h"

// Editor mode status line string length, not counting escape sequences
#define EDITMODE_STATLINE_STR_PWIDTH 7

class EditorMode final
{
public:
    enum class _EditorMode
    {
        Normal,
        Insert,
        Replace,
    };

private:
    _EditorMode m_editorMode;

public:
    EditorMode(){}

    _EditorMode get();
    void set(_EditorMode mode);

    char asChar();
    std::string asString();
    std::string asStatLineStr();
};
