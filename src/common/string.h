#pragma once

#include "../types.h"
#include "unicode/unistr.h"
#include <iomanip>
#include <sstream>
#include <string.h>
#include <string>
#include <time.h>
#include <filesystem>
#include <vector>
#include "../Logger.h"

String strToUtf32(const icu::UnicodeString& input);
String strToUtf32(const std::string& input);
std::string strToAscii(const String& str);

#define _CHECK_T_STR_TYPE \
    static_assert(\
            std::is_base_of<String, T>::value | std::is_base_of<std::string, T>::value,\
            "T must be a string type");

template <typename T>
inline int strCountLines(const T& str)
{
    _CHECK_T_STR_TYPE;

    int lines{};
    for (auto it=str.begin(); it != str.end(); ++it)
    {
        if (*it == '\n')
            ++lines;
    }
    return lines;
}

int getLongestLineLen(const std::string& str);

std::string strToLower(std::string str);
String strToLower(String str);

inline std::string quoteStr(const std::string& str) { return '"'+str+'"'; }
inline String quoteStr(const String& str) { return U'"'+str+U'"'; }

inline String charToStr(Char c) { return String(1, c); }
inline std::string charToStr(char c) { return std::string(1, c); }

template <typename T>
class LineIterator
{
private:
    const T& m_strR;
    size_t m_charI{};

public:
    LineIterator(const T& str)
        : m_strR{str}
    {
        _CHECK_T_STR_TYPE;
    }

    [[nodiscard]] bool next(T& output)
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
    _CHECK_T_STR_TYPE;

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

std::string dateToStr(time_t date);

std::string getFileExt(const std::string& path);
std::string getParentPath(const std::string& path);

template <typename T>
inline bool isValidFilePath(const T& str)
{
    _CHECK_T_STR_TYPE;

    const auto path = std::filesystem::path{str};
    return std::filesystem::exists(path)
        && (std::filesystem::is_regular_file(path)
         || std::filesystem::is_symlink(path)
         || std::filesystem::is_block_file(path)
         || std::filesystem::is_character_file(path)
    );
}

template <typename T>
std::vector<T> splitStrToLines(const T& str, bool keepBreaks=false)
{
    _CHECK_T_STR_TYPE;

    std::vector<T> output;
    auto it = LineIterator<T>(str);
    T line;
    while (it.next(line))
    {
        if (keepBreaks) line += '\n';
        output.push_back(std::move(line));
    }

    return output;
}

template <typename T>
inline T repeatChar(typename T::value_type character, size_t count)
{
    _CHECK_T_STR_TYPE;

    return T(count, character);
}

size_t countLineListLen(const std::vector<String>& lines);
String lineVecConcat(const std::vector<String>& lines);
