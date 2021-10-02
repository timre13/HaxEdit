#pragma once
#include "../external/cJSON/cJSON.h"
#include <string>

class SessionHandler
{
private:
    std::string m_sessionFilePath;

public:
    SessionHandler(const std::string& path);
    void loadFromFile();
    void writeToFile();
};
