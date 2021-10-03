#include "SessionHandler.h"
#include "Logger.h"
#include "Split.h"
#include "Buffer.h"
#include <sstream>
#include <fstream>
#include <string.h>

SessionHandler::SessionHandler(const std::string& path)
{
    m_sessionFilePath = path;
}

static Split* loadTabRecursively(const cJSON* node)
{
    Split* output = new Split{};
    const cJSON* children = cJSON_GetObjectItemCaseSensitive(node, "children");
    if (!children)
    {
        Logger::err << "Split without children" << Logger::End;
        return nullptr;
    }
    const cJSON* child;
    cJSON_ArrayForEach(child, children)
    {
        if (!cJSON_IsObject(child))
        {
            Logger::err << "Child of invalid type, expected Object, got something else" << Logger::End;
            return nullptr;
        }
        const cJSON* typeVal = cJSON_GetObjectItemCaseSensitive(child, "type");
        if (!typeVal)
        {
            Logger::err << "Key 'type' not found in Object" << Logger::End;
            return nullptr;
        }
        else if (!cJSON_IsString(typeVal))
        {
            Logger::err << "Invalid 'type' value, expected a string" << Logger::End;
            return nullptr;
        }

        if (strcmp(typeVal->valuestring, "split") == 0)
        {
            loadTabRecursively(child);
        }
        else if (strcmp(typeVal->valuestring, "buffer") == 0)
        {
            Buffer* buff = new Buffer;
            const cJSON* fileVal = cJSON_GetObjectItemCaseSensitive(child, "file");
            if (!fileVal)
            {
                Logger::err << "Key 'file' not found in Object" << Logger::End;
                return nullptr;
            }
            else if (!cJSON_IsString(fileVal))
            {
                Logger::err << "Invalid 'file' value, expected a string" << Logger::End;
                return nullptr;
            }
            buff->open(fileVal->valuestring);
            if (const cJSON* cursorLineVal = cJSON_GetObjectItemCaseSensitive(child, "cursorLine"))
            {
                if (!cJSON_IsNumber(cursorLineVal) || cursorLineVal->valueint < 0)
                {
                    Logger::err << "Invalid value for 'cursorLine', expected a number" << Logger::End;
                    return nullptr;
                }
                // Not very efficient but oh, well, at least it does all the checks
                for (int i{}; i < cursorLineVal->valueint; ++i)
                {
                    buff->moveCursor(Buffer::CursorMovCmd::Down);
                    buff->updateCursor();
                }
            }
            if (const cJSON* cursorColVal = cJSON_GetObjectItemCaseSensitive(child, "cursorCol"))
            {
                if (!cJSON_IsNumber(cursorColVal) || cursorColVal->valueint < 0)
                {
                    Logger::err << "Invalid value for 'cursorCol', expected a positive number" << Logger::End;
                    return nullptr;
                }
                // Not very efficient but oh, well, at least it does all the checks
                for (int i{}; i < cursorColVal->valueint; ++i)
                {
                    buff->moveCursor(Buffer::CursorMovCmd::Right);
                    buff->updateCursor();
                }
            }
            output->addChild(buff);
        }
        else
        {
            Logger::err << "Invalid value for 'key', expected 'buffer' or 'split'" << Logger::End;
            return nullptr;
        }
    }
    if (const cJSON* activeChildI = cJSON_GetObjectItemCaseSensitive(node, "activeChildI"))
    {
        if (activeChildI->valueint < 0 || (size_t)activeChildI->valueint >= output->getNumOfChildren())
        {
            Logger::err << "Invalid value for 'activeChildI'" << Logger::End;
            return nullptr;
        }
        output->setActiveChildI(activeChildI->valueint);
    }
    return output;
}

