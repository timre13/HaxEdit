#pragma once

#include "Logger.h"

#define EDITMODE_STATLINE_STR_LEN 15

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
