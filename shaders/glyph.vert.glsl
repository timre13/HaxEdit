#version 330 core

layout (location=0) in vec4 inData;

out vec2 texCoords;

uniform mat4 projectionMat;

void main()
{
    gl_Position = projectionMat * vec4(inData.x, -inData.y, 0.0f, 1.0f);
    texCoords = inData.zw;
}

