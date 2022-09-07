#pragma once

#include "Dialog.h"
#include "../globals.h"

class AskerDialog : public Dialog
{
protected:
    std::string m_msg;

    Dimensions m_entryRect{};
    Dimensions m_titleDims{};

    String m_buffer;

    virtual void recalculateDimensions() override;

    AskerDialog(
            callback_t cb,
            void* cbUserData,
            const std::string& msg
            );

public:
    static void create(
            callback_t cb,
            void* cbUserData,
            const std::string& msg
            )
    {
        g_dialogs.push_back(std::unique_ptr<AskerDialog>(
                    new AskerDialog{cb, cbUserData, msg}));
        g_isRedrawNeeded = true;
    }

    virtual void render() override;
    virtual void handleKey(int key, int mods) override;
    virtual void handleChar(uint c) override;
    inline const String& getValue() const { return m_buffer; }
};
