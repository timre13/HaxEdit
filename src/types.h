#pragma once

#include <string>

using uint = unsigned int;

struct RGBColor
{
    float r;
    float g;
    float b;
};
#define UNPACK_RGB_COLOR(x) x.r, x.g, x.b

struct RGBAColor
{
    float r;
    float g;
    float b;
    float a=1.0f;
};
#define UNPACK_RGBA_COLOR(x) x.r, x.g, x.b, x.a

#define RGB_COLOR_TO_RGBA(x) RGBAColor{x.r, x.g, x.b, 1.0f}
