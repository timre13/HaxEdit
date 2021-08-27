#pragma once

#include <chrono>
#include <iostream>
#include "config.h"
#include "types.h"

#if !defined(NDEBUG) && DEBUG_ENABLE_FUNC_TIMER
#   define TIMER_BEGIN_FUNC() do {\
        timer.reset(); \
        } while (0)
#   define TIMER_END_FUNC() do {\
        std::cout << "\033[35m[TIMER]\033[0m: Function \033[3m" << __PRETTY_FUNCTION__ << "\033[0m took \033[4m" << timer.getElapsedTimeMs() << "\033[0mms\n";\
        std::cout.flush();\
        } while (0)
#else
#   define TIMER_BEGIN_FUNC() do {\
        } while (0)
#   define TIMER_END_FUNC() do {\
        } while (0)
#endif

class Timer
{
private:
    using clock = std::chrono::high_resolution_clock;
    std::chrono::time_point<clock> m_startTime{};

public:
    /*
     * Restart the timer from zero.
     */
    inline void reset()
    {
        m_startTime = clock::now();
    }

    /*
     * Get elapsed time since starting the timer in milliseconds.
     */
    double getElapsedTimeMs() const
    {
        return std::chrono::duration<double, std::milli>(clock::now() - m_startTime).count();
    }
};

extern Timer timer;
