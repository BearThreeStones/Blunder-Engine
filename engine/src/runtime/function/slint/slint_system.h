#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include <optional>

#include <slint-platform.h>

#include "EASTL/array.h"
#include "EASTL/vector.h"

#include "editor_window.h"

#include "runtime/platform/window/window_system.h"
#include "runtime/function/render/blinn_phong_editor_settings.h"

namespace Blunder {

class MaterialAsset;

struct ViewportLogicalRect {
  int32_t x{0};
  int32_t y{0};
  uint32_t width{0};
  uint32_t height{0};
};

using BrowserLogicalRect = ViewportLogicalRect;

struct SlintSystemInitInfo {
  WindowSystem* window_system{nullptr};
};

class SlintSystem final {
 public:
  SlintSystem() = default;
  ~SlintSystem();

  void initialize(const SlintSystemInitInfo& init_info);
  void shutdown();

  /// Drives Slint's per-frame work: timers/animations + window present.
  /// With the SkiaRenderer backend, this also performs the GPU composite
  /// and Present on the window's HWND.
  void update();

  /// Layout pass: timers, window resize sync, and cache of panel/viewport rects.
  /// Call before rendererTick() so 3D off-screen size matches current UI layout.
  void beginFrame();

  /// Skia composite/present for the editor window.
  void endFrame();

  /// True while a window-edge resize drag is in progress (includes cooldown
  /// frames after the last SDL resize event — Windows often omits events
  /// between drag steps).
  bool isWindowResizeActive() const {
    return m_window_resize_active || m_resize_cooldown_frames > 0;
  }

  /// Skip Vulkan readback and deferred Slint commits (window resize + dock splitters).
  bool isDockLayoutDragActive() const;

  bool shouldDeferHeavyFrameWork() const {
    return m_win32_size_modal || isWindowResizeActive() ||
           m_layout_cooldown_frames > 0 || isDockLayoutDragActive();
  }

  /// True only for Win32 border sizing — skip Skia present (OS stretch).
  bool shouldSkipSkiaPresentDuringDefer() const;

  /// Refreshes the resize cooldown (SDL resize / live-resize expose).
  void notifyWindowResizeActivity();

  /// Records drawable size during Win32 modal resize; must stay cheap.
  void noteWin32SizingTick();

  /// Win32 WM_SIZE (maximize/restore); client_w/h are WM_SIZE lParam client pixels.
  void notifyWin32ClientAreaChanged(uintptr_t wm_size_param = 0, int client_w = 0,
                                    int client_h = 0);

  /// Slint layout commit after WM_EXITSIZEMOVE (no composite).
  void finishLiveResizeModal();

  void clearDeferAfterWindowResize();

  void compositeEditorFrame();

  void setWin32SizeModalActive(bool active) { m_win32_size_modal = active; }

  void processEvent(const SDL_Event& event);

  /// Pushes a fresh 3D viewport image (RGBA8, top-left origin) into the
  /// `viewport-image` property of the editor window. The pixel data is
  /// copied into a Slint SharedPixelBuffer; the caller does not need to
  /// keep the source buffer alive after the call returns.
  void setViewportImage(const uint8_t* pixels_rgba, uint32_t width,
                        uint32_t height);

  /// Returns the logical pixel rect of the central 3D viewport rectangle in
  /// the Slint UI. All values are zero until the window has performed its
  /// first layout.
  ViewportLogicalRect getViewportLogicalRect() const;

  /// Returns the logical pixel size of the central 3D viewport rectangle in
  /// the Slint UI. {0, 0} until the window has performed its first layout.
  eastl::array<uint32_t, 2> getViewportLogicalSize() const;

  /// Reads the inspector Blinn-Phong properties. Returns defaults if the window
  /// is not ready.
  BlinnPhongEditorSettings getBlinnPhongEditorSettings() const;

  /// Source mesh material used by "Sync Asset" and initial inspector sync.
  void setBlinnPhongMaterialSource(const MaterialAsset* material);
  void syncBlinnPhongFromMaterialSource();

  /// Pushes ContentBrowserSystem tree/grid rows into the Slint panel.
  void syncContentBrowser();

  void syncHierarchy();
  void syncInspectorFromSelection();
  void applyInspectorTransform();

  void refreshEditorScenePanels();

  BrowserLogicalRect getBrowserLogicalRect() const;
  BrowserLogicalRect getHierarchyLogicalRect() const;
  bool isContentBrowserDragActive() const;

  /// Select/expand a content-browser tree folder from window mouse coordinates.
  /// Returns true when a folder row was hit and state was updated.
  bool trySelectContentBrowserTreeFolder(float window_x, float window_y);

  void clearContentBrowserSlintClickFlag();

  /// Clears per-frame content-browser input state; call once before pumpEvents.
  void beginContentBrowserInputFrame();

  /// Detects tree-folder clicks via cursor poll (Win32/SDL); call after pumpEvents.
  void tickContentBrowserTreePointerPoll();

  bool trySelectHierarchyEntity(float window_x, float window_y);
  void tickHierarchyPointerPoll();

  void clearHierarchySlintClickFlag();

