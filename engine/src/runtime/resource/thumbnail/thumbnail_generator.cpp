#include "runtime/resource/thumbnail/thumbnail_generator.h"

#include <cstring>

#include "runtime/core/base/macro.h"
#include "runtime/function/render/mesh_preview/mesh_preview_render.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/asset_yaml.h"
#include "runtime/resource/asset/material_asset.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset/texture2d_asset.h"
#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/resource/content/content_entry.h"
#include "runtime/resource/thumbnail/thumbnail_cache.h"
#include "runtime/resource/thumbnail/thumbnail_placeholders.h"
#include "runtime/resource/thumbnail/thumbnail_resize.h"

namespace Blunder {

void ThumbnailGenerator::initialize(const ThumbnailGeneratorInit& init) {
  ASSERT(init.file_system);
  ASSERT(init.asset_manager);
  m_file_system = init.file_system;
  m_asset_manager = init.asset_manager;
  m_mesh_preview_service = init.mesh_preview_service;
  m_thumbnail_size = init.thumbnail_size > 0 ? init.thumbnail_size : 128;
  m_cache.bind(m_file_system);
  m_queue.bind(this);
  m_is_initialized = true;
}

void ThumbnailGenerator::shutdown() {
  m_is_initialized = false;
  m_file_system = nullptr;
  m_asset_manager = nullptr;
  m_mesh_preview_service = nullptr;
  m_queue.clear();
  m_cache.bind(nullptr);
}

void ThumbnailGenerator::setMeshPreviewService(
    MeshPreviewRenderService* service) {
  m_mesh_preview_service = service;
}

bool ThumbnailGenerator::endsWithSuffix(const eastl::string& value,
                                        const char* suffix) {
  const size_t suffix_length = std::strlen(suffix);
  if (value.size() < suffix_length) {
    return false;
  }
  return value.compare(value.size() - suffix_length, suffix_length, suffix) == 0;
}

bool ThumbnailGenerator::shouldSkipEntry(const ContentEntry& entry) {
  if (entry.virtual_path.empty()) {
    return true;
  }
  const eastl::string ext = extensionLower(entry.virtual_path);
  return ext == ".gitkeep";
}

eastl::string ThumbnailGenerator::extensionLower(
    const eastl::string& virtual_path) {
  const size_t dot = virtual_path.rfind('.');
  if (dot == eastl::string::npos) {
    return eastl::string();
  }
  eastl::string ext(virtual_path.c_str() + dot);
  for (char& c : ext) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }
  return ext;
}

bool ThumbnailGenerator::generatePlaceholder(ThumbnailPlaceholderKind kind,
                                             eastl::vector<uint8_t>& out_rgba) {
  fillThumbnailPlaceholder(kind, m_thumbnail_size, m_thumbnail_size, out_rgba);
  return !out_rgba.empty();
}

bool ThumbnailGenerator::generateImageThumbnail(
    const eastl::string& virtual_path, eastl::vector<uint8_t>& out_rgba) {
  const eastl::shared_ptr<Texture2DAsset> texture =
      m_asset_manager->loadTexture2D(virtual_path);
  if (!texture) {
    return false;
  }
  resizeTexture2DAssetToRgba8(*texture, m_thumbnail_size, m_thumbnail_size,
                              out_rgba);
  return !out_rgba.empty();
}

bool ThumbnailGenerator::generateMeshThumbnail(const eastl::string& virtual_path,
                                               eastl::vector<uint8_t>& out_rgba) {
  if (m_mesh_preview_service != nullptr) {
    MeshPreviewRenderRequest request{};
    request.mesh_virtual_path = virtual_path;
    request.width = m_thumbnail_size;
    request.height = m_thumbnail_size;
    const MeshPreviewRenderResult preview =
        m_mesh_preview_service->renderMeshAsset(virtual_path, request);
    if (preview.ok && !preview.rgba.empty()) {
      out_rgba = eastl::move(preview.rgba);
      return true;
    }
    return generatePlaceholder(ThumbnailPlaceholderKind::Mesh, out_rgba);
  }

  const eastl::shared_ptr<MeshAsset> mesh = m_asset_manager->loadMesh(virtual_path);
  if (!mesh) {
    return false;
  }

  return generatePlaceholder(ThumbnailPlaceholderKind::Mesh, out_rgba);
}

bool ThumbnailGenerator::resolveDescriptorSource(
    const eastl::string& descriptor_virtual_path,
    eastl::string& out_source_path) const {
  eastl::string relative = descriptor_virtual_path;
  if (relative.compare(0, 7, "assets/") == 0) {
    relative.erase(0, 7);
  }
  const std::filesystem::path absolute =
      m_file_system->resolveAsset(std::filesystem::path(relative.c_str()));

  eastl::string yaml_text;
  if (!m_file_system->readText(absolute, yaml_text)) {
    return false;
  }
  return AssetYaml::parseSourceField(yaml_text, out_source_path);
}

