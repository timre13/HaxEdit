#pragma once

#include "IProvider.h"
#include <string>

namespace Autocomp
{

class PathProvider final : public IProvider
{
private:
    std::string m_prefix{};

public:
    virtual void get(Popup* popupP) override;
    void setPrevix(const std::string& prefix);
};

} // namespace Autocomp
