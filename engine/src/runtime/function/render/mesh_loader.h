#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>

#include "EASTL/shared_ptr.h"
#include "EASTL/string.h"
#include "EASTL/unique_ptr.h"
#include "EASTL/vector.h"

namespace Blunder {

class AssetManager;
class JobSystem;
class MeshAsset;

struct MeshLoaderImpl;

/// Render-owned unique Mesh Asset residency. CPU half is Jobs (cooked
/// `.meshbin` / Fast Path into Job data). GPU half is a per-tick `GpuMesh`
/// budget on the RHI thread. Not AssetManager. Not Texture Loader.
class MeshLoader final {
 public:
  static constexpr uint32_t kDefaultGpuMeshBudget = 4;

  struct InitInfo {
    JobSystem* job_system{nullptr};
    AssetManager* asset_manager{nullptr};
    uint32_t gpu_budget{kDefaultGpuMeshBudget};
  };

  struct Request {
    eastl::string key;
    eastl::string virtual_path;
    eastl::string guid;
    std::filesystem::path cooked_path;
    std::filesystem::path source_path;
    std::filesystem::path descriptor_path;
  };

  MeshLoader();
  ~MeshLoader();

  MeshLoader(const MeshLoader&) = delete;
  MeshLoader& operator=(const MeshLoader&) = delete;

  void initialize(const InitInfo& info);
  void shutdown();

  /// Poll Job data. Does not JobSystem::wait(). Does not create GpuMesh.
  void tick();

  /// Scene drop / replace: stale completions do not publish CPU meshes.
  void dropScene();

  /// Stop new work, wait remaining CPU Jobs.
  void stopAndWaitCpu();

  void enableGpu(bool enabled);
  void request(const Request& request);

  eastl::shared_ptr<MeshAsset> cpuMesh(const eastl::string& key) const;
  bool isFailed(const eastl::string& key) const;
  bool isGpuUploaded(const eastl::string& key) const;

  eastl::vector<eastl::string> gpuPendingKeys() const;
  void markGpuUploaded(const eastl::string& key);
  void markGpuFailed(const eastl::string& key);

  uint32_t submittedJobCount() const;
  uint32_t gpuEnqueueCount() const;
  uint32_t inFlightCount() const;
  uint32_t gpuBudget() const;
  uint64_t generation() const;
  bool isGpuEnabled() const;
  eastl::vector<eastl::string> queuedKeys() const;
  bool consumeResidencyChanged();

 private:
  eastl::unique_ptr<MeshLoaderImpl> m_impl;
};

}  // namespace Blunder
