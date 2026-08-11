#pragma once

#include <cstdint>

#include "EASTL/string.h"

namespace Blunder {

/// ADR 0029: editor-user Detection Action for Asset Watch.
enum class DetectionAction : uint8_t {
  Prompt = 0,  // product default — coalesced confirm before Reimport
  Auto = 1,    // debounced Reimport without prompt
};

/// Load/save Detection Action under the user config home (e.g. %APPDATA%/Blunder/).
class EditorDetectionSettings {
 public:
  static DetectionAction load();
  static void save(DetectionAction action);
  static DetectionAction defaultAction() { return DetectionAction::Prompt; }

  /// Override path for tests; empty restores default store location.
  static void setStorePathForTest(const eastl::string& absolute_path);
};

}  // namespace Blunder
