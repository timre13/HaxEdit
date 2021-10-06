#pragma once

#include "types.h"
#include "TextRenderer.h"
#include <array>
#include <string>

namespace Syntax
{

enum SyntaxMarks: uint8_t
{
    MARK_NONE,
    MARK_KEYWORD,
    MARK_TYPE,
    MARK_OPERATOR,
    MARK_NUMBER,
    MARK_STRING,
    MARK_COMMENT,
    MARK_CHARLIT,
    MARK_PREPRO,
    MARK_SPECCHAR,
    MARK_FILEPATH,
    _SYNTAX_MARK_COUNT,
};

constexpr RGBColor syntaxColors[_SYNTAX_MARK_COUNT] = {
    RGBColor{1.000f, 1.000f, 1.000f}, // None, default
    RGBColor{0.000f, 0.400f, 0.800f}, // Keyword
    RGBColor{0.900f, 0.900f, 0.000f}, // Type
    RGBColor{1.000f, 0.100f, 0.100f}, // Operator
    RGBColor{0.938f, 0.402f, 0.031f}, // Number
    RGBColor{0.200f, 0.900f, 0.300f}, // String
    RGBColor{0.500f, 0.500f, 0.500f}, // Comment
    RGBColor{0.050f, 0.600f, 0.100f}, // Character literal
    RGBColor{0.000f, 0.600f, 0.200f}, // Preprocessor
    RGBColor{0.500f, 0.500f, 0.500f}, // Special character
    RGBColor{0.0800, 0.510f, 0.710f}, // Filepath
};

constexpr FontStyle syntaxStyles[_SYNTAX_MARK_COUNT] = {
    FontStyle::Regular, // None, default
    FontStyle::Bold,    // Keyword
    FontStyle::Regular, // Type
    FontStyle::Regular, // Operator
    FontStyle::Regular, // Number
    FontStyle::Italic,  // String
    FontStyle::Regular, // Comment
    FontStyle::Regular, // Character literal
    FontStyle::Regular, // Preprocessor
    FontStyle::Bold,    // Special character
    FontStyle::Italic,  // Filepath - Should be "Underlined" in the future, when we support it
};

inline std::array<std::string, 85> keywordList{
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

inline std::array<std::string, 12> typeList{
        "bool", "char", "char8_t", "char16_t", "char32_t", "double",
        "float", "int", "long", "short", "void", "size_t"
};

inline std::array<std::string, 27> operatorList{
        ":", "+", "-", "type", "!", "~", "^", "|", "*", "/", "&", "sizeof", "co_await", "new", "delete",
        "<", ">", "=", "?", "throw", "co_yield", "=", "%", ",", "[", "]", ".",
};

inline std::string lineCommentPrefix{"//"};

inline std::string blockCommentBegin{"/*"};
inline std::string blockCommentEnd{"*/"};

inline std::string preprocessorPrefix{"#"};

}
