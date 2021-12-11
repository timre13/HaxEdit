#pragma once

#include "Logger.h"

class EditorMode final
{
public:
    enum class _EditorMode
    {
        Normal,
        Insert,
    };

private:
    _EditorMode m_editorMode;

public:
    EditorMode(){}

    _EditorMode get();
    void set(_EditorMode mode);

    char asChar();
    std::string asString();
};
