#include "runtime/function/slint/slint_system.h"

#include <slint.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <exception>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3/SDL_timer.h>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/vec3.hpp>

#include "runtime/core/base/macro.h"
#include "runtime/resource/asset/material_asset.h"
#include "runtime/resource/content_browser/content_browser_system.h"
#include "runtime/resource/asset_import/asset_import_service.h"
#include "runtime/resource/asset_cook/asset_compiler_service.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/editor/editor_selection_system.h"
#include "runtime/function/editor/editor_scene_edit_system.h"
#include "runtime/function/editor/hierarchy_system.h"
#include "runtime/function/scene/entity.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_serializer.h"
#include "runtime/function/scene/scene_system.h"
#include "runtime/function/scene/scene_render_bridge.h"
#include "runtime/function/scene/scene_render_bridge.h"
#include "runtime/core/math/math_types.h"
#include "runtime/function/render/editor_camera.h"
#include "runtime/function/slint/window_pointer_map.h"
#include "runtime/function/render/overlay/navigate_gizmo_layout.h"
#include "runtime/function/render/gizmo/transform_gizmo_types.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/ui/ui_callback_binder.h"
#include "runtime/function/ui/ui_events.h"
#include "runtime/function/ui/ui_host.h"
#include "runtime/function/ui/docking/dock_widget.h"
#include "runtime/function/ui/docking/dock_node.h"
#include "runtime/platform/file_system/file_system.h"

namespace Blunder {
namespace {

slint::SharedString makeSpecialKeyString(std::u8string_view value) {
  std::string utf8(value.begin(), value.end());
  return slint::SharedString(utf8.c_str());
}

const char* renderingStateToString(slint::RenderingState state) {
  switch (state) {
    case slint::RenderingState::RenderingSetup:
      return "RenderingSetup";
    case slint::RenderingState::BeforeRendering:
      return "BeforeRendering";
    case slint::RenderingState::AfterRendering:
      return "AfterRendering";
    case slint::RenderingState::RenderingTeardown:
      return "RenderingTeardown";
    default:
      return "UnknownRenderingState";
  }
}

const char* graphicsApiToString(slint::GraphicsAPI graphics_api) {
  switch (graphics_api) {
    case slint::GraphicsAPI::NativeOpenGL:
      return "NativeOpenGL";
    case slint::GraphicsAPI::Inaccessible:
      return "Inaccessible";
    default:
      return "UnknownGraphicsAPI";
  }
}

SlintSystem::SlintPlatform* g_slint_platform_instance = nullptr;

int g_chrome_prev_logical_w = 0;
int g_chrome_prev_logical_h = 0;
int g_chrome_prev_px_w = 0;
int g_chrome_prev_px_h = 0;
float g_chrome_prev_scale = 0.0f;

void resetChromeSizeCache() {
  g_chrome_prev_logical_w = 0;
  g_chrome_prev_logical_h = 0;
  g_chrome_prev_px_w = 0;
  g_chrome_prev_px_h = 0;
  g_chrome_prev_scale = 0.0f;
}

}  // namespace

namespace {

class ScopedDispatchGuard final {
 public:
  explicit ScopedDispatchGuard(int& depth) : m_depth(depth) { ++m_depth; }
  ~ScopedDispatchGuard() { --m_depth; }

