#include "runtime/function/render/overlay/overlay_system.h"

#include "runtime/function/render/forward/forward_frame_state.h"
#include "runtime/function/render/overlay/overlay_state.h"
#include "runtime/function/render/rhi/i_offscreen_render_target.h"
#include "runtime/function/render/slang/slang_compiler.h"
#include "runtime/function/render/vulkan/vulkan_allocator.h"
#include "runtime/function/render/vulkan/vulkan_context.h"

namespace Blunder {

OverlaySystem::~OverlaySystem() {
  shutdown();
}

void OverlaySystem::initialize(VulkanContext* ctx, VulkanAllocator* alloc,
                               rhi::IOffscreenRenderTarget* offscreen,
                               SlangCompiler* compiler) {
  m_resources.vk_context = ctx;
  m_resources.vk_allocator = alloc;
  m_resources.slang_compiler = compiler;
  m_resources.offscreen = offscreen;

  // Initialize overlays that own GPU resources.
  m_grid.initialize(ctx, alloc, offscreen, compiler);
  m_anti_aliasing.initialize(ctx, alloc, offscreen, compiler);

  // Stub overlays need no GPU resources yet.
}

void OverlaySystem::shutdown() {
  m_anti_aliasing.shutdown();
  m_grid.shutdown();

  m_resources.offscreen = nullptr;
  m_resources.slang_compiler = nullptr;
  m_resources.vk_allocator = nullptr;
  m_resources.vk_context = nullptr;
}

void OverlaySystem::begin_sync(const ForwardFrameState& frame_state,
                               uint32_t current_frame) {
  m_state = OverlayState::fromFrameState(frame_state, current_frame);

  // Sync all overlays.
  m_grid.begin_sync(m_resources, m_state);
  m_axes.begin_sync(m_resources, m_state);
  m_wireframe.begin_sync(m_resources, m_state);
  m_origins.begin_sync(m_resources, m_state);
  m_anti_aliasing.begin_sync(m_resources, m_state);
}

void OverlaySystem::draw(VkCommandBuffer cmd) {
  // Line pass — grid, axes, wireframe (depth test ON, depth write OFF).
  m_grid.draw_line(cmd, m_state);
  m_axes.draw_line(cmd, m_state);
  m_wireframe.draw_line(cmd, m_state);

  // Solid pass.
  m_grid.draw(cmd, m_state);
  m_axes.draw(cmd, m_state);
  m_wireframe.draw(cmd, m_state);

  // Color-only pass (no depth test).
  m_origins.draw_color_only(cmd, m_state);

  // TODO: AA output pass (requires separate framebuffer).
  // m_anti_aliasing.draw_output(cmd, m_state);
}

}  // namespace Blunder
