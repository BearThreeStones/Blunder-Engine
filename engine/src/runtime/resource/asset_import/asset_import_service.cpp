#include "runtime/resource/asset_import/asset_import_service.h"

#include "runtime/resource/asset_import/companion_animation_gltf.h"
#include "runtime/resource/asset_import/gltf_animation_clip_extractor.h"

#include <algorithm>
#include <cctype>
#include <cstring>

#include <assimp/Exporter.hpp>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <cgltf.h>

#include "runtime/core/base/macro.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/asset_yaml.h"
#include "runtime/resource/asset_cook/asset_compiler_service.h"
#include "runtime/resource/asset_cook/asset_watch_path.h"
#include "runtime/resource/asset_registry/asset_registry.h"
#include "runtime/resource/content_browser/content_browser_system.h"

namespace Blunder {

namespace fs = std::filesystem;

namespace {
// Test seam for Intermediate migration fail-soft (Task 1.4).
bool g_force_upgrade_convert_failure_for_test = false;
}  // namespace

void AssetImportService::setForceUpgradeConvertFailureForTest(bool force) {
  g_force_upgrade_convert_failure_for_test = force;
}

namespace {

eastl::string extensionLower(const fs::path& path) {
  eastl::string ext(path.extension().generic_string().c_str());
  for (char& c : ext) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }
  return ext;
}

eastl::string joinVirtualPath(const eastl::string& folder,
                              const eastl::string& file_name) {
  eastl::string result = folder;
  if (!result.empty() && result.back() != '/') {
    result.push_back('/');
  }
  result.append(file_name);
  return result;
}

eastl::string resolveAssetsFolder(const eastl::string& selected_folder) {
  eastl::string folder = selected_folder;
  if (folder.compare(0, 10, "resources/") == 0) {
    folder.replace(0, 10, "assets/");
  }
  if (folder.empty()) {
    folder = "assets/";
  }
  return folder;
}

bool relativePathEscapesRoot(const std::filesystem::path& relative_path) {
  for (const auto& part : relative_path.lexically_normal()) {
    if (part == ".") {
      continue;
    }
    return part == "..";
  }
  return false;
}

bool pathsReferToSameFile(const fs::path& lhs, const fs::path& rhs) {
  std::error_code ec;
  const bool equivalent = fs::equivalent(lhs, rhs, ec);
  if (!ec) {
    return equivalent;
  }

  ec.clear();
  const fs::path lhs_absolute = fs::absolute(lhs, ec).lexically_normal();
  if (ec) {
    return lhs.lexically_normal() == rhs.lexically_normal();
  }
  ec.clear();
  const fs::path rhs_absolute = fs::absolute(rhs, ec).lexically_normal();
  return !ec && lhs_absolute == rhs_absolute;
}

bool collectExternalGltfResourcePaths(
    const fs::path& gltf_absolute,
    std::vector<fs::path>& out_relative_paths) {
  out_relative_paths.clear();
  if (extensionLower(gltf_absolute) != ".gltf") {
    return true;
  }

  cgltf_options options{};
  cgltf_data* data = nullptr;
  const cgltf_result parse_result =
      cgltf_parse_file(&options, gltf_absolute.string().c_str(), &data);
  if (parse_result != cgltf_result_success || data == nullptr) {
    return false;
  }

  auto append_uri = [&](const char* uri) -> bool {
    if (uri == nullptr || uri[0] == '\0' ||
        std::strncmp(uri, "data:", 5) == 0) {
      return true;
    }

    std::vector<char> decoded(uri, uri + std::strlen(uri) + 1);
    cgltf_decode_uri(decoded.data());
    const fs::path relative(decoded.data());
    if (relative.empty() || relative.is_absolute() ||
        relative.has_root_name() || relativePathEscapesRoot(relative)) {
      return false;
    }

    const fs::path normalized = relative.lexically_normal();
    if (std::find(out_relative_paths.begin(), out_relative_paths.end(),
                  normalized) == out_relative_paths.end()) {
      out_relative_paths.push_back(normalized);
    }
    return true;
  };

  bool valid = true;
  for (cgltf_size index = 0; valid && index < data->buffers_count; ++index) {
    valid = append_uri(data->buffers[index].uri);
  }
  for (cgltf_size index = 0; valid && index < data->images_count; ++index) {
    valid = append_uri(data->images[index].uri);
  }
  cgltf_free(data);
  return valid;
}

bool copyCompanionGltfResources(
    FileSystem* file_system, const fs::path& source_gltf,
    const fs::path& destination_gltf,
    std::vector<fs::path>& copied_paths) {
  std::vector<fs::path> relative_resources;
  if (!collectExternalGltfResourcePaths(source_gltf, relative_resources)) {
    return false;
  }

  for (const fs::path& relative : relative_resources) {
    const fs::path source = source_gltf.parent_path() / relative;
    const fs::path destination = destination_gltf.parent_path() / relative;
    if (!file_system->exists(source)) {
      return false;
    }
    if (pathsReferToSameFile(source, destination)) {
      continue;
    }
    if (file_system->exists(destination) ||
        !file_system->copyFile(source, destination, false)) {
      return false;
    }
    copied_paths.push_back(destination);
  }
  return true;
}

eastl::string toLowerAscii(eastl::string value) {
  for (char& c : value) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }
  return value;
}

/// Map an absolute path under Resources/ to resources/... virtual path.
/// Returns empty when the path is outside Resources/.
eastl::string resolveIntermediateVirtualPath(FileSystem* file_system,
                                             const fs::path& absolute_path) {
  std::error_code ec;
  const fs::path resources_root = file_system->getResourcesRoot();
  const fs::path relative = fs::relative(absolute_path, resources_root, ec);
  if (!ec && !relative.empty() && !relativePathEscapesRoot(relative)) {
    eastl::string virtual_path("resources/");
    virtual_path.append(relative.generic_string().c_str());
    return virtual_path;
  }
  return eastl::string();
}

bool isSourceArchiveVirtualPath(const eastl::string& resources_virtual) {
  // resources/Source/... is Source archive, not Intermediate data.
  const eastl::string lower = toLowerAscii(resources_virtual);
  return lower.compare(0, 17, "resources/source/") == 0 ||
         lower == "resources/source";
}

