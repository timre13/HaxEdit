#pragma once

#include "IProvider.h"
#include "../types.h"
#include <vector>

namespace Autocomp
{

class BufferWordProvider final : public IProvider
{
private:
    std::vector<String> m_words;

public:
    virtual void get(Popup* popupP) override;
    virtual void add(const String& word);
    virtual void clear();
};

} // namespace Autocomp
