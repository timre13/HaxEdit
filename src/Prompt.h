#pragma once

#include "types.h"

class Prompt final
{
private:
    Prompt(){}

    // The prompt slides down when opening it.
    // If this is 0, the prompt is hidden.
    float m_slideVal{};
    bool m_isSlideDirDown{};

    void runCommand();

public:
    inline static Prompt* get()
    {
        static Prompt instance{};
        return &instance;
    }

    void render() const;
    void update(float frameTimeMs);
    inline void toggleWithAnim() { m_isSlideDirDown = !m_isSlideDirDown; }
    inline void hideWithAnim() { m_isSlideDirDown = false; }
    inline void showWithAnim() { m_isSlideDirDown = true; }
    bool isOpen() const { return m_slideVal > 0.001; }

    void handleKey(int key, int mods);
    void handleChar(uint codePoint);
};
