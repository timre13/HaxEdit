#include "Shader.h"
#include <fstream>
#include <sstream>
#include <assert.h>
#include "Logger.h"

static std::string loadFile(const std::string& filePath)
{
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
        return ss.str();
    }
    catch (std::exception& e)
    {
        Logger::fatal << "Failed to open file: " << filePath << e.what() << Logger::End;
        return "";
    }
}

static uint setUpShader(const std::string& filePath, bool isVert)
{
    Logger::dbg << "Loading " << (isVert ? "vertex" : "fragment") << " shader: " << filePath << Logger::End;
    auto shaderCode = loadFile(filePath);
    //Logger::dbg << "Shader code:\n" << shaderCode << Logger::End;
    const char* shaderCodeCS = shaderCode.c_str();

    uint shaderId = glCreateShader(isVert ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER);

    glShaderSource(shaderId, 1, &shaderCodeCS, NULL);
    glCompileShader(shaderId);

    int compStat;
    glGetShaderiv(shaderId, GL_COMPILE_STATUS, &compStat);
    if (!compStat)
    {
        int logSize;
        glGetShaderiv(shaderId, GL_INFO_LOG_LENGTH, &logSize);
        assert(logSize > 0 && logSize <= 1024*1024);

        auto logBuffer = new char[logSize];
        glGetShaderInfoLog(shaderId, logSize, NULL, logBuffer);
        Logger::fatal << "Failed to compile " << (isVert ? "vertex" : "fragment") << " shader: " << logBuffer << Logger::End;
        delete[] logBuffer;

        glDeleteShader(shaderId);
    }
    return shaderId;
}

static uint setUpShaderProgram(const std::string& vertShaderPath, const std::string& fragShaderPath)
{
    uint vertShaderId = setUpShader(vertShaderPath, true);
    uint fragShaderId = setUpShader(fragShaderPath, false);

    uint programId = glCreateProgram();
    glAttachShader(programId, vertShaderId);
    glAttachShader(programId, fragShaderId);
    glLinkProgram(programId);

    int linkStatus;
    glGetProgramiv(programId, GL_LINK_STATUS, &linkStatus);
    if (!linkStatus)
    {
        int logSize;
        glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &logSize);
        assert(logSize > 0 && logSize <= 1024*1024);

        auto logBuffer = new char[logSize];
        glGetProgramInfoLog(programId, logSize, NULL, logBuffer);
        Logger::fatal << "Failed to link shader program: " << logBuffer << Logger::End;
        delete[] logBuffer;

        glDeleteShader(vertShaderId);
        glDeleteShader(fragShaderId);
        glDeleteProgram(programId);
        abort();
    }

    glDeleteShader(vertShaderId);
    glDeleteShader(fragShaderId);

    return programId;
}

Shader::Shader(const std::string& vertShaderPath, const std::string& fragShaderPath)
{
    m_programId = setUpShaderProgram(vertShaderPath, fragShaderPath);
    Logger::dbg << "Created shader program" << Logger::End;
}

Shader::~Shader()
{
    glDeleteProgram(m_programId);
    Logger::dbg << "Free'd shader program" << Logger::End;
}

