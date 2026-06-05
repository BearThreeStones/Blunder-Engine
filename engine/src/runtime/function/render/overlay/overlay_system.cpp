#include "runtime/function/render/overlay/overlay_system.h"

#include "runtime/core/base/macro.h"
#include "runtime/function/editor/editor_selection_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/render/forward/forward_frame_state.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_system.h"
#include "runtime/function/render/offscreen_render_target.h"
#include "runtime/function/render/overlay/overlay_state.h"
#include "runtime/function/render/rhi/i_offscreen_render_target.h"
#include "runtime/function/render/slang/slang_compiler.h"
#include "runtime/function/render/vulkan/vulkan_allocator.h"
#include "runtime/function/render/vulkan/vulkan_context.h"
#include "runtime/function/render/vulkan_backend/vulkan_offscreen_target.h"

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

  auto& vk_target =
      static_cast<vulkan_backend::VulkanOffscreenTarget&>(*offscreen);
  m_native_offscreen = vk_target.nativeTarget();
  ASSERT(m_native_offscreen);

  m_screen_pass.initialize(ctx, m_native_offscreen);
  m_resources.screen_render_pass = m_screen_pass.renderPass();

  m_line_targets.initialize(ctx, alloc, m_native_offscreen);
  m_line_pass.initialize(ctx, m_native_offscreen, &m_line_targets);

  m_grid.initialize(ctx, alloc, m_resources, compiler);
  m_navigate_gizmo.initialize(m_resources, compiler);
  m_transform_gizmo.initialize(m_resources, compiler, m_native_offscreen);
  m_anti_aliasing.initialize(ctx, alloc, offscreen, compiler, &m_line_targets);
}

void OverlaySystem::shutdown() {
  m_anti_aliasing.shutdown();
  m_navigate_gizmo.shutdown();
  m_transform_gizmo.shutdown();
  m_grid.shutdown();
  m_line_pass.shutdown();
  m_line_targets.shutdown();
  m_screen_pass.shutdown();

  m_native_offscreen = nullptr;
  m_resources.screen_render_pass = VK_NULL_HANDLE;
  m_resources.offscreen = nullptr;
  m_resources.slang_compiler = nullptr;
  m_resources.vk_allocator = nullptr;
  m_resources.vk_context = nullptr;
}

void OverlaySystem::resize(uint32_t width, uint32_t height) {
  m_line_targets.resize(width, height);
  m_line_pass.resize(width, height);
  m_anti_aliasing.resize(width, height);
}

void OverlaySystem::begin_sync(const ForwardFrameState& frame_state,
                               uint32_t current_frame) {
  m_state = OverlayState::fromFrameState(frame_state, current_frame);
  m_state.gizmo_mode = m_transform_gizmo.controller().getMode();
  m_state.gizmo_space = m_transform_gizmo.controller().getSpace();

  if (g_runtime_global_context.m_editor_selection &&
      g_runtime_global_context.m_editor_selection->hasSelection() &&
      g_runtime_global_context.m_scene_system) {
    if (SceneInstance* scene =
            g_runtime_global_context.m_scene_system->getActiveInstance()) {
      m_state.has_selection = true;
      m_state.selection_world = scene->getWorldMatrix(
          g_runtime_global_context.m_editor_selection->getSelection());
    }
  }

  m_grid.begin_sync(m_resources, m_state);
  m_axes.begin_sync(m_resources, m_state);
  m_wireframe.begin_sync(m_resources, m_state);
  m_origins.begin_sync(m_resources, m_state);
  m_navigate_gizmo.begin_sync(m_resources, m_state);
  m_transform_gizmo.begin_sync(m_resources, m_state);
  m_anti_aliasing.begin_sync(m_resources, m_state);
}

bool OverlaySystem::hasActiveLineOverlays() const {
  return m_axes.isEnabled() || m_wireframe.isEnabled();
}

void OverlaySystem::draw_scene_overlays(VkCommandBuffer cmd) {
  m_axes.draw(cmd, m_state);
  m_wireframe.draw(cmd, m_state);
  m_origins.draw_color_only(cmd, m_state);
  m_transform_gizmo.draw_color_only(cmd, m_state);
}

void OverlaySystem::draw_overlay_lines(VkCommandBuffer cmd) {
  m_line_pass.begin(cmd);
  m_axes.draw_line(cmd, m_state);
  m_wireframe.draw_line(cmd, m_state);
  m_line_pass.end(cmd);
}

void OverlaySystem::draw_overlay_aa(VkCommandBuffer cmd) {
  m_anti_aliasing.apply(cmd, m_native_offscreen, m_state);
}

void OverlaySystem::draw_screen_overlays(VkCommandBuffer cmd) {
  m_screen_pass.begin(cmd);
  m_grid.draw_screen(cmd, m_state);
  m_navigate_gizmo.draw_screen(cmd, m_state);
  m_screen_pass.end(cmd);
}

}  // namespace Blunder
