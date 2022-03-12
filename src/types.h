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

    inline RGBColor()
    {
        r = 0.0f;
        g = 0.0f;
        b = 0.0f;
    }

    inline constexpr RGBColor(float _r, float _g, float _b)
        : r{_r}, g{_g}, b{_b}
    {
        assert(_r >= 0.0f && _r <= 1.0f
            && _g >= 0.0f && _g <= 1.0f
            && _b >= 0.0f && _b <= 1.0f);
    }

    std::string asStrFloat() const;
    std::string asStrPrefixedFloat() const;
    std::string asStrHex() const;
};
#define UNPACK_RGB_COLOR(x) x.r, x.g, x.b

class RGBAColor
{
public:
    float r;
    float g;
    float b;
    float a=1.0f;

    inline RGBAColor()
    {
        r = 0.0f;
        g = 0.0f;
        b = 0.0f;
        a = 1.0f;
    }

    inline constexpr RGBAColor(float _r, float _g, float _b, float _a=1.0f)
        : r{_r}, g{_g}, b{_b}, a{_a}
    {
        assert(_r >= 0.0f && _r <= 1.0f
            && _g >= 0.0f && _g <= 1.0f
            && _b >= 0.0f && _b <= 1.0f
            && _a >= 0.0f && _a <= 1.0f);
    }

    std::string asStrFloat() const;
    std::string asStrPrefixedFloat() const;
    std::string asStrHex() const;
};
#define UNPACK_RGBA_COLOR(x) x.r, x.g, x.b, x.a

#define RGB_COLOR_TO_RGBA(x) RGBAColor{x.r, x.g, x.b, 1.0f}

inline RGBColor calcCompColor(RGBColor col)
{
    col.r = 1.0f-col.r;
    col.g = 1.0f-col.g;
    col.b = 1.0f-col.b;
    return col;
}

inline float limit(float min, float max, float val)
{
    if (val < min)
        return min;
    if (val > max)
        return max;
    return val;
}