bool isUsableIntermediateVirtualPath(const eastl::string& resources_virtual) {
  return !resources_virtual.empty() &&
         !isSourceArchiveVirtualPath(resources_virtual);
}

/// Ensure Intermediate body lives under Resources (non-Source). Copies when the
/// input is external or under Resources/Source/.
eastl::string registerIntermediateBody(FileSystem* file_system,
                                       const fs::path& input_absolute,
                                       const char* resources_subdir) {
  const eastl::string existing =
      resolveIntermediateVirtualPath(file_system, input_absolute);
  if (isUsableIntermediateVirtualPath(existing)) {
    return existing;
  }

  const eastl::string stem(input_absolute.stem().generic_string().c_str());
  const eastl::string file_name(
      input_absolute.filename().generic_string().c_str());

  auto try_dest = [&](const eastl::string& folder_stem) -> eastl::string {
    const fs::path relative =
        fs::path(resources_subdir) / folder_stem.c_str() / file_name.c_str();
    const fs::path absolute = file_system->resolveResource(relative);
    if (file_system->exists(absolute)) {
      return eastl::string();
    }
    if (!file_system->copyFile(input_absolute, absolute, false)) {
      return eastl::string();
    }
    eastl::string virtual_path("resources/");
    virtual_path.append(relative.generic_string().c_str());
    return virtual_path;
  };

  eastl::string virtual_path = try_dest(stem);
  if (!virtual_path.empty()) {
    return virtual_path;
  }

  for (uint32_t index = 1; index < 10000; ++index) {
    char alt[128];
    std::snprintf(alt, sizeof(alt), "%s_%u", stem.c_str(), index);
    virtual_path = try_dest(eastl::string(alt));
    if (!virtual_path.empty()) {
      return virtual_path;
    }
  }
  return eastl::string();
}

bool registerCompanionAnimationIntermediates(
    FileSystem* file_system, const eastl::string& host_resource_virtual,
    const std::vector<fs::path>& companion_inputs,
    eastl::vector<eastl::string>& out_virtual_paths,
    std::vector<fs::path>& out_absolute_paths) {
  out_virtual_paths.clear();
  out_absolute_paths.clear();
  if (companion_inputs.empty()) {
    return true;
  }
  if (host_resource_virtual.compare(0, 10, "resources/") != 0) {
    return false;
  }

  // Convention: companions live beside the host body under
  // resources/<host-parent>/companions/<companion-file>. The explicit
  // descriptor list remains authoritative for Reimport.
  const fs::path host_relative(host_resource_virtual.substr(10).c_str());
  const fs::path companions_relative =
      host_relative.parent_path() / "companions";
  std::vector<fs::path> copied_paths;
  const auto fail = [&]() {
    for (const fs::path& copied : copied_paths) {
      std::error_code ec;
      fs::remove(copied, ec);
    }
    out_virtual_paths.clear();
    out_absolute_paths.clear();
    return false;
  };

  for (const fs::path& input : companion_inputs) {
    const fs::path original_name = input.filename();
    if (original_name.empty()) {
      return fail();
    }

    fs::path destination_relative = companions_relative / original_name;
    fs::path destination_absolute =
        file_system->resolveResource(destination_relative);
    if (file_system->exists(destination_absolute) &&
        !pathsReferToSameFile(input, destination_absolute)) {
      bool found_unique = false;
      for (uint32_t index = 1; index < 10000; ++index) {
        const fs::path candidate_name =
            original_name.stem().generic_string() + "_" +
            std::to_string(index) + original_name.extension().generic_string();
        destination_relative = companions_relative / candidate_name;
        destination_absolute =
            file_system->resolveResource(destination_relative);
        if (!file_system->exists(destination_absolute)) {
          found_unique = true;
          break;
        }
      }
      if (!found_unique) {
        return fail();
      }
    }

    if (!pathsReferToSameFile(input, destination_absolute)) {
      if (!file_system->copyFile(input, destination_absolute, false)) {
        return fail();
      }
      copied_paths.push_back(destination_absolute);
    }
    if (!copyCompanionGltfResources(file_system, input, destination_absolute,
                                    copied_paths)) {
      return fail();
    }

    eastl::string virtual_path("resources/");
    virtual_path.append(destination_relative.generic_string().c_str());
    out_virtual_paths.push_back(virtual_path);
    out_absolute_paths.push_back(destination_absolute);
  }
  return true;
}

/// Copy Source Asset under Resources/Source/{subdir}/{stem}/filename.
eastl::string archiveSourceAsset(FileSystem* file_system,
                                 const fs::path& input_absolute,
                                 const char* resources_subdir) {
  const eastl::string stem(input_absolute.stem().generic_string().c_str());
  const eastl::string file_name(
      input_absolute.filename().generic_string().c_str());

  auto try_dest = [&](const eastl::string& folder_stem) -> eastl::string {
    const fs::path relative = fs::path("Source") / resources_subdir /
                              folder_stem.c_str() / file_name.c_str();
    const fs::path absolute = file_system->resolveResource(relative);
    if (file_system->exists(absolute)) {
      return eastl::string();
    }
    if (!file_system->copyFile(input_absolute, absolute, false)) {
      return eastl::string();
    }
    eastl::string virtual_path("resources/");
    virtual_path.append(relative.generic_string().c_str());
    return virtual_path;
  };

  eastl::string virtual_path = try_dest(stem);
  if (!virtual_path.empty()) {
    return virtual_path;
  }

  for (uint32_t index = 1; index < 10000; ++index) {
    char alt[128];
    std::snprintf(alt, sizeof(alt), "%s_%u", stem.c_str(), index);
    virtual_path = try_dest(eastl::string(alt));
    if (!virtual_path.empty()) {
      return virtual_path;
    }
  }
  return eastl::string();
}