 private:
  int& m_depth;
};
}

slint::PhysicalSize queryLogicalWindowSize(WindowSystem* window_system) {
  if (!window_system) {
    return slint::PhysicalSize{{1u, 1u}};
  }
  const eastl::array<int, 2> window_size = window_system->getLogicalWindowSize();
  return slint::PhysicalSize(
      {static_cast<uint32_t>(window_size[0]), static_cast<uint32_t>(window_size[1])});
}

/// SDL logical size can lag after maximize; derive logical points from physical px / DPI.
slint::PhysicalSize reconcileLogicalWithPhysical(WindowSystem* window_system,
                                                 slint::PhysicalSize physical_size,
                                                 slint::PhysicalSize reported_logical) {
  if (!window_system || physical_size.width == 0 || physical_size.height == 0) {
    return reported_logical;
  }
  const float scale = window_system->getDisplayScale();
  if (scale <= 0.0f) {
    return reported_logical;
  }
  const uint32_t from_physical_w =
      static_cast<uint32_t>(static_cast<float>(physical_size.width) / scale + 0.5f);
  const uint32_t from_physical_h =
      static_cast<uint32_t>(static_cast<float>(physical_size.height) / scale + 0.5f);
  return slint::PhysicalSize(
      {eastl::max(reported_logical.width, from_physical_w),
       eastl::max(reported_logical.height, from_physical_h)});
}

float queryWindowScaleFactor(WindowSystem* window_system) {
  if (!window_system) {
    return 1.0f;
  }
  const float display_scale = window_system->getDisplayScale();
  if (display_scale > 0.0f) {
    return display_scale;
  }
  const eastl::array<int, 2> logical_size = window_system->getLogicalWindowSize();
  const eastl::array<int, 2> drawable_size = window_system->getDrawableSize();
  if (logical_size[0] <= 0) {
    return 1.0f;
  }
  return static_cast<float>(drawable_size[0]) /
         static_cast<float>(logical_size[0]);
}

SlintSystem::SlintWindowAdapter::SlintWindowAdapter(
    WindowSystem* window_system, uint64_t vk_instance,
    uint64_t vk_physical_device, uint64_t vk_device, uint32_t vk_queue_family)
    : m_window_system(window_system),
      m_vk_instance(vk_instance),
      m_vk_physical_device(vk_physical_device),
      m_vk_device(vk_device),
      m_vk_queue_family(vk_queue_family) {}

SlintSystem::~SlintSystem() { shutdown(); }

slint::platform::AbstractRenderer& SlintSystem::SlintWindowAdapter::renderer() {
  ensureRenderer();
  return *m_renderer;
}

slint::PhysicalSize SlintSystem::SlintWindowAdapter::size() {
  eastl::array<int, 2> drawable_size = m_window_system->getDrawableSize();
  return slint::PhysicalSize(
      {static_cast<uint32_t>(eastl::max(drawable_size[0], 1)),
       static_cast<uint32_t>(eastl::max(drawable_size[1], 1))});
}

void SlintSystem::SlintWindowAdapter::ensureRenderer() {
  if (m_renderer) {
    return;
  }
  ASSERT(m_window_system);
  if (!m_window_system) {
    return;
  }
#if defined(_WIN32) || defined(_WIN64)
  void* hwnd = m_window_system->getNativeWin32Hwnd();
  void* hinstance = m_window_system->getNativeWin32HInstance();
  const slint::PhysicalSize phys = size();
  if (!hwnd) {
    LOG_ERROR("[SlintSystem] native Win32 HWND is null");
    return;
  }
  slint::platform::NativeWindowHandle handle =
      slint::platform::NativeWindowHandle::from_win32(hwnd, hinstance);
  m_target_size = phys;
  m_committed_size = phys;
  m_renderer_size = phys;
  if (m_vk_device != 0) {
    // Shared-device path: composite on the engine's Vulkan device so the 3D
    // viewport image can be sampled zero-copy. Falls back internally to a
    // self-owned device if the shared handles cannot be adopted.
    LOG_INFO(
        "[SlintSystem] creating Skia renderer on shared engine Vulkan device");
    m_renderer = std::make_unique<slint::platform::SkiaRenderer>(
        handle, m_committed_size, m_vk_instance, m_vk_physical_device,
        m_vk_device, m_vk_queue_family);
  } else {
    m_renderer = std::make_unique<slint::platform::SkiaRenderer>(
        handle, m_committed_size);
  }
  // Records whether the shared-device path actually succeeded (the C++ ctor
  // falls back to a self-owned device on failure). Drives the engine's choice
  // between the zero-copy and CPU-readback viewport paths.
  m_uses_shared_device = m_renderer && m_renderer->uses_shared_vulkan();
  if (m_vk_device != 0 && !m_uses_shared_device) {
    LOG_WARN(
        "[SlintSystem] shared Vulkan device unavailable; using self-owned "
        "device (CPU viewport readback)");
  }
#else
#  error "Slint SkiaRenderer integration is currently implemented for Win32 only"
#endif
}

void SlintSystem::SlintWindowAdapter::set_visible(bool visible) {
  m_visible = visible;
  if (visible) {
    ensureRenderer();
    window().dispatch_scale_factor_change_event(
        queryWindowScaleFactor(m_window_system));
    pollDrawableSize();
    commitWindowSize(true);
    request_redraw();
  }
}

void SlintSystem::SlintWindowAdapter::request_redraw() { m_needs_redraw = true; }

void SlintSystem::SlintWindowAdapter::update_window_properties(
    const WindowProperties& properties) {
  const slint::SharedString title = properties.title();
  if (!title.empty()) {
    const std::string_view title_view(title.data(), title.size());
    std::string title_string(title_view.begin(), title_view.end());
    m_window_system->setTitle(title_string.c_str());
  }
}

void SlintSystem::SlintWindowAdapter::pollDrawableSize() {
  if (!m_visible) {
    return;
  }

  const slint::PhysicalSize physical_size = size();
  const slint::PhysicalSize reported_logical = queryLogicalWindowSize(m_window_system);
  const slint::PhysicalSize logical_size =
      reconcileLogicalWithPhysical(m_window_system, physical_size, reported_logical);
  if (physical_size.width == m_target_size.width &&
      physical_size.height == m_target_size.height &&
      logical_size.width == m_target_logical_size.width &&
      logical_size.height == m_target_logical_size.height) {
    return;
  }

  const slint::PhysicalSize previous_target = m_target_size;
  m_target_size = physical_size;
  m_target_logical_size = logical_size;
  m_size_stable_frames = 0;

  if (previous_target.width > 0 && previous_target.height > 0) {
    const uint32_t dw = previous_target.width > physical_size.width
                            ? previous_target.width - physical_size.width
                            : physical_size.width - previous_target.width;
    const uint32_t dh = previous_target.height > physical_size.height
                            ? previous_target.height - physical_size.height
                            : physical_size.height - previous_target.height;
    if (dw > previous_target.width / 3u || dh > previous_target.height / 3u) {
      const bool growing =
          physical_size.width > previous_target.width ||
          physical_size.height > previous_target.height;
      // Maximize grows the client — suppressing present leaves the old quarter-layout
      // frame stretched until cooldown ends.
      if (!growing) {
        m_present_suppress_frames = eastl::max(m_present_suppress_frames, 4u);
      }
    }
  }
}

void SlintSystem::SlintWindowAdapter::refreshTargetSizesFromWindow() {
  if (!m_visible || !m_window_system) {
    return;
  }
  const slint::PhysicalSize physical_size = size();
  const slint::PhysicalSize reported_logical = queryLogicalWindowSize(m_window_system);
  const slint::PhysicalSize logical_size =
      reconcileLogicalWithPhysical(m_window_system, physical_size, reported_logical);
  if (physical_size.width != m_target_size.width ||
      physical_size.height != m_target_size.height ||
      logical_size.width != m_target_logical_size.width ||
      logical_size.height != m_target_logical_size.height) {
    m_target_size = physical_size;
    m_target_logical_size = logical_size;
    m_size_stable_frames = 0;
  }
}

void SlintSystem::SlintWindowAdapter::applyWindowLayoutNow(int override_logical_w,
                                                           int override_logical_h) {
  if (!m_visible) {
    return;
  }

  refreshTargetSizesFromWindow();
  const float layout_scale = queryWindowScaleFactor(m_window_system);
  if (layout_scale > 0.0f && m_target_size.width > 0 && m_target_size.height > 0) {
    const uint32_t from_physical_w = static_cast<uint32_t>(
        static_cast<float>(m_target_size.width) / layout_scale + 0.5f);
    const uint32_t from_physical_h = static_cast<uint32_t>(
        static_cast<float>(m_target_size.height) / layout_scale + 0.5f);
    m_target_logical_size = slint::PhysicalSize(
        {eastl::max(from_physical_w, 1u), eastl::max(from_physical_h, 1u)});
  }
  if (override_logical_w > 0 && override_logical_h > 0) {
    const uint32_t override_w = static_cast<uint32_t>(override_logical_w);
    const uint32_t override_h = static_cast<uint32_t>(override_logical_h);
    m_target_logical_size = slint::PhysicalSize(
        {eastl::max(override_w, m_target_logical_size.width),
         eastl::max(override_h, m_target_logical_size.height)});
  }

  constexpr uint32_t k_min_present_extent = 8u;
  if (m_target_size.width < k_min_present_extent ||
      m_target_size.height < k_min_present_extent) {
    return;
  }

  const bool physical_size_changed =
      m_renderer &&
      (m_target_size.width != m_renderer_size.width ||
       m_target_size.height != m_renderer_size.height);
  if (physical_size_changed) {
    if (m_renderer) {
      m_renderer->resize(m_target_size);
      request_redraw();
      m_present_suppress_frames = eastl::max(m_present_suppress_frames, 1u);
    }
  }

  if (!m_renderer) {
    ensureRenderer();
    if (!m_renderer) {
      return;
    }
    m_renderer_size = m_target_size;
    m_committed_size = m_target_size;
    m_committed_logical_size = m_target_logical_size;
  }

  const float scale = queryWindowScaleFactor(m_window_system);
  window().dispatch_resize_event(
      slint::LogicalSize({static_cast<float>(m_target_logical_size.width),
                          static_cast<float>(m_target_logical_size.height)}));
  window().dispatch_scale_factor_change_event(scale);
  m_committed_logical_size = m_target_logical_size;
  m_committed_size = m_target_size;
  m_renderer_size = m_target_size;
  m_size_stable_frames = 0;
}

void SlintSystem::SlintWindowAdapter::commitWindowSize(bool force) {
  if (!m_visible) {
    return;
  }

  refreshTargetSizesFromWindow();

  constexpr uint32_t k_min_present_extent = 8u;
  if (m_target_size.width < k_min_present_extent ||
      m_target_size.height < k_min_present_extent) {
    return;
  }

  const bool layout_pending =
      m_target_logical_size.width != m_committed_logical_size.width ||
      m_target_logical_size.height != m_committed_logical_size.height ||
      m_target_size.width != m_committed_size.width ||
      m_target_size.height != m_committed_size.height;
  if (!layout_pending) {
    return;
  }

  if (force) {
    applyWindowLayoutNow();
    return;
  }

  if (++m_size_stable_frames < 2u) {
    return;
  }

  applyWindowLayoutNow();
}

void SlintSystem::SlintWindowAdapter::suppressPresentFrames(uint32_t frame_count) {
  m_present_suppress_frames = eastl::max(m_present_suppress_frames, frame_count);
}

void SlintSystem::SlintWindowAdapter::compositeFrame() {
  if (!m_visible || !m_renderer) {
    return;
  }
  if (m_present_suppress_frames > 0) {
    --m_present_suppress_frames;
    request_redraw();
    return;
  }
  if (m_committed_size.width < 8u || m_committed_size.height < 8u ||
      m_committed_logical_size.width < 8u || m_committed_logical_size.height < 8u) {
    request_redraw();
    return;
  }
  if (m_target_logical_size.width != m_committed_logical_size.width ||
      m_target_logical_size.height != m_committed_logical_size.height ||
      m_target_size.width != m_committed_size.width ||
      m_target_size.height != m_committed_size.height) {
    applyWindowLayoutNow();
    if (m_present_suppress_frames > 0) {
      request_redraw();
      return;
    }
    if (m_target_logical_size.width != m_committed_logical_size.width ||
        m_target_logical_size.height != m_committed_logical_size.height) {
      request_redraw();
      return;
    }
  }

  // Always re-render when requested: the engine updates the shared viewport
  // VkImage in place; Slint samples it during composite without re-binding each frame.
  try {
    m_renderer->render();
  } catch (const std::exception& e) {
    LOG_ERROR("[SlintSystem] SkiaRenderer::render failed: {}", e.what());
    m_present_suppress_frames = 2u;
    return;
  } catch (...) {
    LOG_ERROR("[SlintSystem] SkiaRenderer::render failed: unknown exception");
    m_present_suppress_frames = 2u;
    return;
  }
}

void SlintSystem::SlintWindowAdapter::renderIfNeeded() {
  pollDrawableSize();
  commitWindowSize();
  compositeFrame();
}

SlintSystem::SlintPlatform::SlintPlatform(
    WindowSystem* window_system, uint64_t vk_instance,
    uint64_t vk_physical_device, uint64_t vk_device, uint32_t vk_queue_family)
    : m_window_system(window_system),
      m_vk_instance(vk_instance),
      m_vk_physical_device(vk_physical_device),
      m_vk_device(vk_device),
      m_vk_queue_family(vk_queue_family) {}

std::unique_ptr<slint::platform::WindowAdapter>
SlintSystem::SlintPlatform::create_window_adapter() {
  auto adapter = std::make_unique<SlintWindowAdapter>(
      m_window_system, m_vk_instance, m_vk_physical_device, m_vk_device,
      m_vk_queue_family);
  m_window_adapter = adapter.get();
  return adapter;
}

slint::SharedString SlintSystem::mapKeycode(SDL_Keycode keycode) {
  // Fully qualify key_codes: Win32 wingdi.h declares ::Escape(HDC,...) which
  // collides with unqualified lookup when windows.h is included (SkiaRenderer).
  namespace keys = slint::platform::key_codes;

  switch (keycode) {
    case SDLK_RETURN:
      return makeSpecialKeyString(keys::Return);
    case SDLK_ESCAPE:
      return makeSpecialKeyString(keys::Escape);
    case SDLK_BACKSPACE:
      return makeSpecialKeyString(keys::Backspace);
    case SDLK_TAB:
      return makeSpecialKeyString(keys::Tab);
    case SDLK_SPACE:
      return makeSpecialKeyString(keys::Space);
    case SDLK_DELETE:
      return makeSpecialKeyString(keys::Delete);
    case SDLK_UP:
      return makeSpecialKeyString(keys::UpArrow);
    case SDLK_DOWN:
      return makeSpecialKeyString(keys::DownArrow);
    case SDLK_LEFT:
      return makeSpecialKeyString(keys::LeftArrow);
    case SDLK_RIGHT:
      return makeSpecialKeyString(keys::RightArrow);
    default:
      if (keycode >= 32 && keycode <= 126) {
        char text[2] = {static_cast<char>(keycode), '\0'};
        return slint::SharedString(text);
      }
      return {};
  }
}

slint::PointerEventButton SlintSystem::mapPointerButton(Uint8 button) {
  switch (button) {
    case SDL_BUTTON_LEFT:
      return slint::PointerEventButton::Left;
    case SDL_BUTTON_MIDDLE:
      return slint::PointerEventButton::Middle;
    case SDL_BUTTON_RIGHT:
      return slint::PointerEventButton::Right;
    case SDL_BUTTON_X1:
      return slint::PointerEventButton::Back;
    case SDL_BUTTON_X2:
      return slint::PointerEventButton::Forward;
    default:
      return slint::PointerEventButton::Other;
  }
}

bool SlintSystem::isSpecialKey(SDL_Keycode keycode) {
  switch (keycode) {
    case SDLK_RETURN:
    case SDLK_ESCAPE:
    case SDLK_BACKSPACE:
    case SDLK_TAB:
    case SDLK_SPACE:
    case SDLK_DELETE:
    case SDLK_UP:
    case SDLK_DOWN:
    case SDLK_LEFT:
    case SDLK_RIGHT:
      return true;
    default:
      return false;
  }
}

void SlintSystem::initialize(const SlintSystemInitInfo& init_info) {
  ASSERT(init_info.window_system);
  m_window_system = init_info.window_system;
  m_ui_host = init_info.ui_host;

  try {
    if (!g_slint_platform_instance) {
      auto platform = std::make_unique<SlintPlatform>(
          m_window_system, init_info.shared_vk_instance,
          init_info.shared_vk_physical_device, init_info.shared_vk_device,
          init_info.shared_vk_queue_family);
      g_slint_platform_instance = platform.get();
      slint::platform::set_platform(std::move(platform));
    }

    LOG_INFO(
        "[SlintSystem::initialize] creating Slint editor window and Skia renderer");

    auto component = MainEditorWindow::create();
    if (std::optional<slint::SetRenderingNotifierError> notifier_error =
        component->window().set_rendering_notifier(
          [](slint::RenderingState state,
             slint::GraphicsAPI graphics_api) {
            if (state == slint::RenderingState::RenderingSetup) {
            const char* graphics_api_name =
              graphicsApiToString(graphics_api);
            if (graphics_api == slint::GraphicsAPI::Inaccessible) {
              LOG_INFO(
                "[SlintSystem] Slint rendering setup via {}. "
                "This backend does not expose a native graphics API "
                "handle through the public C++ API; with "
                "RENDERER_SKIA_OPENGL disabled this is the expected "
                "shape for Vulkan-backed Skia.",
                graphics_api_name);
            } else {
              LOG_INFO(
                "[SlintSystem] Slint rendering setup via {}",
                graphics_api_name);
            }
            return;
            }

            if (state == slint::RenderingState::RenderingTeardown) {
            LOG_INFO(
              "[SlintSystem] Slint rendering teardown for {}",
              graphicsApiToString(graphics_api));
            }
          })) {
      LOG_WARN(
        "[SlintSystem::initialize] failed to install Slint rendering "
        "notifier: {}",
        static_cast<int>(*notifier_error));
    }
    component->on_sync_shading_from_asset(UiCallbackBinder::bind(
        m_ui_host, [](UiHost& host) { host.enqueue(UiEvent::simple(UiEventKind::syncShadingFromAsset)); }));

    component->on_save_scene_requested(UiCallbackBinder::bind(
        m_ui_host, [](UiHost& host) { host.enqueue(UiEvent::simple(UiEventKind::saveScene)); }));

    component->on_viewport_projection_toggled([this]() {
      // Slint TouchArea callback intentionally ignored.
      // C++ handles the toggle via SDL MOUSE_BUTTON_DOWN or Win32 pointer poll.
      (void)this;
    });



    component->on_hierarchy_entity_selected(UiCallbackBinder::bind(
        m_ui_host, [this](UiHost& host, int entity_id) {
          m_hierarchy_handled_by_slint = true;
          host.enqueue(UiEvent::selectEntity(static_cast<EntityId>(entity_id)));
        }));

    component->on_hierarchy_entity_toggle(UiCallbackBinder::bind(
        m_ui_host, [this](UiHost& host, int entity_id) {
          m_hierarchy_handled_by_slint = true;
          host.enqueue(UiEvent::toggleHierarchyNode(static_cast<EntityId>(entity_id)));
        }));

    component->on_inspector_transform_edited(UiCallbackBinder::bind(
        m_ui_host,
        [](UiHost& host) { host.enqueue(UiEvent::simple(UiEventKind::inspectorTransformEdited)); }));

    component->on_transform_mode_selected([](int mode) {
      if (!g_runtime_global_context.m_render_system) {
        return;
      }
      TransformGizmoMode gizmo_mode = TransformGizmoMode::none;
      switch (mode) {
        case 1:
          gizmo_mode = TransformGizmoMode::translate;
          break;
        case 2:
          gizmo_mode = TransformGizmoMode::rotate;
          break;
        case 3:
          gizmo_mode = TransformGizmoMode::scale;
          break;
        default:
          gizmo_mode = TransformGizmoMode::none;
          break;
      }
      g_runtime_global_context.m_render_system->setTransformGizmoMode(gizmo_mode);
      if (g_runtime_global_context.m_slint_system) {
        g_runtime_global_context.m_slint_system->syncTransformToolbarFromEngine();
      }
    });

    component->on_gizmo_space_toggled([this]() {
      if (g_runtime_global_context.m_render_system) {
        g_runtime_global_context.m_render_system->toggleTransformGizmoSpace();
        syncTransformToolbarFromEngine();
      }
    });

    component->on_browser_refresh_requested(UiCallbackBinder::bind(
        m_ui_host, [](UiHost& host) { host.enqueue(UiEvent::simple(UiEventKind::browserRefresh)); }));
    component->on_browser_import_requested([this]() { queueOpenImportFileDialog(); });
    component->on_import_mesh_confirmed([this]() { completePendingMeshImport(); });
    component->on_import_mesh_cancelled([this]() {
      m_pending_mesh_import_paths.clear();
      if (m_window_component) {
        m_window_component->operator->()->set_import_mesh_dialog_visible(false);
      }
    });
    component->on_browser_folder_selected(UiCallbackBinder::bind(
        m_ui_host, [this](UiHost& host, const slint::SharedString& path) {
          m_tree_folder_handled_by_slint = true;
          host.enqueue(UiEvent::withPath(UiEventKind::browserFolderSelected,
                                       eastl::string(path.data())));
        }));
    component->on_browser_folder_toggle(UiCallbackBinder::bind(
        m_ui_host, [this](UiHost& host, const slint::SharedString& path) {
          m_tree_folder_handled_by_slint = true;
          host.enqueue(UiEvent::withPath(UiEventKind::browserFolderToggle,
                                       eastl::string(path.data())));
        }));
    component->on_browser_item_press(
        [this](const slint::SharedString& path, float x, float y) {
          const auto services = lockServices();
          if (!services || !services->content_browser) {
            return;
          }
          services->content_browser->dragController().beginPress(
              eastl::string(path.data()), x, y);
          m_drop_highlight_path.clear();
          syncContentBrowser();
        });
    component->on_browser_item_move(
        [this](const slint::SharedString& path, float x, float y) {
          (void)path;
          const auto services = lockServices();
          if (!services || !services->content_browser) {
            return;
          }
          services->content_browser->dragController().updateMove(x, y);
        });
    component->on_browser_item_release(
        [this](const slint::SharedString& path, float x, float y) {
          (void)x;
          (void)y;
          const auto services = lockServices();
          if (!services || !services->content_browser) {
            return;
          }
          ContentBrowserSystem& browser_system = *services->content_browser;
          ContentBrowserDragController& drag = browser_system.dragController();
          const bool was_dragging = drag.isDragging();

          if (was_dragging) {
            finishContentBrowserDragAtCursor();
            return;
          }

          drag.endPress();
          drag.reset();
          m_drop_highlight_path.clear();
          m_viewport_drop_active = false;
          syncContentBrowser();

          const eastl::string path_str(path.data());

          // Discrete navigation/open actions are routed through the UiEventQueue
          // so they run on the main thread outside Slint dispatch.
          const ContentEntry* entry = browser_system.findEntry(path_str);
          if (entry && entry->is_directory) {
            if (const auto host = m_ui_host.lock()) {
              host->enqueue(UiEvent::withPath(UiEventKind::browserFolderSelected,
                                              path_str));
            }
            return;
          }

          constexpr const char* k_scene_suffix = ".scene.asset";
          const size_t suffix_len = 14u;
          if (path_str.size() >= suffix_len &&
              path_str.compare(path_str.size() - suffix_len, suffix_len,
                               k_scene_suffix) == 0) {
            if (const auto host = m_ui_host.lock()) {
              host->enqueue(
                  UiEvent::withPath(UiEventKind::openSceneAsset, path_str));
            }
          }
        });

    component->on_browser_search_changed(UiCallbackBinder::bind(
        m_ui_host, [](UiHost& host, const slint::SharedString& text) {
          host.enqueue(UiEvent::withPath(UiEventKind::browserSearchChanged,
                                       eastl::string(text.data())));
        }));

    component->on_browser_path_segment_clicked(UiCallbackBinder::bind(
        m_ui_host, [](UiHost& host, const slint::SharedString& path) {
          host.enqueue(UiEvent::withPath(UiEventKind::browserPathSegmentClicked,
                                       eastl::string(path.data())));
        }));

    component->on_docking_tab_pressed([this](int widget_id, float x, float y) {
      m_dock_manager.beginDrag(static_cast<DockId>(widget_id), glm::vec2{x, y});
      m_docking_model_dirty = true;
    });
    component->on_docking_tab_moved([this](float x, float y) {
      m_dock_manager.handleDragMove(glm::vec2{x, y});
      m_docking_model_dirty = true;
    });
    component->on_docking_tab_released([this](float x, float y) {
      m_dock_manager.handleDragMove(glm::vec2{x, y});
      m_dock_manager.endDrag();
      m_docking_model_dirty = true;
    });
    component->on_docking_tab_activated([this](int widget_id) {
      m_dock_manager.activateWidget(static_cast<DockId>(widget_id));
      m_docking_model_dirty = true;
    });
    component->on_docking_widget_closed([this](int widget_id) {
      m_dock_manager.closeWidget(static_cast<DockId>(widget_id));
      m_docking_model_dirty = true;
    });
    component->on_docking_splitter_pressed([](int, float, float) {});
    component->on_docking_splitter_moved([this](int node_id, float x, float y) {
      m_dock_manager.resizeSplitter(static_cast<DockId>(node_id), glm::vec2{x, y});
      m_docking_model_dirty = true;
    });
    component->on_docking_splitter_released([]() {});
    component->on_docking_floating_move_pressed([this](int node_id, float x, float y) {
      m_dock_manager.beginFloatingMove(static_cast<DockId>(node_id), glm::vec2{x, y});
      m_docking_model_dirty = true;
    });
    component->on_docking_floating_resize_pressed([this](int node_id, float x, float y) {
      m_dock_manager.beginFloatingResize(static_cast<DockId>(node_id), glm::vec2{x, y});
      m_docking_model_dirty = true;
    });
    component->on_docking_floating_moved([this](float x, float y) {
      m_dock_manager.updateFloatingInteraction(glm::vec2{x, y});
      m_docking_model_dirty = true;
    });
    component->on_docking_floating_released([this]() {
      m_dock_manager.endFloatingInteraction();
      m_docking_model_dirty = true;
    });

    component->show();
    m_window_component = component;

    seedDockingWorkspace();

    m_window_adapter = g_slint_platform_instance->getWindowAdapter();
    ASSERT(m_window_adapter);
    m_window_adapter->setOwner(this);

    m_window_system->setNativeEventCallback(
        [this](const SDL_Event& event) { processEvent(event); });

    syncWindowChromeSize();
    m_force_window_commit = true;
    m_window_adapter->pollDrawableSize();
    m_window_adapter->commitWindowSize(true);
    cacheLayoutRects();
    if (const auto services = lockServices()) {
      if (services->render_system) {
        if (EditorCamera* camera = services->render_system->getEditorCamera()) {
          syncViewportProjectionMode(camera->getProjectionMode() ==
                                     EditorCamera::ProjectionMode::perspective);
        }
      }
    }
    m_open_import_dialog_event = SDL_RegisterEvents(1);
    if (m_open_import_dialog_event == 0) {
      LOG_WARN("[SlintSystem] SDL_RegisterEvents failed for import dialog: {}",
               SDL_GetError());
    }

    LOG_INFO("[SlintSystem] editor initialized");
  } catch (const std::exception& e) {
    LOG_ERROR("[SlintSystem::initialize] {}", e.what());
    m_window_component.reset();
    m_window_adapter = nullptr;
    if (m_window_system) {
      m_window_system->requestClose();
    }
  } catch (...) {
    LOG_ERROR("[SlintSystem::initialize] unknown exception");
    m_window_component.reset();
    m_window_adapter = nullptr;
    if (m_window_system) {
      m_window_system->requestClose();
    }
  }
}

void SlintSystem::shutdown() {
  if (m_window_component) {
    m_window_component->operator->()->hide();
    m_window_component.reset();
  }

  if (m_window_system) {
    m_window_system->setNativeEventCallback({});
  }

  m_window_adapter = nullptr;
  m_window_system = nullptr;
}

void SlintSystem::applyPendingViewportInvalidate() {
  if (!m_pending_viewport_invalidate || !m_window_component) {
    return;
  }
  m_pending_viewport_invalidate = false;
  m_viewport_image_stale = true;
  m_borrowed_viewport_image_bound = false;
  m_borrowed_viewport_vk_image = 0;
  static const uint8_t k_clear_pixel[4] = {10u, 10u, 10u, 255u};
  setViewportImageInternal(k_clear_pixel, 1u, 1u, true);
  LOG_INFO(
      "[SlintSystem] viewport invalidate: hide 3D image until next render ({}x{} "
      "logical)",
      m_cached_viewport_logical_rect.width, m_cached_viewport_logical_rect.height);
}

void SlintSystem::setViewportImage(const uint8_t* pixels_rgba, uint32_t width,
                                   uint32_t height) {
  setViewportImageInternal(pixels_rgba, width, height, false);
}

void SlintSystem::setViewportExternalTexture(uint64_t image, uint32_t format,
                                             uint32_t layout, uint32_t width,
                                             uint32_t height) {
  if (!m_window_component || image == 0 || width == 0 || height == 0) {
    return;
  }
  if (m_slint_dispatch_depth > 0) {
    return;
  }
  static bool s_logged_first_zero_copy = false;
  try {
    ScopedDispatchGuard guard(m_slint_dispatch_depth);
    MainEditorWindow& ui = *m_window_component->operator->();
    const bool rebind =
        !m_borrowed_viewport_image_bound || image != m_borrowed_viewport_vk_image ||
        width != m_viewport_upload_width || height != m_viewport_upload_height;
    if (rebind) {
      slint::Image viewport_image =
          slint::Image::create_from_borrowed_vulkan_texture(
              image, format, layout, slint::Size<uint32_t>{width, height});
      ui.set_viewport_image(viewport_image);
      ui.set_viewport_image_ready(true);
      m_borrowed_viewport_image_bound = true;
      m_borrowed_viewport_vk_image = image;
      m_viewport_image_dirty = true;
      if (!s_logged_first_zero_copy) {
        LOG_INFO(
            "[SlintSystem] zero-copy viewport: first shared VkImage present {}x{}",
            width, height);
        s_logged_first_zero_copy = true;
      }
    }
    m_viewport_image_stale = false;
    m_viewport_upload_width = width;
    m_viewport_upload_height = height;
    m_viewport_frame_ready = true;
    if (rebind && m_window_adapter) {
      m_window_adapter->request_redraw();
    }
  } catch (const std::exception& e) {
    LOG_ERROR("[SlintSystem::setViewportExternalTexture] {}", e.what());
  }
}

bool SlintSystem::viewportUsesSharedDevice() const {
  return m_window_adapter && m_window_adapter->usesSharedDevice();
}

void SlintSystem::setViewportImageInternal(const uint8_t* pixels_rgba,
                                           uint32_t width, uint32_t height,
                                           bool allow_during_dispatch) {
  if (!m_window_component || !pixels_rgba || width == 0 || height == 0) {
    return;
  }
  if (!allow_during_dispatch && m_slint_dispatch_depth > 0) {
    return;
  }
  static bool s_logged_first_viewport_upload = false;
  static uint32_t s_last_upload_w = 0;
  static uint32_t s_last_upload_h = 0;
  const bool image_ready = width > 1u && height > 1u;
  try {
    ScopedDispatchGuard guard(m_slint_dispatch_depth);
    MainEditorWindow& ui = *m_window_component->operator->();
    slint::SharedPixelBuffer<slint::Rgba8Pixel> buffer(
        width, height, reinterpret_cast<const slint::Rgba8Pixel*>(pixels_rgba));
    const bool size_changed =
        width != m_viewport_upload_width || height != m_viewport_upload_height;
    ui.set_viewport_image(slint::Image(buffer));
    ui.set_viewport_image_ready(image_ready);
    m_viewport_image_dirty = size_changed;
    m_viewport_image_stale = !image_ready;
    m_viewport_upload_width = width;
    m_viewport_upload_height = height;
    m_viewport_frame_ready = image_ready;
    if (size_changed && m_window_adapter) {
      m_window_adapter->request_redraw();
    }
    if (image_ready) {
      if (!s_logged_first_viewport_upload) {
        LOG_INFO(
            "[SlintSystem] viewport Persp/Iso uses Slint overlay; first 3D upload "
            "{}x{}",
            width, height);
        s_logged_first_viewport_upload = true;
        s_last_upload_w = width;
        s_last_upload_h = height;
      } else if (width != s_last_upload_w || height != s_last_upload_h) {
        LOG_INFO("[SlintSystem] viewport image upload resized: {}x{} -> {}x{}",
                 s_last_upload_w, s_last_upload_h, width, height);
        s_last_upload_w = width;
        s_last_upload_h = height;
      }
    }
  } catch (const std::exception& e) {
    LOG_ERROR("[SlintSystem::setViewportImageInternal] {}", e.what());
    if (m_window_system) {
      m_window_system->requestClose();
    }
  } catch (...) {
    LOG_ERROR("[SlintSystem::setViewportImageInternal] unknown exception");
    if (m_window_system) {
      m_window_system->requestClose();
    }
  }
}

ViewportLogicalRect SlintSystem::getViewportLogicalRect() const {
  return m_cached_viewport_logical_rect;
}

eastl::array<uint32_t, 2> SlintSystem::getViewportLogicalSize() const {
  return {m_cached_viewport_logical_rect.width,
          m_cached_viewport_logical_rect.height};
}

eastl::array<uint32_t, 2> SlintSystem::getViewportRenderTargetSize() const {
  uint32_t vp_w = m_cached_viewport_logical_rect.width;
  uint32_t vp_h = m_cached_viewport_logical_rect.height;
  float scale = 1.0f;
  if (m_window_system) {
    scale = eastl::max(m_window_system->getDisplayScale(), 1.0f);
  }
  return {static_cast<uint32_t>(static_cast<float>(vp_w) * scale + 0.5f),
          static_cast<uint32_t>(static_cast<float>(vp_h) * scale + 0.5f)};
}

void SlintSystem::syncViewportProjectionMode(bool is_perspective) {
  if (!m_window_component) {
    return;
  }
  if (m_cached_viewport_is_perspective == is_perspective) {
    return;
  }
  m_cached_viewport_is_perspective = is_perspective;
  try {
    ScopedDispatchGuard guard(m_slint_dispatch_depth);
    m_window_component->operator->()->set_viewport_is_perspective(is_perspective);
  } catch (const std::exception& e) {
    LOG_ERROR("[SlintSystem::syncViewportProjectionMode] {}", e.what());
  } catch (...) {
    LOG_ERROR("[SlintSystem::syncViewportProjectionMode] unknown exception");
  }
}

BlinnPhongEditorSettings SlintSystem::getBlinnPhongEditorSettings() const {
  BlinnPhongEditorSettings settings{};
  if (!m_window_component) {
    return settings;
  }

  try {
    const auto& ui = *m_window_component;
    settings.light_direction = glm::vec3(ui->get_light_dir_x(), ui->get_light_dir_y(),
                                         ui->get_light_dir_z());
    settings.light_color = glm::vec3(ui->get_light_color_r(), ui->get_light_color_g(),
                                     ui->get_light_color_b());
    settings.ambient_color = glm::vec3(ui->get_ambient_r(), ui->get_ambient_g(),
                                       ui->get_ambient_b());
    settings.diffuse_color = glm::vec3(ui->get_diffuse_r(), ui->get_diffuse_g(),
                                       ui->get_diffuse_b());
    settings.specular_color = glm::vec3(ui->get_specular_r(), ui->get_specular_g(),
                                        ui->get_specular_b());
    settings.shininess = ui->get_shininess();
    settings.unlit = ui->get_shading_unlit();
    settings.ssao_enabled = ui->get_ssao_enabled();
    settings.ssao_radius = ui->get_ssao_radius();
    settings.ssao_bias = ui->get_ssao_bias();
    settings.ssao_strength = ui->get_ssao_strength();
  } catch (const std::exception& e) {
    LOG_ERROR("[SlintSystem::getBlinnPhongEditorSettings] {}", e.what());
  } catch (...) {
    LOG_ERROR("[SlintSystem::getBlinnPhongEditorSettings] unknown exception");
  }

  return settings;
}

BlinnPhongEditorSettings SlintSystem::pullPreviewSettingsFromSlint() const {
  return getBlinnPhongEditorSettings();
}

void SlintSystem::pushPreviewSettingsToSlint(const BlinnPhongEditorSettings& settings) {
  if (!m_window_component) {
    return;
  }

  try {
    ScopedDispatchGuard guard(m_slint_dispatch_depth);
    auto& ui = *m_window_component;
    ui->set_light_dir_x(settings.light_direction.x);
    ui->set_light_dir_y(settings.light_direction.y);
    ui->set_light_dir_z(settings.light_direction.z);
    ui->set_light_color_r(settings.light_color.r);
    ui->set_light_color_g(settings.light_color.g);
    ui->set_light_color_b(settings.light_color.b);
    ui->set_ambient_r(settings.ambient_color.r);
    ui->set_ambient_g(settings.ambient_color.g);
    ui->set_ambient_b(settings.ambient_color.b);
    ui->set_diffuse_r(settings.diffuse_color.r);
    ui->set_diffuse_g(settings.diffuse_color.g);
    ui->set_diffuse_b(settings.diffuse_color.b);
    ui->set_specular_r(settings.specular_color.r);
    ui->set_specular_g(settings.specular_color.g);
    ui->set_specular_b(settings.specular_color.b);
    ui->set_shininess(settings.shininess);
    ui->set_shading_unlit(settings.unlit);
    ui->set_ssao_enabled(settings.ssao_enabled);
    ui->set_ssao_radius(settings.ssao_radius);
    ui->set_ssao_bias(settings.ssao_bias);
    ui->set_ssao_strength(settings.ssao_strength);
  } catch (const std::exception& e) {
    LOG_ERROR("[SlintSystem::pushPreviewSettingsToSlint] {}", e.what());
  } catch (...) {
    LOG_ERROR("[SlintSystem::pushPreviewSettingsToSlint] unknown exception");
  }
}

void SlintSystem::setBlinnPhongMaterialSource(const MaterialAsset* material) {
  m_blinn_phong_material_source = material;
}

void SlintSystem::syncBlinnPhongFromMaterialSource() {
  if (!m_window_component || !m_blinn_phong_material_source) {
    return;
  }
  if (m_slint_dispatch_depth > 0) {
    return;
  }

  const MaterialAsset& material = *m_blinn_phong_material_source;
  try {
    ScopedDispatchGuard guard(m_slint_dispatch_depth);
    auto& ui = *m_window_component;
    ui->set_ambient_r(material.getAmbientColor().x);
    ui->set_ambient_g(material.getAmbientColor().y);
    ui->set_ambient_b(material.getAmbientColor().z);
    ui->set_diffuse_r(material.getDiffuseColor().x);
    ui->set_diffuse_g(material.getDiffuseColor().y);
    ui->set_diffuse_b(material.getDiffuseColor().z);
    ui->set_specular_r(material.getSpecularColor().x);
    ui->set_specular_g(material.getSpecularColor().y);
    ui->set_specular_b(material.getSpecularColor().z);
    ui->set_shininess(material.getShininess());
    ui->set_shading_unlit(material.isUnlit());
  } catch (const std::exception& e) {
    LOG_ERROR("[SlintSystem::syncBlinnPhongFromMaterialSource] {}", e.what());
  } catch (...) {
    LOG_ERROR("[SlintSystem::syncBlinnPhongFromMaterialSource] unknown exception");
  }
}

namespace {

slint::Image loadThumbnailImage(const eastl::string& cache_path) {
  if (cache_path.empty()) {
    return slint::Image();
  }
  try {
    return slint::Image::load_from_path(
        slint::SharedString(cache_path.c_str()));
  } catch (...) {
    return slint::Image();
  }
}

bool pointInRect(float x, float y, const BrowserLogicalRect& rect) {
  if (rect.width == 0 || rect.height == 0) {
    return false;
  }
  return x >= static_cast<float>(rect.x) &&
         y >= static_cast<float>(rect.y) &&
         x < static_cast<float>(rect.x + static_cast<int32_t>(rect.width)) &&
         y < static_cast<float>(rect.y + static_cast<int32_t>(rect.height));
}

BrowserLogicalRect readBrowserRectFromUi(const MainEditorWindow& ui) {
  BrowserLogicalRect rect{};
  const float w = ui.get_browser_width();
  const float h = ui.get_browser_height();
  rect.x = static_cast<int32_t>(ui.get_browser_origin_x());
  rect.y = static_cast<int32_t>(ui.get_browser_origin_y());
  rect.width = w > 0.0f ? static_cast<uint32_t>(w) : 0u;
  rect.height = h > 0.0f ? static_cast<uint32_t>(h) : 0u;
  return rect;
}

struct SlintPointerCoords {
  float x{0.0f};
  float y{0.0f};
};

SlintPointerCoords mapWindowPointerToSlint(WindowSystem* window_system, float x,
                                           float y) {
  const eastl::array<float, 2> logical =
      mapWindowPointerToLogical(window_system, x, y);
  return {logical[0], logical[1]};
}

struct WindowClientMouseState {
  float x{0.0f};
  float y{0.0f};
  bool left_down{false};
  const char* source{"none"};
};

bool querySdlLeftMouseDown() {
  return (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK) != 0;
}



WindowClientMouseState queryWindowClientMouseState(WindowSystem* window_system) {
  WindowClientMouseState state{};
  if (window_system) {
    SDL_MouseButtonFlags buttons = SDL_GetMouseState(&state.x, &state.y);
    state.left_down = (buttons & SDL_BUTTON_LMASK) != 0;
    state.source = "sdl";
  }
  return state;
}

slint::LogicalPosition slintPointerPosition(WindowSystem* window_system, float window_x,
                                           float window_y) {
  const SlintPointerCoords pointer =
      mapWindowPointerToSlint(window_system, window_x, window_y);
  return slint::LogicalPosition({pointer.x, pointer.y});
}

int32_t resolveHierarchyTreeOriginY(const MainEditorWindow& ui) {
  const int32_t hierarchy_origin_y =
      static_cast<int32_t>(ui.get_hierarchy_origin_y());
  const int32_t slint_tree_y =
      static_cast<int32_t>(ui.get_hierarchy_tree_origin_y());
  constexpr int32_t k_max_tree_origin_below_panel = 120;
  if (slint_tree_y > hierarchy_origin_y &&
      slint_tree_y <= hierarchy_origin_y + k_max_tree_origin_below_panel) {
    return slint_tree_y;
  }
  constexpr int32_t k_tree_chrome_height_px = 36;
  return hierarchy_origin_y + k_tree_chrome_height_px;
}

int32_t resolveContentBrowserTreeOriginY(const MainEditorWindow& ui) {
  const int32_t browser_origin_y = static_cast<int32_t>(ui.get_browser_origin_y());
  const int32_t slint_tree_y =
      static_cast<int32_t>(ui.get_browser_tree_origin_y());
  // Slint absolute_position for the tree is in the same space as
  // browser-origin (window/logical layout units, not drawable pixels).
  constexpr int32_t k_max_tree_origin_below_browser = 220;
  if (slint_tree_y > browser_origin_y &&
      slint_tree_y <= browser_origin_y + k_max_tree_origin_below_browser) {
    return slint_tree_y;
  }
  // Fallback when grid layout reports a bogus tree Y (e.g. 394).
  constexpr int32_t k_tree_chrome_height_px = 66;
  return browser_origin_y + k_tree_chrome_height_px;
}

}  // namespace

std::optional<UiContext::LockedServices> SlintSystem::lockServices() const {
  if (const auto host = m_ui_host.lock()) {
    return host->lockEditorServices();
  }
  return std::nullopt;
}

void SlintSystem::syncHierarchy() {
  const auto services = lockServices();
  if (!m_window_component || !services || !services->hierarchy) {
    return;
  }

  HierarchySystem& hierarchy = *services->hierarchy;
  const EntityId selected = services->selection
                                ? services->selection->getSelection()
                                : k_invalid_entity_id;

  try {
    ScopedDispatchGuard guard(m_slint_dispatch_depth);
    auto tree_model = std::make_shared<slint::VectorModel<HierarchyTreeRow>>();
    for (const EditorHierarchyTreeRow& row : hierarchy.treeRows()) {
      ::HierarchyTreeRow slint_row{};
      slint_row.entity_id = static_cast<int>(row.entity_id);
      slint_row.name = slint::SharedString(row.display_name.c_str());
      slint_row.depth = row.depth;
      slint_row.expanded = row.is_expanded;
      slint_row.has_children = row.has_children;
      slint_row.selected =
          isValid(selected) && row.entity_id == static_cast<uint32_t>(selected);
      tree_model->push_back(slint_row);
    }
    m_window_component->operator->()->set_hierarchy_tree_rows(tree_model);
    m_window_component->operator->()->set_hierarchy_selected_entity_id(
        isValid(selected) ? static_cast<int>(selected) : 0);
  } catch (const std::exception& e) {
    LOG_ERROR("[SlintSystem::syncHierarchy] {}", e.what());
  } catch (...) {
    LOG_ERROR("[SlintSystem::syncHierarchy] unknown exception");
  }
}

void SlintSystem::syncInspectorFromSelection() {
  if (!m_window_component || m_applying_inspector_sync) {
    return;
  }

  const auto services = lockServices();
  if (!services) {
    return;
  }
  EditorSelectionSystem* selection = services->selection.get();
  SceneInstance* scene =
      services->scene ? services->scene->getActiveInstance() : nullptr;

  try {
    ScopedDispatchGuard guard(m_slint_dispatch_depth);
    m_applying_inspector_sync = true;
    auto& ui = *m_window_component;

    if (!selection || !scene || !selection->hasSelection()) {
      ui->set_inspector_has_selection(false);
      ui->set_inspector_entity_name(slint::SharedString(""));
      m_applying_inspector_sync = false;
      if (selection) {
        selection->clearDirty();
      }
      return;
    }

    const Entity* entity = scene->getEntity(selection->getSelection());
    if (entity == nullptr) {
      ui->set_inspector_has_selection(false);
      ui->set_inspector_entity_name(slint::SharedString(""));
      m_applying_inspector_sync = false;
      selection->clearDirty();
      return;
    }

    const Vec3 euler = SceneSerializer::rotationToEulerDegrees(entity->getRotation());
    ui->set_inspector_has_selection(true);
    ui->set_inspector_entity_name(
        slint::SharedString(entity->getName().c_str()));
    ui->set_inspector_pos_x(entity->getPosition().x);
    ui->set_inspector_pos_y(entity->getPosition().y);
    ui->set_inspector_pos_z(entity->getPosition().z);
    ui->set_inspector_rot_x(euler.x);
    ui->set_inspector_rot_y(euler.y);
    ui->set_inspector_rot_z(euler.z);
    ui->set_inspector_scale_x(entity->getScale().x);
    ui->set_inspector_scale_y(entity->getScale().y);
    ui->set_inspector_scale_z(entity->getScale().z);
    selection->clearDirty();
    m_applying_inspector_sync = false;
  } catch (const std::exception& e) {
    m_applying_inspector_sync = false;
    LOG_ERROR("[SlintSystem::syncInspectorFromSelection] {}", e.what());
  } catch (...) {
    m_applying_inspector_sync = false;
    LOG_ERROR("[SlintSystem::syncInspectorFromSelection] unknown exception");
  }
}

void SlintSystem::applyInspectorTransform() {
  if (!m_window_component || m_applying_inspector_sync) {
    return;
  }

  const auto services = lockServices();
  if (!services) {
    return;
  }
  EditorSelectionSystem* selection = services->selection.get();
  SceneInstance* scene =
      services->scene ? services->scene->getActiveInstance() : nullptr;
  if (!selection || !scene || !selection->hasSelection()) {
    return;
  }

  Entity* entity = scene->getEntity(selection->getSelection());
  if (entity == nullptr) {
    return;
  }

  try {
    const auto& ui = *m_window_component;
    entity->setPosition(
        Vec3(ui->get_inspector_pos_x(), ui->get_inspector_pos_y(),
             ui->get_inspector_pos_z()));
    entity->setRotation(SceneSerializer::rotationFromEulerDegrees(Vec3(
        ui->get_inspector_rot_x(), ui->get_inspector_rot_y(),
        ui->get_inspector_rot_z())));
    entity->setScale(Vec3(ui->get_inspector_scale_x(), ui->get_inspector_scale_y(),
                          ui->get_inspector_scale_z()));
    scene->markTransformsDirty();
    if (services->editor_scene_edit) {
      services->editor_scene_edit->markDirty();
    }
  } catch (const std::exception& e) {
    LOG_ERROR("[SlintSystem::applyInspectorTransform] {}", e.what());
  } catch (...) {
    LOG_ERROR("[SlintSystem::applyInspectorTransform] unknown exception");
  }
}

void SlintSystem::refreshEditorScenePanels() {
  if (const auto services = lockServices()) {
    if (services->scene && services->hierarchy) {
      services->hierarchy->rebuildVisibleTree(services->scene->getActiveInstance());
      services->hierarchy->markDirty();
    }
  }
  syncHierarchy();
  syncInspectorFromSelection();
  syncTransformToolbarFromEngine();
}

void SlintSystem::syncTransformToolbarFromEngine() {
  if (!m_window_component || !g_runtime_global_context.m_render_system) {
    return;
  }
  try {
    ScopedDispatchGuard guard(m_slint_dispatch_depth);
    auto& ui = *m_window_component;
    TransformGizmoMode mode =
        g_runtime_global_context.m_render_system->getTransformGizmoMode();
    int slint_mode = 0;
    switch (mode) {
      case TransformGizmoMode::translate:
        slint_mode = 1;
        break;
      case TransformGizmoMode::rotate:
        slint_mode = 2;
        break;
      case TransformGizmoMode::scale:
        slint_mode = 3;
        break;
      default:
        slint_mode = 0;
        break;
    }
    ui->set_transform_gizmo_mode(slint_mode);
    ui->set_transform_gizmo_space_global(
        g_runtime_global_context.m_render_system->isTransformGizmoSpaceGlobal());
  } catch (...) {
  }
}

namespace {

void onImportFileDialogCallback(void* userdata, const char* const* filelist,
                                int filter) {
  (void)filter;
  auto* slint_system = static_cast<SlintSystem*>(userdata);
  if (slint_system == nullptr) {
    return;
  }
  if (filelist == nullptr) {
    const char* err = SDL_GetError();
    LOG_ERROR("[SDL_Dialog] file dialog callback error: {}", err ? err : "unknown error");
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Import Error",
                             err ? err : "Unknown file dialog error",
                             slint_system->getWindowSystem() ? slint_system->getWindowSystem()->getNativeWindow() : nullptr);
    return;
  }
  if (*filelist == nullptr) {
    LOG_INFO("[SDL_Dialog] file dialog cancelled by user");
    return;
  }
  eastl::vector<eastl::string> paths;
  for (int index = 0; filelist[index] != nullptr; ++index) {
    paths.push_back(eastl::string(filelist[index]));
  }
  slint_system->queueFileDialogImports(paths);
}

eastl::string extensionLowerFromPath(const eastl::string& absolute_path) {
  const size_t dot = absolute_path.find_last_of('.');
  if (dot == eastl::string::npos) {
    return eastl::string();
  }
  eastl::string ext(absolute_path.c_str() + dot);
  for (char& c : ext) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }
  return ext;
}

}  // namespace

