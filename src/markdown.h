#pragma once

#include <string>
#define MD4C_USE_ASCII
#include "../external/md4c/src/md4c.h"

namespace Markdown
{

std::string markdownToAnsiEscaped(const std::string& input);

} // namespace Markdown