/// Assimp Import. Returns nullptr on failure (importer owns the scene).
const aiScene* readSourceScene(Assimp::Importer& importer,
                               const fs::path& input_absolute) {
  const aiScene* scene = importer.ReadFile(
      input_absolute.string(),
      aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
          aiProcess_GenNormals);
  if (!scene || !scene->mRootNode ||
      (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0u) {
    LOG_WARN("[AssetImport] Assimp failed to read {}: {}",
             input_absolute.generic_string(), importer.GetErrorString());
    return nullptr;
  }
  return scene;
}

/// Assimp Export to an absolute Intermediate glTF/GLB path (overwrites).
bool exportSceneToGltfFile(const aiScene* scene, FileSystem* file_system,
                           const fs::path& output_absolute) {
  if (!scene || !file_system) {
    return false;
  }
  if (!file_system->ensureParentDirectory(output_absolute)) {
    return false;
  }

  const eastl::string ext = extensionLower(output_absolute);
  const char* format_id = (ext == ".glb") ? "glb2" : "gltf2";

  Assimp::Exporter exporter;
  const aiReturn status =
      exporter.Export(scene, format_id, output_absolute.string());
  if (status != aiReturn_SUCCESS) {
    LOG_WARN("[AssetImport] Assimp glTF export failed for {}: {}",
             output_absolute.generic_string(), exporter.GetErrorString());
    return false;
  }
  return file_system->exists(output_absolute);
}

/// Assimp Import + Export to Intermediate glTF under Resources/Models/{stem}/.
eastl::string exportSourceToIntermediateGltf(FileSystem* file_system,
                                             const fs::path& input_absolute) {
  Assimp::Importer importer;
  const aiScene* scene = readSourceScene(importer, input_absolute);
  if (!scene) {
    return eastl::string();
  }

  const eastl::string stem(input_absolute.stem().generic_string().c_str());

  auto try_export = [&](const eastl::string& folder_stem) -> eastl::string {
    const eastl::string file_name = folder_stem + ".gltf";
    const fs::path relative =
        fs::path("Models") / folder_stem.c_str() / file_name.c_str();
    const fs::path absolute = file_system->resolveResource(relative);
    if (file_system->exists(absolute)) {
      return eastl::string();
    }
    if (!exportSceneToGltfFile(scene, file_system, absolute)) {
      return eastl::string();
    }

    eastl::string virtual_path("resources/");
    virtual_path.append(relative.generic_string().c_str());
    return virtual_path;
  };

  eastl::string virtual_path = try_export(stem);
  if (!virtual_path.empty()) {
    return virtual_path;
  }

  for (uint32_t index = 1; index < 10000; ++index) {
    char alt[128];
    std::snprintf(alt, sizeof(alt), "%s_%u", stem.c_str(), index);
    virtual_path = try_export(eastl::string(alt));
    if (!virtual_path.empty()) {
      return virtual_path;
    }
  }
  return eastl::string();
}

/// Re-export archived Source over an existing Intermediate glTF/GLB (overwrite).
bool reexportArchivedSourceToIntermediate(FileSystem* file_system,
                                          const fs::path& archived_absolute,
                                          const fs::path& intermediate_absolute) {
  Assimp::Importer importer;
  const aiScene* scene = readSourceScene(importer, archived_absolute);
  if (!scene) {
    return false;
  }
  return exportSceneToGltfFile(scene, file_system, intermediate_absolute);
}

fs::path resolveResourcesVirtualPath(FileSystem* file_system,
                                     const eastl::string& virtual_path) {
  eastl::string relative = virtual_path;
  if (relative.compare(0, 10, "resources/") == 0) {
    relative.erase(0, 10);
  }
  return file_system->resolveResource(fs::path(relative.c_str()));
}

fs::path resolveDescriptorAbsolute(FileSystem* file_system,
                                   const eastl::string& descriptor_virtual) {
  eastl::string relative = descriptor_virtual;
  if (relative.compare(0, 7, "assets/") == 0) {
    relative.erase(0, 7);
  }
  return file_system->resolveAsset(fs::path(relative.c_str()));
}

/// When archived_source is Source Export whitelist, overwrite Intermediate via
/// Assimp. GUID / descriptor paths are left unchanged. Returns false only on
/// hard failure of an attempted Source Export re-run.
bool refreshIntermediateFromArchivedSource(FileSystem* file_system,
                                           const eastl::string& descriptor_virtual,
                                           const eastl::string& yaml_text) {
  const bool is_mesh = descriptor_virtual.size() >= 10 &&
                       descriptor_virtual.compare(
                           descriptor_virtual.size() - 10, 10, ".mesh.yaml") == 0;
  if (!is_mesh) {
    return true;  // textures / other: invalidate-only path
  }

  MeshAssetDescriptor descriptor{};
  if (!AssetYaml::parseMeshDescriptor(yaml_text, descriptor)) {
    LOG_WARN("[AssetImport] Reimport: failed to parse mesh {}",
             descriptor_virtual.c_str());
    return false;
  }

  if (descriptor.archived_source.empty()) {
    // Intermediate-only Asset: keep GUID; Finals invalidation is enough.
    return true;
  }

  const fs::path archived_absolute =
      resolveResourcesVirtualPath(file_system, descriptor.archived_source);
  const eastl::string archived_ext = extensionLower(archived_absolute);
  if (!AssetImportService::isMeshSourceExportExtension(archived_ext)) {
    LOG_WARN(
        "[AssetImport] Reimport: archived_source {} is not Source Export "
        "whitelist; invalidating Finals only",
        descriptor.archived_source.c_str());
    return true;
  }

  if (!file_system->exists(archived_absolute)) {
    LOG_WARN("[AssetImport] Reimport: archived Source missing {}",
             archived_absolute.generic_string());
    return false;
  }

  if (descriptor.source.empty()) {
    LOG_WARN("[AssetImport] Reimport: mesh {} has empty Intermediate source",
             descriptor_virtual.c_str());
    return false;
  }

  const fs::path intermediate_absolute =
      resolveResourcesVirtualPath(file_system, descriptor.source);
  if (!reexportArchivedSourceToIntermediate(file_system, archived_absolute,
                                            intermediate_absolute)) {
    LOG_WARN(
        "[AssetImport] Reimport: Assimp re-export failed for {} -> {}",
        archived_absolute.generic_string(),
        intermediate_absolute.generic_string());
    return false;
  }

  LOG_INFO(
      "[AssetImport] Reimport refreshed Intermediate {} from archived {}",
      descriptor.source.c_str(), descriptor.archived_source.c_str());
  return true;
}

/// Filename stem from a virtual path (e.g. resources/Models/rig/rig.gltf → rig).
eastl::string stemFromVirtualPath(const eastl::string& virtual_path) {
  size_t slash = eastl::string::npos;
  for (size_t i = virtual_path.size(); i > 0; --i) {
    const char c = virtual_path[i - 1];
    if (c == '/' || c == '\\') {
      slash = i - 1;
      break;
    }
  }
  const size_t name_start = slash == eastl::string::npos ? 0 : slash + 1;
  eastl::string filename = virtual_path.substr(name_start);

  size_t dot = eastl::string::npos;
  for (size_t i = filename.size(); i > 0; --i) {
    if (filename[i - 1] == '.') {
      dot = i - 1;
      break;
    }
  }
  if (dot != eastl::string::npos) {
    filename.erase(dot);
  }
  return filename;
}

void refreshMeshAnimationClipsFromIntermediate(
    FileSystem* file_system, AssetRegistry* asset_registry,
    ContentBrowserSystem* content_browser,
    const eastl::string& descriptor_virtual, const MeshAssetDescriptor& mesh,
    const MakeUniqueDescriptorNameFn& make_unique_descriptor_name) {
  if (!mesh.import.animations || mesh.source.empty() ||
      !make_unique_descriptor_name) {
    return;
  }

  (void)descriptor_virtual;
  const eastl::string mesh_stem = stemFromVirtualPath(mesh.source);
  if (mesh_stem.empty()) {
    return;
  }

  const ExistingAnimationClipMap existing_clips =
      collectExistingAnimationClipsForMesh(file_system, asset_registry,
                                           mesh_stem);
  const fs::path gltf_absolute =
      resolveResourcesVirtualPath(file_system, mesh.source);

  refreshAnimationClipsFromGltf(file_system, asset_registry, content_browser,
                                gltf_absolute, mesh_stem, existing_clips,
                                make_unique_descriptor_name);

  for (const eastl::string& companion_source :
       mesh.companion_animation_sources) {
    const eastl::string companion_stem =
        stemFromVirtualPath(companion_source);
    if (companion_source.empty() || companion_stem.empty()) {
      continue;
    }

    const fs::path companion_absolute =
        resolveResourcesVirtualPath(file_system, companion_source);
    warnOnCompanionAnimationBoneMismatches(gltf_absolute,
                                           companion_absolute);
    refreshAnimationClipsFromGltf(
        file_system, asset_registry, content_browser, companion_absolute,
        mesh_stem, existing_clips, make_unique_descriptor_name,
        companion_stem);
  }
}

bool endsWithIgnoreCase(const eastl::string& value, const char* suffix) {
  const size_t suffix_length = std::strlen(suffix);
  if (value.size() < suffix_length) {
    return false;
  }
  for (size_t i = 0; i < suffix_length; ++i) {
    char a = value[value.size() - suffix_length + i];
    char b = suffix[i];
    if (a >= 'A' && a <= 'Z') {
      a = static_cast<char>(a - 'A' + 'a');
    }
    if (b >= 'A' && b <= 'Z') {
      b = static_cast<char>(b - 'A' + 'a');
    }
    if (a != b) {
      return false;
    }
  }
  return true;
}

bool isLegacyColladaIntermediateSource(const eastl::string& source) {
  return endsWithIgnoreCase(source, ".dae");
}

eastl::string replaceExtensionWithGltf(const eastl::string& virtual_path) {
  const size_t dot = virtual_path.find_last_of('.');
  if (dot == eastl::string::npos) {
    return virtual_path + ".gltf";
  }
  eastl::string result = virtual_path;
  result.replace(dot, eastl::string::npos, ".gltf");
  return result;
}

/// Assimp Import `.dae` + Export sibling `.gltf`. Empty on failure.
eastl::string convertLegacyColladaToSiblingGltf(
    FileSystem* file_system, const eastl::string& source_virtual,
    const fs::path& source_absolute) {
  if (g_force_upgrade_convert_failure_for_test) {
    return eastl::string();
  }

  Assimp::Importer importer;
  const aiScene* scene = readSourceScene(importer, source_absolute);
  if (!scene) {
    return eastl::string();
  }

  const eastl::string gltf_virtual = replaceExtensionWithGltf(source_virtual);
  const fs::path gltf_absolute =
      resolveResourcesVirtualPath(file_system, gltf_virtual);
  if (!exportSceneToGltfFile(scene, file_system, gltf_absolute)) {
    if (file_system->exists(gltf_absolute)) {
      std::error_code ec;
      fs::remove(gltf_absolute, ec);
    }
    return eastl::string();
  }
  return gltf_virtual;
}

/// Produce glTF Intermediate for a legacy `.dae` descriptor: Reimport from
/// archived Source Export when possible, else Assimp convert the `.dae`.
eastl::string migrateLegacyDaeIntermediateToGltf(
    FileSystem* file_system, const MeshAssetDescriptor& descriptor) {
  const fs::path dae_absolute =
      resolveResourcesVirtualPath(file_system, descriptor.source);
  if (!file_system->exists(dae_absolute)) {
    return eastl::string();
  }

  const eastl::string gltf_virtual = replaceExtensionWithGltf(descriptor.source);
  const fs::path gltf_absolute =
      resolveResourcesVirtualPath(file_system, gltf_virtual);

  if (!descriptor.archived_source.empty()) {
    const fs::path archived_absolute =
        resolveResourcesVirtualPath(file_system, descriptor.archived_source);
    const eastl::string archived_ext = extensionLower(archived_absolute);
    if (AssetImportService::isMeshSourceExportExtension(archived_ext) &&
        file_system->exists(archived_absolute)) {
      if (g_force_upgrade_convert_failure_for_test) {
        return eastl::string();
      }
      if (reexportArchivedSourceToIntermediate(file_system, archived_absolute,
                                                 gltf_absolute)) {
        return gltf_virtual;
      }
    }
  }

  return convertLegacyColladaToSiblingGltf(file_system, descriptor.source,
                                           dae_absolute);
}

}  // namespace

