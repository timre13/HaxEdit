#include "StatusMsg.h"
#include "globals.h"

RGBColor StatusMsg::getTypeColor()
{
    switch (m_type)
    {
    case Type::Info:  return {0.3f, 1.0f, 0.6f};
    case Type::Error: return {0.9f, 0.3f, 0.3f};
    }
}

void StatusMsg::tick(float frameTime)
{
    const float oldVal = m_secsUntilClear;
    m_secsUntilClear -= frameTime;
    if (oldVal > 0 && m_secsUntilClear <= 0)
        g_isRedrawNeeded = true;
}

void StatusMsg::set(const std::string& val, Type type)
{
    m_msg = val;
    m_type = type;
    m_secsUntilClear = STAT_MSG_ALIVE_TIME_S;
    g_isRedrawNeeded = true;
}
