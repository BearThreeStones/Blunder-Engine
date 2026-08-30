#include "runtime/project/editor_session_restore.h"

#include "runtime/function/global/engine_host_mode.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/ui/docking/dock_manager.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset_registry/asset_registry.h"

#include <fstream>
#include <memory>
#include <string>
#include <system_error>

#include <yaml-cpp/yaml.h>

namespace Blunder {

namespace {

eastl::string toEastl(eastl::string_view view) {
  return eastl::string(view.data(), view.size());
}

const char* panelKindYaml(DockPanelKind kind) {
  switch (kind) {
    case DockPanelKind::viewport:
      return "viewport";
    case DockPanelKind::hierarchy:
      return "hierarchy";
    case DockPanelKind::inspector:
      return "inspector";
    case DockPanelKind::content_browser:
      return "content_browser";
    case DockPanelKind::animation:
      return "animation";
    case DockPanelKind::console:
      return "console";
    case DockPanelKind::custom:
    default:
      return "custom";
  }
}

bool parsePanelKind(const std::string& name, DockPanelKind& out) {
  if (name == "viewport") {
    out = DockPanelKind::viewport;
    return true;
  }
  if (name == "hierarchy") {
    out = DockPanelKind::hierarchy;
    return true;
  }
  if (name == "inspector") {
    out = DockPanelKind::inspector;
    return true;
  }
  if (name == "content_browser") {
    out = DockPanelKind::content_browser;
    return true;
  }
  if (name == "animation") {
    out = DockPanelKind::animation;
    return true;
  }
  if (name == "console") {
    out = DockPanelKind::console;
    return true;
  }
  if (name == "custom") {
    out = DockPanelKind::custom;
    return true;
  }
  return false;
}

const char* splitDirectionYaml(SplitDirection direction) {
  return direction == SplitDirection::vertical ? "vertical" : "horizontal";
}

bool parseSplitDirection(const std::string& name, SplitDirection& out) {
  if (name == "vertical") {
    out = SplitDirection::vertical;
    return true;
  }
  if (name == "horizontal") {
    out = SplitDirection::horizontal;
    return true;
  }
  return false;
}

const char* dockEdgeYaml(DockEdge edge) {
  switch (edge) {
    case DockEdge::right:
      return "right";
    case DockEdge::top:
      return "top";
    case DockEdge::bottom:
      return "bottom";
    case DockEdge::left:
    default:
      return "left";
  }
}

bool parseDockEdge(const std::string& name, DockEdge& out) {
  if (name == "right") {
    out = DockEdge::right;
    return true;
  }
  if (name == "top") {
    out = DockEdge::top;
    return true;
  }
  if (name == "bottom") {
    out = DockEdge::bottom;
    return true;
  }
  if (name == "left") {
    out = DockEdge::left;
    return true;
  }
  return false;
}

void emitNode(YAML::Emitter& emitter, const DockLayoutNodeSnapshot& node) {
  emitter << YAML::BeginMap;
  if (node.kind == DockNodeKind::split) {
    emitter << YAML::Key << "kind" << YAML::Value << "split";
    emitter << YAML::Key << "direction" << YAML::Value
            << splitDirectionYaml(node.split_direction);
    emitter << YAML::Key << "ratio" << YAML::Value << node.split_ratio;
    if (node.first) {
      emitter << YAML::Key << "first" << YAML::Value;
      emitNode(emitter, *node.first);
    }
    if (node.second) {
      emitter << YAML::Key << "second" << YAML::Value;
      emitNode(emitter, *node.second);
    }
  } else {
    emitter << YAML::Key << "kind" << YAML::Value << "container";
    emitter << YAML::Key << "active" << YAML::Value << node.active_index;
    emitter << YAML::Key << "widgets" << YAML::Value << YAML::BeginSeq;
    for (const DockPanelKind kind : node.widgets) {
      emitter << panelKindYaml(kind);
    }
    emitter << YAML::EndSeq;
  }
  emitter << YAML::EndMap;
}

bool parseNode(const YAML::Node& yaml, DockLayoutNodeSnapshot& out) {
  if (!yaml || !yaml.IsMap()) {
    return false;
  }
  const YAML::Node kind_node = yaml["kind"];
  if (!kind_node || !kind_node.IsScalar()) {
    return false;
  }
  const std::string kind = kind_node.as<std::string>();
  if (kind == "split") {
    out.kind = DockNodeKind::split;
    const YAML::Node direction_node = yaml["direction"];
    if (!direction_node || !direction_node.IsScalar() ||
        !parseSplitDirection(direction_node.as<std::string>(), out.split_direction)) {
      return false;
    }
    const YAML::Node ratio_node = yaml["ratio"];
    if (ratio_node && ratio_node.IsScalar()) {
      out.split_ratio = ratio_node.as<float>();
    }
    const YAML::Node first_node = yaml["first"];
    const YAML::Node second_node = yaml["second"];
    if (!first_node || !second_node) {
      return false;
    }
    out.first = std::make_shared<DockLayoutNodeSnapshot>();
    out.second = std::make_shared<DockLayoutNodeSnapshot>();
    return parseNode(first_node, *out.first) && parseNode(second_node, *out.second);
  }
  if (kind != "container") {
    return false;
  }
  out.kind = DockNodeKind::container;
  if (const YAML::Node active = yaml["active"]; active && active.IsScalar()) {
    out.active_index = active.as<int>();
  }
  const YAML::Node widgets = yaml["widgets"];
  if (widgets && widgets.IsSequence()) {
    for (const YAML::Node& widget : widgets) {
      if (!widget || !widget.IsScalar()) {
        continue;
      }
      DockPanelKind panel_kind = DockPanelKind::custom;
      if (parsePanelKind(widget.as<std::string>(), panel_kind)) {
        out.widgets.push_back(panel_kind);
      }
    }
  }
  return true;
}

bool parseDock(const YAML::Node& yaml, DockLayoutSnapshot& out) {
  if (!yaml || !yaml.IsMap()) {
    return false;
  }
  const YAML::Node root = yaml["root"];
  if (!parseNode(root, out.root)) {
    return false;
  }
  if (const YAML::Node floating = yaml["floating"]; floating && floating.IsSequence()) {
    for (const YAML::Node& node : floating) {
      if (!node || !node.IsMap()) {
        continue;
      }
      DockFloatingSnapshot entry;
      if (const YAML::Node native = node["native"]; native && native.IsScalar()) {
        entry.native = native.as<bool>();
      }
      if (const YAML::Node x = node["x"]; x && x.IsScalar()) {
        entry.rect.x = x.as<float>();
      }
      if (const YAML::Node y = node["y"]; y && y.IsScalar()) {
        entry.rect.y = y.as<float>();
      }
      if (const YAML::Node width = node["width"]; width && width.IsScalar()) {
        entry.rect.width = width.as<float>();
      }
      if (const YAML::Node height = node["height"]; height && height.IsScalar()) {
        entry.rect.height = height.as<float>();
      }
      if (!parseNode(node["content"], entry.content)) {
        return false;
      }
      out.floating.push_back(entry);
    }
  }
  if (const YAML::Node auto_hide = yaml["auto_hide"]; auto_hide && auto_hide.IsSequence()) {
    for (const YAML::Node& node : auto_hide) {
      if (!node || !node.IsMap()) {
        continue;
      }
      DockAutoHideSnapshot entry;
      const YAML::Node kind_node = node["kind"];
      if (!kind_node || !kind_node.IsScalar() ||
          !parsePanelKind(kind_node.as<std::string>(), entry.kind)) {
        continue;
      }
      if (const YAML::Node edge = node["edge"]; edge && edge.IsScalar()) {
        if (!parseDockEdge(edge.as<std::string>(), entry.edge)) {
          continue;
        }
      }
      if (const YAML::Node expanded = node["expanded"]; expanded && expanded.IsScalar()) {
        entry.expanded = expanded.as<bool>();
      }
      if (const YAML::Node span = node["span"]; span && span.IsScalar()) {
        entry.expanded_span = span.as<float>();
      }
      out.auto_hide.push_back(entry);
    }
  }
  return true;
}

void emitDock(YAML::Emitter& emitter, const DockLayoutSnapshot& dock) {
  emitter << YAML::BeginMap;
  emitter << YAML::Key << "root" << YAML::Value;
  emitNode(emitter, dock.root);
  emitter << YAML::Key << "floating" << YAML::Value << YAML::BeginSeq;
  for (const DockFloatingSnapshot& entry : dock.floating) {
    emitter << YAML::BeginMap;
    emitter << YAML::Key << "native" << YAML::Value << entry.native;
    emitter << YAML::Key << "x" << YAML::Value << entry.rect.x;
    emitter << YAML::Key << "y" << YAML::Value << entry.rect.y;
    emitter << YAML::Key << "width" << YAML::Value << entry.rect.width;
    emitter << YAML::Key << "height" << YAML::Value << entry.rect.height;
    emitter << YAML::Key << "content" << YAML::Value;
    emitNode(emitter, entry.content);
    emitter << YAML::EndMap;
  }
  emitter << YAML::EndSeq;
  emitter << YAML::Key << "auto_hide" << YAML::Value << YAML::BeginSeq;
  for (const DockAutoHideSnapshot& entry : dock.auto_hide) {
    emitter << YAML::BeginMap;
    emitter << YAML::Key << "kind" << YAML::Value << panelKindYaml(entry.kind);
    emitter << YAML::Key << "edge" << YAML::Value << dockEdgeYaml(entry.edge);
    emitter << YAML::Key << "expanded" << YAML::Value << entry.expanded;
    emitter << YAML::Key << "span" << YAML::Value << entry.expanded_span;
    emitter << YAML::EndMap;
  }
  emitter << YAML::EndSeq;
  emitter << YAML::EndMap;
}

}  // namespace

eastl::string resolveWindowedLiveScenePath(eastl::string_view cli_scene,
                                           eastl::string_view remembered_guid,
                                           eastl::string_view guid_resolved_path,
                                           eastl::string_view env_startup,
                                           eastl::string_view compiled_default) {
  if (!cli_scene.empty()) {
    return toEastl(cli_scene);
  }
  if (!remembered_guid.empty() && !guid_resolved_path.empty()) {
    return toEastl(guid_resolved_path);
  }
  if (!env_startup.empty()) {
    return toEastl(env_startup);
  }
  return toEastl(compiled_default);
}

bool loadEditorSessionRestore(const std::filesystem::path& path,
                              EditorSessionRestoreRecord& out) {
  out = EditorSessionRestoreRecord{};
  try {
    const YAML::Node doc = YAML::LoadFile(path.string());
    if (!doc || !doc.IsMap()) {
      return false;
    }
    if (const YAML::Node version = doc["version"]; version && version.IsScalar()) {
      out.version = version.as<int>();
    }
    if (const YAML::Node guid = doc["last_live_guid"]; guid && guid.IsScalar()) {
      out.last_live_guid = guid.as<std::string>().c_str();
    }
    if (const YAML::Node dock = doc["dock"]; dock) {
      if (!parseDock(dock, out.dock)) {
        out.has_dock = false;
        out.dock = DockLayoutSnapshot{};
      } else {
        out.has_dock = true;
      }
    }
    return true;
  } catch (const YAML::Exception&) {
    out = EditorSessionRestoreRecord{};
    return false;
  }
}

bool saveEditorSessionRestore(const std::filesystem::path& path,
                              const EditorSessionRestoreRecord& record) {
  std::error_code ec;
  if (!path.parent_path().empty()) {
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
      return false;
    }
  }