void SlintSystem::queueFileDialogImports(
    const eastl::vector<eastl::string>& paths) {
  m_pending_file_dialog_paths.insert(m_pending_file_dialog_paths.end(),
                                     paths.begin(), paths.end());
}

void SlintSystem::finalizeAssetImport(
    const eastl::vector<ImportResult>& results) {
  if (results.empty()) {
    return;
  }
  if (const auto services = lockServices(); services && services->asset_compiler) {
    services->asset_compiler->cookIfStale();
  }
  syncContentBrowser();
  LOG_INFO("[SlintSystem] imported {} asset descriptor(s)", results.size());
}

void SlintSystem::showImportMeshDialogForPendingPaths() {
  if (!m_window_component || m_pending_mesh_import_paths.empty()) {
    return;
  }
  m_window_component->operator->()->set_import_mesh_dialog_visible(true);
  m_window_component->operator->()->set_import_mesh_materials(true);
  m_window_component->operator->()->set_import_mesh_animations(true);
  m_window_component->operator->()->set_import_mesh_scale(1.0f);
}

void SlintSystem::completePendingMeshImport() {
  const auto services = lockServices();
  if (!m_window_component || !services || !services->asset_import ||
      !services->content_browser || m_pending_mesh_import_paths.empty()) {
    return;
  }

  MeshImportSettings settings{};
  settings.materials = m_window_component->operator->()->get_import_mesh_materials();
  settings.animations =
      m_window_component->operator->()->get_import_mesh_animations();
  settings.scale = m_window_component->operator->()->get_import_mesh_scale();

  const eastl::string target_folder = services->content_browser->selectedFolder();
  const eastl::vector<ImportResult> results =
      services->asset_import->importExternalFiles(
          m_pending_mesh_import_paths, target_folder, settings);

  m_pending_mesh_import_paths.clear();
  m_window_component->operator->()->set_import_mesh_dialog_visible(false);
  finalizeAssetImport(results);
}

