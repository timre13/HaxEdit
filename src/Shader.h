#pragma once

#include "glstuff.h"
#include <string>
#include "types.h"

class Shader
{
private:
    uint m_programId;

public:
    Shader(const std::string& vertShaderPath, const std::string& fragShaderPath);

    inline uint getId() const { return m_programId; }
    inline void use() { glUseProgram(m_programId); }

    ~Shader();
};
