#include "common.h"
#include "config.h"
#include "Logger.h"
#include <fstream>
#include <vector>
#include <stdint.h>
#include <cstring>
#include <string>

String loadUnicodeFile(const std::string& filePath)
{
    std::ifstream file;
    file.open(filePath, std::ios::in | std::ios::binary);
    if (file.fail())
    {
        throw std::runtime_error{std::strerror(errno)};
    }

    auto fileSize = file.tellg();
    Logger::dbg << "Loading a Unicode file (\"" << filePath << "\") of " << fileSize
        << '/' << std::hex << fileSize << std::dec << " bytes" << Logger::End;
    if (fileSize > MAX_FILE_SIZE)
    {
        Logger::err << "File is too large, not opening it" << Logger::End;
        return {};
    }

    std::vector<uint8_t> input;
    file.seekg(0, std::ios::end);
    input.reserve(fileSize);
    file.seekg(0, std::ios::beg);
    input.assign(std::istreambuf_iterator<char>(file),
                 std::istreambuf_iterator<char>());

    file.close();

    const auto icuString = icu::UnicodeString{(const char*)input.data(), (int32_t)input.size()};
    if (icuString.isBogus())
    {
        throw InvalidUnicodeError{};
    }
    return strToUtf32(icuString);
}

std::string loadAsciiFile(const std::string& filePath)
{
    std::ifstream file;
    file.open(filePath);
    if (file.fail())
    {
        throw std::runtime_error{std::strerror(errno)};
    }

    auto fileSize = file.tellg();
    Logger::dbg << "Loading an ASCII file (\"" << filePath << "\") of " << fileSize
        << '/' << std::hex << fileSize << std::dec << " bytes" << Logger::End;
    if (fileSize > MAX_FILE_SIZE)
    {
        Logger::err << "File is too large, not opening it" << Logger::End;
        return {};
    }

    std::vector<char> input;
    file.seekg(0, std::ios::end);
    input.reserve(fileSize);
    file.seekg(0, std::ios::beg);
    input.assign(std::istreambuf_iterator<char>(file),
                 std::istreambuf_iterator<char>());

    file.close();

    return std::string(input.data(), input.size());
}