void SlintSystem::processPendingAssetImports() {
  if (m_pending_file_dialog_paths.empty()) {
    return;
  }

  const auto services = lockServices();
  if (!services || !services->asset_import || !services->content_browser) {
    m_pending_file_dialog_paths.clear();
    return;
  }

  eastl::vector<eastl::string> paths = m_pending_file_dialog_paths;
  m_pending_file_dialog_paths.clear();

  eastl::vector<eastl::string> mesh_paths;
  eastl::vector<eastl::string> direct_paths;
  for (const eastl::string& path : paths) {
    const eastl::string ext = extensionLowerFromPath(path);
    if (AssetImportService::isMeshSourceExtension(ext)) {
      mesh_paths.push_back(path);
    } else if (AssetImportService::isTextureSourceExtension(ext)) {
      direct_paths.push_back(path);
    }
  }

  const eastl::string target_folder = services->content_browser->selectedFolder();

  if (!direct_paths.empty()) {
    finalizeAssetImport(services->asset_import->importExternalFiles(
        direct_paths, target_folder));
  }

  if (!mesh_paths.empty()) {
    m_pending_mesh_import_paths.insert(m_pending_mesh_import_paths.end(),
                                       mesh_paths.begin(), mesh_paths.end());
    showImportMeshDialogForPendingPaths();
  }
}

void SlintSystem::queueOpenImportFileDialog() {
  if (m_open_import_dialog_event != 0) {
    SDL_Event user_event{};
    user_event.type = m_open_import_dialog_event;
    user_event.user.code = 1;
    user_event.user.data1 = this;
    if (!SDL_PushEvent(&user_event)) {
      LOG_WARN("[SlintSystem] SDL_PushEvent(import dialog) failed: {}",
               SDL_GetError());
      m_pending_file_dialog_is_import = true;
    }
    return;
  }
  m_pending_file_dialog_is_import = true;
}

