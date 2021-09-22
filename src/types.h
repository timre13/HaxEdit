#pragma once

#include <string>
#include <sstream>
#include <filesystem>

using uint = unsigned int;

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

inline int strCountLines(const std::string& str)
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

inline std::string getFileExt(const std::string& path)
{
    return std::filesystem::path{path}.extension().string().substr(1);
}
