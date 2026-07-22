#include "runtime/project/project_list.h"

#include "runtime/project/project_file.h"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <system_error>

#include <yaml-cpp/yaml.h>

namespace Blunder {

namespace {

namespace fs = std::filesystem;

fs::path normalizePath(const fs::path& path) {
  std::error_code ec;
  fs::path out = fs::weakly_canonical(path, ec);
  if (ec || out.empty()) {
    out = path;
  }
  return out;
}

bool pathsEqual(const fs::path& a, const fs::path& b) {
  return normalizePath(a) == normalizePath(b);
}

std::int64_t nowUnixSeconds() {
  using clock = std::chrono::system_clock;
  return std::chrono::duration_cast<std::chrono::seconds>(
             clock::now().time_since_epoch())
      .count();
}

}  // namespace

bool ProjectList::load(const fs::path& store_path) {
  m_entries.clear();
  if (!fs::is_regular_file(store_path)) {
    return true;
  }

  try {
    const YAML::Node doc = YAML::LoadFile(store_path.string());
    if (!doc || !doc.IsMap()) {
      return false;
    }
    const YAML::Node projects = doc["projects"];
    if (!projects || !projects.IsSequence()) {
      return true;
    }
    for (const YAML::Node& node : projects) {
      if (!node || !node.IsMap()) {
        continue;
      }
      const YAML::Node path_node = node["path"];
      if (!path_node || !path_node.IsScalar()) {
        continue;
      }
      ProjectListEntry entry;
      entry.path = normalizePath(fs::path(path_node.as<std::string>()));
      if (const YAML::Node name_node = node["name"];
          name_node && name_node.IsScalar()) {
        entry.name = name_node.as<std::string>().c_str();
      }
      if (const YAML::Node opened = node["last_opened"];
          opened && opened.IsScalar()) {
        entry.last_opened_unix = opened.as<std::int64_t>();
      }
      m_entries.push_back(entry);
    }
    refreshMissing();
    return true;
  } catch (const YAML::Exception&) {
    m_entries.clear();
    return false;
  }
}

bool ProjectList::save(const fs::path& store_path) const {
  std::error_code ec;
  if (!store_path.parent_path().empty()) {
    fs::create_directories(store_path.parent_path(), ec);
    if (ec) {
      return false;
    }
  }

  YAML::Emitter emitter;
  emitter << YAML::BeginMap;
  emitter << YAML::Key << "version" << YAML::Value << 1;
  emitter << YAML::Key << "projects" << YAML::Value << YAML::BeginSeq;
  for (const ProjectListEntry& entry : m_entries) {
    emitter << YAML::BeginMap;
    emitter << YAML::Key << "path" << YAML::Value
            << entry.path.generic_string();
    emitter << YAML::Key << "name" << YAML::Value << entry.name.c_str();
    emitter << YAML::Key << "last_opened" << YAML::Value
            << entry.last_opened_unix;
    emitter << YAML::EndMap;
  }
  emitter << YAML::EndSeq;
  emitter << YAML::EndMap;
  if (!emitter.good()) {
    return false;
  }

  std::ofstream stream(store_path, std::ios::trunc);
  if (!stream) {
    return false;
  }
  stream << emitter.c_str() << '\n';
  return static_cast<bool>(stream);
}

bool ProjectList::addOrUpdate(const fs::path& project_root) {
  const fs::path normalized = normalizePath(project_root);
  ProjectInfo info;
  eastl::string name;
  if (readProjectFile(normalized, info)) {
    name = info.name;
  } else {
    name = normalized.filename().string().c_str();
    if (name.empty()) {
      name = "Project";
    }
  }

  for (ProjectListEntry& entry : m_entries) {
    if (pathsEqual(entry.path, normalized)) {
      entry.path = normalized;
      entry.name = name;
      entry.missing = !isProjectDirectory(normalized);
      return true;
    }
  }

  ProjectListEntry entry;
  entry.path = normalized;
  entry.name = name;
  entry.missing = !isProjectDirectory(normalized);
  m_entries.push_back(entry);
  return true;
}

bool ProjectList::remove(const fs::path& project_root) {
  const fs::path normalized = normalizePath(project_root);
  for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
    if (pathsEqual(it->path, normalized)) {
      m_entries.erase(it);
      return true;
    }
  }
  return false;
}

void ProjectList::refreshMissing() {
  for (ProjectListEntry& entry : m_entries) {
    entry.missing = !isProjectDirectory(entry.path);
    if (!entry.missing) {
      ProjectInfo info;
      if (readProjectFile(entry.path, info)) {
        entry.name = info.name;
      }
    }
  }
}

bool ProjectList::markOpened(const fs::path& project_root) {
  const fs::path normalized = normalizePath(project_root);
  for (ProjectListEntry& entry : m_entries) {
    if (pathsEqual(entry.path, normalized)) {
      entry.last_opened_unix = nowUnixSeconds();
      entry.missing = !isProjectDirectory(normalized);
      return true;
    }
  }
  return false;
}

fs::path defaultProjectListStorePath() {
#if defined(_WIN32)
  const char* appdata = std::getenv("APPDATA");
  if (appdata != nullptr && appdata[0] != '\0') {
    return fs::path(appdata) / "Blunder" / "project_list.yaml";
  }
  return fs::path("Blunder") / "project_list.yaml";
#else
  const char* xdg = std::getenv("XDG_CONFIG_HOME");
  if (xdg != nullptr && xdg[0] != '\0') {
    return fs::path(xdg) / "Blunder" / "project_list.yaml";
  }
  const char* home = std::getenv("HOME");
  if (home != nullptr && home[0] != '\0') {
    return fs::path(home) / ".config" / "Blunder" / "project_list.yaml";
  }
  return fs::path("Blunder") / "project_list.yaml";
#endif
}

fs::path defaultProjectsDirectory() {
#if defined(_WIN32)
  fs::path root = fs::path("E:/") / "Blunder Projects";
#else
  fs::path root;
  const char* home = std::getenv("HOME");
  if (home != nullptr && home[0] != '\0') {
    root = fs::path(home) / "Blunder Projects";
  } else {
    root = fs::path("Blunder Projects");
  }
#endif
  std::error_code ec;
  fs::create_directories(root, ec);
  return root;
}

eastl::string formatProjectLastOpenedDisplay(std::int64_t last_opened_unix) {
  if (last_opened_unix <= 0) {
    return "—";
  }

  const std::time_t when = static_cast<std::time_t>(last_opened_unix);
  std::tm local{};
#if defined(_WIN32)
  if (localtime_s(&local, &when) != 0) {
    return "—";
  }
#else
  if (localtime_r(&when, &local) == nullptr) {
    return "—";
  }
#endif

  char buffer[32]{};
  if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &local) == 0) {
    return "—";
  }
  return eastl::string(buffer);
}

}  // namespace Blunder