  YAML::Emitter emitter;
  emitter << YAML::BeginMap;
  emitter << YAML::Key << "version" << YAML::Value << record.version;
  emitter << YAML::Key << "last_live_guid" << YAML::Value << record.last_live_guid.c_str();
  if (record.has_dock) {
    emitter << YAML::Key << "dock" << YAML::Value;
    emitDock(emitter, record.dock);
  }
  emitter << YAML::EndMap;
  if (!emitter.good()) {
    return false;
  }

  std::ofstream stream(path, std::ios::trunc);
  if (!stream) {
    return false;
  }
  stream << emitter.c_str() << '\n';
  return static_cast<bool>(stream);
}

std::filesystem::path editorSessionRestorePath(const FileSystem& file_system) {
  return file_system.getProjectRoot() / ".blunder" / "editor_session_restore.yaml";
}

bool loadProjectEditorSessionRestore(const FileSystem& file_system,
                                     EditorSessionRestoreRecord& out) {
  out = EditorSessionRestoreRecord{};
  const std::filesystem::path path = editorSessionRestorePath(file_system);
  if (!file_system.isFile(path)) {
    return true;
  }
  return loadEditorSessionRestore(path, out);
}

bool persistEditorSessionRestore(FileSystem& file_system,
                                 const eastl::string* last_live_guid,
                                 const DockLayoutSnapshot* dock) {
  if (file_system.getProjectRoot().empty()) {
    return false;
  }
  EditorSessionRestoreRecord current;
  if (!loadProjectEditorSessionRestore(file_system, current)) {
    current = EditorSessionRestoreRecord{};
  }
  if (last_live_guid != nullptr && !last_live_guid->empty()) {
    current.last_live_guid = *last_live_guid;
  }
  if (dock != nullptr) {
    current.dock = *dock;
    current.has_dock = true;
  }
  current.version = k_editor_session_restore_version;
  return saveEditorSessionRestore(editorSessionRestorePath(file_system), current);
}