void SlintSystem::openImportFileDialog() {
  if (!m_window_system) {
    return;
  }

  SDL_ClearError();

  static const SDL_DialogFileFilter filters[] = {
      {"Models and Images", "gltf;glb;png;jpg;jpeg;bmp;tga"},
      {"All Files", "*"},
  };

  SDL_Window* parent_window = m_window_system->getNativeWindow();
  if (parent_window != nullptr) {
    SDL_RaiseWindow(parent_window);
  }

  SDL_ShowOpenFileDialog(onImportFileDialogCallback, this, parent_window, filters,
                         2, nullptr, true);
  SDL_PumpEvents();

  const char* err = SDL_GetError();
  if (err && *err) {
    SDL_ClearError();
    SDL_ShowOpenFileDialog(onImportFileDialogCallback, this, nullptr, filters, 2,
                           nullptr, true);
    SDL_PumpEvents();
    err = SDL_GetError();
  }
  if (err && *err) {
    LOG_ERROR("[SDL_Dialog] SDL_ShowOpenFileDialog failed to start: {}", err);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Import Dialog Error", err, m_window_system->getNativeWindow());
  } else {
    LOG_INFO("[SDL_Dialog] SDL_ShowOpenFileDialog successfully called");
  }
}

void SlintSystem::syncContentBrowser() {
  const auto services = lockServices();
  if (!m_window_component || !services || !services->content_browser) {
    return;
  }
  ContentBrowserSystem& browser_system = *services->content_browser;

  try {
    ScopedDispatchGuard guard(m_slint_dispatch_depth);
    auto tree_model = std::make_shared<slint::VectorModel<BrowserTreeRow>>();
    for (const ContentBrowserTreeRow& row : browser_system.treeRows()) {
      BrowserTreeRow slint_row{};
      slint_row.path = slint::SharedString(row.virtual_path.c_str());
      slint_row.name = slint::SharedString(row.display_name.c_str());
      slint_row.depth = row.depth;
      slint_row.is_dir = row.is_directory;
      slint_row.expanded = row.is_expanded;
      slint_row.has_children = row.has_children;
      tree_model->push_back(slint_row);
    }
    m_window_component->operator->()->set_browser_tree_rows(tree_model);

    auto grid_model = std::make_shared<slint::VectorModel<BrowserGridRow>>();
    for (const ContentBrowserGridItem& item : browser_system.gridItems()) {
      BrowserGridRow slint_row{};
      slint_row.path = slint::SharedString(item.virtual_path.c_str());
      slint_row.name = slint::SharedString(item.display_name.c_str());
      slint_row.thumb = loadThumbnailImage(item.thumbnail_cache_path);
      slint_row.is_dir = item.is_directory;
      grid_model->push_back(slint_row);
    }
    m_window_component->operator->()->set_browser_grid_rows(grid_model);

    // Path segments (breadcrumb).
    auto path_model =
        std::make_shared<slint::VectorModel<BrowserPathSegment>>();
    for (const ContentBrowserPathSegment& seg :
         browser_system.pathSegments()) {
      BrowserPathSegment slint_seg{};
      slint_seg.path = slint::SharedString(seg.virtual_path.c_str());
      slint_seg.name = slint::SharedString(seg.display_name.c_str());
      path_model->push_back(slint_seg);
    }
    m_window_component->operator->()->set_browser_path_segments(path_model);

    // Status text and selected folder.
    m_window_component->operator->()->set_browser_status_text(
        slint::SharedString(browser_system.statusText().c_str()));
    m_window_component->operator->()->set_browser_selected_folder_path(
        slint::SharedString(browser_system.selectedFolder().c_str()));

    const ContentBrowserDragController& drag = browser_system.dragController();
    m_window_component->operator->()->set_browser_drag_active(drag.isDragging());
    m_window_component->operator->()->set_browser_drag_source_path(
        slint::SharedString(drag.sourcePath().c_str()));
    m_window_component->operator->()->set_browser_drop_highlight_path(
        slint::SharedString(m_drop_highlight_path.c_str()));
    m_window_component->operator->()->set_browser_viewport_drop_active(
        m_viewport_drop_active);
  } catch (const std::exception& e) {
    LOG_ERROR("[SlintSystem::syncContentBrowser] {}", e.what());
  } catch (...) {
    LOG_ERROR("[SlintSystem::syncContentBrowser] unknown exception");
  }
}

BrowserLogicalRect SlintSystem::getBrowserLogicalRect() const {
  return m_cached_browser_logical_rect;
}

bool SlintSystem::isPointerOverViewport(float logical_x, float logical_y) const {
  return pointInRect(logical_x, logical_y, m_cached_viewport_logical_rect);
}

bool SlintSystem::shouldDeferHeavyFrameWork() const {
  if (m_win32_size_modal || m_resize_cooldown_frames > 0 ||
      m_layout_cooldown_frames > 0 || isDockLayoutDragActive()) {
    return true;
  }
  return false;
}

bool SlintSystem::shouldRouteMouseToInputLayers(const SDL_Event& event) const {
  // Block input layers (camera orbit etc.) while a modal dialog is open.
  if (m_window_component &&
      m_window_component->operator->()->get_import_mesh_dialog_visible()) {
    return false;
  }
  if (!m_docking_has_viewport) {
    return false;
  }
  // Editor camera capture (relative mouse + grab): keep routing motion deltas even
  // when the warped/hidden cursor is outside the viewport rect.
  if (m_window_system && m_window_system->getFocusMode()) {
    return true;
  }
  float px = m_last_mouse_window_x;
  float py = m_last_mouse_window_y;
  if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
      event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
    px = static_cast<float>(event.button.x);
    py = static_cast<float>(event.button.y);
  }
  const eastl::array<float, 2> logical =
      mapWindowPointerToLogical(m_window_system, px, py);
  const ViewportLogicalRect& vp = m_cached_viewport_logical_rect;
  const float vp_right = static_cast<float>(vp.x) + static_cast<float>(vp.width);
  const float vp_bottom = static_cast<float>(vp.y) + static_cast<float>(vp.height);
  return logical[0] >= static_cast<float>(vp.x) && logical[0] < vp_right &&
         logical[1] >= static_cast<float>(vp.y) && logical[1] < vp_bottom;
}

bool SlintSystem::probeProjectionButtonAtLogical(float logical_x,
                                                float logical_y) const {
  // Hit-test must match ViewportProjectionToggle.slint (Slint layout), not the
  // chrome-expanded cache used for Vulkan render target sizing (G79).
  float vp_x = 0.0f;
  float vp_y = 0.0f;
  float vp_w = 0.0f;
  float vp_h = 0.0f;
  if (m_window_component) {
    const auto& ui = *m_window_component;
    vp_w = ui->get_viewport_width();
    vp_h = ui->get_viewport_height();
    if (vp_w > 0.0f && vp_h > 0.0f) {
      vp_x = ui->get_viewport_origin_x();
      vp_y = ui->get_viewport_origin_y();
    }
  }
  if (vp_w <= 0.0f || vp_h <= 0.0f) {
    vp_x = static_cast<float>(m_cached_viewport_logical_rect.x);
    vp_y = static_cast<float>(m_cached_viewport_logical_rect.y);
    vp_w = static_cast<float>(m_cached_viewport_logical_rect.width);
    vp_h = static_cast<float>(m_cached_viewport_logical_rect.height);
  }
  if (vp_w <= 0.0f || vp_h <= 0.0f) {
    return false;
  }

  if (hitProjectionToggleAtWindowLogical(logical_x, logical_y, vp_x, vp_y, vp_w, vp_h)) {
    return true;
  }

  const uint32_t vp_w_u = static_cast<uint32_t>(vp_w);
  const uint32_t vp_h_u = static_cast<uint32_t>(vp_h);
  if (logical_x >= vp_x && logical_y >= vp_y && logical_x < vp_x + vp_w &&
      logical_y < vp_y + vp_h) {
    const float local_x = logical_x - vp_x;
    const float local_y = logical_y - vp_y;
    if (hitTestProjectionButtonViewportLocal(local_x, local_y, vp_w_u, vp_h_u)) {
      return true;
    }
    if (hitViewportTopRightChromeLocal(local_x, local_y, vp_w, vp_h)) {
      return true;
    }
  }
  return false;
}

bool SlintSystem::probeProjectionButtonAtWindow(float window_x,
                                                 float window_y) const {
  if (!m_window_system) {
    return false;
  }
  const SlintPointerCoords ptr =
      mapWindowPointerToSlint(m_window_system, window_x, window_y);
  return probeProjectionButtonAtLogical(ptr.x, ptr.y);
}

bool SlintSystem::applyViewportProjection(bool is_perspective, const char* source) {
  const auto services = lockServices();
  if (!services || !services->render_system) {
    return false;
  }
  EditorCamera* camera = services->render_system->getEditorCamera();
  if (camera == nullptr) {
    return false;
  }

  const EditorCamera::ProjectionMode mode =
      is_perspective ? EditorCamera::ProjectionMode::perspective
                     : EditorCamera::ProjectionMode::orthographic;
  if (camera->getProjectionMode() != mode) {
    camera->setProjectionMode(mode);
  }
  syncViewportProjectionMode(is_perspective);

  LOG_INFO("[SlintSystem] viewport projection -> {} ({})",
           is_perspective ? "perspective" : "orthographic",
           source != nullptr ? source : "unknown");

  return true;
}

void SlintSystem::requestViewportProjectionToggle(const char* source) {
  if (m_projection_toggle_consumed_this_frame) {
    return;
  }
  const auto services = lockServices();
  if (!services || !services->render_system || !m_window_component) {
    return;
  }
  EditorCamera* camera = services->render_system->getEditorCamera();
  if (camera == nullptr) {
    return;
  }
  const bool is_perspective =
      camera->getProjectionMode() == EditorCamera::ProjectionMode::perspective;
  const bool next_perspective = !is_perspective;
  try {
    ScopedDispatchGuard guard(m_slint_dispatch_depth);
    m_window_component->operator->()->set_viewport_is_perspective(next_perspective);
  } catch (...) {
    // Slint property update failed; still apply the camera change.
  }
  applyViewportProjection(next_perspective, source != nullptr ? source : "engine");
  m_projection_toggle_consumed_this_frame = true;
}

bool SlintSystem::tryToggleProjectionAtWindow(float window_x, float window_y) {
  cacheViewportLogicalRectOnly();
  if (probeProjectionButtonAtWindow(window_x, window_y)) {
    requestViewportProjectionToggle("render_mouse_press");
    return true;
  }

  return false;
}

void SlintSystem::finishContentBrowserDrag(float logical_x, float logical_y) {
  const auto services = lockServices();
  if (!services || !services->content_browser) {
    return;
  }

  ContentBrowserSystem& browser_system = *services->content_browser;
  ContentBrowserDragController& drag = browser_system.dragController();
  if (!drag.isDragging()) {
    return;
  }

  const eastl::string source = drag.sourcePath();
  drag.endPress();

  if (!source.empty()) {
    if (isPointerOverViewport(logical_x, logical_y) &&
        services->editor_scene_edit) {
      const SpawnAssetResult spawn_result =
          services->editor_scene_edit->spawnAssetAtWindowPosition(
              source, logical_x, logical_y);
      if (spawn_result.success && services->render_system && services->scene) {
        syncSceneToRender(services->render_system.get(),
                          services->scene->getActiveInstance());
        refreshEditorScenePanels();
      }
    } else if (!m_drop_highlight_path.empty()) {
      browser_system.reparentEntry(source, m_drop_highlight_path);
    }
  }

  m_drop_highlight_path.clear();
  m_viewport_drop_active = false;
  drag.reset();
  syncContentBrowser();
}

void SlintSystem::finishContentBrowserDragAtCursor() {
  const WindowClientMouseState mouse =
      queryWindowClientMouseState(m_window_system);
  const SlintPointerCoords pointer =
      mapWindowPointerToSlint(m_window_system, mouse.x, mouse.y);
  finishContentBrowserDrag(pointer.x, pointer.y);
}

BrowserLogicalRect SlintSystem::getHierarchyLogicalRect() const {
  return m_cached_hierarchy_logical_rect;
}

bool SlintSystem::isContentBrowserDragActive() const {
  const auto services = lockServices();
  if (!services || !services->content_browser) {
    return false;
  }
  return services->content_browser->dragController().isDragging();
}

void SlintSystem::clearContentBrowserSlintClickFlag() {
  m_tree_folder_handled_by_slint = false;
}

void SlintSystem::beginContentBrowserInputFrame() {
  m_tree_folder_handled_by_slint = false;
  m_hierarchy_handled_by_slint = false;
}

void SlintSystem::clearHierarchySlintClickFlag() {
  m_hierarchy_handled_by_slint = false;
}

void SlintSystem::tickProjectionTogglePointerPoll() {
  if (!m_window_system || !m_window_component) {
    return;
  }
  if (isDockLayoutDragActive() || isSplitterResizeInteractionActive()) {
    return;
  }

  const WindowClientMouseState mouse =
      queryWindowClientMouseState(m_window_system);
  cacheViewportLogicalRectOnly();
  const SlintPointerCoords ptr =
      mapWindowPointerToSlint(m_window_system, mouse.x, mouse.y);
  // Use only the precise hit-test that matches the Slint TouchArea bounds.
  // No generous viewport-quadrant fallback.
  const bool chrome_hit = probeProjectionButtonAtLogical(ptr.x, ptr.y);

  const bool was_down = m_left_mouse_down_prev;
  const bool pressed_edge = mouse.left_down && !was_down;

  if (pressed_edge) {
    if (chrome_hit && m_projection_toggle_sdl_frame != m_frame_counter &&
        (m_frame_counter == 0 || m_projection_toggle_sdl_frame != m_frame_counter - 1)) {
      // SDL did NOT already handle this press — fire as fallback.
      requestViewportProjectionToggle("chrome_win32_poll_press");
    }
  }

  // Win32 poll must track button state even when Skia never delivers SDL mouse-up.
  m_left_mouse_down_prev = mouse.left_down;
}

void SlintSystem::tickContentBrowserTreePointerPoll() {
  if (!m_window_system || !m_window_component) {
    return;
  }

  if (const auto services = lockServices();
      !services || !services->content_browser) {
    const WindowClientMouseState mouse =
        queryWindowClientMouseState(m_window_system);
    m_left_mouse_down_prev = mouse.left_down;
    return;
  }

  const WindowClientMouseState mouse =
      queryWindowClientMouseState(m_window_system);
  const SlintPointerCoords pointer =
      mapWindowPointerToSlint(m_window_system, mouse.x, mouse.y);

  if (mouse.left_down && !m_left_mouse_down_prev) {
    cacheLayoutRects();
    if (!m_hierarchy_handled_by_slint) {
      trySelectHierarchyEntity(pointer.x, pointer.y);
    }
    if (!m_tree_folder_handled_by_slint) {
      trySelectContentBrowserTreeFolder(pointer.x, pointer.y);
    }
  }

  m_left_mouse_down_prev = mouse.left_down;
}

