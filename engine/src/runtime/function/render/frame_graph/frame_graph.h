#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "runtime/function/render/rhi/i_gpu_buffer.h"
#include "runtime/function/render/rhi/i_gpu_texture.h"

namespace Blunder {

enum class FrameGraphResourceKind : uint8_t {
  Transient = 0,
  External = 1,
};

enum class FrameGraphResourceShape : uint8_t {
  Texture = 0,
  Buffer = 1,
};

enum class FrameGraphAccessKind : uint8_t {
  Read = 0,
  Write = 1,
};

enum class FrameGraphUsage : uint8_t {
  Sampled = 0,
  ColorAttachment = 1,
  DepthAttachment = 2,
  Storage = 3,
};

enum class FrameGraphCompileReason : uint8_t {
  Ok = 0,
  Cycle = 1,
  DanglingAccess = 2,
  NoSink = 3,
  InvalidPass = 4,
  InvalidDesc = 5,
  InvalidImport = 6,
};

enum class FrameGraphFormat : uint8_t {
  Undefined = 0,
  R8G8B8A8_UNORM = 1,
  D32_SFLOAT = 2,
};

struct FrameGraphResourceDesc {
  FrameGraphFormat format{FrameGraphFormat::Undefined};
  uint32_t width{0};
  uint32_t height{0};
  uint32_t sample_count{0};
  uint32_t mip_count{0};
  uint64_t size{0};
};

enum class FrameGraphExecuteReason : uint8_t {
  Ok = 0,
  NotCompiled = 1,
  NotAllocated = 2,
  NotPlanned = 3,
};

enum class FrameGraphAllocateReason : uint8_t {
  Ok = 0,
  NotCompiled = 1,
  AllocFailed = 2,
};

enum class FrameGraphPlanBarriersReason : uint8_t {
  Ok = 0,
  NotCompiled = 1,
};

struct FrameGraphHandle {
  uint32_t index{~0u};

  bool valid() const { return index != ~0u; }
};

struct FrameGraphPassHandle {
  uint32_t index{~0u};

  bool valid() const { return index != ~0u; }
};

struct FrameGraphCompileResult {
  bool ok{false};
  FrameGraphCompileReason reason{FrameGraphCompileReason::NoSink};
};

struct FrameGraphExecuteResult {
  bool ok{false};
  FrameGraphExecuteReason reason{FrameGraphExecuteReason::NotCompiled};
};

struct FrameGraphAllocateResult {
  bool ok{false};
  FrameGraphAllocateReason reason{FrameGraphAllocateReason::NotCompiled};
};

struct FrameGraphPlanBarriersResult {
  bool ok{false};
  FrameGraphPlanBarriersReason reason{FrameGraphPlanBarriersReason::NotCompiled};
};

struct FrameGraphResourceState {
  bool undefined{false};
  FrameGraphAccessKind access{FrameGraphAccessKind::Read};
  FrameGraphUsage usage{FrameGraphUsage::Sampled};
};

struct FrameGraphBarrier {
  FrameGraphHandle resource{};
  FrameGraphResourceState from{};
  FrameGraphResourceState to{};
  FrameGraphPassHandle after{};
  FrameGraphPassHandle before{};
};

class IFrameGraphAllocator {
 public:
  virtual ~IFrameGraphAllocator() = default;

  virtual std::unique_ptr<rhi::IGpuTexture> createTexture(
      const FrameGraphResourceDesc& desc) = 0;
  virtual std::unique_ptr<rhi::IGpuBuffer> createBuffer(
      const FrameGraphResourceDesc& desc) = 0;
};

class IFrameGraphRecorder {
 public:
  virtual ~IFrameGraphRecorder() = default;

  virtual void pipelineBarrier(const FrameGraphBarrier& barrier) = 0;
};

struct FrameGraphLifetime {
  uint32_t first{0};
  uint32_t last{0};
};

class FrameGraph;

class GraphBuilder final {
 public:
  explicit GraphBuilder(FrameGraph& graph);

  FrameGraphHandle createTransient(FrameGraphResourceShape shape,
                                   const FrameGraphResourceDesc& desc,
                                   const char* debug_name = nullptr);
  FrameGraphHandle importExternal(const FrameGraphResourceDesc& desc,
                                  rhi::IGpuTexture* texture,
                                  const char* debug_name = nullptr);
  FrameGraphHandle importExternal(const FrameGraphResourceDesc& desc,
                                  rhi::IGpuBuffer* buffer,
                                  const char* debug_name = nullptr);

