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
    const std::string out = std::filesystem::path{path}.extension().string();
    if (!out.empty())
        return out.substr(1);
    return "";
}

inline std::string getParentPath(const std::string& path)
{
    return std::filesystem::path{path}.parent_path();
}

inline bool isValidFilePath(const std::string& str)
{
    const auto path = std::filesystem::path{str};
    return std::filesystem::exists(path)
        && (std::filesystem::is_regular_file(path)
         || std::filesystem::is_symlink(path)
         || std::filesystem::is_block_file(path)
         || std::filesystem::is_character_file(path)
    );
}
