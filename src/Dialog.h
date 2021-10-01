#pragma once

#include "Logger.h"
#include <string>
#include <glm/glm.hpp>

class Dialog
{
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

    virtual void recalculateDimensions() = 0;

public:
    virtual void render() = 0;
    virtual void handleKey(int key, int mods) = 0;
    virtual void handleChar(uint codePoint) = 0;

    virtual inline bool isClosed() const final { return m_isClosed; }
    virtual inline bool isInsideButton(const glm::ivec2& pos) { (void)pos; return false; }
    virtual inline void pressButtonAt(const glm::ivec2 pos) { (void)pos; }

    virtual ~Dialog() {};
};
