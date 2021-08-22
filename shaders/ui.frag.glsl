#version 330 core

out vec4 fragColor;

uniform vec3 fillColor;

void main()
{
    fragColor = vec4(fillColor, 1.0f);
}

