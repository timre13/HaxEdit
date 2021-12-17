#pragma once

#include <string>

#define STAT_MSG_ALIVE_TIME_S 3.0f

class StatusMsg final
{
private:
    std::string m_msg;
    float m_secsUntilClear{};

public:
    StatusMsg(){}

    std::string get();
    bool isEmpty();

    void tick(float frameTime);

    void set(const std::string& val);
};
