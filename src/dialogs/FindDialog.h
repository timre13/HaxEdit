#pragma once

#include "AskerDialog.h"
#include "../globals.h"

class FindDialog final : public AskerDialog
{
private:
    //virtual void recalculateDimensions() override;

    /*
     * Use FindDialog::create() to create a dialog and place it in the
     * global container.
     */
    FindDialog(callback_t cb, void* cbUserData);

public:
    static inline void create(callback_t cb, void* cbUserData)
    {
        g_dialogs.push_back(std::unique_ptr<FindDialog>(
                new FindDialog{cb, cbUserData}));
        g_isRedrawNeeded = true;
    }

    //virtual void render() override;
    //virtual void handleKey(int key, int mods) override;
    //virtual void handleChar(uint) override;
    //virtual bool isInsideButton(const glm::ivec2& pos) const override;
    //virtual void pressButtonAt(const glm::ivec2 pos) override;

    // TODO: Ignore case checkbox
    // TODO: Regex checkbox
};
