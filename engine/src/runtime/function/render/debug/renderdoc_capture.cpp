#include "runtime/function/render/debug/renderdoc_capture.h"

#include "runtime/core/base/macro.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "renderdoc_app.h"

namespace Blunder {

namespace {

using RenderDocApi = RENDERDOC_API_1_6_0;

RenderDocApi* castApi(void* api) { return static_cast<RenderDocApi*>(api); }

}  // namespace

void RenderDocCapture::initialize() {
  m_api = nullptr;
  m_capture_pending = false;
  m_capture_active = false;

  pRENDERDOC_GetAPI get_api = nullptr;

#if defined(_WIN32)
  // RenderDoc injects renderdoc.dll into the target process before
  // CreateProcess returns. If the module is not loaded, we are running
  // outside of RenderDoc and there is nothing to do.
  HMODULE module_handle = GetModuleHandleA("renderdoc.dll");
  if (module_handle == nullptr) {
    return;
  }
  get_api = reinterpret_cast<pRENDERDOC_GetAPI>(
      GetProcAddress(module_handle, "RENDERDOC_GetAPI"));
#else
  // RTLD_NOLOAD: only succeed if librenderdoc.so was already loaded by the
  // RenderDoc launcher, mirroring the Windows-side detection.
  void* module_handle = dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD);
  if (module_handle == nullptr) {
    return;
  }
  get_api = reinterpret_cast<pRENDERDOC_GetAPI>(
      dlsym(module_handle, "RENDERDOC_GetAPI"));
#endif

  if (get_api == nullptr) {
    LOG_WARN(
        "[RenderDocCapture] RenderDoc module is loaded but RENDERDOC_GetAPI "
        "is missing; in-app capture disabled");
    return;
  }

  RenderDocApi* api = nullptr;
  if (get_api(eRENDERDOC_API_Version_1_6_0,
              reinterpret_cast<void**>(&api)) != 1 ||
      api == nullptr) {
    LOG_WARN(
        "[RenderDocCapture] RENDERDOC_GetAPI(1.6.0) failed; in-app capture "
        "disabled");
    return;
  }

  // Best-effort cosmetics: disable RenderDoc's overlay so it does not draw on
  // top of Slint's compositor (the engine never presents through a Vulkan
  // swapchain, so the overlay would be invisible anyway, but this keeps the
  // RenderDoc UI tidy).
  api->MaskOverlayBits(eRENDERDOC_Overlay_None, eRENDERDOC_Overlay_None);

  int major = 0;
  int minor = 0;
  int patch = 0;
  api->GetAPIVersion(&major, &minor, &patch);
  LOG_INFO(
      "[RenderDocCapture] attached to RenderDoc {}.{}.{} (press F11 to "
      "capture one frame)",
      major, minor, patch);

  m_api = api;
}

void RenderDocCapture::shutdown() {
  // The RenderDoc API has no Release entry; the module's lifetime is owned
  // by the RenderDoc launcher. Just drop our handle.
  m_api = nullptr;
  m_capture_pending = false;
  m_capture_active = false;
}

void RenderDocCapture::triggerCapture() {
  if (m_api == nullptr) {
    return;
  }
  if (m_capture_pending || m_capture_active) {
    return;
  }
  m_capture_pending = true;
  LOG_INFO("[RenderDocCapture] capture queued for next frame");
}

void RenderDocCapture::beginFrame(VkInstance instance) {
  if (m_api == nullptr || !m_capture_pending) {
    return;
  }
  if (instance == VK_NULL_HANDLE) {
    return;
  }

  RENDERDOC_DevicePointer device_pointer =
      RENDERDOC_DEVICEPOINTER_FROM_VKINSTANCE(instance);
  castApi(m_api)->StartFrameCapture(device_pointer, nullptr);
  m_capture_pending = false;
  m_capture_active = true;
}

void RenderDocCapture::endFrame(VkInstance instance) {
  if (m_api == nullptr || !m_capture_active) {
    return;
  }
  if (instance == VK_NULL_HANDLE) {
    return;
  }

  RENDERDOC_DevicePointer device_pointer =
      RENDERDOC_DEVICEPOINTER_FROM_VKINSTANCE(instance);
  const uint32_t result =
      castApi(m_api)->EndFrameCapture(device_pointer, nullptr);
  m_capture_active = false;
  if (result == 1) {
    LOG_INFO("[RenderDocCapture] frame captured");
  } else {
    LOG_WARN(
        "[RenderDocCapture] EndFrameCapture returned failure; capture "
        "discarded");
  }
}

}  // namespace Blunder
