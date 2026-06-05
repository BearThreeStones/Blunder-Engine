#pragma once

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>

namespace Blunder::agent_debug {

inline void log(const char* hypothesis_id, const char* location, const char* message,
                const char* data_json) {
  const char* paths[] = {
      std::getenv("BLUNDER_DEBUG_LOG"),
      "debug-058803.log",
      "e:/Dev/Blunder-Engine/debug-058803.log",
  };
  std::ofstream out;
  for (const char* path : paths) {
    if (path == nullptr || path[0] == '\0') {
      continue;
    }
    out.open(path, std::ios::app);
    if (out) {
      break;
    }
  }
  if (!out) {
    return;
  }
  const auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::system_clock::now().time_since_epoch())
                             .count();
  out << "{\"sessionId\":\"058803\",\"hypothesisId\":\"" << hypothesis_id
      << "\",\"location\":\"" << location << "\",\"message\":\"" << message
      << "\",\"data\":" << data_json << ",\"timestamp\":" << timestamp << "}\n";
}

}  // namespace Blunder::agent_debug
