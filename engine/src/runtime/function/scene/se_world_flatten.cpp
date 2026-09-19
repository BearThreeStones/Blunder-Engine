#include "runtime/function/scene/se_world_flatten.h"
#include "runtime/project/editor_session_restore.h"

#include <cgltf.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "EASTL/hash_map.h"
#include "EASTL/hash_set.h"
#include "EASTL/vector.h"

#include "runtime/core/base/macro.h"
#include "runtime/core/math/coordinate_system.h"
#include "runtime/function/scene/gltf_node_extras.h"
#include "runtime/function/scene/scene.h"
#include "runtime/function/scene/scene_serializer.h"
#include "runtime/function/scene/scene_starter.h"
#include "runtime/function/scene/skeleton_from_gltf.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/guid.h"
#include "runtime/resource/asset_import/asset_import_service.h"
#include "runtime/resource/asset_registry/asset_registry.h"

namespace Blunder {
namespace {

namespace fs = std::filesystem;

enum class FlattenKind { root, set, library };

struct AssetIndexEntry {
  eastl::string name;
  eastl::string filepath;
};

eastl::string toLowerCopy(eastl::string value) {
  for (size_t i = 0; i < value.size(); ++i) {
    const char c = value[i];
    if (c >= 'A' && c <= 'Z') {
      value[i] = static_cast<char>(c - 'A' + 'a');
    }
  }
  return value;
}

eastl::string genericSlash(eastl::string value) {
  for (size_t i = 0; i < value.size(); ++i) {
    if (value[i] == '\\') {
      value[i] = '/';
    }
  }
  return value;
}

bool shouldSkipSuite(const eastl::string& filepath) {
  const eastl::string lower = toLowerCopy(genericSlash(filepath));
  return lower.find("se-asset_ikea") != eastl::string::npos ||
         lower.find("se-asset_zoo") != eastl::string::npos ||
         lower.find("vertical_slice") != eastl::string::npos ||
         lower.find("assets/char/") != eastl::string::npos;
}

bool isSetFilepath(const eastl::string& filepath) {
  const eastl::string lower = toLowerCopy(genericSlash(filepath));
  return lower.find("assets/sets/hub/") != eastl::string::npos ||
         lower.find("assets/sets/fence/") != eastl::string::npos ||
         lower.find("assets/sets/clearing/") != eastl::string::npos ||
         lower.find("assets/sets/world/") != eastl::string::npos;
}

bool isLibraryOrPropFilepath(const eastl::string& filepath) {
  const eastl::string lower = toLowerCopy(genericSlash(filepath));
  return lower.find("assets/lib/") != eastl::string::npos ||
         lower.find("assets/props/") != eastl::string::npos;
}

eastl::string jsonStringAfterKey(const char* object_begin, const char* object_end,
                                 const char* key) {
  eastl::string out;
  if (object_begin == nullptr || object_end == nullptr || key == nullptr ||
      object_begin >= object_end) {
    return out;
  }
  const size_t key_len = std::strlen(key);
  for (const char* p = object_begin; p + key_len < object_end; ++p) {
    if (std::strncmp(p, key, key_len) != 0) {
      continue;
    }
    const char* colon = std::strchr(p + key_len, ':');
    if (colon == nullptr || colon >= object_end) {
      continue;
    }
    const char* quote = std::strchr(colon, '"');
    if (quote == nullptr || quote >= object_end) {
      continue;
    }
    const char* end = std::strchr(quote + 1, '"');
    if (end == nullptr || end > object_end) {
      continue;
    }
    out.assign(quote + 1, static_cast<eastl::string::size_type>(end - quote - 1));
    return out;
  }
  return out;
}

bool parseAssetIndexJson(const eastl::string& json,
                         eastl::hash_map<eastl::string, AssetIndexEntry>& out) {
  out.clear();
  const char* p = json.c_str();
  const char* end = p + json.size();
  while (p < end) {
    const char* filepath_key = std::strstr(p, "\"filepath\"");
    if (filepath_key == nullptr || filepath_key >= end) {
      break;
    }
    const char* object_start = filepath_key;
    while (object_start > json.c_str() && *object_start != '{') {
      --object_start;
    }
    if (*object_start != '{') {
      p = filepath_key + 10;
      continue;
    }
    const char* object_end = std::strchr(filepath_key, '}');
    if (object_end == nullptr || object_end > end) {
      break;
    }
    AssetIndexEntry entry;
    entry.filepath = jsonStringAfterKey(object_start, object_end, "\"filepath\"");
    entry.name = jsonStringAfterKey(object_start, object_end, "\"name\"");

    eastl::string id;
    const char* cursor = object_start;
    while (cursor > json.c_str() &&
           std::isspace(static_cast<unsigned char>(*(cursor - 1)))) {
      --cursor;
    }
    if (cursor > json.c_str() && *(cursor - 1) == ':') {
      --cursor;
      while (cursor > json.c_str() &&
             std::isspace(static_cast<unsigned char>(*(cursor - 1)))) {
        --cursor;
      }
      if (cursor > json.c_str() && *(cursor - 1) == '"') {
        const char* id_end = cursor - 1;
        const char* id_start = id_end;
        while (id_start > json.c_str() && *(id_start - 1) != '"') {
          --id_start;
        }
        id.assign(id_start, static_cast<eastl::string::size_type>(id_end - id_start));
      }
    }
    if (!id.empty() && !entry.filepath.empty()) {
      out[id] = entry;
    }
    p = object_end + 1;
  }
  return !out.empty();
}

bool readTextFile(const fs::path& path, eastl::string& out) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    return false;
  }
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  const std::string text = buffer.str();
  out.assign(text.c_str(), static_cast<eastl::string::size_type>(text.size()));
  return true;
}

