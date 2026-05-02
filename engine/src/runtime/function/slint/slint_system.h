#pragma once

#include <memory>
#include <optional>

#include <slint-platform.h>

#include "EASTL/vector.h"

#include "editor_window.h"

#include "runtime/platform/window/window_system.h"

namespace Blunder {

struct UiCpuTextureView;

struct SlintSystemInitInfo {
  WindowSystem* window_system{nullptr};
};

class SlintSystem final {
 public:
  SlintSystem() = default;
  ~SlintSystem();

  void initialize(const SlintSystemInitInfo& init_info);
  void shutdown();
  void update();
  void processEvent(const SDL_Event& event);

  /// Fills @p out with the last Slint CPU framebuffer (RGBA8). Valid until the next update().
  bool tryFillUiOverlay(UiCpuTextureView& out) const;

  class SlintWindowAdapter final : public slint::platform::WindowAdapter {
   public:
    explicit SlintWindowAdapter(WindowSystem* window_system);

    slint::platform::AbstractRenderer& renderer() override { return m_renderer; }
    slint::PhysicalSize size() override;
    void set_visible(bool visible) override;
    void request_redraw() override;
    void update_window_properties(const WindowProperties& properties) override;

    void renderIfNeeded();
    bool needsRedraw() const { return m_needs_redraw; }
    bool fillUiOverlay(UiCpuTextureView& out) const;

   private:
    WindowSystem* m_window_system{nullptr};
    slint::platform::SoftwareRenderer m_renderer{
        slint::platform::SoftwareRenderer::RepaintBufferType::ReusedBuffer};
    bool m_visible{false};
    bool m_needs_redraw{true};
    bool m_has_ui_bitmap{false};
    eastl::vector<slint::Rgb8Pixel> m_buffer;
    eastl::vector<uint8_t> m_rgba_buffer;
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
};
}  // namespace Blunder
