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

inline std::array<String, 87> keywordList{
        U"alignas", U"alignof", U"and", U"and_eq", U"asm", U"atomic_cancel", U"atomic_commit", U"atomic_noexcept",
        U"auto", U"bitand", U"bitor", U"break", U"case", U"catch", U"class", U"compl", U"concept", U"const",
        U"consteval", U"constexpr", U"constinit", U"const_cast", U"continue", U"co_await", U"co_return",
        U"co_yield", U"decltype", U"default", U"delete", U"do", U"dynamic_cast", U"else", U"enum", U"explicit",
        U"export", U"extern", U"false", U"final", U"for", U"friend", U"goto", U"if", U"inline", U"mutable", U"namespace",
        U"new", U"noexcept", U"not", U"not_eq", U"nullptr", U"operator", U"or", U"or_eq", U"override", U"private", U"protected",
        U"public", U"reflexpr", U"register", U"reinterpret_cast", U"requires", U"return", U"signed", U"sizeof",
        U"static", U"static_assert", U"static_cast", U"struct", U"switch", U"synchronized", U"template", U"this",
        U"thread_local", U"throw", U"true", U"try", U"typedef", U"typeid", U"typename", U"union", U"unsigned",
        U"using", U"virtual", U"volatile", U"while", U"xor", U"xor_eq"
};

inline std::array<String, 12> typeList{
        U"bool", U"char", U"char8_t", U"char16_t", U"char32_t", U"double",
        U"float", U"int", U"long", U"short", U"void", U"size_t"
};

inline std::array<String, 27> operatorList{
        U":", U"+", U"-", U"type", U"!", U"~", U"^", U"|", U"*", U"/", U"&", U"sizeof", U"co_await", U"new", U"delete",
        U"<", U">", U"=", U"?", U"throw", U"co_yield", U"=", U"%", U",", U"[", U"]", U".",
};

inline String lineCommentPrefix{U"//"};

inline String blockCommentBegin{U"/*"};
inline String blockCommentEnd{U"*/"};

inline String preprocessorPrefix{U"#"};

}
