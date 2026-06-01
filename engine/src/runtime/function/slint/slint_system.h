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
#include "runtime/function/ui/editor_ui_presentation.h"
#include "runtime/function/ui/ui_context.h"
#include "runtime/resource/asset_import/asset_import_service.h"

namespace Blunder {

class UiHost;

enum class DockSplitterDrag : uint8_t {
  none = 0,
  leftVertical,
  rightVertical,
  hierarchyHorizontal,
  browserTreeVertical,
  bottomHorizontal,
};

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
  eastl::weak_ptr<UiHost> ui_host;
  // Blunder shared-device path: when non-zero, the Slint Skia renderer adopts
  // the engine's Vulkan device (raw VkInstance/VkPhysicalDevice/VkDevice +
  // graphics queue family) instead of creating its own, enabling a zero-copy
  // 3D viewport. Zero means "use the self-owned Vulkan device".
  uint64_t shared_vk_instance{0};
  uint64_t shared_vk_physical_device{0};
  uint64_t shared_vk_device{0};
  uint32_t shared_vk_queue_family{0};
};

class SlintSystem final : public IEditorUiPresentation {
 public:
  SlintSystem() = default;
  ~SlintSystem();

  void initialize(const SlintSystemInitInfo& init_info);
  void shutdown();
  WindowSystem* getWindowSystem() const { return m_window_system; }

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

  /// Skip Vulkan readback and defer expensive layout/sync (window resize + dock splitters).
  bool isDockLayoutDragActive() const {
    return m_dock_layout_drag_active || m_dock_splitter_drag != DockSplitterDrag::none ||
           m_splitter_resize_active || m_pending_dock_pane_apply ||
           m_deferred_apply_kind != DockSplitterDrag::none;
  }

  /// True while the user is dragging or about to drag a dock splitter (C++ path).
  bool isSplitterResizeInteractionActive() const;

  bool shouldDeferHeavyFrameWork() const;

  /// False during dock-splitter drags so editor camera / layers do not see mouse events.
  bool shouldRouteMouseToInputLayers(const SDL_Event& event) const;

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

  void setViewportImageInternal(const uint8_t* pixels_rgba, uint32_t width,
                                uint32_t height, bool allow_during_dispatch);

  /// Zero-copy path: binds a borrowed engine `VkImage` directly to the viewport
  /// `Image` property (no CPU copy). Only valid when the Slint renderer runs on
  /// the engine's shared Vulkan device (see `viewportUsesSharedDevice()`).
  void setViewportExternalTexture(uint64_t image, uint32_t format,
                                  uint32_t layout, uint32_t width,
                                  uint32_t height);

  /// True when the Slint Skia renderer composites on the engine's shared Vulkan
  /// device, making zero-copy viewport presentation valid.
  bool viewportUsesSharedDevice() const;

  /// Updates the Persp/Iso label on the viewport overlay (Slint, not GPU).
  void syncViewportProjectionMode(bool is_perspective);

  /// Clears a stale 3D readback (e.g. after viewport resize) before the next render upload.
  void applyPendingViewportInvalidate();

  /// Returns the logical pixel rect of the central 3D viewport rectangle in
  /// the Slint UI. All values are zero until the window has performed its
  /// first layout.
  ViewportLogicalRect getViewportLogicalRect() const;

  /// Returns the logical pixel size of the central 3D viewport rectangle in
  /// the Slint UI. {0, 0} until the window has performed its first layout.
  eastl::array<uint32_t, 2> getViewportLogicalSize() const;

  /// Logical viewport size scaled for offscreen render target pixels (HiDPI).
  eastl::array<uint32_t, 2> getViewportRenderTargetSize() const;

  /// Reads the inspector Blinn-Phong properties. Returns defaults if the window
  /// is not ready.
  BlinnPhongEditorSettings getBlinnPhongEditorSettings() const;

  // IEditorUiPresentation
  BlinnPhongEditorSettings pullPreviewSettingsFromSlint() const override;
  void pushPreviewSettingsToSlint(const BlinnPhongEditorSettings& settings) override;
  void syncHierarchy() override;
  void syncInspectorFromSelection() override;
  void syncContentBrowser() override;
  void applyInspectorTransform() override;
  void refreshEditorScenePanels() override;
  void setBlinnPhongMaterialSource(const MaterialAsset* material) override;
  void syncBlinnPhongFromMaterialSource() override;
  void tickContentBrowserTreePointerPoll() override;

  BrowserLogicalRect getBrowserLogicalRect() const;
  BrowserLogicalRect getHierarchyLogicalRect() const;
  bool isContentBrowserDragActive() const;

