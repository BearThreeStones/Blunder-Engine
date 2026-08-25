#include "runtime/resource/content_browser/content_browser_names.h"

#include <cctype>
#include <cstdio>
#include <cstring>

namespace Blunder {

namespace {

bool isAsciiSpace(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

char asciiUpper(char c) {
  if (c >= 'a' && c <= 'z') {
    return static_cast<char>(c - 'a' + 'A');
  }
  return c;
}

bool isReservedWindowsDeviceName(const eastl::string& trimmed) {
  eastl::string upper;
  upper.reserve(trimmed.size());
  for (size_t i = 0; i < trimmed.size(); ++i) {
    upper.push_back(asciiUpper(trimmed[i]));
  }
  static const char* k_reserved[] = {
      "CON",  "PRN",  "AUX",  "NUL",  "COM1", "COM2", "COM3", "COM4",
      "COM5", "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3",
      "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};
  for (const char* name : k_reserved) {
    if (upper == name) {
      return true;
    }
  }
  return false;
}

bool hasIllegalCharacter(const eastl::string& trimmed) {
  for (size_t i = 0; i < trimmed.size(); ++i) {
    const char c = trimmed[i];
    if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' ||
        c == '"' || c == '<' || c == '>' || c == '|') {
      return true;
    }
  }
  return false;
}

bool endsWithSuffix(const eastl::string& value, const char* suffix) {
  const size_t suffix_length = std::strlen(suffix);
  if (value.size() < suffix_length) {
    return false;
  }
  return value.compare(value.size() - suffix_length, suffix_length, suffix) ==
         0;
}

}  // namespace

eastl::string trimBrowserEntryName(const eastl::string& raw) {
  size_t begin = 0;
  while (begin < raw.size() && isAsciiSpace(raw[begin])) {
    ++begin;
  }
  size_t end = raw.size();
  while (end > begin && isAsciiSpace(raw[end - 1])) {
    --end;
  }
  return raw.substr(begin, end - begin);
}

BrowserNameError classifyBrowserEntryName(const eastl::string& trimmed) {
  if (trimmed.empty()) {
    return BrowserNameError::empty;
  }
  if (trimmed == "." || trimmed == "..") {
    return BrowserNameError::illegal_character;
  }
  if (hasIllegalCharacter(trimmed)) {
    return BrowserNameError::illegal_character;
  }
  if (isReservedWindowsDeviceName(trimmed)) {
    return BrowserNameError::reserved_device;
  }
  if (trimmed.back() == '.') {
    return BrowserNameError::trailing_dot;
  }
  return BrowserNameError::ok;
}

bool isLegalBrowserEntryName(const eastl::string& trimmed) {
  return classifyBrowserEntryName(trimmed) == BrowserNameError::ok;
}

eastl::string uniqueNewFolderName(
    bool (*taken)(const eastl::string& name, void* user), void* user) {
  eastl::string name("New Folder");
  if (taken == nullptr || !taken(name, user)) {
    return name;
  }
  for (uint32_t index = 1; index < 10000; ++index) {
    char suffix[32];
    std::snprintf(suffix, sizeof(suffix), "New Folder_%u", index);
    name = suffix;
    if (!taken(name, user)) {
      return name;
    }
  }
  return eastl::string("New Folder_9999");
}

bool splitBrowserFileName(const eastl::string& file_name, eastl::string& stem,
                          eastl::string& suffix) {
  static const char* k_typed[] = {".animation.yaml", ".texture.yaml",
                                  ".mesh.yaml", ".mesh.asset",
                                  ".scene.asset"};
  for (const char* typed : k_typed) {
    if (endsWithSuffix(file_name, typed)) {
      suffix = typed;
      stem = file_name.substr(0, file_name.size() - suffix.size());
      return true;
    }
  }
  const size_t dot = file_name.find_last_of('.');
  if (dot == eastl::string::npos || dot == 0) {
    stem = file_name;
    suffix.clear();
    return true;
  }
  stem = file_name.substr(0, dot);
  suffix = file_name.substr(dot);
  return true;
}

}  // namespace Blunder
