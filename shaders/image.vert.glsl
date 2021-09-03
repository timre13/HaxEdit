#version 330 core

layout (location=0) in vec2 inVertices;
layout (location=1) in vec2 inTexCoord;

uniform mat4 projectionMat;

out vec2 texCoord;

void main()
{
    gl_Position = projectionMat * vec4(inVertices.xy, 0.0f, 1.0f);
    texCoord = inTexCoord;
}

