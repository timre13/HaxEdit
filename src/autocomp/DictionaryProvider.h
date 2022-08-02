#pragma once

#include "IProvider.h"
#include "../types.h"
#include <vector>

namespace Autocomp
{

class DictionaryProvider final : public IProvider
{
private:
    // This is the list of words in the dictionary
    // It is used by all the buffers
    std::vector<String> m_words;

public:
    DictionaryProvider();

    virtual void get(bufid_t, Popup* popupP) override;
};

} // namespace Autocomp
