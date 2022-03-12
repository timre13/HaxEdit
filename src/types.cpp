#include "types.h"
#include "common/string.h"
#include <math.h>

//-------------------------------- RGBColor ------------------------------------

std::string RGBColor::asStrFloat() const
{
    assert(r >= 0.0f && r <= 1.0f
        && g >= 0.0f && g <= 1.0f
        && b >= 0.0f && b <= 1.0f);

    return "("
        + std::to_string(r).substr(0, 5) + ", "
        + std::to_string(g).substr(0, 5) + ", "
        + std::to_string(b).substr(0, 5) + ")";
}

std::string RGBColor::asStrPrefixedFloat() const
{
    return "RGBColor" + asStrFloat();
}

std::string RGBColor::asStrHex() const
{
    assert(r >= 0.0f && r <= 1.0f
        && g >= 0.0f && g <= 1.0f
        && b >= 0.0f && b <= 1.0f);

    return "#"
        + intToHexStr(uint8_t(round(r*255)))
        + intToHexStr(uint8_t(round(g*255)))
        + intToHexStr(uint8_t(round(b*255)));
}

//-------------------------------- RGBAColor -----------------------------------

std::string RGBAColor::asStrFloat() const
{
    assert(r >= 0.0f && r <= 1.0f
        && g >= 0.0f && g <= 1.0f
        && b >= 0.0f && b <= 1.0f
        && a >= 0.0f && a <= 1.0f);

    return "("
        + std::to_string(r).substr(0, 5) + ", "
        + std::to_string(g).substr(0, 5) + ", "
        + std::to_string(b).substr(0, 5) + ", "
        + std::to_string(a).substr(0, 5) + ")";
}

std::string RGBAColor::asStrPrefixedFloat() const
{
    return "RGBAColor" + asStrFloat();
}

std::string RGBAColor::asStrHex() const
{
    assert(r >= 0.0f && r <= 1.0f
        && g >= 0.0f && g <= 1.0f
        && b >= 0.0f && b <= 1.0f
        && a >= 0.0f && a <= 1.0f);

    return "#"
        + intToHexStr(uint8_t(round(r*255)))
        + intToHexStr(uint8_t(round(g*255)))
        + intToHexStr(uint8_t(round(b*255)))
        + intToHexStr(uint8_t(round(a*255)));
}
