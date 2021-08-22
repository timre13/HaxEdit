#include "UiRenderer.h"
#include "Logger.h"
#include "config.h"
#include <GL/glew.h>
#include <GL/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

UiRenderer::UiRenderer()
    : m_shader{"../shaders/ui.vert.glsl", "../shaders/ui.frag.glsl"}
{
    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float)*6*2, 0, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    Logger::dbg << "Font renderer setup done" << Logger::End;
}

void UiRenderer::renderRectangle(
        const glm::ivec2& position1,
        const glm::ivec2& position2,
        const RGBColor& fillColor/*={1.0f, 1.0f, 1.0f}*/
    )
{
    assert(m_windowWidth > 0 && m_windowHeight > 0);

    m_shader.use();
    glUniform3f(glGetUniformLocation(m_shader.getId(), "fillColor"), UNPACK_RGB_COLOR(fillColor));
    const auto matrix = glm::ortho(0.0f, (float)m_windowWidth, (float)m_windowHeight, 0.0f);
    glUniformMatrix4fv(
            glGetUniformLocation(m_shader.getId(), "projectionMat"),
            1,
            GL_FALSE,
            glm::value_ptr(matrix));

    glBindVertexArray(m_vao);

    const float vertexData[6][2] = {
        {(float)position1.x, (float)position2.y},
        {(float)position1.x, (float)position1.y},
        {(float)position2.x, (float)position1.y},

        {(float)position1.x, (float)position2.y},
        {(float)position2.x, (float)position1.y},
        {(float)position2.x, (float)position2.y},
    };

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertexData), vertexData);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

UiRenderer::~UiRenderer()
{
    glDeleteBuffers(1, &m_vbo);
    glDeleteVertexArrays(1, &m_vao);
    Logger::dbg << "Cleaned up UI vertex data" << Logger::End;
}
