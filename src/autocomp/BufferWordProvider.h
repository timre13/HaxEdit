#pragma once

#include "../Logger.h"
#include "IProvider.h"
#include "../types.h"
#include <vector>
#include <map>

namespace Autocomp
{

class BufferWordProvider final : public IProvider
{
private:
    std::map<bufid_t, std::vector<String>> m_words;

public:
    virtual void get(bufid_t bufid, Popup* popupP) override;
    virtual void add(bufid_t bufid, const String& word);
    virtual void clear();
};

} // namespace Autocomp
