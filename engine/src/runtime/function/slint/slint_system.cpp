#include "runtime/function/slint/slint_system.h"

#include <slint.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <exception>
#include <string>
#if defined(_WIN32)
#include <windows.h>
#endif
#include <string_view>
#include <utility>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3/SDL_timer.h>
#include <glm/vec3.hpp>

#include "runtime/core/base/macro.h"
#include "runtime/function/global/global_context.h"
#include "runtime/resource/asset/material_asset.h"
#include "runtime/resource/content_browser/content_browser_system.h"
#include "runtime/resource/asset_import/asset_import_service.h"
#include "runtime/resource/asset_cook/asset_compiler_service.h"
#include "runtime/function/editor/editor_selection_system.h"
#include "runtime/function/editor/editor_scene_edit_system.h"
#include "runtime/function/editor/hierarchy_system.h"
#include "runtime/function/scene/entity.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_serializer.h"
#include "runtime/function/scene/scene_system.h"
#include "runtime/function/scene/scene_render_bridge.h"
#include "runtime/function/scene/scene_render_bridge.h"
#include "runtime/function/render/render_system.h"
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

SlintSystem::SlintWindowAdapter::SlintWindowAdapter(WindowSystem* window_system)
    : m_window_system(window_system) {}

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
  m_renderer = std::make_unique<slint::platform::SkiaRenderer>(
      handle, m_committed_size);
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
#if defined(_WIN32)
      if (void* hwnd_opaque = m_window_system->getNativeWin32Hwnd()) {
        InvalidateRect(static_cast<HWND>(hwnd_opaque), nullptr, FALSE);
      }
      SDL_PumpEvents();
#endif
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

  // Always re-render: the engine pushes a new viewport image every frame, so
  // even if Slint's UI itself isn't dirty, the central Image control needs
  // to repaint with the latest 3D texture.
#if defined(_WIN32)
  // H19: Pumping here during dock-splitter drag re-enters SDL_AppEvent (motion
  // flood) before Skia render returns — stalls the UI. Events arrive via SDL_AppEvent.
  const bool skip_pump =
      g_runtime_global_context.m_slint_system &&
      (g_runtime_global_context.m_slint_system->isDockLayoutDragActive() ||
       g_runtime_global_context.m_slint_system->isSplitterResizeInteractionActive());
  if (!skip_pump) {
    SDL_PumpEvents();
  }
#endif
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
  m_needs_redraw = window().has_active_animations();
}

void SlintSystem::SlintWindowAdapter::renderIfNeeded() {
  pollDrawableSize();
  commitWindowSize();
  compositeFrame();
}

SlintSystem::SlintPlatform::SlintPlatform(WindowSystem* window_system)
    : m_window_system(window_system) {}

