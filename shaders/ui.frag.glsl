#version 330 core

out vec4 fragColor;

uniform vec4 fillColor;

void main()
{
    fragColor = fillColor;
}

