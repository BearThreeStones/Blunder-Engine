#pragma once

#include "EASTL/unique_ptr.h"

#include "runtime/function/render/rhi/i_gpu_texture.h"

namespace Blunder {

class VulkanTexture;

namespace vulkan_backend {

class VulkanGpuTexture final : public rhi::IGpuTexture {
 public:
  void setTexture(eastl::unique_ptr<VulkanTexture> texture);

  VulkanTexture* nativeTexture() const { return m_texture.get(); }

 private:
  eastl::unique_ptr<VulkanTexture> m_texture;
};

}  // namespace vulkan_backend
}  // namespace Blunder
