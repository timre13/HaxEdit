#pragma once

#include <vector>
#include "Dialog.h"
#include "../glstuff.h"
#include "../globals.h"

class MessageDialog final : public Dialog
{
public:
    enum class Type
    {
        Information,
        Warning,
        Error,
    };

    struct BtnInfo
    {
        std::string label; // Label
        Bindings::BindingKey key; // Key to press
    };

private:
    std::string m_message;
    Type m_type{Type::Information};
    std::vector<BtnInfo> m_btnInfo;

    Dimensions m_msgTextDims;
    std::vector<Dimensions> m_btnDims;
    std::vector<Dimensions> m_btnTxtDims;

    virtual void recalculateDimensions() override;

    /*
     * Use MessageDialog::create() to create a dialog and place it in the
     * global container.
     */
    MessageDialog(
            callback_t cb,
            void* cbUserData,
            const std::string& msg,
            Type type,
            const std::vector<BtnInfo>& btns
            );

public:
    static inline void create(
        callback_t cb,
        void* cbUserData,
        const std::string& msg,
        MessageDialog::Type type,
        const std::vector<BtnInfo>& btns={{"OK", Bindings::BindingKey{0, U"<Enter>"}}}
        )
    {
        g_dialogs.push_back(std::unique_ptr<MessageDialog>(
                    new MessageDialog{cb, cbUserData, msg, type, btns}));
        g_isRedrawNeeded = true;
    }

    virtual void render() override;
    virtual void handleKey(const Bindings::BindingKey& key) override;
    virtual bool isInsideButton(const glm::ivec2& pos) const override;
    virtual void pressButtonAt(const glm::ivec2 pos) override;

    virtual inline const std::vector<BtnInfo>& getBtns() const { return m_btnInfo; }
};