void SessionHandler::loadFromFile()
{
    g_tabs.clear();
    g_currTabI = 0;
    g_activeBuff = nullptr;

    Logger::log("SessionHandler");
    std::fstream file;
    file.open(m_sessionFilePath, std::ios::in);
    if (file.fail())
    {
        Logger::err << "Failed to load session file: " << m_sessionFilePath << Logger::End;
        Logger::log(Logger::End);
        return;
    }
    std::stringstream ss;
    std::string line;
    while (std::getline(file, line))
    {
        if (!line.empty() && line[0] != '#')
            ss << line+'\n';
    }
    const std::string str = ss.str();
    const char* cstr = str.c_str();

    cJSON* json = cJSON_Parse(cstr);
    if (!json)
    {
        Logger::err << "Failed to parse JSON, error at char " << cJSON_GetErrorPtr()-cstr << Logger::End;
        Logger::log(Logger::End);
        return;
    }
    const char* printed = cJSON_Print(json);
    Logger::dbg << "Loaded JSON:\n" << printed << Logger::End;
    cJSON_free((void*)printed);

    if (!cJSON_IsObject(json))
    {
        Logger::err << "Expected a JSON object, got something else" << Logger::End;
        goto error;
    }

    {
        const cJSON* tabs = cJSON_GetObjectItemCaseSensitive(json, "tabs");
        if (tabs)
        {
            if (!cJSON_IsArray(tabs))
            {
                Logger::err << "Expected an array inside 'tabs', got something else" << Logger::End;
                goto error;
            }
            const cJSON* tab;
            cJSON_ArrayForEach(tab, tabs)
            {
                const cJSON* tabType = cJSON_GetObjectItemCaseSensitive(tab, "type");
                if (!cJSON_IsString(tabType))
                {
                    Logger::err << "Invalid 'type' value, expected a string" << Logger::End;
                    goto error;
                }
                if (strcmp(tabType->valuestring, "buffer") == 0)
                {
                    Logger::err << "Buffer outside a split" << Logger::End;
                    goto error;
                }
                else if (strcmp(tabType->valuestring, "split") == 0)
                {
                    Split* tabObj = loadTabRecursively(tab);
                    if (!tabObj)
                        goto error;
                    g_tabs.push_back(std::unique_ptr<Split>(tabObj));
                }
                else
                {
                    Logger::err << "Invalid 'type' value, expected 'buffer' or 'split'" << Logger::End;
                    goto error;
                }
            }
        }
    }
    {
        const cJSON* activeTabVal = cJSON_GetObjectItemCaseSensitive(json, "activeTabI");
        if (activeTabVal)
        {
            if (!cJSON_IsNumber(activeTabVal))
            {
                Logger::err << "Invalid value for 'activeTabI', expected a number" << Logger::End;
                goto error;
            }
            if (activeTabVal->valueint < 0 || (size_t)activeTabVal->valueint >= g_tabs.size())
            {
                Logger::err << "Invalid value for 'activeTabI', should be 0 <= x < numOfTabs"
                    << Logger::End;
                goto error;
            }
            g_currTabI = activeTabVal->valueint;
        }
    }

    goto success;
error:
    Logger::err << "Session loading failed" << Logger::End;
success:
    cJSON_Delete(json);
    if (!g_tabs.empty())
        g_activeBuff = g_tabs[g_currTabI]->getActiveBufferRecursively();

    Logger::log("SessionHandler");
    Logger::log << "Loaded session from " << m_sessionFilePath << Logger::End;
    Logger::log(Logger::End);
}

static cJSON* storeTabRecursively(const Split* split)
{
    cJSON* output = cJSON_CreateObject();
    cJSON_AddStringToObject(output, "type", "split");
    cJSON* childrenJson = cJSON_AddArrayToObject(output, "children");
    for (const auto& child : split->getChildren())
    {
        if (std::holds_alternative<std::unique_ptr<Buffer>>(child))
        {
            Buffer* buff = std::get<std::unique_ptr<Buffer>>(child).get();
            cJSON* childJson = cJSON_CreateObject();
            cJSON_AddStringToObject(childJson, "type", "buffer");
            cJSON_AddStringToObject(childJson, "file", buff->getFilePath().c_str());
            if (buff->getCursorLine()) // Only save cursor line when not 0
                cJSON_AddNumberToObject(childJson, "cursorLine", buff->getCursorLine());
            if (buff->getCursorCol()) // Only save cursor col when not 0
                cJSON_AddNumberToObject(childJson, "cursorCol", buff->getCursorCol());
            cJSON_AddItemToArray(childrenJson, childJson);
        }
        else if (std::holds_alternative<std::unique_ptr<Split>>(child))
        {
            storeTabRecursively(std::get<std::unique_ptr<Split>>(child).get());
        }
        else
        {
            assert(false);
        }
    }
    if (split->getNumOfChildren() > 1)
        cJSON_AddNumberToObject(output, "activeChildI", split->getActiveChildI());
    return output;
}

void SessionHandler::writeToFile()
{
    Logger::log("SessionHandler");
    cJSON* json = cJSON_CreateObject();
    cJSON* tabListJson = cJSON_AddArrayToObject(json, "tabs");
    for (const auto& tab : g_tabs)
    {
        cJSON* tabJson = storeTabRecursively(tab.get());
        if (!tabJson)
        {
            Logger::err << "Failed to save tabs" << Logger::End;
            goto error;
        }
        cJSON_AddItemToArray(tabListJson, tabJson);
    }
    cJSON_AddNumberToObject(json, "activeTabI", g_currTabI);

    goto success;
error:
    Logger::err << "Session saving failed" << Logger::End;
    Logger::log(Logger::End);
    return;
success:
    const char* str = cJSON_Print(json);
    Logger::dbg << "Generated session JSON:\n" << str << Logger::End;
    std::fstream file{m_sessionFilePath, std::ios_base::out};
    if (file.fail())
    {
        Logger::err << "Failed to write session: Open failed" << Logger::End;
    }
    file.write(str, strlen(str));
    if (file.fail())
    {
        Logger::err << "Failed to write session" << Logger::End;
    }
    file.close();
    cJSON_free((void*)str);
    cJSON_Delete(json);

    Logger::log << "Wrote session to " << m_sessionFilePath << Logger::End;
    Logger::log(Logger::End);
}
