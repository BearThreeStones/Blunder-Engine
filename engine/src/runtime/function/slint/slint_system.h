#pragma once

#include <cstdint>
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

  BrowserLogicalRect getBrowserLogicalRect() const;
  bool isContentBrowserDragActive() const;

  /// Select/expand a content-browser tree folder from window mouse coordinates.
  /// Returns true when a folder row was hit and state was updated.
  bool trySelectContentBrowserTreeFolder(float window_x, float window_y);

  void clearContentBrowserSlintClickFlag();

  /// Clears per-frame content-browser input state; call once before pumpEvents.
  void beginContentBrowserInputFrame();

  /// Detects tree-folder clicks via cursor poll (Win32/SDL); call after pumpEvents.
  void tickContentBrowserTreePointerPoll();

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
    bool needsRedraw() const { return m_needs_redraw; }

   private:
    void ensureRenderer();

    WindowSystem* m_window_system{nullptr};
    std::unique_ptr<slint::platform::SkiaRenderer> m_renderer;
    bool m_visible{false};
    bool m_needs_redraw{true};
    slint::PhysicalSize m_last_size{{0u, 0u}};
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

  WindowSystem* m_window_system{nullptr};
  SlintWindowAdapter* m_window_adapter{nullptr};
  std::optional<slint::ComponentHandle<MainEditorWindow>> m_window_component;
  ViewportLogicalRect m_cached_viewport_logical_rect{};
  BrowserLogicalRect m_cached_browser_logical_rect{};
  int m_slint_dispatch_depth{0};
  const MaterialAsset* m_blinn_phong_material_source{nullptr};
  eastl::string m_drop_highlight_path;
  eastl::vector<eastl::string> m_pending_os_drop_files;
  bool m_os_drop_targets_browser{false};
  bool m_tree_folder_handled_by_slint{false};
  bool m_left_mouse_down_prev{false};
};
}  // namespace Blunder
