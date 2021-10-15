#pragma once

#include "Logger.h"
#include <string>
#include <glm/glm.hpp>
#include <functional>

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
    int m_windowWidth{};
    int m_windowHeight{};
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

    Dialog(callback_t cb, void* cbUserData);

public:
    virtual void render() = 0;
    virtual void handleKey(int key, int mods) = 0;
    virtual void handleChar(uint codePoint) = 0;

    virtual inline bool isClosed() const final { return m_isClosed; }
    virtual inline bool isInsideButton(const glm::ivec2& pos) const { (void)pos; return false; }
    virtual inline void pressButtonAt(const glm::ivec2 pos) { (void)pos; }

    virtual ~Dialog() {};
};
