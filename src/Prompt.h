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
    String m_buffer{};

    void runCommand();

public:
    inline static Prompt* get()
    {
        static Prompt instance{};
        return &instance;
    }

    void render() const;
    void update(float frameTimeMs);
    void toggleWithAnim();
    void hideWithAnim();
    void showWithAnim();
    bool isOpen() const { return m_slideVal > 0.001; }

    void handleKey(int key, int mods);
    void handleChar(uint codePoint);
};
