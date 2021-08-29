#pragma once

#include <string>
#include <glm/glm.hpp>

class Dialog final
{
public:
    enum class Type
    {
        Information,
        Warning,
        Error,
    };

private:
    std::string m_message;
    Type m_type{Type::Information};
    std::string m_buttonText;

    int m_windowWidth{};
    int m_windowHeight{};
    struct Dimensions
    {
        int xPos;
        int yPos;
        int width;
        int height;
    };
    Dimensions m_dialogDims;
    Dimensions m_msgTextDims;
    Dimensions m_btnDims;
    Dimensions m_btnTextDims;

    void recalculateDimensions();

public:
    Dialog(const std::string& msg, Type type, const std::string& btnText="OK");

    void render();
};
