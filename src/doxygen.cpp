#include "doxygen.h"
#include "Logger.h"
#include "common/string.h"
#include <functional>
#include <string_view>
#include <array>
#include <utility>
#include <cassert>

namespace Doxygen
{

enum class CommandId
{
    Brief,
    Param,
    ParamIn,
    ParamOut,
    Errors,
    SeeAlso,
    InGroup,
    _Count,
};

static std::string commandToStr(CommandId input);

struct ConversionTableItem
{
    std::string_view command;
};

std::array<ConversionTableItem, (size_t)CommandId::_Count> conversionTable = {{
    {"brief"},
    {"param"},
    {"param[in]"},
    {"param[out]"},
    {"errors"},
    {"sa"},
    {"ingroup"},
}};

static CommandId commandStrToId(const std::string& command)
{
    for (char c : command)
        assert(!std::isspace(c));

    auto found = std::find_if(conversionTable.begin(), conversionTable.end(),
            [&](const auto& item){ return item.command == command; });
    if (found == conversionTable.end())
    {
        Logger::warn << "Doxygen: Unknown command: \"" << command << '"' << Logger::End;
        return CommandId::_Count;
    }
    return (CommandId)std::distance(conversionTable.begin(), found);
}

static std::string commandToStr(CommandId input)
{
    std::string output;

    bool resetAfter = true;
    switch (input)
    {
    case CommandId::Brief:
        output = "\n\033[3m\033[97m";
        resetAfter = false;
        break;

    case CommandId::Param:
        output = "Parameter: ";
        break;

    case CommandId::ParamIn:
        output = "Parameter (\033[32min\033[90m): ";
        break;

    case CommandId::ParamOut:
        output = "Parameter (\033[31mout\033[90m): ";
        break;

    case CommandId::Errors:
        output = "\033[31mErrors: ";
        break;

    case CommandId::SeeAlso:
        output = "See also: ";
        break;

    case CommandId::InGroup:
        output = "Group: ";
        break;

    case CommandId::_Count:
        assert(false);
        break;

    //default:
    //    //output = strCapitalize(std::string{conversionTable[(int)input].command})+": ";
    //    assert(false);
    //    break;
    }

    return "\033[90m"+output+(resetAfter ? "\033[0m" : "");
}

std::string doxygenToAnsiEscaped(const std::string& raw)
{
    std::string output;
    for (const auto& line : splitStrToLines(raw))
    {
        if (line.starts_with("\\") || line.starts_with("@")) // If the line is a command
        {
            size_t commandEnd = 1;
            while (commandEnd < line.length() && !std::isspace(line[commandEnd]))
                ++commandEnd;
            const std::string command = line.substr(1, commandEnd-1);
            const CommandId id = commandStrToId(command);
            const std::string params = (commandEnd < line.size() ? line.substr(commandEnd+1) : "");
            /*
             * If a command is in the conversion table, it needs special formatting.
             * Otherwise it just needs to be capitalized.
             */
            if (id == CommandId::_Count)
            {
                std::string commandStr = strCapitalize(command);
                std::replace(commandStr.begin(), commandStr.end(), '_', ' ');
                output += "\033[0m\033[90m" + commandStr + "\033[0m: " + params + '\n';
            }
            else
            {
                output += "\033[0m" + commandToStr(id) + params + '\n';
            }
        }
        else
        {
            output += line + '\n';
        }
    }
    return output;
}

} // namespace Doxygen
