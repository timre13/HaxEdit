#pragma once

#include "../types.h"
#include "unicode/unistr.h"
#include <iomanip>
#include <sstream>
#include <string.h>
#include <string>
#include <time.h>
#include <filesystem>

std::u32string strToUtf32(const icu::UnicodeString& input);
std::string strToAscii(const String& str);

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

int getLongestLineLen(const std::string& str);

std::string strToLower(std::string str);
String strToLower(String str);

inline std::string quoteStr(const std::string& str) { return '"'+str+'"'; }
inline String quoteStr(const String& str) { return U'"'+str+U'"'; }

inline String charToStr(Char c) { return String(1, c); }
inline std::string charToStr(char c) { return std::string(1, c); }

class LineIterator
{
private:
    const String& m_strR;
    size_t m_charI{};

public:
    LineIterator(const String& str);

    [[nodiscard]] bool next(String& output);
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

std::string dateToStr(time_t date);

std::string getFileExt(const std::string& path);
std::string getParentPath(const std::string& path);

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
