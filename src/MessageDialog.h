#pragma once

#include "Dialog.h"

class MessageDialog final : public Dialog
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

    Dimensions m_msgTextDims;
    Dimensions m_btnDims;
    Dimensions m_btnTextDims;

    virtual void recalculateDimensions() override;

public:
    MessageDialog(const std::string& msg, Type type, const std::string& btnText="OK");

    virtual void render() override;
    virtual void handleKey(int key, int mods) override;
};
