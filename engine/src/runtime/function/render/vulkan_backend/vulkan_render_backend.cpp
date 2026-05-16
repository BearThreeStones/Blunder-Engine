#include "runtime/function/render/vulkan_backend/vulkan_render_backend.h"

#include "runtime/function/render/slang/slang_compiler.h"
#include "runtime/function/render/vulkan/vulkan_allocator.h"
#include "runtime/function/render/vulkan/vulkan_context.h"
#include "runtime/function/render/vulkan/vulkan_sync.h"

namespace Blunder::vulkan_backend {

VulkanRenderBackend::VulkanRenderBackend(const rhi::RenderBackendInitInfo& init) {
  m_slang_compiler = eastl::make_shared<SlangCompiler>();
  m_slang_compiler->initialize();

  m_context = eastl::make_shared<VulkanContext>();
  VulkanContextCreateInfo context_info{};
  context_info.window_system = init.device_desc.window_system;
  context_info.enable_validation = init.device_desc.enable_validation;
  m_context->initialize(context_info);

  m_allocator = eastl::make_shared<VulkanAllocator>();
  m_allocator->initialize(m_context.get());

  m_sync = eastl::make_shared<VulkanSync>();
  m_sync->initialize(m_context.get(), VulkanSync::k_max_frames_in_flight);

  m_device.bind(m_context.get(), m_allocator.get());
  m_shader_compiler.bind(m_slang_compiler.get());
  m_frame_sync.bind(m_sync.get());
}

VulkanRenderBackend::~VulkanRenderBackend() {
  if (m_sync) {
    m_sync->shutdown();
    m_sync.reset();
  }
  if (m_allocator) {
    m_allocator->shutdown();
    m_allocator.reset();
  }
  if (m_context) {
    m_context->shutdown();
    m_context.reset();
  }
  if (m_slang_compiler) {
    m_slang_compiler->shutdown();
    m_slang_compiler.reset();
  }
}

}  // namespace Blunder::vulkan_backend