void AssetImportService::initialize(const AssetImportServiceInit& init) {
  m_file_system = init.file_system;
  m_asset_registry = init.asset_registry;
  m_content_browser = init.content_browser;
  m_asset_compiler = init.asset_compiler;
  m_is_initialized = m_file_system != nullptr && m_asset_registry != nullptr;
}

void AssetImportService::shutdown() {
  m_file_system = nullptr;
  m_asset_registry = nullptr;
  m_content_browser = nullptr;
  m_asset_compiler = nullptr;
  m_is_initialized = false;
}

bool AssetImportService::isMeshIntermediateExtension(
    const eastl::string& extension_lower) {
  return extension_lower == ".gltf" || extension_lower == ".glb";
}

bool AssetImportService::isTextureIntermediateExtension(
    const eastl::string& extension_lower) {
  return extension_lower == ".png" || extension_lower == ".jpg" ||
         extension_lower == ".jpeg" || extension_lower == ".bmp" ||
         extension_lower == ".tga";
}

bool AssetImportService::isMeshSourceExportExtension(
    const eastl::string& extension_lower) {
  return extension_lower == ".fbx" || extension_lower == ".obj";
}

eastl::string AssetImportService::makeUniqueDescriptorName(
    const eastl::string& folder, const eastl::string& stem,
    const char* suffix) const {
  auto name_exists = [&](const eastl::string& file_name) -> bool {
    const eastl::string virtual_path = joinVirtualPath(folder, file_name);
    if (m_content_browser && m_content_browser->findEntry(virtual_path)) {
      return true;
    }
    eastl::string relative = folder;
    if (relative.compare(0, 7, "assets/") == 0) {
      relative.erase(0, 7);
    }
    const fs::path absolute =
        m_file_system->resolveAsset(fs::path(relative.c_str()) / file_name.c_str());
    return m_file_system->exists(absolute);
  };

  eastl::string file_name = stem;
  file_name.append(suffix);
  if (!name_exists(file_name)) {
    return file_name;
  }

  for (uint32_t index = 1; index < 10000; ++index) {
    char alt[128];
    std::snprintf(alt, sizeof(alt), "%s_%u%s", stem.c_str(), index, suffix);
    if (!name_exists(eastl::string(alt))) {
      return eastl::string(alt);
    }
  }
  return eastl::string();
}

