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
