#pragma once

#include <memory>
#include <optional>

#include <slint-platform.h>

#include "EASTL/vector.h"

#include "editor_window.h"

#include "runtime/platform/window/window_system.h"

namespace Blunder {

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

  /// Returns the logical pixel size of the central 3D viewport rectangle in
  /// the Slint UI. {0, 0} until the window has performed its first layout.
  eastl::array<uint32_t, 2> getViewportLogicalSize() const;

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
  bool m_in_slint_dispatch{false};
};
}  // namespace Blunder