bool ThumbnailGenerator::generateRgbaForEntry(const ContentEntry& entry,
                                              eastl::vector<uint8_t>& out_rgba) {
  if (entry.is_directory) {
    return generatePlaceholder(ThumbnailPlaceholderKind::Folder, out_rgba);
  }

  const eastl::string ext = extensionLower(entry.virtual_path);
  if (endsWithSuffix(entry.virtual_path, ".mesh.yaml")) {
    eastl::string source_path;
    if (resolveDescriptorSource(entry.virtual_path, source_path)) {
      return generateMeshThumbnail(source_path, out_rgba);
    }
    return generatePlaceholder(ThumbnailPlaceholderKind::Mesh, out_rgba);
  }
  if (endsWithSuffix(entry.virtual_path, ".texture.yaml")) {
    eastl::string source_path;
    if (resolveDescriptorSource(entry.virtual_path, source_path)) {
      return generateImageThumbnail(source_path, out_rgba);
    }
    return generatePlaceholder(ThumbnailPlaceholderKind::File, out_rgba);
  }

  if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" ||
      ext == ".tga" || ext == ".ppm") {
    return generateImageThumbnail(entry.virtual_path, out_rgba);
  }

  if (ext == ".gltf" || ext == ".glb" ||
      endsWithSuffix(entry.virtual_path, ".mesh.asset")) {
    return generateMeshThumbnail(entry.virtual_path, out_rgba);
  }

  return generatePlaceholder(ThumbnailPlaceholderKind::File, out_rgba);
}

bool ThumbnailGenerator::generateThumbnailRgba(const ContentEntry& entry,
                                               eastl::vector<uint8_t>& out_rgba) {
  if (!m_is_initialized) {
    return false;
  }
  if (shouldSkipEntry(entry)) {
    return false;
  }
  return generateRgbaForEntry(entry, out_rgba);
}

ThumbnailResult ThumbnailGenerator::probeThumbnailStatus(
    const ContentEntry& entry) {
  ThumbnailResult result{};
  if (!m_is_initialized) {
    result.status = ThumbnailStatus::Failed;
    return result;
  }

  if (shouldSkipEntry(entry)) {
    result.status = ThumbnailStatus::Skipped;
    return result;
  }

  const ThumbnailCachePaths paths = m_cache.pathsForEntry(entry);
  if (m_cache.isCacheValid(entry, paths)) {
    result.status = ThumbnailStatus::CacheHit;
    result.cache_path = paths.png_path;
    return result;
  }

  result.status = ThumbnailStatus::None;
  return result;
}

void ThumbnailGenerator::enqueueThumbnail(const ContentEntry& entry,
                                          ThumbnailQueuePriority priority) {
  if (!m_is_initialized || shouldSkipEntry(entry)) {
    return;
  }
  const ThumbnailResult probe = probeThumbnailStatus(entry);
  if (probe.status == ThumbnailStatus::CacheHit ||
      probe.status == ThumbnailStatus::Skipped) {
    return;
  }
  m_queue.enqueue(entry, priority);
}

void ThumbnailGenerator::demoteAllQueuedThumbnails(
    ThumbnailQueuePriority priority) {
  m_queue.demoteAll(priority);
}

void ThumbnailGenerator::clearThumbnailQueue() {
  m_queue.clear();
}

eastl::vector<ThumbnailQueueCompleted> ThumbnailGenerator::tickThumbnailQueue(
    uint32_t max_items) {
  return m_queue.tick(max_items);
}

bool ThumbnailGenerator::hasQueuedThumbnails() const {
  return m_queue.pendingCount() > 0;
}

ThumbnailResult ThumbnailGenerator::ensureThumbnail(const ContentEntry& entry) {
  ThumbnailResult result{};
  if (!m_is_initialized) {
    result.status = ThumbnailStatus::Failed;
    return result;
  }

  if (shouldSkipEntry(entry)) {
    result.status = ThumbnailStatus::Skipped;
    return result;
  }

  const ThumbnailCachePaths paths = m_cache.pathsForEntry(entry);
  if (m_cache.isCacheValid(entry, paths)) {
    result.status = ThumbnailStatus::CacheHit;
    result.cache_path = paths.png_path;
    return result;
  }

  eastl::vector<uint8_t> rgba;
  if (!generateRgbaForEntry(entry, rgba)) {
    LOG_WARN("[ThumbnailGenerator] failed to generate thumbnail for {}",
             entry.virtual_path.c_str());
    result.status = ThumbnailStatus::Failed;
    return result;
  }

  if (!m_cache.writePng(paths, m_thumbnail_size, m_thumbnail_size, rgba.data(),
                        entry.modified_time)) {
    result.status = ThumbnailStatus::Failed;
    return result;
  }

  result.status = ThumbnailStatus::Generated;
  result.cache_path = paths.png_path;
  return result;
}

void ThumbnailGenerator::ensureThumbnails(const ContentEntry* entries,
                                            uint32_t entry_count) {
  if (entries == nullptr || entry_count == 0) {
    return;
  }
  for (uint32_t i = 0; i < entry_count; ++i) {
    ensureThumbnail(entries[i]);
  }
}

}  // namespace Blunder