bool writeTextFile(const fs::path& path, const eastl::string& text,
                   FileSystem* file_system) {
  if (file_system != nullptr) {
    file_system->ensureParentDirectory(path);
    return file_system->writeText(path, text);
  }
  std::error_code ec;
  fs::create_directories(path.parent_path(), ec);
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    return false;
  }
  stream.write(text.c_str(), static_cast<std::streamsize>(text.size()));
  return static_cast<bool>(stream);
}

bool pathEscapesRoot(const fs::path& relative) {
  for (const auto& part : relative.lexically_normal()) {
    if (part == ".") {
      continue;
    }
    return part == "..";
  }
  return false;
}

bool pathIsUnder(const fs::path& path, const fs::path& root) {
  if (root.empty()) {
    return true;
  }
  std::error_code ec;
  const fs::path rel =
      fs::relative(path.lexically_normal(), root.lexically_normal(), ec);
  return !ec && !pathEscapesRoot(rel);
}

bool copyFileIfMissing(const fs::path& src, const fs::path& dst) {
  std::error_code ec;
  if (!fs::is_regular_file(src, ec)) {
    return false;
  }
  if (fs::exists(dst, ec)) {
    return true;
  }
  fs::create_directories(dst.parent_path(), ec);
  if (ec) {
    return false;
  }
  fs::copy_file(src, dst, ec);
  return !ec;
}

void bakeNodeLocal(const cgltf_node* node, Vec3& out_position, Quat& out_rotation,
                   Vec3& out_scale) {
  out_position = Vec3(0.0f);
  out_rotation = glm::identity<Quat>();
  out_scale = Vec3(1.0f);
  if (node == nullptr) {
    return;
  }
  if (node->has_matrix) {
    Mat4 gltf_matrix(1.0f);
    std::memcpy(glm::value_ptr(gltf_matrix), node->matrix, sizeof(cgltf_float) * 16);
    const Mat4 local = similarityGltfToEngine(gltf_matrix);
    out_position = Vec3(local[3]);
    Vec3 basis_x(local[0]);
    Vec3 basis_y(local[1]);
    Vec3 basis_z(local[2]);
    out_scale = Vec3(glm::length(basis_x), glm::length(basis_y), glm::length(basis_z));
    constexpr float k_epsilon = 1e-8f;
    if (out_scale.x > k_epsilon) {
      basis_x /= out_scale.x;
    }
    if (out_scale.y > k_epsilon) {
      basis_y /= out_scale.y;
    }
    if (out_scale.z > k_epsilon) {
      basis_z /= out_scale.z;
    }
    Mat3 rotation_matrix(basis_x, basis_y, basis_z);
    if (glm::determinant(rotation_matrix) < 0.0f) {
      out_scale.z = -out_scale.z;
      basis_z = -basis_z;
      rotation_matrix = Mat3(basis_x, basis_y, basis_z);
    }
    out_rotation = glm::normalize(glm::quat_cast(rotation_matrix));
    return;
  }
  if (node->has_translation) {
    out_position = transformPointGltfToEngine(
        Vec3(node->translation[0], node->translation[1], node->translation[2]));
  }
  if (node->has_rotation) {
    const Quat gltf_rotation(node->rotation[3], node->rotation[0], node->rotation[1],
                             node->rotation[2]);
    out_rotation = transformRotationGltfToEngine(gltf_rotation);
  }
  if (node->has_scale) {
    out_scale = transformScaleGltfToEngine(
        Vec3(node->scale[0], node->scale[1], node->scale[2]));
  }
}

