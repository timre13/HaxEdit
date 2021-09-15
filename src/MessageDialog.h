#pragma once

#include <vector>
#include "Dialog.h"
#include "glstuff.h"

class MessageDialog final : public Dialog
{
public:
    enum class Id
    {
        Generic,
    };

    enum class Type
    {
        Information,
        Warning,
        Error,
    };

    struct BtnInfo
    {
    std::string label; // Label
    int key{}; // GLFW Keycode to press
    };

private:
    Id m_id;
    std::string m_message;
    Type m_type{Type::Information};
    std::vector<BtnInfo> m_btnInfo;
    int m_pressedBtnI{-1};

    Dimensions m_msgTextDims;
    std::vector<Dimensions> m_btnDims;
    std::vector<Dimensions> m_btnTxtDims;

    virtual void recalculateDimensions() override;

public:
    MessageDialog(
            const std::string& msg,
            Type type,
            Id id=Id::Generic,
            const std::vector<BtnInfo>& btns={{"OK", GLFW_KEY_ENTER}}
            );

    virtual void render() override;
    virtual void handleKey(int key, int mods) override;
    inline int getPressedBtnI() const { return m_pressedBtnI; }
    inline int getPressedKey() const {
        return m_pressedBtnI >= 0 ? m_btnInfo[m_pressedBtnI].key : 0; }
    inline Id getId() const { return m_id; }
};
