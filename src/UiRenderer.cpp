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

static void configShaderBeforeDrawing(Shader& shader, int windowWidth, int windowHeight, const RGBColor& color)
{
    shader.use();
    glUniform3f(glGetUniformLocation(shader.getId(), "fillColor"), UNPACK_RGB_COLOR(color));
    const auto matrix = glm::ortho(0.0f, (float)windowWidth, (float)windowHeight, 0.0f);
    glUniformMatrix4fv(
            glGetUniformLocation(shader.getId(), "projectionMat"),
            1,
            GL_FALSE,
            glm::value_ptr(matrix));
}

static void drawVertices(uint vao, uint vbo, const float** vertexData, int vertexCount)
{
    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertexCount*sizeof(float)*2, vertexData);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glDrawArrays(GL_TRIANGLES, 0, vertexCount);

    glBindVertexArray(0);
}

void UiRenderer::renderFilledRectangle(
        const glm::ivec2& position1,
        const glm::ivec2& position2,
        const RGBColor& fillColor
    )
{
    assert(m_windowWidth > 0 && m_windowHeight > 0);

    configShaderBeforeDrawing(m_shader, m_windowWidth, m_windowHeight, fillColor);

    const float vertexData[6][2] = {
        {(float)position1.x, (float)position2.y},
        {(float)position1.x, (float)position1.y},
        {(float)position2.x, (float)position1.y},

        {(float)position1.x, (float)position2.y},
        {(float)position2.x, (float)position1.y},
        {(float)position2.x, (float)position2.y},
    };
    drawVertices(m_vao, m_vbo, (const float**)vertexData, 6);
}

void UiRenderer::renderRectangleOutline(
        const glm::ivec2& position1,
        const glm::ivec2& position2,
        const RGBColor& fillColor,
        uint borderThickness
    )
{
    assert(m_windowWidth > 0 && m_windowHeight > 0);

    configShaderBeforeDrawing(m_shader, m_windowWidth, m_windowHeight, fillColor);

    // Top
    const float rect1VertexData[6][2] = {
        {(float)position2.x,                 (float)position1.y},
        {(float)position1.x,                 (float)position1.y},
        {(float)position1.x,                 (float)position1.y+borderThickness},
        {(float)position2.x,                 (float)position1.y},
        {(float)position2.x,                 (float)position1.y+borderThickness},
        {(float)position1.x,                 (float)position1.y+borderThickness},
    };
    drawVertices(m_vao, m_vbo, (const float**)rect1VertexData, 6);

    // Right side
    const float rect2VertexData[6][2] = {
        {(float)position2.x,                 (float)position1.y},
        {(float)position2.x,                 (float)position2.y},
        {(float)position2.x-borderThickness, (float)position2.y},
        {(float)position2.x-borderThickness, (float)position2.y},
        {(float)position2.x-borderThickness, (float)position1.y},
        {(float)position2.x,                 (float)position1.y},
    };
    drawVertices(m_vao, m_vbo, (const float**)rect2VertexData, 6);

    // Bottom
    const float rect3VertexData[6][2] = {
        {(float)position1.x,                 (float)position2.y},
        {(float)position2.x,                 (float)position2.y},
        {(float)position2.x,                 (float)position2.y-borderThickness},
        {(float)position1.x,                 (float)position2.y},
        {(float)position1.x,                 (float)position2.y-borderThickness},
        {(float)position2.x,                 (float)position2.y-borderThickness},
    };
    drawVertices(m_vao, m_vbo, (const float**)rect3VertexData, 6);

    // Left side
    const float rect4VertexData[6][2] = {
        {(float)position1.x,                 (float)position2.y},
        {(float)position1.x,                 (float)position1.y},
        {(float)position1.x+borderThickness, (float)position1.y},
        {(float)position1.x+borderThickness, (float)position1.y},
        {(float)position1.x+borderThickness, (float)position2.y},
        {(float)position1.x,                 (float)position2.y},
    };
    drawVertices(m_vao, m_vbo, (const float**)rect4VertexData, 6);
}

UiRenderer::~UiRenderer()
{
    glDeleteBuffers(1, &m_vbo);
    glDeleteVertexArrays(1, &m_vao);
    Logger::dbg << "Cleaned up UI vertex data" << Logger::End;
}