ImportResult AssetImportService::importMesh(
    const fs::path& input_absolute, const eastl::string& assets_folder_virtual,
    const MeshImportSettings& settings) {
  ImportResult result{};
  if (!m_is_initialized || assets_folder_virtual.empty()) {
    return result;
  }

  const eastl::string ext = extensionLower(input_absolute);
  if (isMeshSourceExportExtension(ext)) {
    return importMeshSourceExport(input_absolute, assets_folder_virtual,
                                  settings);
  }
  if (isMeshIntermediateExtension(ext)) {
    return importMeshIntermediate(input_absolute, assets_folder_virtual,
                                  settings);
  }

  // v1: FBX/OBJ Source Export; glTF/GLB Intermediate-direct;
  // .blend / others are a clear reject (success=false), not copy-to-Source
  // and not silent success.
  LOG_WARN(
      "[AssetImport] unsupported mesh input {} "
      "(v1 Source Export whitelist is .fbx/.obj; Intermediate direct is "
      ".gltf/.glb; .blend automatic export is not supported)",
      input_absolute.generic_string());
  return result;
}

ImportResult AssetImportService::importMeshIntermediate(
    const fs::path& input_absolute, const eastl::string& assets_folder_virtual,
    const MeshImportSettings& settings,
    const std::vector<fs::path>& companion_animation_paths) {
  ImportResult result{};

  const eastl::string assets_folder = resolveAssetsFolder(assets_folder_virtual);
  const eastl::string resource_virtual_path =
      registerIntermediateBody(m_file_system, input_absolute, "Models");
  if (resource_virtual_path.empty()) {
    LOG_WARN("[AssetImport] failed to place mesh Intermediate {}",
             input_absolute.generic_string());
    return result;
  }

  eastl::vector<eastl::string> companion_resource_virtual_paths;
  std::vector<fs::path> companion_resource_absolute_paths;
  if (!registerCompanionAnimationIntermediates(
          m_file_system, resource_virtual_path, companion_animation_paths,
          companion_resource_virtual_paths,
          companion_resource_absolute_paths)) {
    LOG_WARN("[AssetImport] failed to place companion Intermediates for {}",
             input_absolute.generic_string());
    return result;
  }

  const eastl::string stem(input_absolute.stem().generic_string().c_str());
  const eastl::string descriptor_name =
      makeUniqueDescriptorName(assets_folder, stem, ".mesh.yaml");
  if (descriptor_name.empty()) {
    LOG_WARN("[AssetImport] descriptor already exists for mesh {}", stem.c_str());
    return result;
  }

  MeshAssetDescriptor descriptor{};
  descriptor.guid = m_asset_registry->allocateGuid();
  // Descriptor field `source` = Intermediate path (glossary), not Source Asset.
  descriptor.source = resource_virtual_path;
  descriptor.companion_animation_sources =
      companion_resource_virtual_paths;
  descriptor.import = settings;

  const eastl::string descriptor_virtual =
      joinVirtualPath(assets_folder, descriptor_name);
  eastl::string relative = descriptor_virtual;
  relative.erase(0, 7);
  const fs::path descriptor_absolute =
      m_file_system->resolveAsset(fs::path(relative.c_str()));

  m_file_system->ensureParentDirectory(descriptor_absolute);
  if (!m_file_system->writeText(descriptor_absolute,
                                AssetYaml::serializeMeshDescriptor(descriptor))) {
    return result;
  }

  m_asset_registry->registerAsset(descriptor.guid, descriptor_virtual);

  result.descriptor_virtual_path = descriptor_virtual;
  result.guid = descriptor.guid;
  result.success = true;
  result.companion_animation_paths = companion_resource_absolute_paths;

  if (settings.animations) {
    const fs::path gltf_absolute =
        resolveResourcesVirtualPath(m_file_system, resource_virtual_path);
    const MakeUniqueDescriptorNameFn make_name =
        [this](const eastl::string& folder, const eastl::string& name_stem,
               const char* suffix) {
          return makeUniqueDescriptorName(folder, name_stem, suffix);
        };
    result.animation_clips = extractAndRegisterAnimationClipsFromGltf(
        m_file_system, m_asset_registry, m_content_browser, gltf_absolute, stem,
        make_name);
    for (const fs::path& companion_absolute :
         companion_resource_absolute_paths) {
      warnOnCompanionAnimationBoneMismatches(gltf_absolute,
                                             companion_absolute);
      const eastl::string companion_stem(
          companion_absolute.stem().generic_string().c_str());
      eastl::vector<ImportResult> companion_clips =
          extractAndRegisterAnimationClipsFromGltf(
              m_file_system, m_asset_registry, m_content_browser,
              companion_absolute, stem, make_name, companion_stem);
      result.animation_clips.insert(result.animation_clips.end(),
                                    companion_clips.begin(),
                                    companion_clips.end());
    }
  }

  LOG_INFO("[AssetImport] mesh {} -> {} (Intermediate: {})",
           input_absolute.generic_string(), descriptor_virtual.c_str(),
           resource_virtual_path.c_str());
  return result;
}

