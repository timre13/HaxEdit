#include "ThemeLoader.h"
#include "Logger.h"
#include "common/file.h"
#include "common/string.h"
#include <algorithm>

#define THEME_LINE_PREF0 "/instance/org.eclipse.cdt.ui/"
#define THEME_LINE_PREF1 "/instance/org.eclipse.ui.editors/"
#define THEME_KEY_BG "AbstractTextEditor.Color.Background"
#define THEME_KEY_CURSLINE "currentLineColor"
#define THEME_KEY_SELFG "AbstractTextEditor.Color.SelectionForeground"
#define THEME_KEY_SELBG "AbstractTextEditor.Color.SelectionBackground"
#define THEME_KEY_FINDRESBG "searchResultIndicationColor"
#define THEME_KEY_LINENFG "lineNumberColor"

RGBColor calcStatLineColor(RGBColor col)
{
    if ((col.r + col.g + col.b) / 3.f > .5f) // If light color
    {
        col.r *= 0.9f;
        col.g *= 0.9f;
        col.b *= 0.9f;
    }
    else // Dark color
    {
        col.r *= 1.2f;
        col.g *= 1.2f;
        col.b *= 1.2f;
    }
    return col;
}

static int strToBool(const std::string& str)
{
    if (str == "true")
        return 1;
    else if (str == "false")
        return 0;
    else
        return -1;
}

static RGBColor strToColor(const std::string& str)
{
    const size_t colonPos1 = str.find(',');
    const size_t colonPos2 = str.find(',', colonPos1+1);

    const RGBColor color = {
        std::stoi(str.substr(0, colonPos1))/255.0f,
        std::stoi(str.substr(colonPos1+1, colonPos2-colonPos1-1))/255.0f,
        std::stoi(str.substr(colonPos2+1))/255.0f
    };

    return color;
}

Theme* ThemeLoader::load(const std::string& path)
{
    Logger::log << "Loading theme: " << path << Logger::End;

    Theme* theme = new Theme{};
    // Initialize theme with default values
    for (size_t i{}; i < theme->values.size(); ++i)
    {
        theme->values[i].color = Syntax::defColors[i];
    }

    if (!std::filesystem::is_regular_file(path))
    {
        Logger::log << "Theme file not found, using defaults" << Logger::End;
        return theme;
    }

    const std::string content = loadAsciiFile(path);
    LineIterator<std::string> it{content};

    std::string line;
    while (it.next(line))
    {
        {
            auto found0 = line.rfind(THEME_LINE_PREF0, 0);
            auto found1 = line.rfind(THEME_LINE_PREF1, 0);
            if (found0 != 0 && found1 != 0)
                continue;

            if (found0 == 0)
                line = line.substr(strlen(THEME_LINE_PREF0));
            else if (found1 == 0)
                line = line.substr(strlen(THEME_LINE_PREF1));
        }

        const size_t keyEnd = line.find('=');
        const std::string key = line.substr(0, keyEnd);
        if (key.empty())
            continue;

        if (key == THEME_KEY_BG)
        {
            RGBColor asColor = strToColor(line.substr(keyEnd+1));
            theme->bgColor = asColor;
            Logger::dbg << "BG = " << asColor.asStrPrefixedFloat() << Logger::End;
            continue;
        }
        if (key == THEME_KEY_CURSLINE)
        {
            RGBColor asColor = strToColor(line.substr(keyEnd+1));
            theme->currLineColor = RGB_COLOR_TO_RGBA(asColor);
            Logger::dbg << "CursLine = " << asColor.asStrPrefixedFloat() << Logger::End;
            continue;
        }
        if (key == THEME_KEY_SELBG)
        {
            RGBColor asColor = strToColor(line.substr(keyEnd+1));
            theme->selBg = asColor;
            Logger::dbg << "SelBG = " << asColor.asStrPrefixedFloat() << Logger::End;
            continue;
        }
        if (key == THEME_KEY_SELFG)
        {
            RGBColor asColor = strToColor(line.substr(keyEnd+1));
            theme->selFg = asColor;
            Logger::dbg << "SelFG = " << asColor.asStrPrefixedFloat() << Logger::End;
            continue;
        }
        if (key == THEME_KEY_FINDRESBG)
        {
            RGBColor asColor = strToColor(line.substr(keyEnd+1));
            theme->findResultBg = asColor;
            Logger::dbg << "FindResultBG = " << asColor.asStrPrefixedFloat() << Logger::End;
            continue;
        }
        if (key == THEME_KEY_LINENFG)
        {
            RGBColor asColor = strToColor(line.substr(keyEnd+1));
            theme->lineNColor = asColor;
            Logger::dbg << "LineNColor = " << asColor.asStrPrefixedFloat() << Logger::End;
            continue;
        }

        auto themeKeyIt = std::find(themeKeys.begin(), themeKeys.end(), key);
        if (themeKeyIt == themeKeys.end())
            continue;
        const size_t themeKeyI = themeKeyIt-themeKeys.begin();

        const std::string val = line.substr(keyEnd+1);
        Logger::dbg << quoteStr(key) << " = ";// << quoteStr(val) << Logger::End;
        const int _asBool = strToBool(val);
        if (_asBool != -1)
        {
            //const bool asBool = !!_asBool;
            //Logger::dbg << "Bool: " << (_asBool ? "true" : "false") << Logger::End;
        }
        else
        {
            RGBColor asColor = strToColor(val);
            Logger::dbg << "RGBColor(" << asColor.r << ", " << asColor.g << ", " << asColor.b << ")" << Logger::End;
            theme->values[themeKeyI].color = asColor;
        }
    }

    Logger::log << "Loaded theme" << Logger::End;
    return theme;
}
