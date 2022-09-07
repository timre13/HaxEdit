#pragma once

#include "types.h"
#include "../external/md4c/src/md4c.h"

namespace Markdown
{

String markdownToAnsiEscaped(const std::string& input);

} // namespace Markdown