  class SlintWindowAdapter final : public slint::platform::WindowAdapter {
   public:
    explicit SlintWindowAdapter(WindowSystem* window_system);
    ~SlintWindowAdapter() override = default;

    slint::platform::AbstractRenderer& renderer() override;
    slint::PhysicalSize size() override;
    void set_visible(bool visible) override;
    void request_redraw() override;
    void update_window_properties(const WindowProperties& properties) override;

    /// Lazily creates the SkiaRenderer once the window is shown for the
    /// first time, then composites and presents the Slint scene to the
    /// HWND. Called from SlintSystem::update().
    void renderIfNeeded();

    /// Reads SDL drawable size; defers Slint dispatch until commitWindowSize().
    void pollDrawableSize();

    /// Re-reads logical + physical size from SDL (used at commit time).
    void refreshTargetSizesFromWindow();

    /// Dispatches Slint resize/scale and syncs committed sizes (for compositeFrame).
    void applyWindowLayoutNow(int override_logical_w = 0, int override_logical_h = 0);

    /// Dispatches Slint resize + rebuilds Skia after size is stable (or when forced).
    void commitWindowSize(bool force = false);

    /// Skia composite/present only (assumes commitWindowSize ran in beginFrame).
    void compositeFrame();

    void suppressPresentFrames(uint32_t frame_count);
    void clearPresentSuppress() { m_present_suppress_frames = 0; }

    bool needsRedraw() const { return m_needs_redraw; }

   private:
    void ensureRenderer();

    WindowSystem* m_window_system{nullptr};
    std::unique_ptr<slint::platform::SkiaRenderer> m_renderer;
    bool m_visible{false};
    bool m_needs_redraw{true};
    slint::PhysicalSize m_target_size{{0u, 0u}};
    slint::PhysicalSize m_target_logical_size{{0u, 0u}};
    slint::PhysicalSize m_committed_size{{0u, 0u}};
    slint::PhysicalSize m_committed_logical_size{{0u, 0u}};
    slint::PhysicalSize m_renderer_size{{0u, 0u}};
    uint32_t m_size_stable_frames{0};
    /// Skip Skia present for N frames after resize (swapchain settle).
    uint32_t m_present_suppress_frames{0};
  };

  class SlintPlatform final : public slint::platform::Platform {
   public:
    explicit SlintPlatform(WindowSystem* window_system);
    std::unique_ptr<slint::platform::WindowAdapter> create_window_adapter()
        override;

    SlintWindowAdapter* getWindowAdapter() const { return m_window_adapter; }

   private:
    WindowSystem* m_window_system{nullptr};
    SlintWindowAdapter* m_window_adapter{nullptr};
  };

  static slint::SharedString mapKeycode(SDL_Keycode keycode);
  static slint::PointerEventButton mapPointerButton(Uint8 button);
  static bool isSpecialKey(SDL_Keycode keycode);

 private:
  void cacheLayoutRects();
  void cacheViewportLogicalRectOnly();
  void runEditorPanelSync();
  void refreshBrowserRectCache();
  /// Polls SDL/Win32 size and applies Slint layout (dispatch_resize + committed).
  /// Optional override from SDL_EVENT_WINDOW_RESIZED (logical px).
  void syncWindowChromeSize(int override_logical_w = 0, int override_logical_h = 0);

  WindowSystem* m_window_system{nullptr};
  SlintWindowAdapter* m_window_adapter{nullptr};
  std::optional<slint::ComponentHandle<MainEditorWindow>> m_window_component;
  ViewportLogicalRect m_cached_viewport_logical_rect{};
  BrowserLogicalRect m_cached_browser_logical_rect{};
  BrowserLogicalRect m_cached_hierarchy_logical_rect{};
  int m_slint_dispatch_depth{0};
  const MaterialAsset* m_blinn_phong_material_source{nullptr};
  eastl::string m_drop_highlight_path;
  eastl::vector<eastl::string> m_pending_os_drop_files;
  bool m_os_drop_targets_browser{false};
  bool m_tree_folder_handled_by_slint{false};
  bool m_hierarchy_handled_by_slint{false};
  bool m_left_mouse_down_prev{false};
  bool m_applying_inspector_sync{false};
  bool m_force_window_commit{false};
  bool m_window_resize_active{false};
  uint32_t m_resize_events_pumped{0};
  uint32_t m_resize_cooldown_frames{0};
  uint32_t m_layout_cooldown_frames{0};
  uint32_t m_last_viewport_w{0};
  uint32_t m_last_viewport_h{0};
  bool m_win32_size_modal{false};
  bool m_dock_layout_drag_active{false};
  uint32_t m_layout_resync_frames{0};
  bool m_pending_win32_chrome_sync{false};
  uint32_t m_maximize_layout_frames{0};
  static constexpr uint32_t k_resize_cooldown_frames = 4;
  static constexpr uint32_t k_maximize_layout_frames = 12;
  static constexpr uint32_t k_layout_cooldown_frames = 6;
  static constexpr uint32_t k_layout_resync_frames = 16;
};
}  // namespace Blunder