std::unique_ptr<slint::platform::WindowAdapter>
SlintSystem::SlintPlatform::create_window_adapter() {
  auto adapter = std::make_unique<SlintWindowAdapter>(m_window_system);
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

  try {
    if (!g_slint_platform_instance) {
      auto platform = std::make_unique<SlintPlatform>(m_window_system);
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
    component->on_sync_shading_from_asset(
        [this]() { syncBlinnPhongFromMaterialSource(); });

    component->on_save_scene_requested([this]() {
      if (g_runtime_global_context.m_editor_scene_edit) {
        g_runtime_global_context.m_editor_scene_edit->saveActiveScene();
      }
    });

    // Slint splitters are visual-only; C++ owns drag (see tryBeginCppDockSplitterDrag).
    // Ignoring this callback prevents Slint from hiding the Content Browser mid-drag.
    component->on_dock_layout_drag_changed([](bool) {});

    component->on_hierarchy_entity_selected([this](int entity_id) {
      m_hierarchy_handled_by_slint = true;
      if (!g_runtime_global_context.m_editor_selection) {
        return;
      }
      g_runtime_global_context.m_editor_selection->setSelection(
          static_cast<EntityId>(entity_id));
      syncInspectorFromSelection();
      syncHierarchy();
    });

    component->on_hierarchy_entity_toggle([this](int entity_id) {
      m_hierarchy_handled_by_slint = true;
      if (!g_runtime_global_context.m_hierarchy) {
        return;
      }
      g_runtime_global_context.m_hierarchy->toggleExpanded(
          static_cast<EntityId>(entity_id));
      if (g_runtime_global_context.m_scene_system &&
          g_runtime_global_context.m_scene_system->getActiveInstance()) {
        g_runtime_global_context.m_hierarchy->rebuildVisibleTree(
            g_runtime_global_context.m_scene_system->getActiveInstance());
      }
      syncHierarchy();
    });

    component->on_inspector_transform_edited([this]() { applyInspectorTransform(); });

    component->on_browser_refresh_requested([this]() {
      if (!g_runtime_global_context.m_content_browser) {
        return;
      }
      g_runtime_global_context.m_content_browser->refresh();
      syncContentBrowser();
    });
    component->on_browser_import_requested([this]() { openImportFileDialog(); });
    component->on_import_mesh_confirmed([this]() { completePendingMeshImport(); });
    component->on_import_mesh_cancelled([this]() {
      m_pending_mesh_import_paths.clear();
      if (m_window_component) {
        m_window_component->operator->()->set_import_mesh_dialog_visible(false);
      }
    });
    component->on_browser_folder_selected(
        [this](const slint::SharedString& path) {
      m_tree_folder_handled_by_slint = true;
      if (!g_runtime_global_context.m_content_browser) {
        return;
      }
      g_runtime_global_context.m_content_browser->setSelectedFolder(
          eastl::string(path.data()));
      syncContentBrowser();
    });
    component->on_browser_folder_toggle(
        [this](const slint::SharedString& path) {
      m_tree_folder_handled_by_slint = true;
      if (!g_runtime_global_context.m_content_browser) {
        return;
      }
      g_runtime_global_context.m_content_browser->toggleFolderExpanded(
          eastl::string(path.data()));
      syncContentBrowser();
    });
    component->on_browser_item_press(
        [this](const slint::SharedString& path, float x, float y) {
          if (!g_runtime_global_context.m_content_browser) {
            return;
          }
          g_runtime_global_context.m_content_browser->dragController().beginPress(
              eastl::string(path.data()), x, y);
          m_drop_highlight_path.clear();
          syncContentBrowser();
        });
    component->on_browser_item_move(
        [this](const slint::SharedString& path, float x, float y) {
          (void)path;
          if (!g_runtime_global_context.m_content_browser) {
            return;
          }
          g_runtime_global_context.m_content_browser->dragController().updateMove(
              x, y);
        });
    component->on_browser_item_release(
        [this](const slint::SharedString& path, float x, float y) {
          (void)x;
          (void)y;
          if (!g_runtime_global_context.m_content_browser) {
            return;
          }
          ContentBrowserSystem& browser_system =
              *g_runtime_global_context.m_content_browser;
          ContentBrowserDragController& drag = browser_system.dragController();
          const bool was_dragging = drag.isDragging();

          if (was_dragging) {
            finishContentBrowserDragAtCursor();
          } else {
            drag.endPress();
            drag.reset();
            m_drop_highlight_path.clear();
            m_viewport_drop_active = false;
            syncContentBrowser();

            const eastl::string path_str(path.data());
            constexpr const char* k_scene_suffix = ".scene.asset";
            const size_t suffix_len = 14u;
            if (path_str.size() >= suffix_len &&
                path_str.compare(path_str.size() - suffix_len, suffix_len,
                                 k_scene_suffix) == 0 &&
                g_runtime_global_context.m_editor_scene_edit) {
              if (g_runtime_global_context.m_editor_scene_edit->openScene(path_str)) {
                if (g_runtime_global_context.m_render_system &&
                    g_runtime_global_context.m_scene_system) {
                  syncSceneToRender(
                      g_runtime_global_context.m_render_system.get(),
                      g_runtime_global_context.m_scene_system->getActiveInstance());
                }
                refreshEditorScenePanels();
              }
            }
          }
        });

    component->show();
    m_window_component = component;

    m_window_adapter = g_slint_platform_instance->getWindowAdapter();
    ASSERT(m_window_adapter);

    m_window_system->setNativeEventCallback(
        [this](const SDL_Event& event) { processEvent(event); });

    syncWindowChromeSize();
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

void SlintSystem::setViewportImage(const uint8_t* pixels_rgba, uint32_t width,
                                   uint32_t height) {
  if (!m_window_component || !pixels_rgba || width == 0 || height == 0) {
    return;
  }
  if (m_slint_dispatch_depth > 0) {
    return;
  }
  static bool s_logged_first_viewport_upload = false;
  static uint32_t s_last_upload_w = 0;
  static uint32_t s_last_upload_h = 0;
  try {
    ScopedDispatchGuard guard(m_slint_dispatch_depth);
    slint::SharedPixelBuffer<slint::Rgba8Pixel> buffer(
        width, height, reinterpret_cast<const slint::Rgba8Pixel*>(pixels_rgba));
    slint::Image image(buffer);
    m_window_component->operator->()->set_viewport_image(image);
    m_viewport_image_dirty = true;
    if (m_window_adapter) {
      m_window_adapter->request_redraw();
    }
    if (!s_logged_first_viewport_upload) {
      LOG_INFO("[SlintSystem] first viewport image upload: {}x{}", width, height);
      s_logged_first_viewport_upload = true;
      s_last_upload_w = width;
      s_last_upload_h = height;
    } else if (width != s_last_upload_w || height != s_last_upload_h) {
      LOG_INFO("[SlintSystem] viewport image upload resized: {}x{} -> {}x{}",
               s_last_upload_w, s_last_upload_h, width, height);
      s_last_upload_w = width;
      s_last_upload_h = height;
    }
  } catch (const std::exception& e) {
    LOG_ERROR("[SlintSystem::setViewportImage] {}", e.what());
    if (m_window_system) {
      m_window_system->requestClose();
    }
  } catch (...) {
    LOG_ERROR("[SlintSystem::setViewportImage] unknown exception");
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
  float scale_x{1.0f};
  float scale_y{1.0f};
};

SlintPointerCoords mapWindowPointerToSlint(WindowSystem* window_system, float x,
                                           float y) {
  SlintPointerCoords out{x, y, 1.0f, 1.0f};
  if (!window_system) {
    return out;
  }
  const eastl::array<int, 2> logical = window_system->getLogicalWindowSize();
  const eastl::array<int, 2> drawable = window_system->getDrawableSize();

  // Slint layout uses logical client size. Map SDL/Win32 client coords into that space.
#if defined(_WIN32)
  // SDL3 README-highdpi: on Windows, mouse events are always physical client pixels.
  if (logical[0] > 0 && logical[1] > 0) {
    if (drawable[0] > logical[0] + 1 || drawable[1] > logical[1] + 1) {
      out.x = x * static_cast<float>(logical[0]) / static_cast<float>(drawable[0]);
      out.y = y * static_cast<float>(logical[1]) / static_cast<float>(drawable[1]);
      out.scale_x = static_cast<float>(drawable[0]) / static_cast<float>(logical[0]);
      out.scale_y = static_cast<float>(drawable[1]) / static_cast<float>(logical[1]);
      return out;
    }
    const float display_scale = window_system->getDisplayScale();
    if (display_scale > 1.01f) {
      out.x = x / display_scale;
      out.y = y / display_scale;
      out.scale_x = display_scale;
      out.scale_y = display_scale;
    }
  }
  return out;
#else
  const eastl::array<int, 2> sdl_win = window_system->getWindowSize();
  if (sdl_win[0] > 0 && sdl_win[1] > 0 && logical[0] > 0 && logical[1] > 0 &&
      (logical[0] != sdl_win[0] || logical[1] != sdl_win[1])) {
    out.x = x * static_cast<float>(logical[0]) / static_cast<float>(sdl_win[0]);
    out.y = y * static_cast<float>(logical[1]) / static_cast<float>(sdl_win[1]);
  }
  return out;
#endif
}

struct WindowClientMouseState {
  float x{0.0f};
  float y{0.0f};
  bool left_down{false};
  const char* source{"none"};
};

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <Windows.h>
namespace win32_mouse_query {
bool sample(WindowSystem* window_system, float* out_x, float* out_y,
            bool* out_left_down) {
  if (!window_system || !out_x || !out_y || !out_left_down) {
    return false;
  }
  void* hwnd_opaque = window_system->getNativeWin32Hwnd();
  if (!hwnd_opaque) {
    return false;
  }
  HWND hwnd = static_cast<HWND>(hwnd_opaque);
  POINT cursor{};
  if (GetCursorPos(&cursor) == 0 || ScreenToClient(hwnd, &cursor) == 0) {
    return false;
  }
  *out_x = static_cast<float>(cursor.x);
  *out_y = static_cast<float>(cursor.y);
  *out_left_down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
  return true;
}
}  // namespace win32_mouse_query
#endif

WindowClientMouseState queryWindowClientMouseState(WindowSystem* window_system) {
  WindowClientMouseState state{};
#if defined(_WIN32)
  if (win32_mouse_query::sample(window_system, &state.x, &state.y, &state.left_down)) {
    state.source = "win32";
    return state;
  }
#endif
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

void SlintSystem::syncHierarchy() {
  if (!m_window_component || !g_runtime_global_context.m_hierarchy) {
    return;
  }

  HierarchySystem& hierarchy = *g_runtime_global_context.m_hierarchy;
  const EntityId selected =
      g_runtime_global_context.m_editor_selection
          ? g_runtime_global_context.m_editor_selection->getSelection()
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

  EditorSelectionSystem* selection = g_runtime_global_context.m_editor_selection.get();
  SceneInstance* scene =
      g_runtime_global_context.m_scene_system
          ? g_runtime_global_context.m_scene_system->getActiveInstance()
          : nullptr;

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

  EditorSelectionSystem* selection = g_runtime_global_context.m_editor_selection.get();
  SceneInstance* scene =
      g_runtime_global_context.m_scene_system
          ? g_runtime_global_context.m_scene_system->getActiveInstance()
          : nullptr;
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
    if (g_runtime_global_context.m_editor_scene_edit) {
      g_runtime_global_context.m_editor_scene_edit->markDirty();
    }
  } catch (const std::exception& e) {
    LOG_ERROR("[SlintSystem::applyInspectorTransform] {}", e.what());
  } catch (...) {
    LOG_ERROR("[SlintSystem::applyInspectorTransform] unknown exception");
  }
}

void SlintSystem::refreshEditorScenePanels() {
  if (g_runtime_global_context.m_scene_system &&
      g_runtime_global_context.m_hierarchy) {
    g_runtime_global_context.m_hierarchy->rebuildVisibleTree(
        g_runtime_global_context.m_scene_system->getActiveInstance());
    g_runtime_global_context.m_hierarchy->markDirty();
  }
  syncHierarchy();
  syncInspectorFromSelection();
}

namespace {

void onImportFileDialogCallback(void* userdata, const char* const* filelist,
                                int filter) {
  (void)filter;
  auto* slint_system = static_cast<SlintSystem*>(userdata);
  if (slint_system == nullptr || filelist == nullptr) {
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
  if (g_runtime_global_context.m_asset_compiler) {
    g_runtime_global_context.m_asset_compiler->cookIfStale();
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
  if (!m_window_component || !g_runtime_global_context.m_asset_import ||
      !g_runtime_global_context.m_content_browser ||
      m_pending_mesh_import_paths.empty()) {
    return;
  }

  MeshImportSettings settings{};
  settings.materials = m_window_component->operator->()->get_import_mesh_materials();
  settings.animations =
      m_window_component->operator->()->get_import_mesh_animations();
  settings.scale = m_window_component->operator->()->get_import_mesh_scale();

  const eastl::string target_folder =
      g_runtime_global_context.m_content_browser->selectedFolder();
  const eastl::vector<ImportResult> results =
      g_runtime_global_context.m_asset_import->importExternalFiles(
          m_pending_mesh_import_paths, target_folder, settings);

  m_pending_mesh_import_paths.clear();
  m_window_component->operator->()->set_import_mesh_dialog_visible(false);
  finalizeAssetImport(results);
}

void SlintSystem::processPendingAssetImports() {
  if (m_pending_file_dialog_paths.empty()) {
    return;
  }

  if (!g_runtime_global_context.m_asset_import ||
      !g_runtime_global_context.m_content_browser) {
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

  const eastl::string target_folder =
      g_runtime_global_context.m_content_browser->selectedFolder();

  if (!direct_paths.empty()) {
    finalizeAssetImport(
        g_runtime_global_context.m_asset_import->importExternalFiles(
            direct_paths, target_folder));
  }

  if (!mesh_paths.empty()) {
    m_pending_mesh_import_paths.insert(m_pending_mesh_import_paths.end(),
                                       mesh_paths.begin(), mesh_paths.end());
    showImportMeshDialogForPendingPaths();
  }
}

void SlintSystem::openImportFileDialog() {
  if (!m_window_system) {
    return;
  }

  static const SDL_DialogFileFilter filters[] = {
      {"Models and Images", "gltf;glb;png;jpg;jpeg;bmp;tga"},
      {"All Files", "*"},
  };

  SDL_ShowOpenFileDialog(onImportFileDialogCallback, this,
                         m_window_system->getNativeWindow(), filters, 2,
                         nullptr, true);
}

void SlintSystem::syncContentBrowser() {
  if (!m_window_component || !g_runtime_global_context.m_content_browser) {
    return;
  }
  ContentBrowserSystem& browser_system =
      *g_runtime_global_context.m_content_browser;

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
      grid_model->push_back(slint_row);
    }
    m_window_component->operator->()->set_browser_grid_rows(grid_model);

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

void SlintSystem::finishContentBrowserDrag(float logical_x, float logical_y) {
  if (!g_runtime_global_context.m_content_browser) {
    return;
  }

  ContentBrowserSystem& browser_system = *g_runtime_global_context.m_content_browser;
  ContentBrowserDragController& drag = browser_system.dragController();
  if (!drag.isDragging()) {
    return;
  }

  const eastl::string source = drag.sourcePath();
  drag.endPress();

  if (!source.empty()) {
    if (isPointerOverViewport(logical_x, logical_y) &&
        g_runtime_global_context.m_editor_scene_edit) {
      const SpawnAssetResult spawn_result =
          g_runtime_global_context.m_editor_scene_edit->spawnAssetAtWindowPosition(
              source, logical_x, logical_y);
      if (spawn_result.success && g_runtime_global_context.m_render_system &&
          g_runtime_global_context.m_scene_system) {
        syncSceneToRender(g_runtime_global_context.m_render_system.get(),
                          g_runtime_global_context.m_scene_system->getActiveInstance());
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
  if (!g_runtime_global_context.m_content_browser) {
    return false;
  }
  return g_runtime_global_context.m_content_browser->dragController().isDragging();
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

void SlintSystem::tickContentBrowserTreePointerPoll() {
  if (!m_window_system || !m_window_component ||
      !g_runtime_global_context.m_content_browser) {
    return;
  }

  const WindowClientMouseState mouse =
      queryWindowClientMouseState(m_window_system);
  const SlintPointerCoords pointer =
      mapWindowPointerToSlint(m_window_system, mouse.x, mouse.y);

  if (mouse.left_down && !m_left_mouse_down_prev) {
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
  if (!m_window_system || !m_window_component ||
      !g_runtime_global_context.m_hierarchy ||
      !g_runtime_global_context.m_editor_selection) {
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

  HierarchySystem& hierarchy = *g_runtime_global_context.m_hierarchy;
  EditorSelectionSystem& selection = *g_runtime_global_context.m_editor_selection;

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
  if (!m_window_system || !m_window_component ||
      !g_runtime_global_context.m_content_browser) {
    return false;
  }

  if (m_tree_folder_handled_by_slint) {
    return false;
  }

  ContentBrowserSystem& browser_system =
      *g_runtime_global_context.m_content_browser;
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
  if (tree_panel_height <= 0.0f) {
    return false;
  }
  const bool in_browser_x =
      window_x >= static_cast<float>(browser_origin_x) &&
      window_x <
          static_cast<float>(browser_origin_x +
                             static_cast<int32_t>(browser_rect.width));
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
    const auto& component = *m_window_component;
    const float x = component->get_viewport_origin_x();
    const float y = component->get_viewport_origin_y();
    const float w = component->get_viewport_width();
    const float h = component->get_viewport_height();
    m_cached_viewport_logical_rect.x = static_cast<int32_t>(x);
    m_cached_viewport_logical_rect.y = static_cast<int32_t>(y);
    m_cached_viewport_logical_rect.width =
        w > 0.0f ? static_cast<uint32_t>(w) : 0u;
    m_cached_viewport_logical_rect.height =
        h > 0.0f ? static_cast<uint32_t>(h) : 0u;

    m_cached_browser_logical_rect.x =
        static_cast<int32_t>(component->get_browser_origin_x());
    m_cached_browser_logical_rect.y =
        static_cast<int32_t>(component->get_browser_origin_y());
    const float browser_w = component->get_browser_width();
    const float browser_h = component->get_browser_height();
    m_cached_browser_logical_rect.width =
        browser_w > 0.0f ? static_cast<uint32_t>(browser_w) : 0u;
    m_cached_browser_logical_rect.height =
        browser_h > 0.0f ? static_cast<uint32_t>(browser_h) : 0u;

    m_cached_hierarchy_logical_rect.x =
        static_cast<int32_t>(component->get_hierarchy_origin_x());
    m_cached_hierarchy_logical_rect.y =
        static_cast<int32_t>(component->get_hierarchy_origin_y());
    const float hierarchy_w = component->get_hierarchy_width();
    const float hierarchy_h = component->get_hierarchy_height();
    m_cached_hierarchy_logical_rect.width =
        hierarchy_w > 0.0f ? static_cast<uint32_t>(hierarchy_w) : 0u;
    m_cached_hierarchy_logical_rect.height =
        hierarchy_h > 0.0f ? static_cast<uint32_t>(hierarchy_h) : 0u;

    const uint32_t vp_w = m_cached_viewport_logical_rect.width;
    const uint32_t vp_h = m_cached_viewport_logical_rect.height;
    const bool viewport_changed =
        m_last_viewport_w != 0 &&
        (vp_w != m_last_viewport_w || vp_h != m_last_viewport_h);
    if (viewport_changed) {
      m_layout_cooldown_frames = k_layout_cooldown_frames;
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
  if (!m_window_component) {
    return;
  }
  const auto& component = *m_window_component;
  const float x = component->get_viewport_origin_x();
  const float y = component->get_viewport_origin_y();
  const float w = component->get_viewport_width();
  const float h = component->get_viewport_height();
  m_cached_viewport_logical_rect.x = static_cast<int32_t>(x);
  m_cached_viewport_logical_rect.y = static_cast<int32_t>(y);
  m_cached_viewport_logical_rect.width = w > 0.0f ? static_cast<uint32_t>(w) : 0u;
  m_cached_viewport_logical_rect.height = h > 0.0f ? static_cast<uint32_t>(h) : 0u;

  const uint32_t vp_w = m_cached_viewport_logical_rect.width;
  const uint32_t vp_h = m_cached_viewport_logical_rect.height;
  const bool viewport_changed =
      m_last_viewport_w != 0 &&
      (vp_w != m_last_viewport_w || vp_h != m_last_viewport_h);
  if (viewport_changed) {
    m_layout_cooldown_frames = k_layout_cooldown_frames;
  }
  m_last_viewport_w = vp_w;
  m_last_viewport_h = vp_h;
  if (!viewport_changed && m_layout_cooldown_frames > 0) {
    --m_layout_cooldown_frames;
  }
}

void SlintSystem::refreshBrowserRectCache() {
  if (!m_window_component) {
    return;
  }
  m_cached_browser_logical_rect =
      readBrowserRectFromUi(*m_window_component->operator->());
}

void SlintSystem::runEditorPanelSync() {
  if (g_runtime_global_context.m_editor_selection &&
      g_runtime_global_context.m_editor_selection->isDirty()) {
    syncInspectorFromSelection();
  }
  if (g_runtime_global_context.m_hierarchy &&
      g_runtime_global_context.m_hierarchy->isDirty()) {
    syncHierarchy();
  }

  tickContentBrowserTreePointerPoll();
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
  if (wm_size_param == SIZE_MAXIMIZED) {
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
}

void SlintSystem::setDockLayoutDragActive(bool active) {
  m_dock_layout_drag_active = active;
  if (m_window_component) {
    m_window_component->operator->()->set_dock_layout_drag_active(active);
  }
  if (!active) {
    m_layout_cooldown_frames = k_layout_cooldown_frames;
    m_pending_dock_layout_present = true;
    m_pending_pointer_move = false;
    m_coalesced_pointer_moves = 0;
    m_dock_splitter_drag = DockSplitterDrag::none;
    m_pending_dock_pane_apply = false;
    m_last_applied_dock_pane_size = -1.0f;
  } else {
    m_coalesced_pointer_moves = 0;
  }
  LOG_INFO("[SlintSystem] dock layout drag active={}", active);
}

namespace {

float roundDockPaneLength(float value) {
  return std::round(value / 8.0f) * 8.0f;
}

float clampFloat(float value, float min_v, float max_v) {
  return eastl::max(min_v, eastl::min(max_v, value));
}

#if defined(_WIN32)
void setWin32DockSplitterCursor(DockSplitterDrag kind) {
  LPCTSTR cursor_id = IDC_ARROW;
  switch (kind) {
    case DockSplitterDrag::leftVertical:
    case DockSplitterDrag::rightVertical:
      cursor_id = IDC_SIZEWE;
      break;
    case DockSplitterDrag::hierarchyHorizontal:
    case DockSplitterDrag::browserTreeHorizontal:
      cursor_id = IDC_SIZENS;
      break;
    default:
      break;
  }
  SetCursor(LoadCursor(nullptr, cursor_id));
}
#endif

DockSplitterDrag hitTestDockSplitterAt(const ViewportLogicalRect& vp,
                                       const BrowserLogicalRect& hierarchy,
                                       const BrowserLogicalRect& browser,
                                       float tree_split_y, float grab_px, float x,
                                       float y) {
  DockSplitterDrag kind = DockSplitterDrag::none;
  if (vp.width > 0 && vp.height > 0) {
    const float vp_top = static_cast<float>(vp.y);
    const float vp_bottom = vp_top + static_cast<float>(vp.height);
    const float vp_left = static_cast<float>(vp.x);
    const float vp_right = vp_left + static_cast<float>(vp.width);
    if (y >= vp_top && y < vp_bottom) {
      if (x >= vp_left - grab_px && x <= vp_left + 2.0f) {
        kind = DockSplitterDrag::leftVertical;
      } else if (x >= vp_right - 2.0f && x <= vp_right + grab_px) {
        kind = DockSplitterDrag::rightVertical;
      }
    }
  }

  if (kind == DockSplitterDrag::none && hierarchy.width > 0 && hierarchy.height > 0) {
    const float split_y =
        static_cast<float>(hierarchy.y + static_cast<int32_t>(hierarchy.height));
    if (x >= static_cast<float>(hierarchy.x) &&
        x < static_cast<float>(hierarchy.x + static_cast<int32_t>(hierarchy.width)) &&
        y >= split_y - 3.0f && y <= split_y + grab_px) {
      kind = DockSplitterDrag::hierarchyHorizontal;
    }
  }

  if (kind == DockSplitterDrag::none && browser.width > 0 && tree_split_y > 0.0f) {
    if (x >= static_cast<float>(browser.x) &&
        x < static_cast<float>(browser.x + static_cast<int32_t>(browser.width)) &&
        y >= tree_split_y - 3.0f && y <= tree_split_y + grab_px) {
      kind = DockSplitterDrag::browserTreeHorizontal;
    }
  }
  return kind;
}

/// Fallback when viewport-origin properties lag: use pane widths from Slint state.
DockSplitterDrag hitTestDockSplitterFromPaneWidths(const MainEditorWindow& ui,
                                                   float editor_logical_w, float x,
                                                   float y, float grab_px) {
  if (editor_logical_w <= 0.0f) {
    return DockSplitterDrag::none;
  }
  constexpr float k_menu_bar_h = 44.0f;
  const float left_w = ui.get_left_dock_width();
  const float right_w = ui.get_right_dock_width();
  if (y >= k_menu_bar_h) {
    if (x >= left_w - grab_px && x <= left_w + 24.0f) {
      return DockSplitterDrag::leftVertical;
    }
    const float right_split_x = editor_logical_w - right_w;
    if (x >= right_split_x - 24.0f && x <= right_split_x + grab_px) {
      return DockSplitterDrag::rightVertical;
    }
  }

  const float hierarchy_h = ui.get_hierarchy_pane_height();
  const float hierarchy_w = ui.get_hierarchy_width();
  if (hierarchy_w > 0.0f && hierarchy_h > 0.0f) {
    const float hier_x = ui.get_hierarchy_origin_x();
    const float hier_y = ui.get_hierarchy_origin_y();
    const float split_y = hier_y + hierarchy_h;
    if (x >= hier_x && x < hier_x + hierarchy_w && y >= split_y - grab_px &&
        y <= split_y + 16.0f) {
      return DockSplitterDrag::hierarchyHorizontal;
    }
  }

  const float browser_w = ui.get_browser_width();
  const float tree_panel_h = ui.get_browser_tree_panel_height();
  if (browser_w > 0.0f && tree_panel_h > 0.0f) {
    const float browser_x = ui.get_browser_origin_x();
    const float tree_split_y =
        static_cast<float>(resolveContentBrowserTreeOriginY(ui)) + tree_panel_h;
    if (x >= browser_x && x < browser_x + browser_w && y >= tree_split_y - grab_px &&
        y <= tree_split_y + 16.0f) {
      return DockSplitterDrag::browserTreeHorizontal;
    }
  }
  return DockSplitterDrag::none;
}

DockSplitterDrag hitTestDockSplitterFromLayoutProperties(const MainEditorWindow& ui,
                                                         float slint_x, float slint_y,
                                                         float grab_px) {
  const float vp_x = ui.get_viewport_origin_x();
  const float vp_y = ui.get_viewport_origin_y();
  const float vp_w = ui.get_viewport_width();
  const float vp_h = ui.get_viewport_height();
  if (vp_w > 0.0f && vp_h > 0.0f) {
    const float vp_bottom = vp_y + vp_h;
    if (slint_y >= vp_y && slint_y < vp_bottom) {
      if (slint_x >= vp_x - grab_px && slint_x <= vp_x + 6.0f) {
        return DockSplitterDrag::leftVertical;
      }
      const float vp_right = vp_x + vp_w;
      if (slint_x >= vp_right - 6.0f && slint_x <= vp_right + grab_px) {
        return DockSplitterDrag::rightVertical;
      }
    }
  }

  const float hierarchy_w = ui.get_hierarchy_width();
  const float hierarchy_h = ui.get_hierarchy_height();
  if (hierarchy_w > 0.0f && hierarchy_h > 0.0f) {
    const float hier_x = ui.get_hierarchy_origin_x();
    const float hier_y = ui.get_hierarchy_origin_y();
    const float split_y = hier_y + hierarchy_h;
    if (slint_x >= hier_x && slint_x < hier_x + hierarchy_w && slint_y >= split_y - 4.0f &&
        slint_y <= split_y + grab_px) {
      return DockSplitterDrag::hierarchyHorizontal;
    }
  }

  const float browser_w = ui.get_browser_width();
  const float tree_panel_h = ui.get_browser_tree_panel_height();
  if (browser_w > 0.0f && tree_panel_h > 0.0f) {
    const float browser_x = ui.get_browser_origin_x();
    const float tree_split_y =
        static_cast<float>(resolveContentBrowserTreeOriginY(ui)) + tree_panel_h;
    if (slint_x >= browser_x && slint_x < browser_x + browser_w &&
        slint_y >= tree_split_y - 4.0f && slint_y <= tree_split_y + grab_px) {
      return DockSplitterDrag::browserTreeHorizontal;
    }
  }
  return DockSplitterDrag::none;
}

#if defined(_WIN32)
void captureWin32MouseForCppSplitterDrag(WindowSystem* window_system) {
  if (!window_system) {
    return;
  }
  void* hwnd_opaque = window_system->getNativeWin32Hwnd();
  if (hwnd_opaque != nullptr) {
    SetCapture(static_cast<HWND>(hwnd_opaque));
  }
}

void releaseWin32MouseCapture() {
  ReleaseCapture();
}
#endif

}  // namespace

void SlintSystem::refreshDockSplitterGeometryFromUi() {
  if (!m_window_component) {
    return;
  }
  const MainEditorWindow& ui = *m_window_component->operator->();
  const float vp_w = ui.get_viewport_width();
  const float vp_h = ui.get_viewport_height();
  m_cached_viewport_logical_rect.x = static_cast<int32_t>(ui.get_viewport_origin_x());
  m_cached_viewport_logical_rect.y = static_cast<int32_t>(ui.get_viewport_origin_y());
  m_cached_viewport_logical_rect.width = vp_w > 0.0f ? static_cast<uint32_t>(vp_w) : 0u;
  m_cached_viewport_logical_rect.height = vp_h > 0.0f ? static_cast<uint32_t>(vp_h) : 0u;
  m_cached_browser_logical_rect = readBrowserRectFromUi(ui);
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

void SlintSystem::refreshDockSplitterHitCache() {
  if (!m_window_component) {
    m_dock_hit_cache_valid = false;
    return;
  }
  refreshDockSplitterGeometryFromUi();
  const MainEditorWindow& ui = *m_window_component->operator->();
  const float tree_panel_h = ui.get_browser_tree_panel_height();
  m_cached_tree_split_y =
      tree_panel_h > 0.0f
          ? static_cast<float>(resolveContentBrowserTreeOriginY(ui)) + tree_panel_h
          : 0.0f;
  m_dock_hit_cache_valid = true;
}

DockSplitterDrag SlintSystem::queryDockSplitterAtFast(float window_x,
                                                      float window_y) const {
  if (!m_window_component || !m_dock_hit_cache_valid) {
    return DockSplitterDrag::none;
  }
  const MainEditorWindow& ui = *m_window_component->operator->();
  const SlintPointerCoords pointer =
      mapWindowPointerToSlint(m_window_system, window_x, window_y);
  constexpr float k_grab_px = 80.0f;
  DockSplitterDrag kind =
      hitTestDockSplitterFromLayoutProperties(ui, pointer.x, pointer.y, k_grab_px);
  if (kind == DockSplitterDrag::none) {
    kind = hitTestDockSplitterFromLayoutProperties(ui, window_x, window_y, k_grab_px);
  }
  if (kind == DockSplitterDrag::none) {
    kind = hitTestDockSplitterAt(m_cached_viewport_logical_rect,
                               m_cached_hierarchy_logical_rect,
                               m_cached_browser_logical_rect, m_cached_tree_split_y,
                               k_grab_px, pointer.x, pointer.y);
  }
  if (kind == DockSplitterDrag::none) {
    kind = hitTestDockSplitterAt(m_cached_viewport_logical_rect,
                               m_cached_hierarchy_logical_rect,
                               m_cached_browser_logical_rect, m_cached_tree_split_y,
                               k_grab_px, window_x, window_y);
  }
  if (kind == DockSplitterDrag::none && m_window_system) {
    const eastl::array<int, 2> logical_win = m_window_system->getLogicalWindowSize();
    const float editor_w = static_cast<float>(eastl::max(logical_win[0], 1));
    kind = hitTestDockSplitterFromPaneWidths(ui, editor_w, pointer.x, pointer.y, k_grab_px);
    if (kind == DockSplitterDrag::none) {
      kind = hitTestDockSplitterFromPaneWidths(ui, editor_w, window_x, window_y, k_grab_px);
    }
  }
  return kind;
}

bool SlintSystem::isSplitterResizeInteractionActive() const {
  return m_dock_splitter_drag != DockSplitterDrag::none || m_splitter_resize_active ||
         m_pending_dock_pane_apply || m_deferred_apply_kind != DockSplitterDrag::none;
}

bool SlintSystem::shouldDeferHeavyFrameWork() const {
  if (m_win32_size_modal || isWindowResizeActive() || m_layout_cooldown_frames > 0 ||
      isDockLayoutDragActive()) {
    return true;
  }
#if defined(_WIN32)
  if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0 && m_last_mouse_window_x >= 0.0f &&
      isPointerNearDockSplitter(m_last_mouse_window_x, m_last_mouse_window_y)) {
    return true;
  }
#endif
  return false;
}

bool SlintSystem::shouldRouteMouseToInputLayers(const SDL_Event& event) const {
  if (m_dock_splitter_drag != DockSplitterDrag::none || m_splitter_resize_active) {
    return false;
  }
  if (event.type == SDL_EVENT_MOUSE_MOTION) {
#if defined(_WIN32)
    if (m_last_mouse_window_x >= 0.0f &&
        (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0 &&
        queryDockSplitterAtFast(m_last_mouse_window_x, m_last_mouse_window_y) !=
            DockSplitterDrag::none) {
      return false;
    }
#endif
    return true;
  }
  switch (event.type) {
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
      const float x = static_cast<float>(event.button.x);
      const float y = static_cast<float>(event.button.y);
      return queryDockSplitterAtFast(x, y) == DockSplitterDrag::none;
    }
    default:
      return true;
  }
}

DockSplitterDrag SlintSystem::queryDockSplitterAt(float window_x, float window_y) {
  return queryDockSplitterAtFast(window_x, window_y);
}

bool SlintSystem::isPointerNearDockSplitter(float window_x, float window_y) const {
  return queryDockSplitterAtFast(window_x, window_y) != DockSplitterDrag::none;
}

bool SlintSystem::beginCppDockSplitterDragFromKind(DockSplitterDrag kind) {
  if (!m_window_component || kind == DockSplitterDrag::none) {
    return false;
  }
  if (m_dock_splitter_drag != DockSplitterDrag::none) {
    return true;
  }

  float window_x = m_last_mouse_window_x;
  float window_y = m_last_mouse_window_y;
#if defined(_WIN32)
  bool left_down = false;
  if (win32_mouse_query::sample(m_window_system, &window_x, &window_y, &left_down) &&
      left_down) {
    m_last_mouse_window_x = window_x;
    m_last_mouse_window_y = window_y;
  }
#endif
  if (window_x < 0.0f) {
    window_x = 0.0f;
    window_y = 0.0f;
  }

  const MainEditorWindow& ui = *m_window_component->operator->();
  const SlintPointerCoords pointer =
      mapWindowPointerToSlint(m_window_system, window_x, window_y);

  m_dock_splitter_drag = kind;
  m_splitter_resize_active = true;
  // Hide Content Browser grid during drag (Slint only — no mid-drag set_* layout).
  m_window_component->operator->()->set_dock_layout_drag_active(true);
  switch (kind) {
    case DockSplitterDrag::leftVertical:
      m_splitter_drag_mouse_start = pointer.x;
      m_splitter_drag_pane_start = ui.get_left_dock_width();
      break;
    case DockSplitterDrag::rightVertical:
      m_splitter_drag_mouse_start = pointer.x;
      m_splitter_drag_pane_start = ui.get_right_dock_width();
      break;
    case DockSplitterDrag::hierarchyHorizontal:
      m_splitter_drag_mouse_start = pointer.y;
      m_splitter_drag_pane_start = ui.get_hierarchy_pane_height();
      break;
    case DockSplitterDrag::browserTreeHorizontal:
      m_splitter_drag_mouse_start = pointer.y;
      m_splitter_drag_pane_start = ui.get_browser_tree_panel_height();
      break;
    default:
      m_dock_splitter_drag = DockSplitterDrag::none;
      m_splitter_resize_active = false;
      return false;
  }

#if defined(_WIN32)
  setWin32DockSplitterCursor(kind);
  captureWin32MouseForCppSplitterDrag(m_window_system);
#endif
  LOG_INFO("[SlintSystem] C++ dock splitter drag began (kind={})",
           static_cast<unsigned>(kind));
  return true;
}

bool SlintSystem::tryBeginCppDockSplitterDrag(float window_x, float window_y) {
  if (!m_window_component) {
    return false;
  }

  const DockSplitterDrag kind = queryDockSplitterAt(window_x, window_y);
  if (kind == DockSplitterDrag::none) {
    return false;
  }

  m_last_mouse_window_x = window_x;
  m_last_mouse_window_y = window_y;
  return beginCppDockSplitterDragFromKind(kind);
}

void SlintSystem::updateCppDockSplitterDrag(float window_x, float window_y) {
  if (m_dock_splitter_drag == DockSplitterDrag::none || !m_window_component) {
    return;
  }

  const SlintPointerCoords pointer =
      mapWindowPointerToSlint(m_window_system, window_x, window_y);
  auto& ui = *m_window_component->operator->();
  const eastl::array<int, 2> logical_win = m_window_system->getLogicalWindowSize();
  const float editor_w = static_cast<float>(eastl::max(logical_win[0], 1));

  float new_size = m_splitter_drag_pane_start;
  switch (m_dock_splitter_drag) {
    case DockSplitterDrag::leftVertical: {
      const float max_pane =
          eastl::max(180.0f, editor_w - ui.get_right_dock_width() - 360.0f);
      new_size = clampFloat(m_splitter_drag_pane_start + (pointer.x - m_splitter_drag_mouse_start),
                            180.0f, max_pane);
      break;
    }
    case DockSplitterDrag::rightVertical: {
      const float max_pane =
          eastl::max(260.0f, editor_w - ui.get_left_dock_width() - 360.0f);
      new_size = clampFloat(
          m_splitter_drag_pane_start + (m_splitter_drag_mouse_start - pointer.x), 260.0f,
          max_pane);
      break;
    }
    case DockSplitterDrag::hierarchyHorizontal: {
      const float max_pane =
          eastl::max(120.0f, static_cast<float>(m_cached_hierarchy_logical_rect.height) > 0
                                  ? static_cast<float>(logical_win[1]) - 200.0f
                                  : 600.0f);
      new_size =
          clampFloat(m_splitter_drag_pane_start + (pointer.y - m_splitter_drag_mouse_start),
                     120.0f, max_pane);
      break;
    }
    case DockSplitterDrag::browserTreeHorizontal: {
      const float browser_body_h =
          m_cached_browser_logical_rect.height > 0
              ? static_cast<float>(m_cached_browser_logical_rect.height)
              : static_cast<float>(logical_win[1]);
      const float max_pane = eastl::max(80.0f, browser_body_h - 120.0f);
      new_size =
          clampFloat(m_splitter_drag_pane_start + (pointer.y - m_splitter_drag_mouse_start),
                     80.0f, max_pane);
      break;
    }
    default:
      break;
  }

  m_pending_dock_pane_size = roundDockPaneLength(new_size);
  m_pending_dock_pane_apply = true;
}

void SlintSystem::queueCppDockSplitterMotion(float window_x, float window_y) {
  m_pending_cpp_drag_x = window_x;
  m_pending_cpp_drag_y = window_y;
  m_pending_cpp_drag_motion = true;
}

void SlintSystem::flushPendingCppDockSplitterMotion() {
  if (!m_pending_cpp_drag_motion) {
    return;
  }
  m_pending_cpp_drag_motion = false;
  updateCppDockSplitterDrag(m_pending_cpp_drag_x, m_pending_cpp_drag_y);
}

void SlintSystem::applyDockPaneResize(DockSplitterDrag kind) {
  if (!m_pending_dock_pane_apply || !m_window_component || kind == DockSplitterDrag::none) {
    return;
  }

  if (m_last_applied_dock_pane_size >= 0.0f &&
      std::fabs(m_pending_dock_pane_size - m_last_applied_dock_pane_size) < 0.5f) {
    m_pending_dock_pane_apply = false;
    return;
  }

  m_pending_dock_pane_apply = false;
  m_last_applied_dock_pane_size = m_pending_dock_pane_size;
  const uint64_t apply_t0_ns = SDL_GetTicksNS();
  MainEditorWindow& ui = *m_window_component->operator->();
  switch (kind) {
    case DockSplitterDrag::leftVertical:
      ui.set_left_dock_width(m_pending_dock_pane_size);
      break;
    case DockSplitterDrag::rightVertical:
      ui.set_right_dock_width(m_pending_dock_pane_size);
      break;
    case DockSplitterDrag::hierarchyHorizontal:
      ui.set_hierarchy_pane_height(m_pending_dock_pane_size);
      break;
    case DockSplitterDrag::browserTreeHorizontal:
      ui.set_browser_tree_panel_height(m_pending_dock_pane_size);
      break;
    default:
      break;
  }
  const uint64_t apply_ms = (SDL_GetTicksNS() - apply_t0_ns) / 1'000'000u;
  m_viewport_image_dirty = true;
  if (m_window_adapter) {
    m_window_adapter->request_redraw();
  }
  LOG_INFO("[SlintSystem] dock pane apply kind={} size={} took {} ms",
           static_cast<unsigned>(kind), m_pending_dock_pane_size, apply_ms);
}

void SlintSystem::applyPendingDockPaneResize() {
  applyDockPaneResize(m_dock_splitter_drag);
}

void SlintSystem::pollWin32DockSplitterPointerState() {
#if defined(_WIN32)
  if (!m_window_system || !m_window_component || m_win32_size_modal) {
    return;
  }
  float x = 0.0f;
  float y = 0.0f;
  bool left_down = false;
  if (!win32_mouse_query::sample(m_window_system, &x, &y, &left_down)) {
    return;
  }
  m_last_mouse_window_x = x;
  m_last_mouse_window_y = y;

  if (left_down) {
    if (m_dock_splitter_drag != DockSplitterDrag::none) {
      updateCppDockSplitterDrag(x, y);
      return;
    }
    const DockSplitterDrag near_kind = queryDockSplitterAtFast(x, y);
    if (near_kind != DockSplitterDrag::none || isPointerNearDockSplitter(x, y)) {
      m_splitter_resize_active = true;
    }
    if (!m_left_mouse_down_prev) {
      refreshDockSplitterHitCache();
      const DockSplitterDrag kind = queryDockSplitterAtFast(x, y);
      if (kind != DockSplitterDrag::none || isPointerNearDockSplitter(x, y)) {
        m_splitter_resize_active = true;
        if (tryBeginCppDockSplitterDrag(x, y)) {
          m_left_mouse_down_prev = true;
        }
      }
    }
    return;
  }

  if (m_dock_splitter_drag != DockSplitterDrag::none) {
    updateCppDockSplitterDrag(x, y);
    endCppDockSplitterDrag();
    m_left_mouse_down_prev = false;
    return;
  }
  if (m_splitter_resize_active) {
    m_splitter_resize_active = false;
  }
#else
  (void)0;
#endif
}

void SlintSystem::endCppDockSplitterDrag() {
  const DockSplitterDrag ended_kind = m_dock_splitter_drag;
  if (ended_kind == DockSplitterDrag::none) {
    return;
  }
  m_last_applied_dock_pane_size = -1.0f;
  // H34: defer Slint layout apply out of SDL_AppEvent (mouse-up) into beginFrame.
  m_deferred_apply_kind = ended_kind;
  m_dock_splitter_drag = DockSplitterDrag::none;
  m_splitter_resize_active = false;
  m_layout_cooldown_frames = k_layout_cooldown_frames;
  refreshDockSplitterHitCache();
#if defined(_WIN32)
  SetCursor(LoadCursor(nullptr, IDC_ARROW));
  releaseWin32MouseCapture();
#endif
  LOG_INFO("[SlintSystem] C++ dock splitter drag ended (kind={})",
           static_cast<unsigned>(ended_kind));
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
  const DockSplitterDrag splitter_under_cursor = queryDockSplitterAtFast(window_x, window_y);

  if (m_dock_splitter_drag != DockSplitterDrag::none) {
    updateCppDockSplitterDrag(window_x, window_y);
    return;
  }
#if defined(_WIN32)
  if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0 &&
      splitter_under_cursor != DockSplitterDrag::none) {
    if (m_dock_splitter_drag == DockSplitterDrag::none) {
      tryBeginCppDockSplitterDrag(window_x, window_y);
    }
    if (m_dock_splitter_drag != DockSplitterDrag::none) {
      updateCppDockSplitterDrag(window_x, window_y);
    }
    return;
  }
#endif
  if (splitter_under_cursor != DockSplitterDrag::none) {
    return;
  }

  if (!isDockLayoutDragActive() && g_runtime_global_context.m_content_browser &&
      g_runtime_global_context.m_content_browser->dragController().isDragging()) {
    ContentBrowserSystem& browser_system = *g_runtime_global_context.m_content_browser;
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
  if (m_dock_splitter_drag != DockSplitterDrag::none || m_splitter_resize_active ||
      !m_pending_pointer_move || !m_window_adapter || !m_window_system) {
    return;
  }
#if defined(_WIN32)
  // H53: LMB drag near a dock splitter must not flood Slint pointer-move relayout.
  if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0 && m_last_mouse_window_x >= 0.0f &&
      queryDockSplitterAtFast(m_last_mouse_window_x, m_last_mouse_window_y) !=
          DockSplitterDrag::none) {
    m_pending_pointer_move = false;
    m_coalesced_pointer_moves = 0;
    if (m_dock_splitter_drag == DockSplitterDrag::none) {
      tryBeginCppDockSplitterDrag(m_last_mouse_window_x, m_last_mouse_window_y);
    }
    return;
  }
#endif
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
  if (m_pending_composite_after_dock_apply || m_pending_dock_layout_present) {
    return true;
  }
  if (m_viewport_image_dirty || m_window_adapter->needsRedraw()) {
    return true;
  }
  return false;
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

void SlintSystem::beginFrame() {
  try {
    ScopedDispatchGuard guard(m_slint_dispatch_depth);
    processPendingAssetImports();
    refreshDockSplitterHitCache();
    pollWin32DockSplitterPointerState();
    processCoalescedSdlMouseMotion();
    flushPendingCppDockSplitterMotion();
    if (m_deferred_apply_kind != DockSplitterDrag::none) {
      const DockSplitterDrag deferred_kind = m_deferred_apply_kind;
      m_deferred_apply_kind = DockSplitterDrag::none;
      applyDockPaneResize(deferred_kind);
      refreshDockSplitterHitCache();
      if (m_window_component) {
        m_window_component->operator->()->set_dock_layout_drag_active(false);
      }
      // H38: composite on the frame after Slint apply — not in the same frame as apply.
      m_pending_composite_after_dock_apply = true;
    }
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

      if (resize_events_this_frame > 8u) {
        LOG_DEBUG("[SlintSystem] coalesced {} resize SDL events this frame",
                  resize_events_this_frame);
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
    if (!defer_heavy) {
      runEditorPanelSync();
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
      if (m_pending_composite_after_dock_apply) {
        if (!shouldDeferHeavyFrameWork()) {
          m_pending_composite_after_dock_apply = false;
          m_window_adapter->compositeFrame();
          m_viewport_image_dirty = false;
          cacheViewportLogicalRectOnly();
        } else {
          m_window_adapter->request_redraw();
        }
      } else if (m_pending_dock_layout_present) {
        m_pending_dock_layout_present = false;
        m_window_adapter->compositeFrame();
        m_viewport_image_dirty = false;
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
            } else {
              m_window_adapter->request_redraw();
            }
          }
        } else if (splitter_interaction) {
          m_window_adapter->request_redraw();
        } else if (shouldPresentSkiaFrame()) {
          static uint64_t s_last_full_composite_ns = 0;
          const uint64_t now_ns = SDL_GetTicksNS();
          if (s_last_full_composite_ns == 0 ||
              now_ns - s_last_full_composite_ns >= 33'000'000) {
            s_last_full_composite_ns = now_ns;
            m_window_adapter->compositeFrame();
            m_viewport_image_dirty = false;
            cacheLayoutRects();
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
          // Allow splitter mouse-down during dock drag cooldown; only block during
          // live window-edge resize or an active C++ splitter capture.
          if (m_window_resize_active && m_win32_size_modal) {
            break;
          }
          if (m_dock_splitter_drag != DockSplitterDrag::none) {
            break;
          }
          const float window_x = static_cast<float>(event.button.x);
          const float window_y = static_cast<float>(event.button.y);
          m_last_mouse_window_x = window_x;
          m_last_mouse_window_y = window_y;
          refreshDockSplitterHitCache();
          if (event.button.button == SDL_BUTTON_LEFT) {
            const DockSplitterDrag splitter_kind =
                queryDockSplitterAtFast(window_x, window_y);
            if (splitter_kind != DockSplitterDrag::none ||
                isPointerNearDockSplitter(window_x, window_y)) {
              m_splitter_resize_active = true;
              tryBeginCppDockSplitterDrag(window_x, window_y);
              m_left_mouse_down_prev = true;
              // H51: do not dispatch_pointer_press into Slint on splitter hits.
              break;
            }
          }
          if (event.button.button == SDL_BUTTON_LEFT) {
            m_left_mouse_down_prev = true;
          }
          window.dispatch_pointer_press_event(
              slintPointerPosition(m_window_system, window_x, window_y),
              mapPointerButton(event.button.button));
        }
        break;
      case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event.button.windowID == window_id) {
          // H35: do not drop mouse-up while m_window_resize_active — layout apply during
          // splitter drag can set it and leave m_dock_splitter_drag / Win32 capture stuck.
          if (m_win32_size_modal) {
            break;
          }
          const float window_x = static_cast<float>(event.button.x);
          const float window_y = static_cast<float>(event.button.y);
          if (m_dock_splitter_drag != DockSplitterDrag::none) {
            m_last_mouse_window_x = window_x;
            m_last_mouse_window_y = window_y;
            updateCppDockSplitterDrag(window_x, window_y);
            endCppDockSplitterDrag();
            if (event.button.button == SDL_BUTTON_LEFT) {
              m_left_mouse_down_prev = false;
            }
            break;
          }
          if (event.button.button == SDL_BUTTON_LEFT) {
            m_splitter_resize_active = false;
          }
          flushPendingPointerMove();
          if (!isDockLayoutDragActive()) {
            if (event.button.button == SDL_BUTTON_LEFT) {
              m_left_mouse_down_prev = false;
            }
            if (event.button.button == SDL_BUTTON_LEFT &&
                g_runtime_global_context.m_content_browser && m_window_component) {
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

          if (g_runtime_global_context.m_asset_import &&
              g_runtime_global_context.m_content_browser) {
            const eastl::string target_folder =
                g_runtime_global_context.m_content_browser->selectedFolder();
            if (!direct_paths.empty()) {
              finalizeAssetImport(
                  g_runtime_global_context.m_asset_import->importExternalFiles(
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
}
}  // namespace Blunder
