#pragma once

#include "unicode/unistr.h"
#include <cassert>
#include <string>
#include <sstream>
#include <filesystem>

using uint = unsigned int;
using uchar = unsigned char;

struct RGBColor
{
    float r;
    float g;
    float b;
};
#define UNPACK_RGB_COLOR(x) x.r, x.g, x.b

struct RGBAColor
{
    float r;
    float g;
    float b;
    float a=1.0f;
};
#define UNPACK_RGBA_COLOR(x) x.r, x.g, x.b, x.a

#define RGB_COLOR_TO_RGBA(x) RGBAColor{x.r, x.g, x.b, 1.0f}

using String = std::u32string;
using Char = char32_t;

inline std::u32string strToUtf32(const icu::UnicodeString& input)
{
    const int32_t len = input.countChar32()+1;
    int32_t* buff = new int32_t[len];
    UErrorCode err = U_ZERO_ERROR;
    input.toUTF32(buff, input.countChar32(), err);
    buff[len-1] = '\0';
    std::u32string output{(const char32_t*)buff};
    delete[] buff;
    assert(U_SUCCESS(err));
    return output;
}

inline std::string strToAscii(const String& str)
{
    std::string output;
    for (Char c : str)
        output += (char)c;
    return output;
}

template <typename T>
inline int strCountLines(const T& str)
{
    int lines{};
    for (auto it=str.begin(); it != str.end(); ++it)
    {
        if (*it == '\n')
            ++lines;
    }
    return lines;
}

inline int getLongestLineLen(const std::string& str)
{
    size_t maxLen{};
    std::stringstream ss;
    ss << str;
    std::string line;
    while (std::getline(ss, line))
    {
        if (line.length() > maxLen)
            maxLen = line.length();
    }
    return (int)maxLen;
}

inline std::string strToLower(std::string str)
{
    for (char& c : str)
        c = tolower(c);
    return str;
}

template <typename T>
inline T getFileExt(const T& path)
{
    const std::string out = std::filesystem::path{path}.extension().string();
    if (!out.empty())
        return out.substr(1);
    return "";
}

template <typename T>
inline T getParentPath(const T& path)
{
    return std::filesystem::path{path}.parent_path();
}

template <typename T>
inline bool isValidFilePath(const T& str)
{
    const auto path = std::filesystem::path{str};
    return std::filesystem::exists(path)
        && (std::filesystem::is_regular_file(path)
         || std::filesystem::is_symlink(path)
         || std::filesystem::is_block_file(path)
         || std::filesystem::is_character_file(path)
    );
}

inline size_t operator "" _st(unsigned long long val) { return val; }

class LineIterator
{
public:
    LineIterator(const String& str)
        : m_strR{str}
    {
    }

    [[nodiscard]] bool next(String& output)
    {
        output.clear();
        if (m_charI >= m_strR.size())
            return false;

        while (m_charI < m_strR.size() && m_strR[m_charI] != '\n')
            output += m_strR[m_charI++];
        ++m_charI;
        return true;
    }

private:
    const String& m_strR;
    size_t m_charI{};
};