ImportResult AssetImportService::importMeshSourceExport(
    const fs::path& input_absolute, const eastl::string& assets_folder_virtual,
    const MeshImportSettings& settings) {
  ImportResult result{};

  const eastl::string archived_virtual =
      archiveSourceAsset(m_file_system, input_absolute, "Models");
  if (archived_virtual.empty()) {
    LOG_WARN("[AssetImport] failed to archive Source Asset {}",
             input_absolute.generic_string());
    return result;
  }

  const eastl::string intermediate_virtual =
      exportSourceToIntermediateGltf(m_file_system, input_absolute);
  if (intermediate_virtual.empty()) {
    LOG_WARN("[AssetImport] Source Export to Intermediate glTF failed for {}",
             input_absolute.generic_string());
    return result;
  }

  const eastl::string assets_folder = resolveAssetsFolder(assets_folder_virtual);
  const eastl::string stem(input_absolute.stem().generic_string().c_str());
  const eastl::string descriptor_name =
      makeUniqueDescriptorName(assets_folder, stem, ".mesh.yaml");
  if (descriptor_name.empty()) {
    LOG_WARN("[AssetImport] descriptor already exists for Source Export {}",
             stem.c_str());
    return result;
  }

  MeshAssetDescriptor descriptor{};
  descriptor.guid = m_asset_registry->allocateGuid();
  descriptor.source = intermediate_virtual;
  descriptor.archived_source = archived_virtual;
  descriptor.import = settings;

  const eastl::string descriptor_virtual =
      joinVirtualPath(assets_folder, descriptor_name);
  eastl::string relative = descriptor_virtual;
  relative.erase(0, 7);
  const fs::path descriptor_absolute =
      m_file_system->resolveAsset(fs::path(relative.c_str()));

  m_file_system->ensureParentDirectory(descriptor_absolute);
  if (!m_file_system->writeText(descriptor_absolute,
                                AssetYaml::serializeMeshDescriptor(descriptor))) {
    return result;
  }

  m_asset_registry->registerAsset(descriptor.guid, descriptor_virtual);

  result.descriptor_virtual_path = descriptor_virtual;
  result.guid = descriptor.guid;
  result.success = true;

  if (settings.animations) {
    const fs::path gltf_absolute =
        resolveResourcesVirtualPath(m_file_system, intermediate_virtual);
    const MakeUniqueDescriptorNameFn make_name =
        [this](const eastl::string& folder, const eastl::string& name_stem,
               const char* suffix) {
          return makeUniqueDescriptorName(folder, name_stem, suffix);
        };
    result.animation_clips = extractAndRegisterAnimationClipsFromGltf(
        m_file_system, m_asset_registry, m_content_browser, gltf_absolute, stem,
        make_name);
  }

  LOG_INFO(
      "[AssetImport] Source Export {} -> {} (Intermediate: {}, archived: {})",
      input_absolute.generic_string(), descriptor_virtual.c_str(),
      intermediate_virtual.c_str(), archived_virtual.c_str());
  return result;
}

ImportResult AssetImportService::importTexture(
    const fs::path& input_absolute, const eastl::string& assets_folder_virtual,
    const TextureImportSettings& settings) {
  ImportResult result{};
  if (!m_is_initialized || assets_folder_virtual.empty()) {
    return result;
  }

  const eastl::string ext = extensionLower(input_absolute);
  if (!isTextureIntermediateExtension(ext)) {
    LOG_WARN("[AssetImport] unsupported texture Intermediate {}",
             input_absolute.generic_string());
    return result;
  }

  const eastl::string assets_folder = resolveAssetsFolder(assets_folder_virtual);
  const eastl::string resource_virtual_path =
      registerIntermediateBody(m_file_system, input_absolute, "Textures");
  if (resource_virtual_path.empty()) {
    LOG_WARN("[AssetImport] failed to place texture Intermediate {}",
             input_absolute.generic_string());
    return result;
  }

  const eastl::string stem(input_absolute.stem().generic_string().c_str());
  const eastl::string descriptor_name =
      makeUniqueDescriptorName(assets_folder, stem, ".texture.yaml");
  if (descriptor_name.empty()) {
    return result;
  }

  TextureAssetDescriptor descriptor{};
  descriptor.guid = m_asset_registry->allocateGuid();
  // Descriptor field `source` = Intermediate path (glossary), not Source Asset.
  descriptor.source = resource_virtual_path;
  descriptor.import = settings;

  const eastl::string descriptor_virtual =
      joinVirtualPath(assets_folder, descriptor_name);
  eastl::string relative = descriptor_virtual;
  relative.erase(0, 7);
  const fs::path descriptor_absolute =
      m_file_system->resolveAsset(fs::path(relative.c_str()));

  m_file_system->ensureParentDirectory(descriptor_absolute);
  if (!m_file_system->writeText(descriptor_absolute,
                                AssetYaml::serializeTextureDescriptor(descriptor))) {
    return result;
  }

  m_asset_registry->registerAsset(descriptor.guid, descriptor_virtual);

  result.descriptor_virtual_path = descriptor_virtual;
  result.guid = descriptor.guid;
  result.success = true;

  LOG_INFO("[AssetImport] texture {} -> {} (Intermediate: {})",
           input_absolute.generic_string(), descriptor_virtual.c_str(),
           resource_virtual_path.c_str());
  return result;
}

