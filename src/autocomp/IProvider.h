#pragma once

#include "../types.h"

namespace Autocomp
{

class Popup;

/*
 * Skeleton for the autocomplete providers.
 */
class IProvider
{
public:
    virtual void get(bufid_t bufid, Popup* popupP) = 0;

    virtual ~IProvider(){}
};

} // namespace Autocomp
