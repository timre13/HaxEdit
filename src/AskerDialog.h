#pragma once

#include "Dialog.h"

class AskerDialog final : public Dialog
{
public:
    enum class Id
    {
        Generic,
        AskSaveFileName,
    };

private:
    Id m_id{Id::Generic};
    std::string m_msg;

    Dimensions m_entryRect{};
    Dimensions m_titleDims{};

    std::string m_buffer;

    virtual void recalculateDimensions() override;

public:
    AskerDialog(
            const std::string& msg,
            Id id=Id::Generic
            );

    virtual void render() override;
    virtual void handleKey(int key, int mods) override;
    virtual void handleChar(uint c) override;
    inline Id getId() const { return m_id; }
    inline const std::string& getValue() const { return m_buffer; }
    virtual ~AskerDialog() { Logger::dbg << "Destroyed an asker dialog" << Logger::End; }
};
