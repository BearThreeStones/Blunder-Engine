#pragma once

#include "EASTL/unique_ptr.h"

#include "runtime/function/render/rhi/i_gpu_buffer.h"

namespace Blunder {

class VulkanBuffer;

namespace vulkan_backend {

class VulkanGpuBuffer final : public rhi::IGpuBuffer {
 public:
  void setBuffer(eastl::unique_ptr<VulkanBuffer> buffer);

  VulkanBuffer* nativeBuffer() const { return m_buffer.get(); }

  void upload(const void* data, uint64_t size) override;
  uint64_t size() const override;

 private:
  eastl::unique_ptr<VulkanBuffer> m_buffer;
};

}  // namespace vulkan_backend
}  // namespace Blunder
