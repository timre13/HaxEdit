#pragma once

#include "IProvider.h"
#include "../types.h"
#include <vector>

namespace Autocomp
{

class DictionaryProvider final : public IProvider
{
public:
    // This is the list of words in the dictionary
    // It is shared accross all the `DictionaryProvider` instances
    static std::vector<String> s_words;

    virtual void get(Popup* popupP) override;
};

} // namespace Autocomp