  /// Select/expand a content-browser tree folder from window mouse coordinates.
  /// Returns true when a folder row was hit and state was updated.
  bool trySelectContentBrowserTreeFolder(float window_x, float window_y);

  void clearContentBrowserSlintClickFlag();

  /// Clears per-frame content-browser input state; call once before pumpEvents.
  void beginContentBrowserInputFrame();


  /// Persp/Iso via Win32/SDL cursor poll when SDL button events are starved.
  void tickProjectionTogglePointerPoll();

  bool trySelectHierarchyEntity(float window_x, float window_y);
  void tickHierarchyPointerPoll();

  void clearHierarchySlintClickFlag();

  void queueFileDialogImports(const eastl::vector<eastl::string>& paths);

  /// Hit-tests the Persp/Iso strip and toggles projection (engine mouse path).
  bool tryToggleProjectionAtWindow(float window_x, float window_y);

  /// Toggles Persp/Iso (keyboard / poll / SDL fallback when Slint did not apply).
  void requestViewportProjectionToggle(const char* source);

  class SlintWindowAdapter final : public slint::platform::WindowAdapter {
   public:
    SlintWindowAdapter(WindowSystem* window_system, uint64_t vk_instance = 0,
                       uint64_t vk_physical_device = 0, uint64_t vk_device = 0,
                       uint32_t vk_queue_family = 0);
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

    /// True once the SkiaRenderer was created on the engine's shared Vulkan
    /// device (zero-copy viewport eligible).
    bool usesSharedDevice() const { return m_uses_shared_device; }

    /// Owning SlintSystem (set after construction) for self drag-state queries.
    void setOwner(SlintSystem* owner) { m_owner = owner; }

   private:
    void ensureRenderer();

    SlintSystem* m_owner{nullptr};
    WindowSystem* m_window_system{nullptr};
    // Engine-owned Vulkan handles for the shared-device renderer (0 = self-owned).
    uint64_t m_vk_instance{0};
    uint64_t m_vk_physical_device{0};
    uint64_t m_vk_device{0};
    uint32_t m_vk_queue_family{0};
    bool m_uses_shared_device{false};
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
    SlintPlatform(WindowSystem* window_system, uint64_t vk_instance = 0,
                  uint64_t vk_physical_device = 0, uint64_t vk_device = 0,
                  uint32_t vk_queue_family = 0);
    std::unique_ptr<slint::platform::WindowAdapter> create_window_adapter()
        override;

    SlintWindowAdapter* getWindowAdapter() const { return m_window_adapter; }

   private:
    WindowSystem* m_window_system{nullptr};
    SlintWindowAdapter* m_window_adapter{nullptr};
    uint64_t m_vk_instance{0};
    uint64_t m_vk_physical_device{0};
    uint64_t m_vk_device{0};
    uint32_t m_vk_queue_family{0};
  };

  static slint::SharedString mapKeycode(SDL_Keycode keycode);
  static slint::PointerEventButton mapPointerButton(Uint8 button);
  static bool isSpecialKey(SDL_Keycode keycode);

 private:
  /// Resolves editor services through the UiHost weak_ptr (UiContext-guarded).
  /// Returns nullopt while the engine is shutting down or UiHost is gone.
  std::optional<UiContext::LockedServices> lockServices() const;

  void cacheLayoutRects();
  void cacheViewportLogicalRectOnly();
  void refreshDockSplitterHitCache();
  void refreshDockSplitterGeometryFromUi();
  DockSplitterDrag queryDockSplitterAtFast(float window_x, float window_y) const;
  void refreshBrowserRectCache();
  void processPendingAssetImports();
  void showImportMeshDialogForPendingPaths();
  void completePendingMeshImport();
  void openImportFileDialog();
  /// Schedules SDL_ShowOpenFileDialog on the main thread (outside Slint dispatch).
  void queueOpenImportFileDialog();
  void finalizeAssetImport(const eastl::vector<ImportResult>& results);
  /// Polls SDL/Win32 size and applies Slint layout (dispatch_resize + committed).
  /// Optional override from SDL_EVENT_WINDOW_RESIZED (logical px).
  void syncWindowChromeSize(int override_logical_w = 0, int override_logical_h = 0);
  /// Applies one coalesced pointer move (dock splitter drags flood SDL_AppEvent).
  void flushPendingPointerMove();
  void processCoalescedSdlMouseMotion();
  void setDockLayoutDragActive(bool active);
  DockSplitterDrag queryDockSplitterAt(float window_x, float window_y);
  bool isPointerNearDockSplitter(float window_x, float window_y) const;
  bool tryBeginCppDockSplitterDrag(float window_x, float window_y);
  bool beginCppDockSplitterDragFromKind(DockSplitterDrag kind);
  void updateCppDockSplitterDrag(float window_x, float window_y);
  void queueCppDockSplitterMotion(float window_x, float window_y);
  void flushPendingCppDockSplitterMotion();
  /// SDL mouse poll so splitter drag/release work when SDL_AppEvent is starved.
  void pollDockSplitterPointerState();
  void endCppDockSplitterDrag();
  void applyDockPaneResize(DockSplitterDrag kind);
  void applyPendingDockPaneResize();
  void finishContentBrowserDrag(float logical_x, float logical_y);
  void finishContentBrowserDragAtCursor();
  bool isPointerOverViewport(float logical_x, float logical_y) const;

