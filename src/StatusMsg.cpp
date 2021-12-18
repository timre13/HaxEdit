#include "StatusMsg.h"
#include "globals.h"

std::string StatusMsg::get()
{
    return isEmpty() ? "" : m_msg;
}

bool StatusMsg::isEmpty()
{
    return m_secsUntilClear <= 0;
}

void StatusMsg::tick(float frameTime)
{
    const float oldVal = m_secsUntilClear;
    m_secsUntilClear -= frameTime;
    if (oldVal > 0 && m_secsUntilClear <= 0)
        g_isRedrawNeeded = true;
}

void StatusMsg::set(const std::string& val)
{
    m_msg = val;
    m_secsUntilClear = STAT_MSG_ALIVE_TIME_S;
    g_isRedrawNeeded = true;
}
