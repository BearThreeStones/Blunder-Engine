#pragma once

#include <cstdint>

#include "EASTL/vector.h"

#include "runtime/resource/thumbnail/thumbnail_cache.h"
#include "runtime/resource/thumbnail/thumbnail_generation_queue.h"
#include "runtime/resource/thumbnail/thumbnail_placeholders.h"
#include "runtime/resource/thumbnail/thumbnail_types.h"

namespace Blunder {

class AssetManager;
class FileSystem;
class MeshPreviewRenderService;
class SceneThumbnailRenderService;
struct ContentEntry;

struct ThumbnailGeneratorInit {
  FileSystem* file_system{nullptr};
  AssetManager* asset_manager{nullptr};
  MeshPreviewRenderService* mesh_preview_service{nullptr};
  SceneThumbnailRenderService* scene_thumbnail_service{nullptr};
  uint32_t thumbnail_size{128};
};

class ThumbnailGenerator final {
 public:
  ThumbnailGenerator() = default;

  void initialize(const ThumbnailGeneratorInit& init);
  void shutdown();

  void setMeshPreviewService(MeshPreviewRenderService* service);
  void setSceneThumbnailService(SceneThumbnailRenderService* service);

  ThumbnailResult ensureThumbnail(const ContentEntry& entry);
  void ensureThumbnails(const ContentEntry* entries, uint32_t entry_count);

  /// Cache probe only; does not enqueue or generate.
  ThumbnailResult probeThumbnailStatus(const ContentEntry& entry);

  void enqueueThumbnail(const ContentEntry& entry,
                        ThumbnailQueuePriority priority);
  void demoteAllQueuedThumbnails(ThumbnailQueuePriority priority);
  void clearThumbnailQueue();
  eastl::vector<ThumbnailQueueCompleted> tickThumbnailQueue(uint32_t max_items);
  bool hasQueuedThumbnails() const;

  /// Synchronous RGBA generation without touching the disk cache (tests / hooks).
  bool generateThumbnailRgba(const ContentEntry& entry,
                             eastl::vector<uint8_t>& out_rgba);

 private:
  bool generateRgbaForEntry(const ContentEntry& entry,
                            eastl::vector<uint8_t>& out_rgba);
  bool generateMeshThumbnail(const eastl::string& virtual_path,
                             eastl::vector<uint8_t>& out_rgba);
  bool generateSceneThumbnail(const eastl::string& virtual_path,
                              eastl::vector<uint8_t>& out_rgba);
  bool generateImageThumbnail(const eastl::string& virtual_path,
                              eastl::vector<uint8_t>& out_rgba);
  bool generatePlaceholder(ThumbnailPlaceholderKind kind,
                           eastl::vector<uint8_t>& out_rgba);
  bool resolveDescriptorSource(const eastl::string& descriptor_virtual_path,
                               eastl::string& out_source_path) const;

  static bool shouldSkipEntry(const ContentEntry& entry);
  static bool endsWithSuffix(const eastl::string& value, const char* suffix);
  static eastl::string extensionLower(const eastl::string& virtual_path);

  FileSystem* m_file_system{nullptr};
  AssetManager* m_asset_manager{nullptr};
  MeshPreviewRenderService* m_mesh_preview_service{nullptr};
  SceneThumbnailRenderService* m_scene_thumbnail_service{nullptr};
  ThumbnailCache m_cache;
  ThumbnailGenerationQueue m_queue;
  uint32_t m_thumbnail_size{128};
  bool m_is_initialized{false};
};

}  // namespace Blunder
