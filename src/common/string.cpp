#include "string.h"
#include "../config.h"
#include "../Logger.h"
#include <unicode/uchar.h>
#include <unicode/unistr.h>
#include <unicode/errorcode.h>
#include <cassert>
#include <stdint.h>

String icuStrToUtf32(const icu::UnicodeString& input)
{
    String output;

    assert(!input.isBogus());
    output.resize(input.countChar32());
    icu::ErrorCode errCode{};
    static_assert(sizeof(char32_t) == sizeof(UChar32));
    input.toUTF32(reinterpret_cast<UChar32*>(output.data()), output.length(), errCode);
    if (U_FAILURE(errCode))
    {
        Logger::fatal << "Failed to extract ICU string to an UTF-32 string: "
            << errCode.errorName() << Logger::End;
        return U"";
    }
    return output;
}

String utf8To32(const std::string& input)
{
    return icuStrToUtf32(icu::UnicodeString::fromUTF8(input));
}

std::string utf32To8(const String& input)
{
    std::string output;

    static_assert(sizeof(char32_t) == sizeof(UChar32));
    icu::UnicodeString ustring = icu::UnicodeString::fromUTF32(
            reinterpret_cast<const UChar32*>(input.c_str()), -1);
    assert(!ustring.isBogus());
    icu::ErrorCode errCode{};
    ustring.toUTF8String(output);
    if (U_FAILURE(errCode))
    {
        Logger::fatal << "Failed to convert a UTF-32 string to UTF-8: "
            << errCode.errorName() << Logger::End;
        return "";
    }
    return output;
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
