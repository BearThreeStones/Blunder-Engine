#pragma once

#include <filesystem>
#include <string>
#include <system_error>

namespace Blunder {

/// Consumes the argv token after `--project-root`.
///
/// Windows (and some IDEs) split an unquoted path on spaces, so
/// `E:/Blunder Projects/Test` arrives as `E:/Blunder` then `Projects/Test`.
/// While the next token is not a flag / stop token and the joined path
/// exists, keep appending so the longer real directory wins over a prefix
/// that also exists (`E:\Blunder`).
inline bool takeSpacedExistingPath(int argc, char** argv, int& i,
                                   std::filesystem::path& out,
                                   bool (*is_stop_token)(const char*)) {
  if (i + 1 >= argc || argv[i + 1] == nullptr) {
    return false;
  }
  ++i;
  std::string path = argv[i];
  while (i + 1 < argc && argv[i + 1] != nullptr) {
    const char* next = argv[i + 1];
    if (next[0] == '-' ||
        (is_stop_token != nullptr && is_stop_token(next))) {
      break;
    }
    std::error_code ec;
    const std::filesystem::path candidate{path + " " + next};
    if (!std::filesystem::exists(candidate, ec) || ec) {
      break;
    }
    path = candidate.string();
    ++i;
  }
  out = path;
  return true;
}

}  // namespace Blunder
