#pragma once

#include "Dialog.h"
#include "../globals.h"
#include <memory>
#include <vector>

class FindListDialog final : public Dialog
{
public:
    struct ListEntry
    {
        String label;
    };
    using entryList_t = std::vector<ListEntry>;

    using typeCallback_t = std::function<void(FindListDialog* dlg, String buffer, entryList_t* outEntryList, void* userData)>;

private:
    String m_buffer;
    int m_timeUntilFetch{};
    entryList_t m_entries;
    size_t m_selectedEntryI{};
    String m_msg;
    Dimensions m_titleRect;
    Dimensions m_entryRect;

    typeCallback_t m_typeCb;
    void* m_typeCbUserData{};

    FindListDialog(
            callback_t enterCb,
            void* enterCbUserData,
            typeCallback_t typeCb,
            void* typeCbUserData,
            String message
    );

    virtual void recalculateDimensions() override;

public:
    static inline void create(
            callback_t enterCb,
            void* enterCbUserData,
            typeCallback_t typeCb,
            void* typeCbUserData,
            String message
    )
    {
        g_dialogs.push_back(std::unique_ptr<FindListDialog>(
                    new FindListDialog{
                    enterCb, enterCbUserData, typeCb, typeCbUserData, message}));
        g_isRedrawNeeded = true;
    }

    virtual void render() override;
    virtual void handleKey(int key, int mods) override;
    virtual void handleChar(uint c) override;

    virtual void tick(int elapsedMs);
};
