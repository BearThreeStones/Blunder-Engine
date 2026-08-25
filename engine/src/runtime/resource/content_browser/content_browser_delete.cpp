#include "runtime/resource/content_browser/content_browser_delete.h"

#include <algorithm>
#include <cstring>
#include <filesystem>

#include "EASTL/hash_set.h"
#include "EASTL/sort.h"

#include "runtime/core/base/macro.h"
#include "runtime/core/object/object.h"
#include "runtime/function/editor/editor_scene_edit_system.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_system.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/asset_descriptor.h"
#include "runtime/resource/asset/asset_yaml.h"
#include "runtime/resource/asset_cook/asset_compiler_service.h"
#include "runtime/resource/asset_import/asset_import_service.h"
#include "runtime/resource/asset_registry/asset_registry.h"
#include "runtime/resource/content/content_entry.h"
#include "runtime/resource/content_browser/content_browser_names.h"
#include "runtime/resource/content_browser/content_browser_system.h"

namespace Blunder {

namespace fs = std::filesystem;

namespace {

bool endsWithSuffix(const eastl::string& value, const char* suffix) {
  const size_t suffix_length = std::strlen(suffix);
  if (value.size() < suffix_length) {
    return false;
  }
  return value.compare(value.size() - suffix_length, suffix_length, suffix) ==
         0;
}

bool isDescriptorPath(const eastl::string& path) {
  return endsWithSuffix(path, ".mesh.yaml") ||
         endsWithSuffix(path, ".texture.yaml") ||
         endsWithSuffix(path, ".animation.yaml") ||
         endsWithSuffix(path, ".scene.asset");
}

bool isAssetsRootPath(const eastl::string& path) {
  return path == "assets" || path == "assets/" || path == "Assets" ||
         path == "Assets/";
}

eastl::string normalizeFolder(const eastl::string& path) {
  eastl::string folder = path;
  if (!folder.empty() && folder.back() != '/') {
    folder.push_back('/');
  }
  return folder;
}

eastl::string displayName(const eastl::string& path) {
  eastl::string trimmed = path;
  if (!trimmed.empty() && trimmed.back() == '/') {
    trimmed.pop_back();
  }
  const size_t slash = trimmed.find_last_of('/');
  if (slash == eastl::string::npos) {
    return trimmed;
  }
  return trimmed.substr(slash + 1);
}

bool containsString(const eastl::vector<eastl::string>& list,
                    const eastl::string& value) {
  for (const eastl::string& item : list) {
    if (item == value) {
      return true;
    }
  }
  return false;
}

void appendUnique(eastl::vector<eastl::string>& list, const eastl::string& value) {
  if (!value.empty() && !containsString(list, value)) {
    list.push_back(value);
  }
}

bool pathUnderPrefix(const eastl::string& path, const eastl::string& prefix) {
  if (prefix.empty() || path.size() < prefix.size()) {
    return false;
  }
  return path.compare(0, prefix.size(), prefix) == 0;
}

fs::path resolveAssetsVirtual(FileSystem* file_system, const eastl::string& virtual_path) {
  eastl::string relative = virtual_path;
  if (relative.compare(0, 7, "assets/") == 0) {
    relative.erase(0, 7);
  }
  if (!relative.empty() && relative.back() == '/') {
    relative.pop_back();
  }
  return file_system->resolveAsset(fs::path(relative.c_str()));
}

fs::path resolveResourcesVirtual(FileSystem* file_system,
                                 const eastl::string& virtual_path) {
  eastl::string relative = virtual_path;
  if (relative.compare(0, 10, "resources/") == 0) {
    relative.erase(0, 10);
  } else if (relative.compare(0, 10, "Resources/") == 0) {
    relative.erase(0, 10);
  }
  return file_system->resolveResource(fs::path(relative.c_str()));
}

bool snapshotFile(FileSystem* file_system, const fs::path& absolute,
                  eastl::vector<BrowserDeleteFileBlob>& out) {
  if (file_system == nullptr || !file_system->isFile(absolute)) {
    return false;
  }
  BrowserDeleteFileBlob blob;
  blob.absolute_path = absolute.generic_string().c_str();
  if (!file_system->readBinary(absolute, blob.bytes)) {
    return false;
  }
  for (const BrowserDeleteFileBlob& existing : out) {
    if (existing.absolute_path == blob.absolute_path) {
      return true;
    }
  }
  out.push_back(eastl::move(blob));
  return true;
}

void snapshotIntermediate(FileSystem* file_system, const eastl::string& virtual_path,
                          eastl::vector<BrowserDeleteFileBlob>& out) {
  if (file_system == nullptr || virtual_path.empty()) {
    return;
  }
  fs::path absolute;
  if (virtual_path.compare(0, 10, "resources/") == 0 ||
      virtual_path.compare(0, 10, "Resources/") == 0) {
    absolute = resolveResourcesVirtual(file_system, virtual_path);
  } else {
    absolute = file_system->resolve(fs::path(virtual_path.c_str()));
  }
  snapshotFile(file_system, absolute, out);
}

void snapshotDescriptorSideEffects(FileSystem* file_system,
                                   const eastl::string& descriptor_path,
                                   eastl::vector<BrowserDeleteFileBlob>& out) {
  const fs::path descriptor_abs = resolveAssetsVirtual(file_system, descriptor_path);
  snapshotFile(file_system, descriptor_abs, out);
  eastl::string yaml_text;
  if (!file_system->readText(descriptor_abs, yaml_text)) {
    return;
  }
  if (endsWithSuffix(descriptor_path, ".mesh.yaml")) {
    MeshAssetDescriptor descriptor{};
    if (AssetYaml::parseMeshDescriptor(yaml_text, descriptor)) {
      snapshotIntermediate(file_system, descriptor.source, out);
      for (const eastl::string& companion : descriptor.companion_animation_sources) {
        snapshotIntermediate(file_system, companion, out);
      }
    }
  } else if (endsWithSuffix(descriptor_path, ".texture.yaml")) {
    TextureAssetDescriptor descriptor{};
    if (AssetYaml::parseTextureDescriptor(yaml_text, descriptor)) {
      snapshotIntermediate(file_system, descriptor.source, out);
    }
  } else if (endsWithSuffix(descriptor_path, ".animation.yaml")) {
    AnimationClipAssetDescriptor descriptor{};
    if (AssetYaml::parseAnimationClipDescriptor(yaml_text, descriptor)) {
      snapshotIntermediate(file_system, descriptor.source, out);
    }
  }
}

void snapshotLiveEntities(const BrowserDeleteServices& services,
                          const eastl::vector<eastl::string>& guids,
                          BrowserDeleteSnapshot& snapshot) {
  if (services.scene_system == nullptr) {
    return;
  }
  SceneInstance* instance = services.scene_system->getActiveInstance();
  if (instance == nullptr) {
    return;
  }
  snapshot.live_scene_path = instance->getSourcePath();
  if (services.scene_edit != nullptr) {
    snapshot.live_scene_was_dirty = services.scene_edit->isDirty();
  }
  for (size_t i = 0; i < instance->getEntityCount(); ++i) {
    const EntityId id = instance->getEntityIdAtIndex(i);
    Entity* entity = instance->getEntity(id);
    if (entity == nullptr) {
      continue;
    }
    BrowserDeleteLiveEntity live;
    live.entity_id = id;
    live.mesh_path = entity->getMeshVirtualPath();
    bool keep = containsString(guids, live.mesh_path);
    if (Object* object = instance->findBoundObject(id)) {
      if (object->hasAnimationPlayer() && object->getAnimationPlayer() != nullptr) {
        live.clip_bindings = object->getAnimationPlayer()->getClipBindings();
        live.restore_clips = true;
        for (const AnimationPlayer::ClipBinding& binding : live.clip_bindings) {
          if (containsString(guids, binding.guid)) {
            keep = true;
          }
        }
      }
    }
    if (keep) {
      snapshot.live_entities.push_back(eastl::move(live));
    }
  }
}

void applyLiveDetach(const BrowserDeleteServices& services,
                     const eastl::vector<eastl::string>& guids) {
  if (services.scene_system == nullptr) {
    return;
  }
  SceneInstance* instance = services.scene_system->getActiveInstance();
  if (instance == nullptr) {
    return;
  }
  bool mutated = false;
  for (size_t i = 0; i < instance->getEntityCount(); ++i) {
    const EntityId id = instance->getEntityIdAtIndex(i);
    Entity* entity = instance->getEntity(id);
    if (entity == nullptr) {
      continue;
    }
    if (containsString(guids, entity->getMeshVirtualPath())) {
      entity->setMeshVirtualPath(eastl::string());
      mutated = true;
    }
    if (Object* object = instance->findBoundObject(id)) {
      if (object->hasAnimationPlayer() && object->getAnimationPlayer() != nullptr) {
        AnimationPlayer* player = object->getAnimationPlayer();
        eastl::vector<AnimationPlayer::ClipBinding> remaining;
        for (const AnimationPlayer::ClipBinding& binding : player->getClipBindings()) {
          if (containsString(guids, binding.guid)) {
            mutated = true;
            continue;
          }
          remaining.push_back(binding);
        }
        player->setClipBindings(remaining);
      }
    }
  }
  if (mutated && services.scene_edit != nullptr) {
    services.scene_edit->markDirty();
  }
}

void restoreLiveEntities(const BrowserDeleteServices& services,
                         const BrowserDeleteSnapshot& snapshot) {
  if (services.scene_system == nullptr) {
    return;
  }
  SceneInstance* instance = services.scene_system->getActiveInstance();
  if (instance == nullptr) {
    return;
  }
  for (const BrowserDeleteLiveEntity& live : snapshot.live_entities) {
    Entity* entity = instance->getEntity(live.entity_id);
    if (entity == nullptr) {
      continue;
    }
    entity->setMeshVirtualPath(live.mesh_path);
    if (live.restore_clips) {
      if (Object* object = instance->findBoundObject(live.entity_id)) {
        if (object->hasAnimationPlayer() && object->getAnimationPlayer() != nullptr) {
          object->getAnimationPlayer()->setClipBindings(live.clip_bindings);
        }
      }
    }
  }
  if (services.scene_edit != nullptr) {
    if (snapshot.live_scene_was_dirty) {
      services.scene_edit->markDirty();
    } else {
      services.scene_edit->clearDirty();
    }
  }
}

class DeleteBrowserEntriesCommand final : public IEditorCommand {
 public:
  BrowserDeleteServices services;
  BrowserDeleteSet set;
  BrowserDeleteSnapshot snapshot;

