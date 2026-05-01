#include "runtime/function/slint/slint_system.h"

#include <slint.h>

#include <cstdint>
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

SlintSystem::SlintPlatform* g_slint_platform_instance = nullptr;
}

SlintSystem::SlintWindowAdapter::SlintWindowAdapter(WindowSystem* window_system)
    : m_window_system(window_system) {}

SlintSystem::~SlintSystem() { shutdown(); }

slint::PhysicalSize SlintSystem::SlintWindowAdapter::size() {
  eastl::array<int, 2> drawable_size = m_window_system->getDrawableSize();
  return slint::PhysicalSize(
      {static_cast<uint32_t>(eastl::max(drawable_size[0], 1)),
       static_cast<uint32_t>(eastl::max(drawable_size[1], 1))});
}

void SlintSystem::SlintWindowAdapter::set_visible(bool visible) {
  m_visible = visible;
  if (visible) {
    window().dispatch_scale_factor_change_event(1.0f);
    window().dispatch_resize_event(
        slint::LogicalSize({static_cast<float>(size().width),
                            static_cast<float>(size().height)}));
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
  if (!m_visible) {
    return;
  }

  if (!m_needs_redraw && !window().has_active_animations()) {
    return;
  }

  const slint::PhysicalSize physical_size = size();
  const size_t pixel_count =
      static_cast<size_t>(physical_size.width) * physical_size.height;
  if (m_buffer.size() != pixel_count) {
    m_buffer.resize(pixel_count);
  }

  m_renderer.render(std::span<slint::Rgb8Pixel>(m_buffer.data(), m_buffer.size()),
                    physical_size.width);
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

  if (!g_slint_platform_instance) {
    auto platform = std::make_unique<SlintPlatform>(m_window_system);
    g_slint_platform_instance = platform.get();
    slint::platform::set_platform(std::move(platform));
  }

  auto component = MainEditorWindow::create();
  component->show();
  m_window_component = component;

  m_window_adapter = g_slint_platform_instance->getWindowAdapter();
  ASSERT(m_window_adapter);

  m_window_system->setNativeEventCallback(
      [this](const SDL_Event& event) { processEvent(event); });
}

void SlintSystem::shutdown() {
  if (m_window_component) {
    m_window_component->hide();
    m_window_component.reset();
  }

  if (m_window_system) {
    m_window_system->setNativeEventCallback({});
  }

  m_window_adapter = nullptr;
  m_window_system = nullptr;
}

void SlintSystem::update() {
  slint::platform::update_timers_and_animations();
  if (m_window_adapter) {
    m_window_adapter->renderIfNeeded();
  }
}

void SlintSystem::processEvent(const SDL_Event& event) {
  if (!m_window_adapter || !m_window_component) {
    return;
  }

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
}
}  // namespace Blunder
