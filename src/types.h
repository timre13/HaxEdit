#pragma once

#include "unicode/unistr.h"
#include "unicode/uchar.h"
#include "config.h"
#include "Logger.h"
#include <cassert>
#include <string>
#include <sstream>
#include <filesystem>
#include <time.h>
#include <string.h>

using uint = unsigned int;
using uchar = unsigned char;

class RGBColor
{
public:
    float r;
    float g;
    float b;

    RGBColor()
    {
        r = 0.0f;
        g = 0.0f;
        b = 0.0f;
    }

    constexpr RGBColor(float _r, float _g, float _b)
        : r{_r}, g{_g}, b{_b}
    {
        assert(_r >= 0.0f && _r <= 1.0f
            && _g >= 0.0f && _g <= 1.0f
            && _b >= 0.0f && _b <= 1.0f);
    }
};
#define UNPACK_RGB_COLOR(x) x.r, x.g, x.b

class RGBAColor
{
public:
    float r;
    float g;
    float b;
    float a=1.0f;

    RGBAColor()
    {
        r = 0.0f;
        g = 0.0f;
        b = 0.0f;
        a = 1.0f;
    }

    constexpr RGBAColor(float _r, float _g, float _b, float _a=1.0f)
        : r{_r}, g{_g}, b{_b}, a{_a}
    {
        assert(_r >= 0.0f && _r <= 1.0f
            && _g >= 0.0f && _g <= 1.0f
            && _b >= 0.0f && _b <= 1.0f
            && _a >= 0.0f && _a <= 1.0f);

    }
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
        output += toascii(c);
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

inline String strToLower(String str)
{
    for (Char& c : str)
        c = u_tolower(c);
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

inline std::string quoteStr(const std::string& str) { return '"'+str+'"'; }
inline String quoteStr(const String& str) { return U'"'+str+U'"'; }

inline String charToStr(Char c) { return String(1, c); }
inline std::string charToStr(char c) { return std::string(1, c); }

inline size_t operator "" _st(unsigned long long val) { return val; }

class LineIterator
{
private:
    const String& m_strR;
    size_t m_charI{};

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
};

template <typename T>
inline std::string intToHexStr(T x)
{
    std::stringstream ss;
    ss << std::uppercase << std::hex << x;
    const std::string str = ss.str();
    return ((str.size() % 2) ? "0" : "") + str;
}

/*
 * Get physical string length.
 * Ignores ANSI escape sequences.
 */
template <typename T>
inline size_t strPLen(const T& str)
{
    static_assert(
            std::is_base_of<String, T>::value | std::is_base_of<std::string, T>::value,
            "T must be a string type");

    size_t len{};
    bool isInsideEsc = false;
    for (char c : str)
    {
        if (isInsideEsc)
        {
            if (c == 'm')
                isInsideEsc = false;
        }
        else
        {
            if (c == '\033')
                isInsideEsc = true;
            else
                ++len;
        }
    }
    return len;
}

inline std::string dateToStr(time_t date)
{
    char buff[DATE_TIME_STR_LEN+1];
    auto gm = localtime(&date);
#ifndef NDEBUG
    if (!gm)
        Logger::err << "Failed to convert time_t to tm: " << strerror(errno) << Logger::End;
#endif
    assert(gm);
    assert(strftime(buff, DATE_TIME_STR_LEN+1, DATE_TIME_FORMAT, gm));
    assert(buff[DATE_TIME_STR_LEN] == 0);
    return buff;
}
