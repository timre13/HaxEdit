#pragma once

#include <vector>
#include "../types.h"

namespace Autocomp
{

class Popup;

class DictionaryProvider final
{
private:
    std::vector<String> m_words;

public:
    DictionaryProvider();

    void get(Popup* popupP);
};

}