bool nodeHasDrawableMesh(const cgltf_node* node) {
  if (node == nullptr || node->mesh == nullptr) {
    return false;
  }
  if (gltfNodeNameStartsWith(node, "COL-")) {
    return false;
  }
  eastl::string instance_id;
  if (gltfNodeInstanceAssetId(node, instance_id)) {
    return false;
  }
  return true;
}

bool gltfHasDrawableMesh(const cgltf_data* data) {
  if (data == nullptr) {
    return false;
  }
  for (cgltf_size i = 0; i < data->nodes_count; ++i) {
    if (nodeHasDrawableMesh(&data->nodes[i])) {
      return true;
    }
  }
  return false;
}

struct FlattenBaker {
  const SeWorldFlattenOptions* options{nullptr};
  SeWorldFlattenStats* stats{nullptr};
  Scene* scene{nullptr};
  eastl::hash_map<eastl::string, AssetIndexEntry> index;
  eastl::hash_map<eastl::string, cgltf_data*> documents;
  eastl::hash_map<eastl::string, eastl::string> guid_by_asset_id;
  eastl::hash_map<eastl::string, eastl::string> guid_by_path;
  eastl::hash_set<eastl::string> used_names;
  eastl::vector<eastl::string> expand_stack;

  ~FlattenBaker() {
    for (auto& entry : documents) {
      if (entry.second != nullptr) {
        cgltf_free(entry.second);
      }
    }
  }

  eastl::string uniquify(const eastl::string& stem) {
    eastl::string name = stem.empty() ? eastl::string("node") : stem;
    if (used_names.find(name) == used_names.end()) {
      used_names.insert(name);
      return name;
    }
    for (uint32_t i = 1; i < 100000; ++i) {
      char candidate[256];
      std::snprintf(candidate, sizeof(candidate), "%s_%u", name.c_str(), i);
      const eastl::string next(candidate);
      if (used_names.find(next) == used_names.end()) {
        used_names.insert(next);
        return next;
      }
    }
    used_names.insert(name);
    return name;
  }

  eastl::string emitEntity(const eastl::string& stem, const Vec3& position,
                           const Quat& rotation, const Vec3& scale,
                           const eastl::string& parent,
                           const eastl::string& mesh_guid) {
    SceneEntityDefinition entity;
    entity.name = uniquify(stem);
    entity.position = position;
    entity.rotation = rotation;
    entity.scale = scale;
    entity.parent_name = parent;
    entity.mesh_virtual_path = mesh_guid;
    scene->getEntities().push_back(eastl::move(entity));
    return scene->getEntities().back().name;
  }

  cgltf_data* loadGltf(const fs::path& absolute) {
    const eastl::string key(absolute.generic_string().c_str());
    const auto found = documents.find(key);
    if (found != documents.end()) {
      return found->second;
    }
    cgltf_options parse_options{};
    cgltf_data* data = nullptr;
    const cgltf_result result =
        cgltf_parse_file(&parse_options, absolute.string().c_str(), &data);
    if (result != cgltf_result_success || data == nullptr) {
      if (data != nullptr) {
        cgltf_free(data);
      }
      documents[key] = nullptr;
      return nullptr;
    }
    documents[key] = data;
    return data;
  }

