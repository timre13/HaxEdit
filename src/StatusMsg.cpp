#include "StatusMsg.h"

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
    m_secsUntilClear -= frameTime;
}

void StatusMsg::set(const std::string& val)
{
    m_msg = val;
    m_secsUntilClear = STAT_MSG_ALIVE_TIME_S;
}