  FrameGraphPassHandle addPass(const char* debug_name = nullptr);
  void markSink(FrameGraphPassHandle pass);
  void read(FrameGraphPassHandle pass, FrameGraphHandle resource,
            FrameGraphUsage usage);
  void write(FrameGraphPassHandle pass, FrameGraphHandle resource,
             FrameGraphUsage usage);
  void setExecute(FrameGraphPassHandle pass,
                  std::function<void(IFrameGraphRecorder&)> fn);

 private:
  FrameGraph* m_graph{nullptr};
};

class FrameGraph final {
 public:
  FrameGraph() = default;

  FrameGraphCompileResult compile();
  FrameGraphAllocateResult allocate(IFrameGraphAllocator& allocator);
  FrameGraphPlanBarriersResult planBarriers();
  FrameGraphExecuteResult execute(IFrameGraphRecorder& recorder);

  const std::vector<FrameGraphPassHandle>& livePasses() const {
    return m_live_passes;
  }

  const std::vector<FrameGraphBarrier>& barriers() const;

  bool isResourceLive(FrameGraphHandle resource) const;
  FrameGraphLifetime lifetime(FrameGraphHandle resource) const;
  FrameGraphResourceKind resourceKind(FrameGraphHandle resource) const;
  FrameGraphResourceShape resourceShape(FrameGraphHandle resource) const;
  FrameGraphResourceDesc resourceDesc(FrameGraphHandle resource) const;
  rhi::IGpuTexture* resolvedTexture(FrameGraphHandle resource) const;
  rhi::IGpuBuffer* resolvedBuffer(FrameGraphHandle resource) const;

 private:
  friend class GraphBuilder;

  struct Access {
    FrameGraphHandle resource{};
    FrameGraphAccessKind kind{FrameGraphAccessKind::Read};
    FrameGraphUsage usage{FrameGraphUsage::Sampled};
  };

  struct Resource {
    FrameGraphResourceKind kind{FrameGraphResourceKind::Transient};
    FrameGraphResourceShape shape{FrameGraphResourceShape::Texture};
    FrameGraphResourceDesc desc{};
    rhi::IGpuTexture* texture{nullptr};
    rhi::IGpuBuffer* buffer{nullptr};
    std::unique_ptr<rhi::IGpuTexture> owned_texture;
    std::unique_ptr<rhi::IGpuBuffer> owned_buffer;
    std::string debug_name;
    bool live{false};
    FrameGraphLifetime lifetime{};
  };

  struct Pass {
    std::string debug_name;
    bool sink{false};
    std::vector<Access> accesses;
    std::function<void(IFrameGraphRecorder&)> execute;
  };

  FrameGraphHandle addResource(FrameGraphResourceKind kind,
                               FrameGraphResourceShape shape,
                               const FrameGraphResourceDesc& desc,
                               const char* debug_name,
                               rhi::IGpuTexture* texture = nullptr,
                               rhi::IGpuBuffer* buffer = nullptr);
  bool descIllegal(const Resource& resource) const;
  bool importIllegal(const Resource& resource) const;
  void addAccess(FrameGraphPassHandle pass, FrameGraphHandle resource,
                 FrameGraphAccessKind kind, FrameGraphUsage usage);
  bool resourceIndexOk(FrameGraphHandle resource) const;
  bool passIndexOk(FrameGraphPassHandle pass) const;
  void clearCompileOutput();
  void destroyOwnedTransients();
  void clearBarrierPlan();
  void noteSetupMutation();
  FrameGraphCompileResult fail(FrameGraphCompileReason reason);

  std::vector<Resource> m_resources;
  std::vector<Pass> m_passes;
  std::vector<FrameGraphPassHandle> m_live_passes;
  std::vector<FrameGraphBarrier> m_barriers;
  std::vector<FrameGraphBarrier> m_empty_barriers;
  bool m_invalid_pass{false};
  bool m_compiled{false};
  bool m_allocated{false};
  bool m_barriers_planned{false};
};

}  // namespace Blunder
