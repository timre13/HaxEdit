#pragma once

#include <vector>
#include "Popup.h"
#include "../types.h"

namespace Autocomp
{

class DictionaryProvider final
{
private:
    std::vector<String> m_words;

public:
    DictionaryProvider();

    void get(Popup* popupP);
};

}
