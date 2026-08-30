#pragma once

#include "EASTL/string.h"
#include "EASTL/string_view.h"

#include "runtime/function/ui/docking/dock_layout_snapshot.h"

#include <filesystem>

namespace Blunder {

class AssetRegistry;
class DockManager;
class FileSystem;

inline constexpr const char* k_editor_session_restore_relative =
    ".blunder/editor_session_restore.yaml";

inline constexpr int k_editor_session_restore_version = 1;

struct EditorSessionRestoreRecord {
  int version{k_editor_session_restore_version};
  eastl::string last_live_guid;
  bool has_dock{false};
  DockLayoutSnapshot dock;
};

/// Windowed Editor Session Live open order (no adapters): `--scene`, then a
/// GUID that still resolves, then env startup, then the compiled default.
eastl::string resolveWindowedLiveScenePath(eastl::string_view cli_scene,
                                           eastl::string_view remembered_guid,
                                           eastl::string_view guid_resolved_path,
                                           eastl::string_view env_startup,
                                           eastl::string_view compiled_default);

bool loadEditorSessionRestore(const std::filesystem::path& path,
                              EditorSessionRestoreRecord& out);
bool saveEditorSessionRestore(const std::filesystem::path& path,
                              const EditorSessionRestoreRecord& record);

std::filesystem::path editorSessionRestorePath(const FileSystem& file_system);

bool loadProjectEditorSessionRestore(const FileSystem& file_system,
                                     EditorSessionRestoreRecord& out);

/// Merges GUID and/or dock into the on-disk record (last-write wins).
bool persistEditorSessionRestore(FileSystem& file_system,
                                 const eastl::string* last_live_guid,
                                 const DockLayoutSnapshot* dock);

bool editorSessionRestoreEnabled();

void rememberEditorSessionLiveScenePath(const eastl::string& virtual_path);
void persistEditorSessionDockLayout(DockManager& dock_manager);

eastl::string resolveGuidToScenePath(const AssetRegistry* registry,
                                     eastl::string_view guid);

}  // namespace Blunder
