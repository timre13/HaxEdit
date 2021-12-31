#pragma once

#include <string>
#include "TextRenderer.h"
#include "types.h"
#include "Syntax.h"
#include "config.h"

inline std::array<std::string, Syntax::_SYNTAX_MARK_COUNT> themeKeys = {
    "c_default",                // None, default
    "c_keyword",                // Keyword
    "c_type",                   // Type
    "c_operators",              // Operator
    "c_numbers",                // Number
    "c_string",                 // String
    "c_single_line_comment",    // Comment
    "c_string",                 // Character literal -- seems like chars don't have their own color
    "pp_directive",             // Preprocessor
    "c_braces",                 // Special character
    "pp_header",                // Filepath -- better than nothing
};

struct ThemeVal
{
    RGBColor color{};
};

struct Theme
{
    std::array<ThemeVal, Syntax::_SYNTAX_MARK_COUNT> values;
    RGBColor bgColor{DEF_BG};
    RGBAColor currLineColor{DEF_CURSOR_LINE_COLOR};
    RGBColor selBg{DEF_SEL_BG};
    RGBColor selFg{DEF_SEL_FG};
};

class ThemeLoader
{
public:
    ThemeLoader() {}

    Theme* load(const std::string& path);
};
