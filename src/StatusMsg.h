#pragma once

#include <string>
#include "types.h"

#define STAT_MSG_ALIVE_TIME_S 3.0f

class StatusMsg final
{
public:
    enum class Type
    {
        Info,
        Error,
    };

private:
    std::string m_msg;
    Type m_type{};
    float m_secsUntilClear{};

public:
    StatusMsg(){}

    inline std::string get() const { return isEmpty() ? "" : m_msg; }
    inline Type getType() const { return m_type; }
    RGBColor getTypeColor();
    inline bool isEmpty() const { return m_secsUntilClear <= 0; }

    void tick(float frameTime);

    void set(const std::string& val, Type type);
};
