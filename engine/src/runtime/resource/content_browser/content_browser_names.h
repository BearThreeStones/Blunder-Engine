#pragma once

#include "EASTL/string.h"

namespace Blunder {

enum class BrowserNameError : int {
  ok = 0,
  empty = 1,
  illegal_character = 2,
  reserved_device = 3,
  trailing_dot = 4,
  collision = 5,
};

/// Trim leading and trailing ASCII whitespace.
eastl::string trimBrowserEntryName(const eastl::string& raw);

/// Classify a trimmed Browser entry name (folder directory name or Asset stem).
BrowserNameError classifyBrowserEntryName(const eastl::string& trimmed);

bool isLegalBrowserEntryName(const eastl::string& trimmed);

/// `New Folder`, then `New Folder_1`, `New Folder_2`, …
/// `taken(name)` is true when that sibling directory name already exists.
eastl::string uniqueNewFolderName(
    bool (*taken)(const eastl::string& name, void* user), void* user);

/// Longest typed descriptor suffix, or the last `.ext`, or empty.
bool splitBrowserFileName(const eastl::string& file_name, eastl::string& stem,
                          eastl::string& suffix);

}  // namespace Blunder