void SlintSystem::tickHierarchyPointerPoll() {
  (void)0;
}

bool SlintSystem::trySelectHierarchyEntity(float window_x, float window_y) {
  const auto services = lockServices();
  if (!m_window_system || !m_window_component || !services ||
      !services->hierarchy || !services->selection) {
    return false;
  }

  if (m_hierarchy_handled_by_slint) {
    return false;
  }

  if (m_window_component) {
    const MainEditorWindow& ui = *m_window_component->operator->();
    m_cached_hierarchy_logical_rect.x =
        static_cast<int32_t>(ui.get_hierarchy_origin_x());
    m_cached_hierarchy_logical_rect.y =
        static_cast<int32_t>(ui.get_hierarchy_origin_y());
    const float hierarchy_w = ui.get_hierarchy_width();
    const float hierarchy_h = ui.get_hierarchy_height();
    m_cached_hierarchy_logical_rect.width =
        hierarchy_w > 0.0f ? static_cast<uint32_t>(hierarchy_w) : 0u;
    m_cached_hierarchy_logical_rect.height =
        hierarchy_h > 0.0f ? static_cast<uint32_t>(hierarchy_h) : 0u;
  }

  HierarchySystem& hierarchy = *services->hierarchy;
  EditorSelectionSystem& selection = *services->selection;

  const int32_t hierarchy_origin_x = m_cached_hierarchy_logical_rect.x;
  const int32_t tree_origin_y =
      m_window_component
          ? resolveHierarchyTreeOriginY(*m_window_component->operator->())
          : m_cached_hierarchy_logical_rect.y + 36;

  constexpr float k_row_pitch = 24.0f;
  const bool selected = hierarchy.selectEntityAt(
      window_x, window_y, hierarchy_origin_x, tree_origin_y,
      static_cast<float>(m_cached_hierarchy_logical_rect.width),
      static_cast<float>(m_cached_hierarchy_logical_rect.height), k_row_pitch,
      selection);
  if (selected) {
    m_hierarchy_handled_by_slint = true;
    syncInspectorFromSelection();
    syncHierarchy();
  }
  return selected;
}

bool SlintSystem::trySelectContentBrowserTreeFolder(float window_x,
                                                    float window_y) {
  const auto services = lockServices();
  if (!m_window_system || !m_window_component || !services ||
      !services->content_browser) {
    return false;
  }

  if (m_tree_folder_handled_by_slint) {
    return false;
  }

  ContentBrowserSystem& browser_system = *services->content_browser;
  if (browser_system.dragController().isDragging()) {
    return false;
  }

  if (m_window_component) {
    m_cached_browser_logical_rect =
        readBrowserRectFromUi(*m_window_component->operator->());
  }

  const MainEditorWindow& ui = *m_window_component->operator->();
  const int32_t browser_origin_x = static_cast<int32_t>(ui.get_browser_origin_x());
  const BrowserLogicalRect browser_rect = readBrowserRectFromUi(ui);
  const int32_t tree_origin_y = resolveContentBrowserTreeOriginY(ui);
  constexpr float k_tree_row_pitch_px = 24.0f;
  const float tree_panel_height =
      static_cast<float>(ui.get_browser_tree_panel_height());
  const float tree_panel_width =
      static_cast<float>(ui.get_browser_tree_panel_width());
  if (tree_panel_height <= 0.0f || tree_panel_width <= 0.0f) {
    return false;
  }
  const bool in_browser_x =
      window_x >= static_cast<float>(browser_origin_x) &&
      window_x <
          static_cast<float>(browser_origin_x) + tree_panel_width;
  const bool in_tree_band =
      in_browser_x && window_y >= static_cast<float>(tree_origin_y) &&
      window_y < static_cast<float>(tree_origin_y) + tree_panel_height;
  if (!in_tree_band) {
    return false;
  }

  const eastl::string folder = browser_system.hitTestFolderAt(
      window_x, window_y, tree_origin_y, k_tree_row_pitch_px);
  if (folder.empty()) {
    return false;
  }

  m_tree_folder_handled_by_slint = true;

  browser_system.setSelectedFolder(folder);
  syncContentBrowser();
  return true;
}

void SlintSystem::syncWindowChromeSize(int override_logical_w,
                                       int override_logical_h) {
  if (!m_window_component || !m_window_system || !m_window_adapter) {
    return;
  }

  m_window_system->refreshClientPixelSizeFromHwnd();

  const eastl::array<int, 2> sdl_win = m_window_system->getWindowSize();
  const eastl::array<int, 2> logical_win = m_window_system->getLogicalWindowSize();
  const eastl::array<int, 2> px = m_window_system->getDrawableSize();
  const float scale = queryWindowScaleFactor(m_window_system);
  const int derived_logical_w =
      scale > 0.0f ? static_cast<int>(static_cast<float>(px[0]) / scale + 0.5f) : logical_win[0];
  const int derived_logical_h =
      scale > 0.0f ? static_cast<int>(static_cast<float>(px[1]) / scale + 0.5f)
                     : logical_win[1];
  const int reconciled_w = eastl::max(logical_win[0], derived_logical_w);
  const int reconciled_h = eastl::max(logical_win[1], derived_logical_h);
  const int logical_w = override_logical_w > 0
                            ? eastl::max(override_logical_w, reconciled_w)
                            : reconciled_w;
  const int logical_h = override_logical_h > 0
                            ? eastl::max(override_logical_h, reconciled_h)
                            : reconciled_h;

  const bool size_changed =
      g_chrome_prev_logical_w == 0 || logical_w != g_chrome_prev_logical_w ||
      logical_h != g_chrome_prev_logical_h;
  const bool px_changed =
      g_chrome_prev_px_w == 0 || px[0] != g_chrome_prev_px_w ||
      px[1] != g_chrome_prev_px_h;
  const bool scale_changed =
      g_chrome_prev_scale <= 0.0f || std::fabs(scale - g_chrome_prev_scale) > 0.01f;
  const bool force_resync =
      m_layout_resync_frames > 0 && (size_changed || px_changed || scale_changed);
  if (!size_changed && !px_changed && !scale_changed && !force_resync &&
      m_maximize_layout_frames == 0) {
    return;
  }

  if (g_chrome_prev_logical_w > 0 &&
      (std::abs(logical_w - g_chrome_prev_logical_w) > 48 ||
       std::abs(logical_h - g_chrome_prev_logical_h) > 48)) {
    m_force_window_commit = true;
    m_layout_resync_frames = k_layout_resync_frames;
  }
  if (g_chrome_prev_px_w > 0 &&
      (std::abs(px[0] - g_chrome_prev_px_w) > 48 ||
       std::abs(px[1] - g_chrome_prev_px_h) > 48)) {
    m_force_window_commit = true;
    m_layout_resync_frames = k_layout_resync_frames;
  }
  g_chrome_prev_logical_w = logical_w;
  g_chrome_prev_logical_h = logical_h;
  g_chrome_prev_px_w = px[0];
  g_chrome_prev_px_h = px[1];
  g_chrome_prev_scale = scale;

  m_window_adapter->pollDrawableSize();
  m_window_adapter->applyWindowLayoutNow(logical_w, logical_h);
  m_window_adapter->request_redraw();
}

void SlintSystem::cacheLayoutRects() {
  if (m_window_component) {
    const uint32_t vp_w = m_cached_viewport_logical_rect.width;
    const uint32_t vp_h = m_cached_viewport_logical_rect.height;

    const bool viewport_changed =
        m_last_viewport_w != 0 &&
        (vp_w != m_last_viewport_w || vp_h != m_last_viewport_h);
    if (viewport_changed) {
      m_layout_cooldown_frames = k_layout_cooldown_frames;
      m_pending_viewport_invalidate = true;
    }
    m_last_viewport_w = vp_w;
    m_last_viewport_h = vp_h;
    if (!viewport_changed && m_layout_cooldown_frames > 0) {
      --m_layout_cooldown_frames;
    }
  } else {
    m_cached_viewport_logical_rect = {};
    m_cached_browser_logical_rect = {};
    m_cached_hierarchy_logical_rect = {};
  }
}

void SlintSystem::cacheViewportLogicalRectOnly() {
  // Keep chrome expansion in sync during defer_heavy (splitter drag) frames too.
  cacheLayoutRects();
}

void SlintSystem::refreshBrowserRectCache() {
  if (!m_window_component) {
    return;
  }
  m_cached_browser_logical_rect =
      readBrowserRectFromUi(*m_window_component->operator->());
}

void SlintSystem::notifyWindowResizeActivity() {
  m_resize_cooldown_frames = k_resize_cooldown_frames;
}

bool SlintSystem::shouldSkipSkiaPresentDuringDefer() const {
  // Only skip present during live Win32 border drag (OS stretches the last frame).
  // Maximize/restore must still composite so the new layout is visible immediately.
  return m_win32_size_modal;
}

void SlintSystem::notifyWin32ClientAreaChanged(uintptr_t wm_size_param, int client_w,
                                               int client_h) {
  resetChromeSizeCache();
  m_layout_resync_frames = k_layout_resync_frames;
  m_force_window_commit = true;
  m_window_resize_active = true;
  // WM_SIZE (maximize/restore) is instantaneous — do not arm resize cooldown; that
  // defers Skia present and leaves the UI drawn at the old size in a larger HWND.
  m_resize_cooldown_frames = 0;
#if defined(_WIN32)
  // WM_SIZE wParam == SIZE_MAXIMIZED (2) — avoid including windows.h in this TU.
  if (wm_size_param == 2u) {
    m_maximize_layout_frames = k_maximize_layout_frames;
  }
#endif
  try {
    if (m_window_system) {
      if (client_w > 0 && client_h > 0) {
        m_window_system->notifyClientPixelSize(client_w, client_h);
      } else {
        m_window_system->refreshClientPixelSizeFromHwnd();
      }
    }
    if (m_window_adapter) {
      m_window_adapter->pollDrawableSize();
    }
    if (m_slint_dispatch_depth == 0) {
      syncWindowChromeSize();
      slint::platform::update_timers_and_animations();
      if (m_window_adapter) {
        m_window_adapter->request_redraw();
      }
      cacheLayoutRects();
    } else {
      m_pending_win32_chrome_sync = true;
    }
  } catch (const std::exception& e) {
    LOG_ERROR("[SlintSystem::notifyWin32ClientAreaChanged] {}", e.what());
  } catch (...) {
    LOG_ERROR("[SlintSystem::notifyWin32ClientAreaChanged] unknown exception");
  }
  applyPendingViewportInvalidate();
}

void SlintSystem::noteWin32SizingTick() {
  notifyWindowResizeActivity();
  try {
    if (m_window_adapter) {
      m_window_adapter->pollDrawableSize();
    }
  } catch (const std::exception& e) {
    LOG_ERROR("[SlintSystem::noteWin32SizingTick] {}", e.what());
  } catch (...) {
    LOG_ERROR("[SlintSystem::noteWin32SizingTick] unknown exception");
  }
}

void SlintSystem::clearDeferAfterWindowResize() {
  m_resize_cooldown_frames = 0;
  m_layout_cooldown_frames = 0;
  m_window_resize_active = false;
}

void SlintSystem::finishLiveResizeModal() {
  try {
    ScopedDispatchGuard guard(m_slint_dispatch_depth);
    slint::platform::update_timers_and_animations();
    resetChromeSizeCache();
    if (m_window_adapter) {
      m_window_adapter->pollDrawableSize();
      m_window_adapter->commitWindowSize(true);
    }
    syncWindowChromeSize();
    cacheLayoutRects();
  } catch (const std::exception& e) {
    LOG_ERROR("[SlintSystem::finishLiveResizeModal] {}", e.what());
  } catch (...) {
    LOG_ERROR("[SlintSystem::finishLiveResizeModal] unknown exception");
  }
  applyPendingViewportInvalidate();
}



void SlintSystem::processCoalescedSdlMouseMotion() {
  if (!m_pending_sdl_mouse_motion) {
    return;
  }
  m_pending_sdl_mouse_motion = false;
  m_sdl_motion_events_coalesced = 0;

  if (m_window_resize_active || !m_window_adapter || !m_window_component) {
    return;
  }

  const float window_x = m_last_mouse_window_x;
  const float window_y = m_last_mouse_window_y;

  const auto drag_services = lockServices();
  if (!isDockLayoutDragActive() && drag_services && drag_services->content_browser &&
      drag_services->content_browser->dragController().isDragging()) {
    ContentBrowserSystem& browser_system = *drag_services->content_browser;
    const slint::LogicalPosition logical_pos =
        slintPointerPosition(m_window_system, window_x, window_y);

    if (isPointerOverViewport(logical_pos.x, logical_pos.y)) {
      if (!m_drop_highlight_path.empty() || !m_viewport_drop_active) {
        m_drop_highlight_path.clear();
        m_viewport_drop_active = true;
        m_pending_content_browser_sync = true;
      }
    } else {
      if (m_viewport_drop_active) {
        m_viewport_drop_active = false;
        m_pending_content_browser_sync = true;
      }
      const int32_t tree_origin_y =
          resolveContentBrowserTreeOriginY(*m_window_component->operator->());
      const eastl::string new_highlight = browser_system.hitTestFolderAt(
          logical_pos.x, logical_pos.y, tree_origin_y, 24.0f);
      if (new_highlight != m_drop_highlight_path) {
        m_drop_highlight_path = new_highlight;
        m_pending_content_browser_sync = true;
      }
    }
    return;
  }

  m_pending_pointer_x = window_x;
  m_pending_pointer_y = window_y;
  m_pending_pointer_move = true;
  ++m_coalesced_pointer_moves;
}

void SlintSystem::flushPendingPointerMove() {
  if (!m_pending_pointer_move || !m_window_adapter || !m_window_system) {
    return;
  }
  m_pending_pointer_move = false;
  m_coalesced_pointer_moves = 0;
  slint::Window& window = m_window_adapter->window();
  window.dispatch_pointer_move_event(
      slintPointerPosition(m_window_system, m_pending_pointer_x, m_pending_pointer_y));
}

bool SlintSystem::shouldPresentSkiaFrame() const {
  if (!m_window_adapter) {
    return false;
  }
  if (m_pending_dock_layout_present) {
    return true;
  }
  return m_viewport_frame_ready || m_viewport_image_dirty;
}

void SlintSystem::compositeEditorFrame() {
  try {
    ScopedDispatchGuard guard(m_slint_dispatch_depth);
    if (m_window_adapter) {
      m_window_adapter->compositeFrame();
    }
  } catch (const std::exception& e) {
    LOG_ERROR("[SlintSystem::compositeEditorFrame] {}", e.what());
  } catch (...) {
    LOG_ERROR("[SlintSystem::compositeEditorFrame] unknown exception");
  }
}

