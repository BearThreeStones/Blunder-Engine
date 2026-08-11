#include "runtime/project/editor_detection_settings.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include <yaml-cpp/yaml.h>

namespace Blunder {

namespace fs = std::filesystem;

namespace {

eastl::string g_store_path_override;

fs::path defaultDetectionSettingsStorePath() {
#if defined(_WIN32)
  const char* appdata = std::getenv("APPDATA");
  if (appdata != nullptr && appdata[0] != '\0') {
    return fs::path(appdata) / "Blunder" / "editor_detection.yaml";
  }
  return fs::path("Blunder") / "editor_detection.yaml";
#else
  const char* xdg = std::getenv("XDG_CONFIG_HOME");
  if (xdg != nullptr && xdg[0] != '\0') {
    return fs::path(xdg) / "Blunder" / "editor_detection.yaml";
  }
  const char* home = std::getenv("HOME");
  if (home != nullptr && home[0] != '\0') {
    return fs::path(home) / ".config" / "Blunder" / "editor_detection.yaml";
  }
  return fs::path("Blunder") / "editor_detection.yaml";
#endif
}

fs::path storePath() {
  if (!g_store_path_override.empty()) {
    return fs::path(g_store_path_override.c_str());
  }
  return defaultDetectionSettingsStorePath();
}

const char* actionToString(DetectionAction action) {
  return action == DetectionAction::Auto ? "auto" : "prompt";
}

DetectionAction actionFromString(const std::string& value) {
  if (value == "auto" || value == "Auto" || value == "AUTO") {
    return DetectionAction::Auto;
  }
  return DetectionAction::Prompt;
}

}  // namespace

void EditorDetectionSettings::setStorePathForTest(
    const eastl::string& absolute_path) {
  g_store_path_override = absolute_path;
}

DetectionAction EditorDetectionSettings::load() {
  const fs::path path = storePath();
  std::error_code ec;
  if (!fs::exists(path, ec)) {
    return defaultAction();
  }
  try {
    const YAML::Node root = YAML::LoadFile(path.string());
    if (!root || !root.IsMap()) {
      return defaultAction();
    }
    const YAML::Node action_node = root["detection_action"];
    if (!action_node || !action_node.IsScalar()) {
      return defaultAction();
    }
    return actionFromString(action_node.as<std::string>());
  } catch (...) {
    return defaultAction();
  }
}

void EditorDetectionSettings::save(DetectionAction action) {
  const fs::path path = storePath();
  std::error_code ec;
  fs::create_directories(path.parent_path(), ec);
  YAML::Emitter emitter;
  emitter << YAML::BeginMap;
  emitter << YAML::Key << "version" << YAML::Value << 1;
  emitter << YAML::Key << "detection_action" << YAML::Value
          << actionToString(action);
  emitter << YAML::EndMap;
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    return;
  }
  out << emitter.c_str();
}

}  // namespace Blunder
