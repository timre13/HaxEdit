#include "string.h"
#include "../config.h"
#include "../Logger.h"
#include "unicode/uchar.h"
#include <cassert>
#include <stdint.h>

String strToUtf32(const icu::UnicodeString& input)
{
    const int32_t len = input.countChar32()+1;
    int32_t* buff = new int32_t[len];
    UErrorCode err = U_ZERO_ERROR;
    input.toUTF32(buff, input.countChar32(), err);
    buff[len-1] = '\0';
    String output{(const Char*)buff};
    delete[] buff;
    assert(U_SUCCESS(err));
    return output;
}

String strToUtf32(const std::string& input)
{
    String output;
    for (char c : input)
        output += c;
    return output;
}

std::string strToAscii(const String& str)
{
    std::string output;
    for (Char c : str)
        output += toascii(c);
    return output;
}

size_t getLongestLineLen(const std::string& str)
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
    return maxLen;
}

size_t countLineListLen(const std::vector<String>& lines)
{
    size_t len{};
    for (const auto& line : lines)
        len += line.length();
    return len;
}

String lineVecConcat(const std::vector<String>& lines)
{
    String output;
    for (const auto& line : lines)
        output += line;
    return output;
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

std::string dateToStr(time_t date)
{
    char buff[DATE_TIME_STR_LEN+1];
    auto gm = localtime(&date);
#ifndef NDEBUG
    if (!gm)
        Logger::err << "Failed to convert time_t to tm: " << strerror(errno) << Logger::End;
#endif
    assert(gm);
    size_t count = strftime(buff, DATE_TIME_STR_LEN+1, DATE_TIME_FORMAT, gm);
    assert(count);
    (void)count;
    assert(buff[DATE_TIME_STR_LEN] == 0);
    return buff;
}