  bool stageGltfUri(const char* uri, const fs::path& source_gltf,
                    const fs::path& dest_gltf, const fs::path& dest_root,
                    bool required) {
    if (uri == nullptr || uri[0] == '\0' || std::strncmp(uri, "data:", 5) == 0) {
      return true;
    }
    std::vector<char> decoded(uri, uri + std::strlen(uri) + 1);
    cgltf_decode_uri(decoded.data());
    const fs::path relative(decoded.data());
    if (relative.empty() || relative.is_absolute() || relative.has_root_name()) {
      return !required;
    }
    const fs::path source = (source_gltf.parent_path() / relative).lexically_normal();
    const fs::path dest = (dest_gltf.parent_path() / relative).lexically_normal();
    if (!pathIsUnder(dest, dest_root)) {
      return !required;
    }
    std::error_code ec;
    if (!fs::is_regular_file(source, ec)) {
      if (!required) {
        LOG_WARN("[se-world-flatten] optional sidecar missing {}",
                 source.generic_string().c_str());
        return true;
      }
      return false;
    }
    return copyFileIfMissing(source, dest);
  }

  fs::path stageGltfWithSidecars(const fs::path& godot_absolute) {
    if (options->file_system == nullptr) {
      return godot_absolute;
    }
    std::error_code ec;
    const fs::path rel = fs::relative(godot_absolute, options->godot_root, ec);
    if (ec || rel.empty() || pathEscapesRoot(rel)) {
      return {};
    }
    const fs::path dest_root =
        options->file_system->getResourcesRoot() / "se-world";
    const fs::path dest_gltf = (dest_root / rel).lexically_normal();
    if (!pathIsUnder(dest_gltf, dest_root)) {
      return {};
    }
    if (!copyFileIfMissing(godot_absolute, dest_gltf)) {
      return {};
    }
    cgltf_data* data = loadGltf(godot_absolute);
    if (data == nullptr) {
      return {};
    }
    for (cgltf_size i = 0; i < data->buffers_count; ++i) {
      if (!stageGltfUri(data->buffers[i].uri, godot_absolute, dest_gltf, dest_root,
                        true)) {
        return {};
      }
    }
    for (cgltf_size i = 0; i < data->images_count; ++i) {
      if (!stageGltfUri(data->images[i].uri, godot_absolute, dest_gltf, dest_root,
                        false)) {
        return {};
      }
    }
    return dest_gltf;
  }

  eastl::string ensureMeshGuid(const eastl::string& asset_id, const fs::path& absolute) {
    const auto by_id = guid_by_asset_id.find(asset_id);
    if (by_id != guid_by_asset_id.end()) {
      return by_id->second;
    }
    const eastl::string path_key(absolute.generic_string().c_str());
    const auto by_path = guid_by_path.find(path_key);
    if (by_path != guid_by_path.end()) {
      guid_by_asset_id[asset_id] = by_path->second;
      return by_path->second;
    }

    fs::path import_path = absolute;
    if (options->import_service != nullptr && options->file_system != nullptr) {
      import_path = stageGltfWithSidecars(absolute);
      if (import_path.empty()) {
        LOG_WARN("[se-world-flatten] failed to stage glTF {} ({})",
                 absolute.generic_string().c_str(), asset_id.c_str());
        return {};
      }
    }

    eastl::string guid;
    if (options->asset_registry != nullptr) {
      const eastl::string stem(import_path.stem().generic_string().c_str());
      eastl::string descriptor = options->mesh_assets_folder;
      if (!descriptor.empty() && descriptor.back() != '/') {
        descriptor.push_back('/');
      }
      descriptor.append(stem.c_str());
      descriptor.append(".mesh.yaml");
      guid = options->asset_registry->findGuidForPath(descriptor);
    }
    if (guid.empty() && options->import_service != nullptr) {
      MeshImportSettings settings{};
      settings.animations = false;
      const ImportResult imported = options->import_service->importMesh(
          import_path, options->mesh_assets_folder, settings);
      if (imported.success) {
        guid = imported.guid;
        ++stats->imported_mesh_assets;
      } else {
        LOG_WARN("[se-world-flatten] Import failed for {} ({})",
                 import_path.generic_string().c_str(), asset_id.c_str());
      }
    } else if (guid.empty()) {
      guid = generateGuidV4();
    }
    if (!guid.empty()) {
      guid_by_asset_id[asset_id] = guid;
      guid_by_path[path_key] = guid;
    }
    return guid;
  }