bool editorSessionRestoreEnabled() {
  return hostMountsEditorShell(g_runtime_global_context.hostMode(),
                               g_runtime_global_context.isHeadless());
}

void rememberEditorSessionLiveScenePath(const eastl::string& virtual_path) {
  if (!editorSessionRestoreEnabled() || virtual_path.empty()) {
    return;
  }
  FileSystem* file_system = g_runtime_global_context.m_file_system.get();
  AssetRegistry* registry = g_runtime_global_context.m_asset_registry.get();
  if (file_system == nullptr) {
    return;
  }
  eastl::string guid;
  if (registry != nullptr) {
    (void)registry->ensureSceneAssetRegistered(virtual_path);
    guid = registry->findGuidForPath(virtual_path);
  }
  if (guid.empty()) {
    return;
  }
  (void)persistEditorSessionRestore(*file_system, &guid, nullptr);
}

void persistEditorSessionDockLayout(DockManager& dock_manager) {
  if (!editorSessionRestoreEnabled()) {
    return;
  }
  FileSystem* file_system = g_runtime_global_context.m_file_system.get();
  if (file_system == nullptr) {
    return;
  }
  const DockLayoutSnapshot snapshot = dock_manager.captureLayoutSnapshot();
  (void)persistEditorSessionRestore(*file_system, nullptr, &snapshot);
}

eastl::string resolveGuidToScenePath(const AssetRegistry* registry,
                                     eastl::string_view guid) {
  if (registry == nullptr || guid.empty()) {
    return {};
  }
  return registry->resolveGuid(toEastl(guid));
}

}  // namespace Blunder
