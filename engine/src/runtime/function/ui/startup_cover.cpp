#include "runtime/function/ui/startup_cover.h"

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/platform/window/window_system.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace Blunder {
namespace {

WindowSystem* g_window{nullptr};
eastl::string g_wordmark;
StartupCoverPhase g_phase{StartupCoverPhase::preparingEditor};
bool g_active{false};

#ifdef _WIN32
WNDPROC g_prev_wndproc{nullptr};
HWND g_hwnd{nullptr};

constexpr COLORREF k_window_fill = RGB(0x2A, 0x2D, 0x31);
constexpr COLORREF k_wordmark_color = RGB(0xE8, 0xEB, 0xEF);
constexpr COLORREF k_stage_color = RGB(0xB3, 0xBB, 0xC4);

void drawUtf8Centered(HDC hdc, const char* utf8, RECT rect, int font_px,
                      COLORREF color) {
  if (!utf8 || utf8[0] == '\0') {
    return;
  }
  const int wide_count = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
  if (wide_count <= 1) {
    return;
  }
  eastl::vector<wchar_t> wide(static_cast<size_t>(wide_count));
  MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide.data(), wide_count);

  HFONT font = CreateFontW(-font_px, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                           CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                           L"Segoe UI");
  HFONT old_font = font ? static_cast<HFONT>(SelectObject(hdc, font)) : nullptr;
  SetBkMode(hdc, TRANSPARENT);
  SetTextColor(hdc, color);
  DrawTextW(hdc, wide.data(), -1, &rect,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
  if (font) {
    SelectObject(hdc, old_font);
    DeleteObject(font);
  }
}

void paintHwnd(HWND hwnd) {
  // WM_PAINT must always BeginPaint/EndPaint. A 0x0 client (minimized)
  // that returns 0 without validating the region livelocks PeekMessage.
  PAINTSTRUCT paint{};
  HDC hdc = BeginPaint(hwnd, &paint);
  if (!hdc) {
    return;
  }
  if (!g_active) {
    EndPaint(hwnd, &paint);
    return;
  }
  RECT client{};
  if (!GetClientRect(hwnd, &client) || client.right <= 0 || client.bottom <= 0) {
    EndPaint(hwnd, &paint);
    return;
  }

  HBRUSH fill = CreateSolidBrush(k_window_fill);
  FillRect(hdc, &client, fill);
  DeleteObject(fill);

  const int dpi = static_cast<int>(GetDpiForWindow(hwnd));
  const int word_px = MulDiv(28, dpi > 0 ? dpi : 96, 96);
  const int stage_px = MulDiv(14, dpi > 0 ? dpi : 96, 96);
  const int height = client.bottom - client.top;
  RECT word_rect = client;
  word_rect.top = client.top + height / 2 - word_px;
  word_rect.bottom = word_rect.top + word_px + MulDiv(8, dpi > 0 ? dpi : 96, 96);
  RECT stage_rect = client;
  stage_rect.top = word_rect.bottom + MulDiv(12, dpi > 0 ? dpi : 96, 96);
  stage_rect.bottom = stage_rect.top + stage_px + MulDiv(6, dpi > 0 ? dpi : 96, 96);

  drawUtf8Centered(hdc, g_wordmark.c_str(), word_rect, word_px, k_wordmark_color);
  drawUtf8Centered(hdc, startupCoverStageName(g_phase), stage_rect, stage_px,
                   k_stage_color);

  EndPaint(hwnd, &paint);
}

LRESULT CALLBACK coverWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  if (g_active && hwnd == g_hwnd) {
    if (msg == WM_ERASEBKGND) {
      return 1;
    }
    if (msg == WM_SIZE) {
      InvalidateRect(hwnd, nullptr, FALSE);
    }
    if (msg == WM_PAINT) {
      paintHwnd(hwnd);
      return 0;
    }
  }
  if (g_prev_wndproc) {
    return CallWindowProcW(g_prev_wndproc, hwnd, msg, wparam, lparam);
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

void installSubclass(HWND hwnd) {
  if (!hwnd || g_prev_wndproc) {
    return;
  }
  g_hwnd = hwnd;
  g_prev_wndproc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
      hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&coverWndProc)));
}

void removeSubclass() {
  if (g_hwnd && g_prev_wndproc) {
    SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC,
                      reinterpret_cast<LONG_PTR>(g_prev_wndproc));
  }
  g_prev_wndproc = nullptr;
  g_hwnd = nullptr;
}

void paintActive() {
  if (g_hwnd) {
    InvalidateRect(g_hwnd, nullptr, FALSE);
    UpdateWindow(g_hwnd);
  }
}
#else
void paintActive() {}
#endif

}  // namespace

bool startupCoverShouldMount(EngineHostMode mode, bool headless) {
  return hostMountsEditorShell(mode, headless);
}

const char* startupCoverStageName(StartupCoverPhase phase) {
  switch (phase) {
    case StartupCoverPhase::cookingAssets:
      return "Cooking assets";
    case StartupCoverPhase::preparingEditor:
      return "Preparing editor";
    case StartupCoverPhase::startingEditor:
      return "Starting editor";
    default:
      return "Preparing editor";
  }
}

void startupCoverBegin(WindowSystem* window, const eastl::string& wordmark) {
  startupCoverDismiss();
  if (!window) {
    return;
  }
  g_window = window;
  g_wordmark = wordmark;
  g_phase = StartupCoverPhase::preparingEditor;
  g_active = true;
  window->setTitle(wordmark.c_str());
#ifdef _WIN32
  HWND hwnd = static_cast<HWND>(window->getNativeWin32Hwnd());
  installSubclass(hwnd);
#endif
  paintActive();
}

void startupCoverSetPhase(StartupCoverPhase phase) {
  if (!g_active) {
    return;
  }
  if (g_phase == phase) {
    return;
  }
  g_phase = phase;
  paintActive();
}

bool startupCoverPump() {
  if (!g_active || !g_window) {
    return true;
  }
  g_window->pumpEvents();
  if (g_window->shouldClose()) {
    return false;
  }
  return true;
}

void startupCoverDismiss() {
  if (!g_active && !g_window) {
#ifdef _WIN32
    removeSubclass();
#endif
    return;
  }
  g_active = false;
#ifdef _WIN32
  removeSubclass();
#endif
  g_window = nullptr;
  g_wordmark.clear();
}

bool startupCoverIsActive() { return g_active; }

}  // namespace Blunder
