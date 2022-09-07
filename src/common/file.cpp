#include "file.h"
#include "string.h"
#include "../config.h"
#include "../Logger.h"
#include "unicode/unistr.h"
#include <cstring>
#include <fstream>
#include <vector>

String loadUnicodeFile(const std::string& filePath)
{
    std::ifstream file;
    file.open(filePath, std::ios::in | std::ios::binary);
    if (file.fail())
    {
        throw std::runtime_error{std::strerror(errno)};
    }

    file.seekg(0, std::ios_base::end);
    auto fileSize = file.tellg();
    file.seekg(0);
    Logger::dbg << "Loading a Unicode file (\"" << filePath << "\") of "
        << fileSize << " bytes" << Logger::End;
    if (fileSize > MAX_FILE_SIZE)
    {
        Logger::err << "File is too large, not opening it" << Logger::End;
        return {};
    }

    std::string read;
    {
        std::stringstream ss;
        ss << file.rdbuf();
        read = ss.str();
    }
    Logger::dbg << "Read: " << read.length() << ", expected: " << fileSize << Logger::End;
    assert(read.length() == (size_t)fileSize);

    return utf8To32(read);
}

std::string loadAsciiFile(const std::string& filePath)
{
    std::ifstream file;
    file.open(filePath);
    if (file.fail())
    {
        throw std::runtime_error{std::strerror(errno)};
    }

    file.seekg(0, std::ios_base::end);
    auto fileSize = file.tellg();
    file.seekg(0);
    Logger::dbg << "Loading an ASCII file (\"" << filePath << "\") of "
        << fileSize << " bytes" << Logger::End;
    if (fileSize > MAX_FILE_SIZE)
    {
        Logger::err << "File is too large, not opening it" << Logger::End;
        return {};
    }

    std::vector<char> input;
    input.reserve(fileSize);
    input.assign(std::istreambuf_iterator<char>(file),
                 std::istreambuf_iterator<char>());

    file.close();

    return std::string(input.data(), input.size());
}

bool isReadOnlyFile(const std::string& filePath)
{
    std::fstream file;
    file.open(filePath, std::ios_base::out | std::ios_base::app);
    if (!file.is_open())
    {
        return true;
    }
    file.close();
    return false;
}
