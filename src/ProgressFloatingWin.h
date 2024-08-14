#pragma once

#include "FloatingWin.h"

class ProgressFloatingWin final : public FloatingWindow
{
protected:
    // Percentage. If has no value, this progress is "infinite".
    boost::optional<uint> m_perc;
    // Used for infinite progress bar.
    uint m_tick{};

public:
    inline void setPercentage(const boost::optional<uint>& value)
    {
        assert(!value.has_value() || value.value() >= 0);
        assert(!value.has_value() || value.value() <= 100);
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

    inline void tick()
    {
        if (m_isShown && !m_perc.has_value())
        {
            ++m_tick;
            g_isRedrawNeeded = true;
        }
    }
};
