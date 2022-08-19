#pragma once

#include "FloatingWin.h"

class ProgressFloatingWin final : public FloatingWindow
{
protected:
    uint m_perc;

public:
    inline void setPercentage(uint value)
    {
        assert(value >= 0);
        assert(value <= 100);
        m_perc = value;
    }

    virtual inline uint calcWidth() const override
    {
        return std::max(FloatingWindow::calcWidth(), 170u);
    }

    virtual inline uint calcHeight() const override
    {
        return std::max(FloatingWindow::calcHeight() + 10, 80u);
    }

    virtual void render() const override;
};
