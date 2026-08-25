#pragma once

#include "EASTL/string.h"
#include "EASTL/unique_ptr.h"
#include "EASTL/utility.h"
#include "EASTL/vector.h"

#include "runtime/core/object/animation_player.h"
#include "runtime/function/editor/document_history.h"
#include "runtime/function/scene/entity_id.h"

namespace Blunder {

class AssetCompilerService;
class AssetImportService;
class AssetRegistry;
class ContentBrowserSystem;
class EditorSceneEditSystem;
class FileSystem;
class SceneSystem;

struct BrowserDeleteSet {
  bool ok{false};
  eastl::string error;
  eastl::vector<eastl::string> selected_paths;
  eastl::vector<eastl::string> descriptor_paths;
  eastl::vector<eastl::string> folder_paths;
  eastl::vector<eastl::string> guids;
  uint32_t asset_count{0};
  bool needs_confirm{false};
  eastl::string confirm_name;
  bool contains_open_scene{false};
  eastl::string open_scene_path;
};

struct BrowserDeleteFileBlob {
  eastl::string absolute_path;
  eastl::vector<uint8_t> bytes;
};

struct BrowserDeleteLiveEntity {
  EntityId entity_id{k_invalid_entity_id};
  eastl::string mesh_path;
  eastl::vector<AnimationPlayer::ClipBinding> clip_bindings;
  bool restore_clips{false};
};

struct BrowserDeleteSnapshot {
  eastl::vector<BrowserDeleteFileBlob> files;
  eastl::vector<eastl::string> directories;
  eastl::vector<eastl::pair<eastl::string, eastl::string>> registry;
  eastl::vector<BrowserDeleteFileBlob> scene_files;
  eastl::vector<BrowserDeleteLiveEntity> live_entities;
  bool live_scene_was_dirty{false};
  eastl::string live_scene_path;
};

struct BrowserDeleteServices {
  ContentBrowserSystem* browser{nullptr};
  AssetImportService* import{nullptr};
  AssetRegistry* registry{nullptr};
  AssetCompilerService* compiler{nullptr};
  FileSystem* file_system{nullptr};
  EditorSceneEditSystem* scene_edit{nullptr};
  SceneSystem* scene_system{nullptr};
};

BrowserDeleteSet buildBrowserDeleteSet(
    const BrowserDeleteServices& services,
    const eastl::vector<eastl::string>& selected_paths);

bool snapshotAndApplyBrowserDelete(const BrowserDeleteServices& services,
                                   const BrowserDeleteSet& set,
                                   BrowserDeleteSnapshot& out_snapshot,
                                   eastl::string* out_error);

bool restoreBrowserDeleteSnapshot(const BrowserDeleteServices& services,
                                  const BrowserDeleteSnapshot& snapshot);

eastl::unique_ptr<IEditorCommand> makeDeleteBrowserEntriesCommand(
    BrowserDeleteServices services, BrowserDeleteSet set,
    BrowserDeleteSnapshot snapshot);

}  // namespace Blunder