eastl::vector<ImportResult> AssetImportService::importExternalFiles(
    const eastl::vector<eastl::string>& absolute_paths,
    const eastl::string& assets_folder_virtual,
    const MeshImportSettings& mesh_settings) {
  eastl::vector<ImportResult> results;
  if (!m_is_initialized) {
    return results;
  }

  eastl::vector<eastl::string> pending_meshes;
  std::vector<fs::path> gltf_batch_paths;
  for (const eastl::string& absolute_path : absolute_paths) {
    const fs::path src(absolute_path.c_str());
    const eastl::string ext = extensionLower(src);
    if (isMeshIntermediateExtension(ext) || isMeshSourceExportExtension(ext)) {
      pending_meshes.push_back(absolute_path);
      if (isMeshIntermediateExtension(ext)) {
        gltf_batch_paths.push_back(src);
      }
      continue;
    }
    if (isTextureIntermediateExtension(ext)) {
      TextureImportSettings texture_settings{};
      ImportResult imported =
          importTexture(src, assets_folder_virtual, texture_settings);
      if (imported.success) {
        results.push_back(imported);
      }
    }
  }

  const CompanionGltfMultiSelectBatchPairingResult pairing =
      pairCompanionAnimationGltfMultiSelectBatch(gltf_batch_paths);
  const bool allow_near_disk_discovery =
      pending_meshes.size() == 1 && gltf_batch_paths.size() == 1;

  if (mesh_settings.animations) {
    for (const fs::path& orphan_path : pairing.orphan_companion_paths) {
      LOG_WARN("[AssetImport] orphan Companion Animation glTF skipped: {}",
               orphan_path.generic_string());
    }
  }

  std::vector<fs::path> companion_paths_to_skip =
      pairing.orphan_companion_paths;
  for (const CompanionGltfBatchHostPairing& host_pairing :
       pairing.host_pairings) {
    companion_paths_to_skip.insert(companion_paths_to_skip.end(),
                                   host_pairing.companion_paths.begin(),
                                   host_pairing.companion_paths.end());
  }

  for (const eastl::string& absolute_path : pending_meshes) {
    const fs::path pending_path(absolute_path.c_str());
    const bool is_companion =
        std::any_of(companion_paths_to_skip.begin(),
                    companion_paths_to_skip.end(),
                    [&pending_path](const fs::path& companion_path) {
                      return pathsReferToSameFile(pending_path, companion_path);
                    });
    if (is_companion) {
      continue;
    }

    const auto host_pairing =
        std::find_if(pairing.host_pairings.begin(),
                     pairing.host_pairings.end(),
                     [&pending_path](
                         const CompanionGltfBatchHostPairing& candidate) {
                       return pathsReferToSameFile(pending_path,
                                                   candidate.host_path);
                     });

    const eastl::string pending_ext = extensionLower(pending_path);
    std::vector<fs::path> companion_paths_for_host;
    if (mesh_settings.animations) {
      if (host_pairing != pairing.host_pairings.end() &&
          !host_pairing->companion_paths.empty()) {
        companion_paths_for_host = host_pairing->companion_paths;
      } else if (allow_near_disk_discovery &&
                 isMeshIntermediateExtension(pending_ext) &&
                 isSkinnedMeshHostCandidateGltf(pending_path)) {
        companion_paths_for_host =
            discoverAcceptedNearDiskCompanionAnimationGltfs(pending_path);
      }
    }

    ImportResult imported;
    if (!companion_paths_for_host.empty() &&
        isMeshIntermediateExtension(pending_ext)) {
      imported = importMeshIntermediate(pending_path, assets_folder_virtual,
                                        mesh_settings,
                                        companion_paths_for_host);
    } else {
      imported =
          importMesh(pending_path, assets_folder_virtual, mesh_settings);
    }
    if (imported.success) {
      results.push_back(imported);
      for (const ImportResult& clip : imported.animation_clips) {
        if (clip.success) {
          results.push_back(clip);
        }
      }
    }
  }

  if (!results.empty() && m_content_browser) {
    m_content_browser->refresh();
  }
  return results;
}

eastl::vector<eastl::string> AssetImportService::findGuidsByArchivedSource(
    const fs::path& absolute_source_path) const {
  if (!m_is_initialized) {
    return {};
  }
  return guidsForArchivedSourcePath(absolute_source_path,
                                    m_file_system->getResourcesRoot(),
                                    *m_asset_registry, *m_file_system);
}

bool AssetImportService::requestReimport(const eastl::string& guid) {
  eastl::vector<eastl::string> guids;
  guids.push_back(guid);
  return requestReimports(guids);
}

bool AssetImportService::requestReimports(
    const eastl::vector<eastl::string>& guids) {
  if (!m_is_initialized || guids.empty()) {
    return false;
  }

  eastl::vector<eastl::string> valid;
  valid.reserve(guids.size());
  for (const eastl::string& guid : guids) {
    if (guid.empty()) {
      continue;
    }
    const eastl::string descriptor_path = m_asset_registry->resolveGuid(guid);
    if (descriptor_path.empty()) {
      LOG_WARN("[AssetImport] requestReimport: unknown guid {}", guid.c_str());
      continue;
    }
    valid.push_back(guid);
  }
  if (valid.empty()) {
    return false;
  }

  // One rebuildDependencyGraph, then per-GUID Source Export refresh + invalidate.
  // GUID is never reallocated: descriptor paths and registry entries stay put.
  if (m_asset_compiler) {
    m_asset_compiler->rebuildDependencyGraph();
  }

  bool any_ok = false;
  for (const eastl::string& guid : valid) {
    const eastl::string descriptor_virtual =
        m_asset_registry->resolveGuid(guid);
    const fs::path descriptor_absolute =
        resolveDescriptorAbsolute(m_file_system, descriptor_virtual);

    eastl::string yaml_text;
    if (!m_file_system->readText(descriptor_absolute, yaml_text)) {
      LOG_WARN("[AssetImport] requestReimport: failed to read {}",
               descriptor_virtual.c_str());
      continue;
    }

    if (!refreshIntermediateFromArchivedSource(m_file_system,
                                               descriptor_virtual, yaml_text)) {
      LOG_WARN(
          "[AssetImport] requestReimport: Intermediate refresh failed for "
          "guid={} (GUID preserved; still invalidating Finals)",
          guid.c_str());
    } else {
      MeshAssetDescriptor mesh_descriptor{};
      if (AssetYaml::parseMeshDescriptor(yaml_text, mesh_descriptor)) {
        const MakeUniqueDescriptorNameFn make_name =
            [this](const eastl::string& folder, const eastl::string& name_stem,
                   const char* suffix) {
              return makeUniqueDescriptorName(folder, name_stem, suffix);
            };
        refreshMeshAnimationClipsFromIntermediate(
            m_file_system, m_asset_registry, m_content_browser,
            descriptor_virtual, mesh_descriptor, make_name);
      }
    }

    LOG_INFO("[AssetImport] requestReimport guid={} descriptor={}",
             guid.c_str(), descriptor_virtual.c_str());
    if (m_asset_compiler) {
      m_asset_compiler->invalidateAssetAndDependents(guid);
    }
    any_ok = true;
  }
  return any_ok;
}