  void visitGltf(cgltf_data* data, const eastl::string& parent_name,
                 FlattenKind parent_kind);

  void expandInstance(const cgltf_node* node, const eastl::string& parent_name,
                      FlattenKind parent_kind) {
    eastl::string asset_id;
    if (!gltfNodeInstanceAssetId(node, asset_id)) {
      return;
    }
    const auto found = index.find(asset_id);
    if (found == index.end()) {
      ++stats->skipped_missing_id;
      stats->skipped_ids.push_back(asset_id);
      LOG_WARN("[se-world-flatten] skip missing asset id {}", asset_id.c_str());
      return;
    }
    const AssetIndexEntry& entry = found->second;
    if (shouldSkipSuite(entry.filepath)) {
      ++stats->skipped_suite;
      return;
    }

    for (const eastl::string& stacked : expand_stack) {
      if (stacked == asset_id) {
        LOG_WARN("[se-world-flatten] skip cycle at {}", asset_id.c_str());
        return;
      }
    }

    const fs::path absolute = options->godot_root / fs::path(entry.filepath.c_str());
    std::error_code ec;
    if (!fs::is_regular_file(absolute, ec)) {
      ++stats->skipped_missing_file;
      stats->skipped_ids.push_back(asset_id);
      LOG_WARN("[se-world-flatten] skip missing glTF {} ({})",
               absolute.generic_string().c_str(), asset_id.c_str());
      return;
    }

    cgltf_data* child_data = loadGltf(absolute);
    if (child_data == nullptr) {
      ++stats->skipped_missing_file;
      LOG_WARN("[se-world-flatten] skip unreadable glTF {}",
               absolute.generic_string().c_str());
      return;
    }

    const bool drawable = gltfHasDrawableMesh(child_data);
    eastl::string mesh_guid;
    if (drawable) {
      mesh_guid = ensureMeshGuid(asset_id, absolute);
      if (mesh_guid.empty() && options->import_service != nullptr) {
        ++stats->skipped_missing_file;
        LOG_WARN("[se-world-flatten] skip instance; mesh Import failed {}",
                 asset_id.c_str());
        return;
      }
    }

    FlattenKind child_kind = parent_kind;
    if (isSetFilepath(entry.filepath)) {
      child_kind = FlattenKind::set;
    } else if (isLibraryOrPropFilepath(entry.filepath)) {
      child_kind = FlattenKind::library;
    }

    Vec3 position{};
    Quat rotation = glm::identity<Quat>();
    Vec3 scale(1.0f);
    bakeNodeLocal(node, position, rotation, scale);
    const eastl::string stem = gltfNodeDisplayName(node);
    const eastl::string entity_name =
        emitEntity(stem, position, rotation, scale, parent_name, mesh_guid);

    if (child_kind == FlattenKind::library) {
      if (parent_kind == FlattenKind::library) {
        ++stats->nested_instances;
      } else {
        ++stats->layout_instances;
      }
    } else {
      ++stats->grouping_entities;
      if (drawable) {
        ++stats->set_geo_mesh_refs;
      }
    }

    expand_stack.push_back(asset_id);
    visitGltf(child_data, entity_name, child_kind);
    expand_stack.pop_back();
  }

  void visitNode(const cgltf_node* node, const eastl::string& parent_name,
                 FlattenKind parent_kind) {
    if (node == nullptr) {
      return;
    }
    // Grill locked: omit COL-* nodes. Do not emit active:false placeholders.
    if (gltfNodeNameStartsWith(node, "COL-")) {
      return;
    }
    eastl::string instance_id;
    if (gltfNodeInstanceAssetId(node, instance_id)) {
      expandInstance(node, parent_name, parent_kind);
      return;
    }
    for (cgltf_size i = 0; i < node->children_count; ++i) {
      visitNode(node->children[i], parent_name, parent_kind);
    }
  }
};

