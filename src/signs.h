#pragma once

#include <string>
#include <memory>
#include "Image.h"


enum class Sign
{
    GitAdd,
    GitRemove,
    GitModify,
    _Count,
};

extern std::unique_ptr<Image> signImages[(int)Sign::_Count];
void loadSigns();

int getSignColumn(Sign sign);
