#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>

#include "EASTL/string.h"
#include "EASTL/unique_ptr.h"

namespace Blunder {

class JobSystem;
class Texture2DAsset;
class VulkanAllocator;
class VulkanContext;
class VulkanTexture;

struct TextureLoaderImpl;

/// Render-owned color material texture residency. CPU file+decode is Jobs.
/// GPU copies use a staging pool and timeline poll. Draws keep Bindless fallback
/// until the copy value is reached. Not AssetManager.
class TextureLoader final {
 public:
  struct InitInfo {
    JobSystem* job_system{nullptr};
    VulkanContext* context{nullptr};
    VulkanAllocator* allocator{nullptr};
  };

  TextureLoader();
  ~TextureLoader();

  TextureLoader(const TextureLoader&) = delete;
  TextureLoader& operator=(const TextureLoader&) = delete;

  void initialize(const InitInfo& info);
  void shutdown();

  /// Poll Job data and GPU timeline values. Does not JobSystem::wait().
  void tick();

  /// True if a copy timeline value adopted a resident image since the last consume.
  bool consumeResidencyChanged();

  /// Scene drop / replace: in-flight completions with a stale generation do
  /// not write Bindless slots or keep dropped GPU images.
  void dropScene();

  /// Stop new work, wait remaining CPU Jobs. GPU objects stay until shutdown
  /// after device idle.
  void stopAndWaitCpu();

  VulkanTexture* request(const Texture2DAsset* asset);
  void requestFile(const eastl::string& key,
                   const std::filesystem::path& absolute_path);

  uint32_t submittedJobCount() const;
  uint32_t gpuEnqueueCount() const;
  uint32_t inFlightCount() const;
  size_t decodedByteSize(const eastl::string& key) const;
  uint64_t generation() const;
  bool isGpuEnabled() const;

 private:
  eastl::unique_ptr<TextureLoaderImpl> m_impl;
};

}  // namespace Blunder