  bool probeProjectionButtonAtLogical(float logical_x, float logical_y) const;
  bool probeProjectionButtonAtWindow(float window_x, float window_y) const;
  /// Applies camera + label to match Slint `viewport-is-perspective` (no toggle).
  bool applyViewportProjection(bool is_perspective, const char* source);

  eastl::weak_ptr<UiHost> m_ui_host;
  WindowSystem* m_window_system{nullptr};
  SlintWindowAdapter* m_window_adapter{nullptr};
  std::optional<slint::ComponentHandle<MainEditorWindow>> m_window_component;
  ViewportLogicalRect m_cached_viewport_logical_rect{};
  BrowserLogicalRect m_cached_browser_logical_rect{};
  BrowserLogicalRect m_cached_hierarchy_logical_rect{};
  int m_slint_dispatch_depth{0};
  const MaterialAsset* m_blinn_phong_material_source{nullptr};
  eastl::string m_drop_highlight_path;
  bool m_viewport_drop_active{false};
  eastl::vector<eastl::string> m_pending_os_drop_files;
  bool m_os_drop_targets_browser{false};
  eastl::vector<eastl::string> m_pending_mesh_import_paths;
  eastl::vector<eastl::string> m_pending_file_dialog_paths;
  bool m_pending_file_dialog_is_import{false};
  Uint32 m_open_import_dialog_event{0};
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
  bool m_dock_hit_cache_valid{false};
  bool m_splitter_resize_active{false};
  float m_cached_tree_split_y{0.0f};
  DockSplitterDrag m_dock_splitter_drag{DockSplitterDrag::none};
  DockSplitterDrag m_deferred_apply_kind{DockSplitterDrag::none};
  float m_splitter_drag_mouse_start{0.0f};
  float m_splitter_drag_pane_start{0.0f};
  float m_pending_dock_pane_size{0.0f};
  float m_last_applied_dock_pane_size{-1.0f};
  bool m_pending_dock_pane_apply{false};
  bool m_pending_dock_layout_present{false};
  bool m_pending_composite_after_dock_apply{false};
  bool m_viewport_image_dirty{false};
  bool m_viewport_image_stale{true};
  bool m_pending_viewport_invalidate{false};
  uint32_t m_viewport_upload_width{0};
  uint32_t m_viewport_upload_height{0};
  bool m_cached_viewport_is_perspective{true};
  /// Monotonic frame counter, incremented at the start of each beginFrame().
  uint64_t m_frame_counter{0};
  /// Frame number when SDL MOUSE_BUTTON_DOWN last toggled projection.
  /// tickProjectionTogglePointerPoll skips if m_projection_toggle_sdl_frame == m_frame_counter.
  uint64_t m_projection_toggle_sdl_frame{0};
  /// Cleared at beginFrame; blocks double-toggle when SDL + poll fire same frame.
  bool m_projection_toggle_consumed_this_frame{false};
  bool shouldPresentSkiaFrame() const;
  bool m_pending_pointer_move{false};
  float m_pending_pointer_x{0.0f};
  float m_pending_pointer_y{0.0f};
  uint32_t m_coalesced_pointer_moves{0};
  bool m_pending_cpp_drag_motion{false};
  float m_pending_cpp_drag_x{0.0f};
  float m_pending_cpp_drag_y{0.0f};
  uint32_t m_layout_resync_frames{0};
  bool m_pending_win32_chrome_sync{false};
  bool m_pending_content_browser_sync{false};
  float m_last_mouse_window_x{-1.0f};
  float m_last_mouse_window_y{-1.0f};
  bool m_pending_sdl_mouse_motion{false};
  uint32_t m_sdl_motion_events_coalesced{0};
  uint32_t m_maximize_layout_frames{0};
  static constexpr uint32_t k_resize_cooldown_frames = 4;
  static constexpr uint32_t k_maximize_layout_frames = 12;
  static constexpr uint32_t k_layout_cooldown_frames = 6;
  static constexpr uint32_t k_layout_resync_frames = 16;
};

}  // namespace Blunder
