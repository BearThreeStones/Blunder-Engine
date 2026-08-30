#pragma once

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include <cstdint>

namespace Blunder {

/// Grade on an Issue. Same three names as Console severity.
enum class IssueSeverity : uint8_t {
  log = 0,
  warning,
  error,
};

struct Issue final {
  eastl::string code;
  IssueSeverity severity{IssueSeverity::error};
  eastl::string address;
  eastl::string explanation;
};

inline constexpr const char* k_issue_play_missing_camera = "play.missing_camera";
inline constexpr const char* k_issue_scripts_dirty = "scripts.dirty";
inline constexpr const char* k_issue_scripts_missing_output =
    "scripts.missing_output";
inline constexpr const char* k_issue_scripts_build_failed =
    "scripts.build_failed";
inline constexpr const char* k_issue_play_patch_unknown_address =
    "play.patch_unknown_address";

inline constexpr const char* k_request_address_unknown = "address.unknown";
inline constexpr const char* k_request_subject_live_required =
    "subject.live_required";
inline constexpr const char* k_request_subject_no_live_document =
    "subject.no_live_document";
inline constexpr const char* k_request_subject_scene_unreadable =
    "subject.scene_unreadable";

inline const Issue* firstErrorIssue(const eastl::vector<Issue>& issues) {
  for (const Issue& issue : issues) {
    if (issue.severity == IssueSeverity::error) {
      return &issue;
    }
  }
  return nullptr;
}

inline bool issueListHasCode(const eastl::vector<Issue>& issues,
                             const char* code) {
  if (code == nullptr) {
    return false;
  }
  for (const Issue& issue : issues) {
    if (issue.code == code) {
      return true;
    }
  }
  return false;
}

}  // namespace Blunder
