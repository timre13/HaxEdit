#pragma once

#include <regex>

#define SYNTAX_MARK_NONE      ' '
#define SYNTAX_MARK_KEYWORD   'k'
#define SYNTAX_MARK_TYPE      't'
#define SYNTAX_MARK_OPERATOR  'o'
#define SYNTAX_MARK_NUMBER    'n'
#define SYNTAX_MARK_STRING    's'
#define SYNTAX_MARK_COMMENT   'c'

#define SYNTAX_COLOR_NONE       RGBColor{1.0f, 1.0f, 1.0f}
#define SYNTAX_COLOR_KEYWORD    RGBColor{0.0f, 0.4f, 0.8f}
#define SYNTAX_COLOR_TYPE       RGBColor{0.8f, 0.8f, 0.2f}
#define SYNTAX_COLOR_OPERATOR   RGBColor{1.0f, 0.0f, 0.0f}
#define SYNTAX_COLOR_NUMBER     RGBColor{0.7f, 0.5f, 0.3f}
#define SYNTAX_COLOR_STRING     RGBColor{0.0f, 0.8f, 0.2f}
#define SYNTAX_COLOR_COMMENT    RGBColor{0.5f, 0.5f, 0.5f}

#define SYNTAX_STYLE_NONE       FontStyle::Regular
#define SYNTAX_STYLE_KEYWORD    FontStyle::Bold
#define SYNTAX_STYLE_TYPE       FontStyle::Regular
#define SYNTAX_STYLE_NUMBER     FontStyle::Regular
#define SYNTAX_STYLE_OPERATOR   FontStyle::Regular
#define SYNTAX_STYLE_STRING     FontStyle::Italic
#define SYNTAX_STYLE_COMMENT    FontStyle::Regular

namespace Syntax
{

std::array<std::string, 85> keywordList{
        "alignas", "alignof", "and", "and_eq", "asm", "atomic_cancel", "atomic_commit", "atomic_noexcept",
        "auto", "bitand", "bitor", "break", "case", "catch", "class", "compl", "concept", "const",
        "consteval", "constexpr", "constinit", "const_cast", "continue", "co_await", "co_return",
        "co_yield", "decltype", "default", "delete", "do", "dynamic_cast", "else", "enum", "explicit",
        "export", "extern", "false", "for", "friend", "goto", "if", "inline", "mutable", "namespace",
        "new", "noexcept", "not", "not_eq", "nullptr", "operator", "or", "or_eq", "private", "protected",
        "public", "reflexpr", "register", "reinterpret_cast", "requires", "return", "signed", "sizeof",
        "static", "static_assert", "static_cast", "struct", "switch", "synchronized", "template", "this",
        "thread_local", "throw", "true", "try", "typedef", "typeid", "typename", "union", "unsigned",
        "using", "virtual", "volatile", "while", "xor", "xor_eq",
};

std::array<std::string, 12> typeList{
        "bool", "char", "char8_t", "char16_t", "char32_t", "double",
        "float", "int", "long", "short", "void", "size_t"
};

std::regex numberRegex{R"(0?[bxBX]?[0-9.]+)", std::regex::optimize};

std::array<std::string, 27> operatorList{
        ":", "+", "-", "type", "!", "~", "^", "|", "*", "/", "&", "sizeof", "co_await", "new", "delete",
        "<", ">", "=", "?", "throw", "co_yield", "=", "%", ",", "[", "]", ".",
};

std::regex stringRegex{
        R"(".*")",
        std::regex::optimize
};

std::string lineCommentPrefix{"//"};

}
