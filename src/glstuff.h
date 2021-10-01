#pragma once

#include <GL/glew.h>
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <string>

namespace GlfwHelpers
{

inline std::string mouseBtnToStr(int btn)
{
    switch (btn)
    {
    case GLFW_MOUSE_BUTTON_LEFT:    return "left";
    case GLFW_MOUSE_BUTTON_RIGHT:   return "right";
    case GLFW_MOUSE_BUTTON_MIDDLE:  return "middle";
    case GLFW_MOUSE_BUTTON_4:       return "4";
    case GLFW_MOUSE_BUTTON_5:       return "5";
    case GLFW_MOUSE_BUTTON_6:       return "6";
    case GLFW_MOUSE_BUTTON_7:       return "7";
    case GLFW_MOUSE_BUTTON_8:       return "8";
    default: return "???";
    }
}

inline std::string keyOrBtnActionToStr(int act)
{
    switch (act)
    {
    case GLFW_PRESS:    return "press";
    case GLFW_RELEASE:  return "release";
    case GLFW_REPEAT:   return "repeat";
    default: return "???";
    }
}

}
