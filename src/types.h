#pragma once

#include <cassert>
#include <string>

using uint      = unsigned int;
using uchar     = unsigned char;
using String    = std::u32string;
using Char      = char32_t;

inline size_t operator "" _st(unsigned long long val) { return val; }

class RGBColor
{
public:
    float r;
    float g;
    float b;

    RGBColor()
    {
        r = 0.0f;
        g = 0.0f;
        b = 0.0f;
    }

    constexpr RGBColor(float _r, float _g, float _b)
        : r{_r}, g{_g}, b{_b}
    {
        assert(_r >= 0.0f && _r <= 1.0f
            && _g >= 0.0f && _g <= 1.0f
            && _b >= 0.0f && _b <= 1.0f);
    }

    std::string str() const
    {
        return "RGBColor(" + std::to_string(r) + ", " + std::to_string(g) + ", " + std::to_string(b) + ")";
    }
};
#define UNPACK_RGB_COLOR(x) x.r, x.g, x.b

class RGBAColor
{
public:
    float r;
    float g;
    float b;
    float a=1.0f;

    RGBAColor()
    {
        r = 0.0f;
        g = 0.0f;
        b = 0.0f;
        a = 1.0f;
    }

    constexpr RGBAColor(float _r, float _g, float _b, float _a=1.0f)
        : r{_r}, g{_g}, b{_b}, a{_a}
    {
        assert(_r >= 0.0f && _r <= 1.0f
            && _g >= 0.0f && _g <= 1.0f
            && _b >= 0.0f && _b <= 1.0f
            && _a >= 0.0f && _a <= 1.0f);

    }
};
#define UNPACK_RGBA_COLOR(x) x.r, x.g, x.b, x.a

#define RGB_COLOR_TO_RGBA(x) RGBAColor{x.r, x.g, x.b, 1.0f}