uint32_t AssetImportService::upgradeLegacyMeshIntermediates() {
  if (!m_is_initialized) {
    return 0;
  }

  const auto entries = m_asset_registry->registeredEntries();
  eastl::vector<eastl::pair<eastl::string, eastl::string>> mesh_candidates;
  mesh_candidates.reserve(entries.size());
  for (const auto& entry : entries) {
    const eastl::string& descriptor_virtual = entry.second;
    if (descriptor_virtual.size() < 10 ||
        descriptor_virtual.compare(descriptor_virtual.size() - 10, 10,
                                   ".mesh.yaml") != 0) {
      continue;
    }
    mesh_candidates.push_back(entry);
  }
  if (mesh_candidates.empty()) {
    return 0;
  }

  if (m_asset_compiler) {
    m_asset_compiler->rebuildDependencyGraph();
  }

  uint32_t migrated = 0;
  for (const auto& entry : mesh_candidates) {
    const eastl::string& guid = entry.first;
    const eastl::string& descriptor_virtual = entry.second;
    const fs::path descriptor_absolute =
        resolveDescriptorAbsolute(m_file_system, descriptor_virtual);

    eastl::string yaml_text;
    if (!m_file_system->readText(descriptor_absolute, yaml_text)) {
      continue;
    }

    MeshAssetDescriptor descriptor{};
    if (!AssetYaml::parseMeshDescriptor(yaml_text, descriptor)) {
      continue;
    }
    if (!isLegacyColladaIntermediateSource(descriptor.source)) {
      continue;
    }

    const fs::path dae_absolute =
        resolveResourcesVirtualPath(m_file_system, descriptor.source);
    if (!m_file_system->exists(dae_absolute)) {
      LOG_WARN(
          "[AssetImport] Intermediate migration skipped: missing source {} "
          "(guid={})",
          descriptor.source.c_str(), guid.c_str());
      continue;
    }

    const eastl::string gltf_virtual =
        migrateLegacyDaeIntermediateToGltf(m_file_system, descriptor);
    if (gltf_virtual.empty()) {
      const eastl::string sibling_gltf =
          replaceExtensionWithGltf(descriptor.source);
      const fs::path sibling_gltf_absolute =
          resolveResourcesVirtualPath(m_file_system, sibling_gltf);
      if (m_file_system->exists(sibling_gltf_absolute)) {
        std::error_code ec;
        fs::remove(sibling_gltf_absolute, ec);
      }
      LOG_WARN(
          "[AssetImport] Intermediate migration failed for guid={} source={} "
          "(leaving legacy .dae source unchanged)",
          guid.c_str(), descriptor.source.c_str());
      continue;
    }

    const eastl::string previous_source = descriptor.source;
    if (descriptor.archived_source.empty()) {
      const eastl::string archived =
          archiveSourceAsset(m_file_system, dae_absolute, "Models");
      if (archived.empty()) {
        LOG_WARN(
            "[AssetImport] Intermediate migration: archive failed for {} "
            "(guid={}); leaving descriptor unchanged",
            previous_source.c_str(), guid.c_str());
        const fs::path gltf_absolute =
            resolveResourcesVirtualPath(m_file_system, gltf_virtual);
        if (m_file_system->exists(gltf_absolute)) {
          std::error_code ec;
          fs::remove(gltf_absolute, ec);
        }
        continue;
      }
      descriptor.archived_source = archived;
    }

    descriptor.source = gltf_virtual;
    // GUID must stay stable across Intermediate migration.
    descriptor.guid = guid;

    if (!m_file_system->writeText(
            descriptor_absolute,
            AssetYaml::serializeMeshDescriptor(descriptor))) {
      LOG_WARN(
          "[AssetImport] Intermediate migration: failed to write descriptor {} "
          "(guid={}); leaving legacy source unchanged",
          descriptor_virtual.c_str(), guid.c_str());
      const fs::path gltf_absolute =
          resolveResourcesVirtualPath(m_file_system, gltf_virtual);
      if (m_file_system->exists(gltf_absolute)) {
        std::error_code ec;
        fs::remove(gltf_absolute, ec);
      }
      continue;
    }

    if (m_file_system->exists(dae_absolute)) {
      std::error_code ec;
      fs::remove(dae_absolute, ec);
    }

    LOG_INFO(
        "[AssetImport] Intermediate migration guid={} {} -> {} (archived={})",
        guid.c_str(), previous_source.c_str(), descriptor.source.c_str(),
        descriptor.archived_source.c_str());

    if (m_asset_compiler) {
      m_asset_compiler->invalidateAssetAndDependents(guid);
    }
    ++migrated;
  }

  return migrated;
}

uint32_t AssetImportService::scanAndUpgradeLegacyIntermediates() {
  if (!m_is_initialized) {
    return 0;
  }
  m_asset_registry->rebuildFromScan();
  return upgradeLegacyMeshIntermediates();
}

}  // namespace Blunder
