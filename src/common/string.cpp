#include "string.h"
#include "../config.h"
#include "../Logger.h"
#include "unicode/uchar.h"
#include <cassert>
#include <stdint.h>
#include <vector>

std::u32string strToUtf32(const icu::UnicodeString& input)
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

std::string strToAscii(const String& str)
{
    std::string output;
    for (Char c : str)
        output += toascii(c);
    return output;
}

int getLongestLineLen(const std::string& str)
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

std::string strToLower(std::string str)
{
    for (char& c : str)
        c = tolower(c);
    return str;
}

String strToLower(String str)
{
    for (Char& c : str)
        c = u_tolower(c);
    return str;
}

std::string getFileExt(const std::string& path)
{
    const std::string out = std::filesystem::path{path}.extension().string();
    if (!out.empty())
        return out.substr(1);
    return "";
}

std::string getParentPath(const std::string& path)
{
    return std::filesystem::path{path}.parent_path();
}

LineIterator::LineIterator(const String& str)
    : m_strR{str}
{
}

[[nodiscard]] bool LineIterator::next(String& output)
{
    output.clear();
    if (m_charI >= m_strR.size())
        return false;

    while (m_charI < m_strR.size() && m_strR[m_charI] != '\n')
        output += m_strR[m_charI++];
    ++m_charI;
    return true;
}

std::string dateToStr(time_t date)
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
