#pragma once

#include "IProvider.h"
#include <string>
#include <map>

namespace Autocomp
{

class PathProvider final : public IProvider
{
private:
    std::map<bufid_t, std::string> m_prefixes{};

public:
    virtual void get(bufid_t bufid, Popup* popupP) override;
    void setPrefix(bufid_t bufid, const std::string& prefix);
};

} // namespace Autocomp