  void undo() override { restoreBrowserDeleteSnapshot(services, snapshot); }

  void redo() override {
    BrowserDeleteSnapshot ignored;
    eastl::string error;
    snapshotAndApplyBrowserDelete(services, set, ignored, &error);
  }

  eastl::string label() const override {
    if (set.asset_count == 0) {
      return eastl::string("Delete Folder");
    }
    if (set.folder_paths.empty() && set.asset_count == 1) {
      return eastl::string("Delete Asset");
    }
    return eastl::string("Delete");
  }
};

}  // namespace

BrowserDeleteSet buildBrowserDeleteSet(
    const BrowserDeleteServices& services,
    const eastl::vector<eastl::string>& selected_paths) {
  BrowserDeleteSet set{};
  set.selected_paths = selected_paths;
  if (services.browser == nullptr || services.registry == nullptr) {
    set.error = "services unavailable";
    return set;
  }

  for (const eastl::string& raw : selected_paths) {
    if (raw.empty()) {
      continue;
    }
    if (isAssetsRootPath(raw)) {
      set.error = "cannot delete the Assets root";
      set.contains_open_scene = false;
      return set;
    }
    const ContentEntry* entry = services.browser->findEntry(raw);
    const bool is_directory =
        (entry != nullptr && entry->is_directory) ||
        (!raw.empty() && raw.back() == '/');
    if (is_directory) {
      appendUnique(set.folder_paths, normalizeFolder(raw));
    } else if (isDescriptorPath(raw)) {
      appendUnique(set.descriptor_paths, raw);
    }
  }

  const eastl::vector<eastl::pair<eastl::string, eastl::string>> registered =
      services.registry->registeredEntries();
  for (const eastl::string& folder : set.folder_paths) {
    for (const auto& entry : registered) {
      if (pathUnderPrefix(entry.second, folder) && isDescriptorPath(entry.second)) {
        appendUnique(set.descriptor_paths, entry.second);
      }
    }
    if (services.browser != nullptr) {
      for (const ContentEntry& indexed : services.browser->indexedEntries()) {
        if (indexed.is_directory && pathUnderPrefix(indexed.virtual_path, folder)) {
          appendUnique(set.folder_paths, normalizeFolder(indexed.virtual_path));
        }
      }
    }
  }

  for (const eastl::string& descriptor : set.descriptor_paths) {
    const eastl::string guid = services.registry->findGuidForPath(descriptor);
    if (!guid.empty()) {
      appendUnique(set.guids, guid);
    }
  }

  if (services.compiler != nullptr) {
    services.compiler->rebuildDependencyGraph();
    for (const eastl::string& guid : set.guids) {
      const eastl::vector<eastl::string> dependents =
          services.compiler->dependentsOf(guid);
      for (const eastl::string& dependent : dependents) {
        if (containsString(set.guids, dependent)) {
          continue;
        }
        const eastl::string path = services.registry->resolveGuid(dependent);
        if (path.empty() || !endsWithSuffix(path, ".scene.asset")) {
          set.error = "asset has non-scene dependents; detach references first: ";
          set.error.append(path.empty() ? dependent : path);
          return set;
        }
      }
    }
  }

  set.asset_count = static_cast<uint32_t>(set.descriptor_paths.size());
  set.needs_confirm = set.asset_count >= 1;
  if (!set.folder_paths.empty()) {
    set.confirm_name = displayName(set.folder_paths.front());
  } else if (!set.descriptor_paths.empty()) {
    eastl::string stem;
    eastl::string suffix;
    splitBrowserFileName(displayName(set.descriptor_paths.front()), stem, suffix);
    set.confirm_name = stem.empty() ? displayName(set.descriptor_paths.front()) : stem;
  } else {
    set.confirm_name = "Selection";
  }
  if (set.selected_paths.size() > 1) {
    set.confirm_name = "Multiple items";
  }

  if (services.scene_edit != nullptr) {
    const eastl::string& open = services.scene_edit->activeScenePath();
    if (!open.empty() && containsString(set.descriptor_paths, open)) {
      set.contains_open_scene = true;
      set.open_scene_path = open;
    }
  }

  if (set.descriptor_paths.empty() && set.folder_paths.empty()) {
    set.error = "nothing to delete";
    return set;
  }
  set.ok = true;
  return set;
}

bool snapshotAndApplyBrowserDelete(const BrowserDeleteServices& services,
                                   const BrowserDeleteSet& set,
                                   BrowserDeleteSnapshot& out_snapshot,
                                   eastl::string* out_error) {
  const auto fail = [&](const char* message) {
    if (out_error != nullptr) {
      *out_error = message;
    }
    return false;
  };
  if (!set.ok || services.file_system == nullptr || services.import == nullptr) {
    return fail("delete set not valid");
  }

  out_snapshot = {};
  for (const eastl::string& descriptor : set.descriptor_paths) {
    snapshotDescriptorSideEffects(services.file_system, descriptor,
                                  out_snapshot.files);
    const eastl::string guid = services.registry != nullptr
                                   ? services.registry->findGuidForPath(descriptor)
                                   : eastl::string();
    if (!guid.empty()) {
      out_snapshot.registry.push_back({guid, descriptor});
    }
  }

  if (services.compiler != nullptr && services.registry != nullptr) {
    services.compiler->rebuildDependencyGraph();
    eastl::hash_set<eastl::string> scene_paths;
    for (const eastl::string& guid : set.guids) {
      for (const eastl::string& dependent : services.compiler->dependentsOf(guid)) {
        const eastl::string path = services.registry->resolveGuid(dependent);
        if (endsWithSuffix(path, ".scene.asset") &&
            scene_paths.insert(path).second) {
          snapshotFile(services.file_system, resolveAssetsVirtual(services.file_system, path),
                       out_snapshot.scene_files);
        }
      }
    }
  }

  for (const eastl::string& folder : set.folder_paths) {
    const fs::path abs = resolveAssetsVirtual(services.file_system, folder);
    out_snapshot.directories.push_back(abs.generic_string().c_str());
    if (!services.file_system->isDirectory(abs)) {
      continue;
    }
    const eastl::vector<DirectoryEntry> listing =
        services.file_system->listDirectoryRecursive(abs, abs, -1);
    for (const DirectoryEntry& entry : listing) {
      if (entry.is_directory) {
        appendUnique(out_snapshot.directories,
                     eastl::string(entry.absolute_path.generic_string().c_str()));
      } else {
        snapshotFile(services.file_system, entry.absolute_path, out_snapshot.files);
      }
    }
  }
  eastl::sort(out_snapshot.directories.begin(), out_snapshot.directories.end(),
              [](const eastl::string& a, const eastl::string& b) {
                return a.size() < b.size();
              });

  snapshotLiveEntities(services, set.guids, out_snapshot);

  if (set.contains_open_scene && services.scene_edit != nullptr) {
    services.scene_edit->closeActiveScene();
  }

  DeleteAssetOptions options;
  options.allow_active_scene = true;
  options.in_set_guids = &set.guids;
  for (const eastl::string& descriptor : set.descriptor_paths) {
    eastl::string error;
    if (!services.import->deleteAsset(descriptor, options, &error)) {
      return fail(error.empty() ? "deleteAsset failed" : error.c_str());
    }
  }

  for (const BrowserDeleteFileBlob& blob : out_snapshot.files) {
    const fs::path abs(blob.absolute_path.c_str());
    if (services.file_system->isFile(abs)) {
      services.file_system->removeFile(abs);
    }
  }

  eastl::vector<eastl::string> dirs = out_snapshot.directories;
  eastl::sort(dirs.begin(), dirs.end(),
              [](const eastl::string& a, const eastl::string& b) {
                return a.size() > b.size();
              });
  for (const eastl::string& dir : dirs) {
    services.file_system->removeEmptyDirectory(fs::path(dir.c_str()));
  }

  applyLiveDetach(services, set.guids);
  if (services.browser != nullptr) {
    services.browser->refresh();
  }
  if (out_error != nullptr) {
    out_error->clear();
  }
  return true;
}

bool restoreBrowserDeleteSnapshot(const BrowserDeleteServices& services,
                                  const BrowserDeleteSnapshot& snapshot) {
  if (services.file_system == nullptr) {
    return false;
  }
  for (const eastl::string& dir : snapshot.directories) {
    const fs::path abs(dir.c_str());
    if (!services.file_system->isDirectory(abs)) {
      services.file_system->createDirectory(abs);
    }
  }
  for (const BrowserDeleteFileBlob& blob : snapshot.files) {
    const fs::path abs(blob.absolute_path.c_str());
    services.file_system->ensureParentDirectory(abs);
    if (!blob.bytes.empty()) {
      services.file_system->writeBinary(abs, blob.bytes.data(), blob.bytes.size());
    }
  }
  for (const BrowserDeleteFileBlob& blob : snapshot.scene_files) {
    const fs::path abs(blob.absolute_path.c_str());
    services.file_system->ensureParentDirectory(abs);
    services.file_system->writeBinary(abs, blob.bytes.data(), blob.bytes.size());
  }
  if (services.registry != nullptr) {
    for (const auto& entry : snapshot.registry) {
      services.registry->registerAsset(entry.first, entry.second);
    }
    services.registry->save();
  }
  restoreLiveEntities(services, snapshot);
  if (services.compiler != nullptr) {
    services.compiler->rebuildDependencyGraph();
  }
  if (services.browser != nullptr) {
    services.browser->refresh();
  }
  return true;
}

eastl::unique_ptr<IEditorCommand> makeDeleteBrowserEntriesCommand(
    BrowserDeleteServices services, BrowserDeleteSet set,
    BrowserDeleteSnapshot snapshot) {
  auto command = eastl::make_unique<DeleteBrowserEntriesCommand>();
  command->services = services;
  command->set = eastl::move(set);
  command->snapshot = eastl::move(snapshot);
  return command;
}

}  // namespace Blunder
