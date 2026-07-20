#include "runtime/project/project_file.h"

#include <fstream>
#include <system_error>

#include <yaml-cpp/yaml.h>

namespace Blunder {

namespace {

namespace fs = std::filesystem;

fs::path resolveRoot(const fs::path& path) {
  if (path.filename() == k_project_file_name) {
    return path.parent_path();
  }
  return path;
}

fs::path projectFilePath(const fs::path& root) {
  return root / k_project_file_name;
}

bool normalizeRoot(const fs::path& root, fs::path& out_root) {
  std::error_code ec;
  out_root = fs::weakly_canonical(root, ec);
  if (ec) {
    out_root = root;
    ec.clear();
  }
  return !out_root.empty();
}

}  // namespace

bool isProjectDirectory(const fs::path& root) {
  ProjectInfo info;
  return readProjectFile(root, info);
}

bool readProjectFile(const fs::path& path, ProjectInfo& out_info) {
  const fs::path root = resolveRoot(path);
  const fs::path file = projectFilePath(root);
  if (!fs::is_regular_file(file)) {
    return false;
  }

  try {
    const YAML::Node doc = YAML::LoadFile(file.string());
    if (!doc || !doc.IsMap()) {
      return false;
    }
    const YAML::Node name_node = doc["name"];
    if (!name_node || !name_node.IsScalar()) {
      return false;
    }
    const std::string name = name_node.as<std::string>();
    if (name.empty()) {
      return false;
    }

    fs::path normalized;
    if (!normalizeRoot(root, normalized)) {
      return false;
    }
    out_info.name = name.c_str();
    out_info.root = normalized;
    return true;
  } catch (const YAML::Exception&) {
    return false;
  }
}

bool writeProjectFile(const fs::path& root, const eastl::string& name) {
  if (name.empty()) {
    return false;
  }

  std::error_code ec;
  fs::create_directories(root, ec);
  if (ec) {
    return false;
  }

  YAML::Emitter emitter;
  emitter << YAML::BeginMap;
  emitter << YAML::Key << "name" << YAML::Value << name.c_str();
  emitter << YAML::EndMap;
  if (!emitter.good()) {
    return false;
  }

  const fs::path file = projectFilePath(root);
  std::ofstream stream(file, std::ios::trunc);
  if (!stream) {
    return false;
  }
  stream << emitter.c_str() << '\n';
  return static_cast<bool>(stream);
}

}  // namespace Blunder