void SlintSystem::seedDockingWorkspace() {
  auto viewport = m_dock_manager.createWidget("Viewport", DockPanelKind::viewport);
  m_dock_manager.dockToRoot(viewport, DockSlot::center);
  const auto center_container = viewport->ownerContainer();
  const DockId center_id =
      center_container ? center_container->id() : k_invalid_dock_id;

  auto hierarchy = m_dock_manager.createWidget("Hierarchy", DockPanelKind::hierarchy);
  m_dock_manager.dockWidget(center_id, DockSlot::left, hierarchy);

  auto inspector = m_dock_manager.createWidget("Inspector", DockPanelKind::inspector);
  m_dock_manager.dockWidget(center_id, DockSlot::right, inspector);

  auto content =
      m_dock_manager.createWidget("Content Browser", DockPanelKind::content_browser);
  m_dock_manager.dockWidget(center_id, DockSlot::bottom, content);

  auto console = m_dock_manager.createWidget("Console", DockPanelKind::custom);
  if (const auto content_container = content->ownerContainer()) {
    m_dock_manager.dockWidget(content_container->id(), DockSlot::center, console);
  }

  m_docking_model_dirty = true;
}

void SlintSystem::syncDockingWorkspace() {
  if (!m_window_component) {
    return;
  }
  auto& component = *m_window_component;
  component->set_docking_visible(true);

  const float host_w = component->get_docking_width();
  const float host_h = component->get_docking_height();
  const bool size_changed = host_w != m_docking_host_w || host_h != m_docking_host_h;
  if (!m_docking_model_dirty && !size_changed) {
    return;
  }
  m_docking_host_w = host_w;
  m_docking_host_h = host_h;
  m_docking_model_dirty = false;

  m_dock_manager.setHostRect(makeDockRect(0.0f, 0.0f, host_w, host_h));
  const DockLayoutModel model = m_dock_manager.buildLayoutModel();

  m_docking_has_viewport = false;
  bool has_hierarchy = false;
  bool has_browser = false;
  DockRect hierarchy_rect{};
  DockRect browser_rect{};

  for (const DockTileView& scan : model.tiles) {
    if (scan.active_panel_kind == DockPanelKind::viewport) {
      m_docking_has_viewport = true;
      m_docking_viewport_local_rect = scan.content_rect;
    } else if (scan.active_panel_kind == DockPanelKind::hierarchy) {
      has_hierarchy = true;
      hierarchy_rect = scan.content_rect;
    } else if (scan.active_panel_kind == DockPanelKind::content_browser) {
      has_browser = true;
      browser_rect = scan.content_rect;
    }
  }

  const float vp_origin_x = component->get_docking_origin_x();
  const float vp_origin_y = component->get_docking_origin_y();

  if (m_docking_has_viewport) {
    ViewportLogicalRect docked_rect;
    docked_rect.x = static_cast<int32_t>(vp_origin_x + m_docking_viewport_local_rect.x);
    docked_rect.y = static_cast<int32_t>(vp_origin_y + m_docking_viewport_local_rect.y);
    docked_rect.width = static_cast<uint32_t>(std::max(0.0f, m_docking_viewport_local_rect.width));
    docked_rect.height = static_cast<uint32_t>(std::max(0.0f, m_docking_viewport_local_rect.height));
    m_cached_viewport_logical_rect = docked_rect;

    component->set_viewport_origin_x(vp_origin_x + m_docking_viewport_local_rect.x);
    component->set_viewport_origin_y(vp_origin_y + m_docking_viewport_local_rect.y);
    component->set_viewport_width(m_docking_viewport_local_rect.width);
    component->set_viewport_height(m_docking_viewport_local_rect.height);
  } else {
    component->set_viewport_width(0.0f);
    component->set_viewport_height(0.0f);
  }

  if (has_hierarchy) {
    ViewportLogicalRect docked_rect;
    docked_rect.x = static_cast<int32_t>(vp_origin_x + hierarchy_rect.x);
    docked_rect.y = static_cast<int32_t>(vp_origin_y + hierarchy_rect.y);
    docked_rect.width = static_cast<uint32_t>(std::max(0.0f, hierarchy_rect.width));
    docked_rect.height = static_cast<uint32_t>(std::max(0.0f, hierarchy_rect.height));
    m_cached_hierarchy_logical_rect = docked_rect;

    component->set_hierarchy_origin_x(vp_origin_x + hierarchy_rect.x);
    component->set_hierarchy_origin_y(vp_origin_y + hierarchy_rect.y);
    component->set_hierarchy_width(hierarchy_rect.width);
    component->set_hierarchy_height(hierarchy_rect.height);
  } else {
    m_cached_hierarchy_logical_rect = {};
    component->set_hierarchy_width(0.0f);
    component->set_hierarchy_height(0.0f);
  }

  if (has_browser) {
    ViewportLogicalRect docked_rect;
    docked_rect.x = static_cast<int32_t>(vp_origin_x + browser_rect.x);
    docked_rect.y = static_cast<int32_t>(vp_origin_y + browser_rect.y);
    docked_rect.width = static_cast<uint32_t>(std::max(0.0f, browser_rect.width));
    docked_rect.height = static_cast<uint32_t>(std::max(0.0f, browser_rect.height));
    m_cached_browser_logical_rect = docked_rect;

    component->set_browser_origin_x(vp_origin_x + browser_rect.x);
    component->set_browser_origin_y(vp_origin_y + browser_rect.y);
    component->set_browser_width(browser_rect.width);
    component->set_browser_height(browser_rect.height);
  } else {
    m_cached_browser_logical_rect = {};
    component->set_browser_width(0.0f);
    component->set_browser_height(0.0f);
  }

  auto tile_existing = component->get_docking_tiles();
  auto tile_model = std::dynamic_pointer_cast<slint::VectorModel<DockTile>>(tile_existing);
  if (!tile_model) {
    tile_model = std::make_shared<slint::VectorModel<DockTile>>();
    component->set_docking_tiles(tile_model);
  }
  size_t tile_idx = 0;
  for (const DockTileView& view : model.tiles) {
    DockTile row{};
    row.node_id = static_cast<int>(view.node_id);
    row.tab_x = view.tab_bar_rect.x;
    row.tab_y = view.tab_bar_rect.y;
    row.tab_width = view.tab_bar_rect.width;
    row.tab_height = view.tab_bar_rect.height;
    row.content_x = view.content_rect.x;
    row.content_y = view.content_rect.y;
    row.content_width = view.content_rect.width;
    row.content_height = view.content_rect.height;
    row.active_widget_id = static_cast<int>(view.active_widget_id);
    row.active_panel_kind = static_cast<int>(view.active_panel_kind);
    row.floating = view.floating;

    if (tile_idx < tile_model->row_count()) {
      tile_model->set_row_data(tile_idx, row);
    } else {
      tile_model->push_back(row);
    }
    tile_idx++;
  }
  while (tile_model->row_count() > model.tiles.size()) {
    tile_model->erase(tile_model->row_count() - 1);
  }

  auto tab_existing = component->get_docking_tabs();
  auto tab_model = std::dynamic_pointer_cast<slint::VectorModel<DockTab>>(tab_existing);
  if (!tab_model) {
    tab_model = std::make_shared<slint::VectorModel<DockTab>>();
    component->set_docking_tabs(tab_model);
  }
  size_t tab_idx = 0;
  for (const DockTabView& view : model.tabs) {
    DockTab row{};
    row.widget_id = static_cast<int>(view.widget_id);
    row.container_id = static_cast<int>(view.container_id);
    row.title = slint::SharedString(view.title.c_str());
    row.panel_kind = static_cast<int>(view.panel_kind);
    row.active = view.active;
    row.x = view.rect.x;
    row.y = view.rect.y;
    row.width = view.rect.width;
    row.height = view.rect.height;

    if (tab_idx < tab_model->row_count()) {
      tab_model->set_row_data(tab_idx, row);
    } else {
      tab_model->push_back(row);
    }
    tab_idx++;
  }
  while (tab_model->row_count() > model.tabs.size()) {
    tab_model->erase(tab_model->row_count() - 1);
  }

  auto splitter_existing = component->get_docking_splitters();
  auto splitter_model = std::dynamic_pointer_cast<slint::VectorModel<DockSplitterHandle>>(splitter_existing);
  if (!splitter_model) {
    splitter_model = std::make_shared<slint::VectorModel<DockSplitterHandle>>();
    component->set_docking_splitters(splitter_model);
  }
  size_t splitter_idx = 0;
  for (const DockSplitterView& view : model.splitters) {
    DockSplitterHandle row{};
    row.node_id = static_cast<int>(view.node_id);
    row.x = view.rect.x;
    row.y = view.rect.y;
    row.width = view.rect.width;
    row.height = view.rect.height;
    row.vertical = view.vertical_handle;

    if (splitter_idx < splitter_model->row_count()) {
      splitter_model->set_row_data(splitter_idx, row);
    } else {
      splitter_model->push_back(row);
    }
    splitter_idx++;
  }
  while (splitter_model->row_count() > model.splitters.size()) {
    splitter_model->erase(splitter_model->row_count() - 1);
  }

  auto guide_existing = component->get_docking_guides();
  auto guide_model = std::dynamic_pointer_cast<slint::VectorModel<DockGuide>>(guide_existing);
  if (!guide_model) {
    guide_model = std::make_shared<slint::VectorModel<DockGuide>>();
    component->set_docking_guides(guide_model);
  }
  size_t guide_idx = 0;
  for (const DockGuideView& view : model.guides) {
    DockGuide row{};
    row.slot = static_cast<int>(view.slot);
    row.x = view.rect.x;
    row.y = view.rect.y;
    row.width = view.rect.width;
    row.height = view.rect.height;
    row.highlighted = view.highlighted;

    if (guide_idx < guide_model->row_count()) {
      guide_model->set_row_data(guide_idx, row);
    } else {
      guide_model->push_back(row);
    }
    guide_idx++;
  }
  while (guide_model->row_count() > model.guides.size()) {
    guide_model->erase(guide_model->row_count() - 1);
  }

  auto floating_existing = component->get_docking_floatings();
  auto floating_model = std::dynamic_pointer_cast<slint::VectorModel<DockFloating>>(floating_existing);
  if (!floating_model) {
    floating_model = std::make_shared<slint::VectorModel<DockFloating>>();
    component->set_docking_floatings(floating_model);
  }
  size_t floating_idx = 0;
  for (const DockFloatingView& view : model.floatings) {
    DockFloating row{};
    row.node_id = static_cast<int>(view.node_id);
    row.x = view.frame_rect.x;
    row.y = view.frame_rect.y;
    row.width = view.frame_rect.width;
    row.height = view.frame_rect.height;
    row.title_height = view.title_rect.height;
    row.grip_size = view.grip_rect.width;
    row.title = slint::SharedString(view.title.c_str());

    if (floating_idx < floating_model->row_count()) {
      floating_model->set_row_data(floating_idx, row);
    } else {
      floating_model->push_back(row);
    }
    floating_idx++;
  }
  while (floating_model->row_count() > model.floatings.size()) {
    floating_model->erase(floating_model->row_count() - 1);
  }

  DockPreview preview{};
  preview.visible = model.preview.visible;
  preview.x = model.preview.rect.x;
  preview.y = model.preview.rect.y;
  preview.width = model.preview.rect.width;
  preview.height = model.preview.rect.height;
  component->set_docking_preview(preview);
  component->set_docking_drag_active(model.preview.visible);
}

void SlintSystem::beginFrame() {
  if (const auto ui_host = m_ui_host.lock()) {
    ui_host->drainEventQueue();
  }

  if (m_pending_file_dialog_is_import) {
    m_pending_file_dialog_is_import = false;
    openImportFileDialog();
  }

  try {
    ScopedDispatchGuard guard(m_slint_dispatch_depth);
    m_projection_toggle_consumed_this_frame = false;
    ++m_frame_counter;
    processPendingAssetImports();
    processCoalescedSdlMouseMotion();
    flushPendingPointerMove();
    if (m_pending_content_browser_sync) {
      m_pending_content_browser_sync = false;
      syncContentBrowser();
    }
    const bool defer_heavy = shouldDeferHeavyFrameWork();
    const bool splitter_interaction = isSplitterResizeInteractionActive();
    if (!defer_heavy) {
      slint::platform::update_timers_and_animations();
    }
    if (m_window_adapter) {
      const uint32_t resize_events_this_frame = m_resize_events_pumped;
      m_resize_events_pumped = 0;

      if (resize_events_this_frame > 0) {
        m_resize_cooldown_frames = k_resize_cooldown_frames;
      } else if (m_resize_cooldown_frames > 0) {
        --m_resize_cooldown_frames;
      }

      if (m_pending_win32_chrome_sync) {
        m_pending_win32_chrome_sync = false;
        syncWindowChromeSize();
      }
      if (m_maximize_layout_frames > 0) {
        --m_maximize_layout_frames;
        m_force_window_commit = true;
        m_resize_cooldown_frames = 0;
        if (m_window_system) {
          m_window_system->refreshClientPixelSizeFromHwnd();
        }
        syncWindowChromeSize();
        if (m_window_adapter) {
          m_window_adapter->request_redraw();
        }
      }
      if (!splitter_interaction) {
        // Always poll: defer must not leave stale logical size after maximize (H-A).
        m_window_adapter->pollDrawableSize();
      }
      if (m_layout_resync_frames > 0) {
        --m_layout_resync_frames;
      }
      if (!splitter_interaction) {
        syncWindowChromeSize();
        if (!defer_heavy || m_force_window_commit) {
          m_window_adapter->commitWindowSize(m_force_window_commit);
          syncWindowChromeSize();
        } else if (m_layout_resync_frames > 0) {
          syncWindowChromeSize();
        }
      } else if (m_force_window_commit) {
        m_window_adapter->commitWindowSize(true);
      }
      m_force_window_commit = false;
    }
    if (shouldSkipSkiaPresentDuringDefer() || splitter_interaction) {
      cacheViewportLogicalRectOnly();
    } else {
      cacheLayoutRects();
    }
    syncDockingWorkspace();
    tickProjectionTogglePointerPoll();
    if (!defer_heavy) {
      if (const auto ui_host = m_ui_host.lock()) {
        ui_host->tickEditorPanels();
        ui_host->syncPreviewSettingsFromPresentation();
      }
    }
  } catch (const std::exception& e) {
    LOG_ERROR("[SlintSystem::beginFrame] {}", e.what());
    if (m_window_system) {
      m_window_system->requestClose();
    }
  } catch (...) {
    LOG_ERROR("[SlintSystem::beginFrame] unknown exception");
    if (m_window_system) {
      m_window_system->requestClose();
    }
  }
}

