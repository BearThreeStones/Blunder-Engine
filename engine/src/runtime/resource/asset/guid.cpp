#include "runtime/resource/asset/guid.h"

#include <cctype>
#include <cstdio>
#include <random>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <rpc.h>
#pragma comment(lib, "rpcrt4.lib")
#endif

namespace Blunder {

bool isValidGuidFormat(const eastl::string& guid) {
  if (guid.size() != 36) {
    return false;
  }
  for (size_t i = 0; i < guid.size(); ++i) {
    const char c = guid[i];
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      if (c != '-') {
        return false;
      }
      continue;
    }
    if (!std::isxdigit(static_cast<unsigned char>(c))) {
      return false;
    }
  }
  return true;
}

eastl::string generateGuidV4() {
#if defined(_WIN32)
  UUID uuid{};
  if (UuidCreate(&uuid) == RPC_S_OK) {
    char buffer[64]{};
    std::snprintf(
        buffer, sizeof(buffer),
        "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        static_cast<unsigned>(uuid.Data1), static_cast<unsigned>(uuid.Data2),
        static_cast<unsigned>(uuid.Data3), uuid.Data4[0], uuid.Data4[1],
        uuid.Data4[2], uuid.Data4[3], uuid.Data4[4], uuid.Data4[5],
        uuid.Data4[6], uuid.Data4[7]);
    return eastl::string(buffer);
  }
#endif

  static thread_local std::mt19937 rng{std::random_device{}()};
  std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFFu);

  const uint32_t a = dist(rng);
  const uint32_t b = dist(rng);
  const uint32_t c = dist(rng);
  const uint32_t d = dist(rng);

  char buffer[64]{};
  std::snprintf(
      buffer, sizeof(buffer),
      "%08x-%04x-%04x-%04x-%012x",
      a,
      (b >> 16) & 0xFFFFu,
      ((b & 0x0FFFu) | 0x4000u),
      ((c & 0x3FFFu) | 0x8000u),
      (static_cast<uint64_t>(d) << 16) | (dist(rng) & 0xFFFFu));
  return eastl::string(buffer);
}

}  // namespace Blunder
