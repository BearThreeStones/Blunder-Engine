#pragma once

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include <filesystem>

namespace Blunder {

class AssetImportService;
class AssetRegistry;
class FileSystem;
class Scene;

struct SeWorldFlattenOptions {
  std::filesystem::path layout_gltf;
  std::filesystem::path asset_index_json;
  std::filesystem::path godot_root;
  std::filesystem::path output_scene;
  /// When set, reachable library/set/prop glTFs Import as Mesh Assets.
  AssetImportService* import_service{nullptr};
  AssetRegistry* asset_registry{nullptr};
  FileSystem* file_system{nullptr};
  eastl::string mesh_assets_folder{"assets/Meshes/se-world"};
};

struct SeWorldFlattenStats {
  bool success{false};
  eastl::string error_message;
  int grouping_entities{0};
  int layout_instances{0};
  int nested_instances{0};
  int skipped_missing_id{0};
  int skipped_missing_file{0};
  int skipped_suite{0};
  int set_geo_mesh_refs{0};
  int col_collider_entities{0};
  int imported_mesh_assets{0};
  eastl::string scene_guid;
  eastl::vector<eastl::string> skipped_ids;
};

/// Offline bake of Godot `instance_asset_id` extras into one flat Scene Asset.
/// Runtime load must not consult `asset_index.json`.
SeWorldFlattenStats bakeSeWorldFlatten(const SeWorldFlattenOptions& options);

/// Same bake into an in-memory Scene (no file write). `stats.success` still set.
SeWorldFlattenStats bakeSeWorldFlattenScene(const SeWorldFlattenOptions& options,
                                            Scene& out_scene);

}  // namespace Blunder
