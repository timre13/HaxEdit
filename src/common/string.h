#pragma once

#include "../types.h"
#include <unicode/uchar.h>
#include <unicode/unistr.h>
#include <iomanip>
#include <sstream>
#include <string.h>
#include <string>
#include <time.h>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <cctype>
#include "../Logger.h"
using namespace std::string_literals;

String icuStrToUtf32(const icu::UnicodeString& input);
String utf8To32(const std::string& input);
std::string utf32To8(const String& input);

template <typename T>
static constexpr bool _isStringType = false;
template <typename T>
static constexpr bool _isStringType<std::basic_string<T>> = true;

template <typename T>
static void checkIfStringObject(const T& _={})
{
    (void)_;
    static_assert(
            _isStringType<T>,
            "T must be a string type");
}

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
        checkIfStringObject(str);
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
inline int strCountLines(const T& str)
{
    checkIfStringObject(str);

    int lines{};
    for (auto it=str.begin(); it != str.end(); ++it)
    {
        if (*it == '\n')
            ++lines;
    }
    return lines;
}

template <typename T>
size_t getLongestLineLen(const T& str)
{
    checkIfStringObject(str);

    size_t maxLen{};
    T line;
    LineIterator it{str};
    while (it.next(line))
    {
        if (line.length() > maxLen)
            maxLen = line.length();
    }
    return maxLen;
}

std::string strToLower(std::string str);
String strToLower(String str);

template <typename T>
inline T strCapitalize(T str)
{
    checkIfStringObject(str);

    if (str.empty())
        return str;
    str[0] = toupper(str[0]);
    return str;
}

template <typename T>
inline T strTrimTrailingLineBreak(const T& str)
{
    checkIfStringObject(str);

    if (str.ends_with('\n'))
    {
        T output = str;
        output.pop_back();
        return output;
    }
    return str;
}

inline std::string quoteStr(const std::string& str) { return '"'+str+'"'; }
inline String quoteStr(const String& str) { return U'"'+str+U'"'; }

inline String charToStr(Char c) { return String(1, c); }
inline std::string charToStr(char c) { return std::string(1, c); }

template <typename T>
inline std::string intToHexStr(T x)
{
    std::stringstream ss;
    ss << std::uppercase << std::hex << +x;
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
    checkIfStringObject(str);

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
    checkIfStringObject(str);

    const auto path = std::filesystem::path{str};
    return std::filesystem::exists(path)
        && (std::filesystem::is_regular_file(path)
         || std::filesystem::is_symlink(path)
         || std::filesystem::is_block_file(path)
         || std::filesystem::is_character_file(path)
    );
}

template <typename T>
inline bool isFormallyValidUrl(const T& str)
{
    checkIfStringObject(str);

    // Check if it has a valid prefix
    bool prefValid = false;
    size_t prefLen{};
    if (str.starts_with(U"https://"))
    {
        prefValid = true;
        prefLen = 8;
        if (str.substr(prefLen).starts_with(U"www."))
            prefLen += 4;
    }
    else if (str.starts_with(U"http://"))
    {
        prefValid = true;
        prefLen = 7;
        if (str.substr(prefLen).starts_with(U"www."))
            prefLen += 4;
    }
    else if (str.starts_with(U"www."))
    {
        prefValid = true;
        prefLen = 4;
    }
    if (!prefValid)
        return false;

    // Check if there are only valid characters
    const bool charsValid = std::all_of(str.begin()+prefLen, str.end(), [](typename T::value_type c){
        if (std::isalnum(c))
            return true;
        switch (c)
        {
        case '-':
        case '.':
        case '_':
        case '~':
        case ':':
        case '/':
        case '?':
        case '#':
        case '[':
        case ']':
        case '@':
        case '!':
        case '$':
        case '&':
        case '\'':
        case '(':
        case ')':
        case '*':
        case '+':
        case ',':
        case ';':
        case '%':
        case '=':
            return true;
        }
        return false;
    });
    if (!charsValid)
        return false;

    // Check if there is a TLD separator
    if (std::find(str.begin()+prefLen, str.end(), '.') == str.end())
        return false;

    return true;
}

template <typename T>
std::vector<T> splitStrToLines(const T& str, bool keepBreaks=false)
{
    checkIfStringObject(str);

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
    checkIfStringObject<T>();

    return T(count, character);
}

size_t countLineListLen(const std::vector<String>& lines);
String lineVecConcat(const std::vector<String>& lines);

inline bool isIdentifierChar(Char c)
{
    return (u_isalnum(c) || c == '_');
}
