#pragma once

#include "EASTL/string.h"

namespace Blunder {

/// Generates a new UUID v4 string (lowercase, hyphenated).
eastl::string generateGuidV4();

bool isValidGuidFormat(const eastl::string& guid);

}  // namespace Blunder
