#include "runtime/function/slint/slint_system.h"

#include <slint.h>

#include <cstdint>
#include <exception>
#include <string>
#include <string_view>
#include <utility>

#include "runtime/core/base/macro.h"

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

class ScopedDispatchGuard final {
 public:
  explicit ScopedDispatchGuard(bool& flag) : m_flag(flag) { m_flag = true; }
  ~ScopedDispatchGuard() { m_flag = false; }

 private:
  bool& m_flag;
};
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
  if (!hwnd) {
    LOG_ERROR("[SlintSystem] native Win32 HWND is null");
    return;
  }
  slint::platform::NativeWindowHandle handle =
      slint::platform::NativeWindowHandle::from_win32(hwnd, hinstance);
  m_last_size = size();
  m_renderer = std::make_unique<slint::platform::SkiaRenderer>(
      handle, m_last_size);
#else
#  error "Slint SkiaRenderer integration is currently implemented for Win32 only"
#endif
}

void SlintSystem::SlintWindowAdapter::set_visible(bool visible) {
  m_visible = visible;
  if (visible) {
    ensureRenderer();
    window().dispatch_scale_factor_change_event(1.0f);
    const slint::PhysicalSize phys = size();
    window().dispatch_resize_event(slint::LogicalSize(
        {static_cast<float>(phys.width), static_cast<float>(phys.height)}));
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

void SlintSystem::SlintWindowAdapter::renderIfNeeded() {
  if (!m_visible || !m_renderer) {
    return;
  }

  const slint::PhysicalSize physical_size = size();
  if (physical_size.width != m_last_size.width ||
      physical_size.height != m_last_size.height) {
    window().dispatch_resize_event(
        slint::LogicalSize({static_cast<float>(physical_size.width),
                            static_cast<float>(physical_size.height)}));
    m_last_size = physical_size;
  }

  // Always re-render: the engine pushes a new viewport image every frame, so
  // even if Slint's UI itself isn't dirty, the central Image control needs
  // to repaint with the latest 3D texture.
  m_renderer->render();
  m_needs_redraw = window().has_active_animations();
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
  using namespace slint::platform::key_codes;

  switch (keycode) {
    case SDLK_RETURN:
      return makeSpecialKeyString(Return);
    case SDLK_ESCAPE:
      return makeSpecialKeyString(Escape);
    case SDLK_BACKSPACE:
      return makeSpecialKeyString(Backspace);
    case SDLK_TAB:
      return makeSpecialKeyString(Tab);
    case SDLK_SPACE:
      return makeSpecialKeyString(Space);
    case SDLK_DELETE:
      return makeSpecialKeyString(Delete);
    case SDLK_UP:
      return makeSpecialKeyString(UpArrow);
    case SDLK_DOWN:
      return makeSpecialKeyString(DownArrow);
    case SDLK_LEFT:
      return makeSpecialKeyString(LeftArrow);
    case SDLK_RIGHT:
      return makeSpecialKeyString(RightArrow);
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

  #if defined(SLINT_FEATURE_RENDERER_SKIA_VULKAN)
    LOG_INFO(
      "[SlintSystem::initialize] Slint was compiled with "
      "RENDERER_SKIA_VULKAN enabled");
  #else
    LOG_WARN(
      "[SlintSystem::initialize] Slint was compiled without "
      "RENDERER_SKIA_VULKAN");
  #endif

  #if defined(SLINT_FEATURE_RENDERER_SKIA_OPENGL)
    LOG_WARN(
      "[SlintSystem::initialize] Slint was compiled with "
      "RENDERER_SKIA_OPENGL enabled; runtime may still choose OpenGL if "
      "the backend allows it");
  #endif

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
    component->show();
    m_window_component = component;

    m_window_adapter = g_slint_platform_instance->getWindowAdapter();
    ASSERT(m_window_adapter);

    m_window_system->setNativeEventCallback(
        [this](const SDL_Event& event) { processEvent(event); });
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
  if (m_in_slint_dispatch) {
    return;
  }
  static bool s_logged_first_viewport_upload = false;
  static uint32_t s_last_upload_w = 0;
  static uint32_t s_last_upload_h = 0;
  try {
    ScopedDispatchGuard guard(m_in_slint_dispatch);
    slint::SharedPixelBuffer<slint::Rgba8Pixel> buffer(
        width, height, reinterpret_cast<const slint::Rgba8Pixel*>(pixels_rgba));
    slint::Image image(buffer);
    m_window_component->operator->()->set_viewport_image(image);
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

eastl::array<uint32_t, 2> SlintSystem::getViewportLogicalSize() const {
  if (!m_window_component) {
    return {0u, 0u};
  }
  const auto& component = *m_window_component;
  const float w = component->get_viewport_width();
  const float h = component->get_viewport_height();
  const uint32_t wu = w > 0.0f ? static_cast<uint32_t>(w) : 0u;
  const uint32_t hu = h > 0.0f ? static_cast<uint32_t>(h) : 0u;
  return {wu, hu};
}

void SlintSystem::update() {
  if (m_in_slint_dispatch) {
    return;
  }
  try {
    ScopedDispatchGuard guard(m_in_slint_dispatch);
    slint::platform::update_timers_and_animations();
    if (m_window_adapter) {
      m_window_adapter->renderIfNeeded();
    }
  } catch (const std::exception& e) {
    LOG_ERROR("[SlintSystem::update] {}", e.what());
    if (m_window_system) {
      m_window_system->requestClose();
    }
  } catch (...) {
    LOG_ERROR("[SlintSystem::update] unknown exception");
    if (m_window_system) {
      m_window_system->requestClose();
    }
  }
}

void SlintSystem::processEvent(const SDL_Event& event) {
  if (!m_window_adapter || !m_window_component) {
    return;
  }
  if (m_in_slint_dispatch) {
    return;
  }

  try {
    ScopedDispatchGuard guard(m_in_slint_dispatch);
    slint::Window& window = m_window_adapter->window();
    const SDL_WindowID window_id = m_window_system->getWindowId();

    switch (event.type) {
      case SDL_EVENT_WINDOW_RESIZED:
      case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        if (event.window.windowID == window_id) {
          window.dispatch_resize_event(slint::LogicalSize(
              {static_cast<float>(eastl::max(event.window.data1, 1)),
               static_cast<float>(eastl::max(event.window.data2, 1))}));
          m_window_adapter->request_redraw();
        }
        break;
      case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
        if (event.window.windowID == window_id) {
          window.dispatch_scale_factor_change_event(1.0f);
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
          window.dispatch_pointer_move_event(
              slint::LogicalPosition({event.motion.x, event.motion.y}));
        }
        break;
      case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (event.button.windowID == window_id) {
          window.dispatch_pointer_press_event(
              slint::LogicalPosition({event.button.x, event.button.y}),
              mapPointerButton(event.button.button));
        }
        break;
      case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event.button.windowID == window_id) {
          window.dispatch_pointer_release_event(
              slint::LogicalPosition({event.button.x, event.button.y}),
              mapPointerButton(event.button.button));
        }
        break;
      case SDL_EVENT_MOUSE_WHEEL:
        if (event.wheel.windowID == window_id) {
          const float wheel_x = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED
                                    ? -event.wheel.x
                                    : event.wheel.x;
          const float wheel_y = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED
                                    ? -event.wheel.y
                                    : event.wheel.y;
          window.dispatch_pointer_scroll_event(
              slint::LogicalPosition({event.wheel.mouse_x, event.wheel.mouse_y}),
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
