#include "Buffer.h"
#include "Logger.h"
#include <fstream>
#include <sstream>

static size_t countLines(const std::string& str)
{
    size_t lines{};
    for (auto it=str.begin(); it != str.end(); ++it)
    {
        if (*it == '\n')
            ++lines;
    }
    return lines;
}

int Buffer::open(const std::string& filePath)
{
    m_filePath = filePath;
    Logger::dbg << "Opening file: " << filePath << Logger::End;

    try
    {
        std::fstream file;
        file.open(filePath, std::ios::in);
        if (file.fail())
        {
            throw std::runtime_error{"Open failed"};
        }
        std::stringstream ss;
        ss << file.rdbuf();

        m_content = ss.str();
        m_numOfLines = countLines(m_content);

        Logger::dbg << "Read "
            << m_content.length() << " characters ("
            << m_numOfLines << " lines) from file" << Logger::End;
        return 0;
    }
    catch (std::exception& e)
    {
        m_content.clear();
        m_numOfLines = 0;

        Logger::err << "Failed to open file: \"" << filePath << "\": " << e.what() << Logger::End;
        return 1;
    }
}
