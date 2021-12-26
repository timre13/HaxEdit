#pragma once

namespace Autocomp
{

class Popup;

/*
 * Skeleton for the autocomplete providers.
 */
class IProvider
{
public:
    virtual void get(Popup* popupP) = 0;
};

} // namespace Autocomp
