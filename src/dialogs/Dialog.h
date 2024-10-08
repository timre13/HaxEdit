#pragma once

#include "../Logger.h"
#include "../types.h"
#include "../Bindings.h"
#include <string>
#include <glm/glm.hpp>
#include <functional>

#define DIALOG_DEF_BG RGBColor{0.1f, 0.2f, 0.4f}

class Dialog
{
public:
    using callback_t = std::function<void(
            int,        // Pressed button index
            Dialog*,    // The dialog itself
            void*       // User data
            )>;
    static callback_t EMPTY_CB;

protected:
    struct Dimensions
    {
        int xPos{};
        int yPos{};
        int width{};
        int height{};
    };
    Dimensions m_dialogDims;

    bool m_isClosed = false;

    callback_t m_callback;
    void* m_cbUserData{}; // The pointer that will be passed to the callback as user data

    virtual void recalculateDimensions() = 0;

    void renderBase(const RGBColor& color=DIALOG_DEF_BG);

    Dialog(callback_t cb, void* cbUserData);

public:
    virtual void render() = 0;
    virtual void handleKey(const Bindings::BindingKey& key) = 0;

    bool isInsideBody(const glm::ivec2& pos) const;
    virtual inline bool isClosed() const final { return m_isClosed; }
    virtual inline bool isInsideButton(const glm::ivec2& pos) const { (void)pos; return false; }
    virtual inline void pressButtonAt(const glm::ivec2 pos) { (void)pos; }

    virtual ~Dialog() {};
};