void FlattenBaker::visitGltf(cgltf_data* data, const eastl::string& parent_name,
                             FlattenKind parent_kind) {
  if (data == nullptr) {
    return;
  }
  if (data->scene != nullptr && data->scene->nodes_count > 0) {
    for (cgltf_size i = 0; i < data->scene->nodes_count; ++i) {
      visitNode(data->scene->nodes[i], parent_name, parent_kind);
    }
    return;
  }
  for (cgltf_size i = 0; i < data->nodes_count; ++i) {
    if (data->nodes[i].parent == nullptr) {
      visitNode(&data->nodes[i], parent_name, parent_kind);
    }
  }
}

}  // namespace

SeWorldFlattenStats bakeSeWorldFlattenScene(const SeWorldFlattenOptions& options,
                                            Scene& out_scene) {
  SeWorldFlattenStats stats;
  if (options.layout_gltf.empty() || options.asset_index_json.empty()) {
    stats.error_message = "layout glTF and asset_index.json are required";
    return stats;
  }

  eastl::string index_json;
  if (!readTextFile(options.asset_index_json, index_json)) {
    stats.error_message = "failed to read asset_index.json";
    return stats;
  }

  FlattenBaker baker;
  baker.options = &options;
  baker.stats = &stats;
  baker.scene = &out_scene;
  if (!parseAssetIndexJson(index_json, baker.index)) {
    stats.error_message = "failed to parse asset_index.json";
    return stats;
  }

  cgltf_data* layout = baker.loadGltf(options.layout_gltf);
  if (layout == nullptr) {
    stats.error_message = "failed to parse layout glTF";
    return stats;
  }

  out_scene.getEntities().clear();
  appendNewSceneStarterEntities(out_scene.getEntities());
  for (const SceneEntityDefinition& entity : out_scene.getEntities()) {
    baker.used_names.insert(entity.name);
  }

  eastl::string root_stem("SE-world");
  if (layout->scene != nullptr && layout->scene->name != nullptr &&
      layout->scene->name[0] != '\0') {
    root_stem = layout->scene->name;
  } else if (!options.layout_gltf.empty()) {
    root_stem = eastl::string(options.layout_gltf.stem().generic_string().c_str());
  }
  out_scene.setName(root_stem);
  const eastl::string root_name = baker.emitEntity(
      root_stem, Vec3(0.0f), glm::identity<Quat>(), Vec3(1.0f), {}, {});
  ++stats.grouping_entities;
  baker.visitGltf(layout, root_name, FlattenKind::root);

  if (options.asset_registry != nullptr) {
    eastl::string existing = options.asset_registry->findGuidForPath(
        eastl::string(k_se_world_scene_virtual_path));
    if (existing.empty()) {
      existing = options.asset_registry->allocateGuid();
    }
    out_scene.setGuid(existing);
  } else if (out_scene.getGuid().empty()) {
    out_scene.setGuid(generateGuidV4());
  }
  stats.scene_guid = out_scene.getGuid();
  stats.success = true;
  return stats;
}

SeWorldFlattenStats bakeSeWorldFlatten(const SeWorldFlattenOptions& options) {
  Scene scene;
  SeWorldFlattenStats stats = bakeSeWorldFlattenScene(options, scene);
  if (!stats.success) {
    return stats;
  }
  if (options.output_scene.empty()) {
    stats.success = false;
    stats.error_message = "output scene path is required";
    return stats;
  }

  eastl::string json;
  if (!SceneSerializer::serialize(scene, json, options.asset_registry)) {
    stats.success = false;
    stats.error_message = "SceneSerializer::serialize failed";
    return stats;
  }
  if (!writeTextFile(options.output_scene, json, options.file_system)) {
    stats.success = false;
    stats.error_message = "failed to write Scene Asset";
    return stats;
  }
  if (options.asset_registry != nullptr && options.file_system != nullptr) {
    (void)options.asset_registry->ensureSceneAssetRegistered(
        eastl::string(k_se_world_scene_virtual_path));
    (void)options.asset_registry->save();
  }
  LOG_INFO(
      "[se-world-flatten] wrote {} (layout={}, nested={}, grouping={}, "
      "skip_id={}, skip_file={}, meshes={})",
      options.output_scene.generic_string().c_str(), stats.layout_instances,
      stats.nested_instances, stats.grouping_entities, stats.skipped_missing_id,
      stats.skipped_missing_file, stats.imported_mesh_assets);
  return stats;
}

}  // namespace Blunder