void SlintSystem::endFrame() {
  try {
    ScopedDispatchGuard guard(m_slint_dispatch_depth);
    if (m_window_adapter) {
      if (m_pending_dock_layout_present) {
        m_pending_dock_layout_present = false;
        m_window_adapter->compositeFrame();
        m_viewport_image_dirty = false;
        m_viewport_frame_ready = false;
        cacheViewportLogicalRectOnly();
      } else {
        const bool defer_heavy = shouldDeferHeavyFrameWork();
        const bool splitter_interaction = isSplitterResizeInteractionActive();
        if (defer_heavy) {
          if (shouldSkipSkiaPresentDuringDefer()) {
            m_window_adapter->request_redraw();
          } else if (splitter_interaction) {
            // H21: Skia composite ~14ms/frame starves SDL_AppIterate during motion flood.
            m_window_adapter->request_redraw();
          } else if (isDockLayoutDragActive()) {
            m_window_adapter->request_redraw();
          } else {
            static uint64_t s_last_defer_present_ns = 0;
            const uint64_t now_ns = SDL_GetTicksNS();
            if (s_last_defer_present_ns == 0 ||
                now_ns - s_last_defer_present_ns >= 50'000'000) {
              s_last_defer_present_ns = now_ns;
              m_window_adapter->compositeFrame();
              m_viewport_image_dirty = false;
              m_viewport_frame_ready = false;
            } else {
              m_window_adapter->request_redraw();
            }
          }
        } else if (splitter_interaction) {
          m_window_adapter->request_redraw();
        } else if (shouldPresentSkiaFrame()) {
          static uint64_t s_last_full_composite_ns = 0;
          const uint64_t now_ns = SDL_GetTicksNS();
          const bool ui_animating =
              m_window_adapter->needsRedraw() && !m_viewport_image_dirty;
          const uint64_t min_interval_ns =
              ui_animating ? 16'000'000ull : 33'000'000ull;
          if (s_last_full_composite_ns == 0 ||
              now_ns - s_last_full_composite_ns >= min_interval_ns) {
            s_last_full_composite_ns = now_ns;
            m_window_adapter->compositeFrame();
            m_viewport_image_dirty = false;
            m_viewport_frame_ready = false;
            cacheLayoutRects();
          } else {
            m_window_adapter->request_redraw();
          }
        }
      }
    }
    m_window_resize_active = false;
  } catch (const std::exception& e) {
    LOG_ERROR("[SlintSystem::endFrame] {}", e.what());
    if (m_window_system) {
      m_window_system->requestClose();
    }
  } catch (...) {
    LOG_ERROR("[SlintSystem::endFrame] unknown exception");
    if (m_window_system) {
      m_window_system->requestClose();
    }
  }

}

void SlintSystem::update() {
  beginFrame();
  endFrame();
}

void SlintSystem::processEvent(const SDL_Event& event) {
  if (m_open_import_dialog_event != 0 &&
      event.type == m_open_import_dialog_event) {
    openImportFileDialog();
    return;
  }

  if (!m_window_adapter || !m_window_component) {
    return;
  }
  try {
    ScopedDispatchGuard guard(m_slint_dispatch_depth);
    slint::Window& window = m_window_adapter->window();
    const SDL_WindowID window_id = m_window_system->getWindowId();

    switch (event.type) {
      case SDL_EVENT_WINDOW_EXPOSED:
        if (event.window.windowID == window_id && event.window.data1 != 0 &&
            m_window_adapter) {
          notifyWindowResizeActivity();
          m_window_resize_active = true;
          m_window_adapter->request_redraw();
        }
        break;
      case SDL_EVENT_WINDOW_RESIZED:
        if (event.window.windowID == window_id && m_window_adapter) {
          m_window_resize_active = true;
          m_layout_resync_frames = k_layout_resync_frames;
          ++m_resize_events_pumped;
          m_force_window_commit = true;
          resetChromeSizeCache();
          if (m_window_system) {
            m_window_system->refreshClientPixelSizeFromHwnd();
          }
          m_window_adapter->pollDrawableSize();
          {
            const eastl::array<int, 2> px = m_window_system->getDrawableSize();
            const float scale = queryWindowScaleFactor(m_window_system);
            const int derived_w =
                scale > 0.0f
                    ? static_cast<int>(static_cast<float>(px[0]) / scale + 0.5f)
                    : static_cast<int>(event.window.data1);
            // Growing client (maximize): do not arm resize cooldown — it defers the
            // Skia present that WM_SIZE / reconcile just prepared.
            if (derived_w <= static_cast<int>(event.window.data1) + 48) {
              m_resize_cooldown_frames = k_resize_cooldown_frames;
            } else {
              m_resize_cooldown_frames = 0;
            }
          }
          m_pending_win32_chrome_sync = true;
          if (m_resize_cooldown_frames > 0) {
            m_window_adapter->clearPresentSuppress();
          }
          // H31: defer Skia composite — synchronous render per resize SDL event
          // starves SDL_AppIterate during border drag.
          m_window_adapter->request_redraw();
          cacheLayoutRects();
        }
        break;
      case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        if (event.window.windowID == window_id && m_window_adapter) {
          m_window_resize_active = true;
          m_layout_resync_frames = k_layout_resync_frames;
          ++m_resize_events_pumped;
          m_force_window_commit = true;
          resetChromeSizeCache();
          if (m_window_system) {
            m_window_system->refreshClientPixelSizeFromHwnd();
          }
          m_window_adapter->pollDrawableSize();
          {
            const eastl::array<int, 2> px = m_window_system->getDrawableSize();
            const float scale = queryWindowScaleFactor(m_window_system);
            const int derived_w =
                scale > 0.0f
                    ? static_cast<int>(static_cast<float>(px[0]) / scale + 0.5f)
                    : 0;
            const eastl::array<int, 2> logical =
                m_window_system->getLogicalWindowSize();
            if (derived_w > logical[0] + 48) {
              m_resize_cooldown_frames = 0;
            } else {
              m_resize_cooldown_frames = k_resize_cooldown_frames;
            }
          }
          m_pending_win32_chrome_sync = true;
          if (m_resize_cooldown_frames > 0) {
            m_window_adapter->clearPresentSuppress();
          }
          m_window_adapter->request_redraw();
          cacheLayoutRects();
        }
        break;
      case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
      case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
      case SDL_EVENT_WINDOW_MAXIMIZED:
      case SDL_EVENT_WINDOW_RESTORED:
        if (event.window.windowID == window_id && m_window_adapter) {
          m_window_resize_active = true;
          m_resize_cooldown_frames = 0;
          ++m_resize_events_pumped;
          m_force_window_commit = true;
          m_layout_resync_frames = k_layout_resync_frames;
          if (event.type == SDL_EVENT_WINDOW_MAXIMIZED) {
            m_maximize_layout_frames = k_maximize_layout_frames;
          }
          resetChromeSizeCache();
          if (m_window_system) {
            m_window_system->refreshClientPixelSizeFromHwnd();
          }
          m_window_adapter->pollDrawableSize();
          syncWindowChromeSize();
          cacheLayoutRects();
          m_window_adapter->request_redraw();
        }
        break;
      case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
        if (event.window.windowID == window_id) {
          window.dispatch_scale_factor_change_event(
              queryWindowScaleFactor(m_window_system));
          m_window_adapter->request_redraw();
        }
        break;
      case SDL_EVENT_WINDOW_FOCUS_GAINED:
        if (event.window.windowID == window_id) {
          window.dispatch_window_active_changed_event(true);
        }
        break;
      case SDL_EVENT_WINDOW_FOCUS_LOST:
        if (event.window.windowID == window_id) {
          window.dispatch_window_active_changed_event(false);
        }
        break;
      case SDL_EVENT_WINDOW_MOUSE_LEAVE:
        if (event.window.windowID == window_id) {
          window.dispatch_pointer_exit_event();
        }
        break;
      case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        if (event.window.windowID == window_id) {
          window.dispatch_close_requested_event();
        }
        break;
      case SDL_EVENT_MOUSE_MOTION:
        if (event.motion.windowID == window_id) {
          m_last_mouse_window_x = static_cast<float>(event.motion.x);
          m_last_mouse_window_y = static_cast<float>(event.motion.y);
          m_pending_sdl_mouse_motion = true;
          ++m_sdl_motion_events_coalesced;
        }
        break;
      case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (event.button.windowID == window_id) {
          if (m_window_resize_active && m_win32_size_modal) {
            break;
          }
          const float raw_x = static_cast<float>(event.button.x);
          const float raw_y = static_cast<float>(event.button.y);
          const SlintPointerCoords logical_ptr =
              mapWindowPointerToSlint(m_window_system, raw_x, raw_y);
          m_last_mouse_window_x = raw_x;
          m_last_mouse_window_y = raw_y;
          if (event.button.button == SDL_BUTTON_LEFT) {
            cacheViewportLogicalRectOnly();
            cacheLayoutRects();
          }
          const bool modal_dialog_open = m_window_component &&
              m_window_component->operator->()->get_import_mesh_dialog_visible();
          if (event.button.button == SDL_BUTTON_LEFT && !modal_dialog_open) {
            const bool proj_hit =
                probeProjectionButtonAtLogical(logical_ptr.x, logical_ptr.y);
            if (proj_hit) {
              requestViewportProjectionToggle("chrome_sdl_press");
              m_projection_toggle_sdl_frame = m_frame_counter;
              m_left_mouse_down_prev = true;
              break;
            }
          }
          window.dispatch_pointer_press_event(
              slintPointerPosition(m_window_system, raw_x, raw_y),
              mapPointerButton(event.button.button));
        }
        break;
      case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event.button.windowID == window_id) {
          if (m_win32_size_modal) {
            break;
          }
          const float window_x = static_cast<float>(event.button.x);
          const float window_y = static_cast<float>(event.button.y);
          flushPendingPointerMove();
          if (!isDockLayoutDragActive()) {
            if (event.button.button == SDL_BUTTON_LEFT) {
              cacheViewportLogicalRectOnly();
              m_left_mouse_down_prev = false;
            }
            if (const auto release_services = lockServices();
                event.button.button == SDL_BUTTON_LEFT && release_services &&
                release_services->content_browser && m_window_component) {
              const slint::LogicalPosition logical_pos =
                  slintPointerPosition(m_window_system, window_x, window_y);
              finishContentBrowserDrag(logical_pos.x, logical_pos.y);
            }
          }
          window.dispatch_pointer_release_event(
              slintPointerPosition(m_window_system, window_x, window_y),
              mapPointerButton(event.button.button));
          if (!isDockLayoutDragActive() && event.button.button == SDL_BUTTON_LEFT &&
              !m_tree_folder_handled_by_slint) {
            const slint::LogicalPosition logical_pos =
                slintPointerPosition(m_window_system, window_x, window_y);
            trySelectContentBrowserTreeFolder(logical_pos.x, logical_pos.y);
          }
        }
        break;
      case SDL_EVENT_MOUSE_WHEEL:
        if (event.wheel.windowID == window_id) {
          if (m_window_resize_active || isDockLayoutDragActive()) {
            break;
          }
          const float window_x = static_cast<float>(event.wheel.mouse_x);
          const float window_y = static_cast<float>(event.wheel.mouse_y);
          const float wheel_x = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED
                                    ? -event.wheel.x
                                    : event.wheel.x;
          const float wheel_y = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED
                                    ? -event.wheel.y
                                    : event.wheel.y;
          window.dispatch_pointer_scroll_event(
              slintPointerPosition(m_window_system, window_x, window_y),
              wheel_x * 20.0f, wheel_y * 20.0f);
        }
        break;
      case SDL_EVENT_KEY_DOWN:
        if (event.key.windowID == window_id) {
          if (!event.key.repeat && event.key.key == SDLK_P) {
            requestViewportProjectionToggle("keyboard_p");
          }
          const slint::SharedString key_text = mapKeycode(event.key.key);
          if (!key_text.empty() && isSpecialKey(event.key.key)) {
            if (event.key.repeat) {
              window.dispatch_key_press_repeat_event(key_text);
            } else {
              window.dispatch_key_press_event(key_text);
            }
          }
        }
        break;
      case SDL_EVENT_KEY_UP:
        if (event.key.windowID == window_id) {
          const slint::SharedString key_text = mapKeycode(event.key.key);
          if (!key_text.empty() && isSpecialKey(event.key.key)) {
            window.dispatch_key_release_event(key_text);
          }
        }
        break;
      case SDL_EVENT_TEXT_INPUT:
        if (event.text.windowID == window_id && event.text.text) {
          window.dispatch_key_press_event(slint::SharedString(event.text.text));
        }
        break;
      case SDL_EVENT_DROP_BEGIN:
        if (event.drop.windowID == window_id) {
          refreshBrowserRectCache();
          m_pending_os_drop_files.clear();
          m_os_drop_targets_browser =
              pointInRect(event.drop.x, event.drop.y, m_cached_browser_logical_rect);
        }
        break;
      case SDL_EVENT_DROP_FILE:
        if (event.drop.windowID == window_id && event.drop.data) {
          refreshBrowserRectCache();
          m_pending_os_drop_files.push_back(
              eastl::string(event.drop.data));
          if (!m_os_drop_targets_browser) {
            m_os_drop_targets_browser = pointInRect(
                event.drop.x, event.drop.y, m_cached_browser_logical_rect);
          }
        }
        break;
      case SDL_EVENT_DROP_COMPLETE:
        if (event.drop.windowID == window_id && m_os_drop_targets_browser &&
            !m_pending_os_drop_files.empty()) {
          eastl::vector<eastl::string> mesh_paths;
          eastl::vector<eastl::string> direct_paths;
          for (const eastl::string& path : m_pending_os_drop_files) {
            const eastl::string ext = extensionLowerFromPath(path);
            if (AssetImportService::isMeshSourceExtension(ext)) {
              mesh_paths.push_back(path);
            } else if (AssetImportService::isTextureSourceExtension(ext)) {
              direct_paths.push_back(path);
            }
          }

          if (const auto drop_services = lockServices();
              drop_services && drop_services->asset_import &&
              drop_services->content_browser) {
            const eastl::string target_folder =
                drop_services->content_browser->selectedFolder();
            if (!direct_paths.empty()) {
              finalizeAssetImport(drop_services->asset_import->importExternalFiles(
                  direct_paths, target_folder));
            }
            if (!mesh_paths.empty()) {
              m_pending_mesh_import_paths.insert(m_pending_mesh_import_paths.end(),
                                                 mesh_paths.begin(),
                                                 mesh_paths.end());
              showImportMeshDialogForPendingPaths();
            }
          }
        }
        m_pending_os_drop_files.clear();
        m_os_drop_targets_browser = false;
        break;
      default:
        break;
    }
  } catch (const std::exception& e) {
    LOG_ERROR("[SlintSystem::processEvent] {}", e.what());
    if (m_window_system) {
      m_window_system->requestClose();
    }
  } catch (...) {
    LOG_ERROR("[SlintSystem::processEvent] unknown exception");
    if (m_window_system) {
      m_window_system->requestClose();
    }
  }
  applyPendingViewportInvalidate();
}
}  // namespace Blunder
