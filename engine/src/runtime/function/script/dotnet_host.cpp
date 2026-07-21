#include "runtime/function/script/dotnet_host.h"

#include <cstdio>
#include <string>
#include <vector>

#if defined(BLUNDER_HAS_NETHOST) && BLUNDER_HAS_NETHOST

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

#include <nethost.h>
#include <coreclr_delegates.h>
#include <hostfxr.h>

namespace Blunder {
namespace {

eastl::string intToEastl(int value) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%d", value);
  return eastl::string(buf);
}

#ifdef _WIN32
using LibHandle = HMODULE;

LibHandle loadLibrary(const char_t* path) { return ::LoadLibraryW(path); }

void* getExport(LibHandle lib, const char* name) {
  return reinterpret_cast<void*>(::GetProcAddress(lib, name));
}

void freeLibrary(LibHandle lib) {
  if (lib != nullptr) {
    ::FreeLibrary(lib);
  }
}

std::wstring toHostPath(const std::filesystem::path& path) {
  return path.wstring();
}

std::string narrow(const std::wstring& wide) {
  if (wide.empty()) {
    return {};
  }
  const int size = ::WideCharToMultiByte(CP_UTF8, 0, wide.c_str(),
                                         static_cast<int>(wide.size()), nullptr,
                                         0, nullptr, nullptr);
  std::string out(static_cast<size_t>(size), '\0');
  ::WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()),
                        out.data(), size, nullptr, nullptr);
  return out;
}
#else
using LibHandle = void*;

LibHandle loadLibrary(const char_t* path) {
  return ::dlopen(path, RTLD_LAZY | RTLD_LOCAL);
}

void* getExport(LibHandle lib, const char* name) { return ::dlsym(lib, name); }

void freeLibrary(LibHandle lib) {
  if (lib != nullptr) {
    ::dlclose(lib);
  }
}

std::string toHostPath(const std::filesystem::path& path) {
  return path.string();
}
#endif

// Intentionally avoid getLogSystem()/RuntimeGlobalContext — that pulls Slint
// into unit-test link graphs. Callers (editor) can also inspect out_error.
void logHostError(const eastl::string& message) {
  std::fprintf(stderr, "DotNetHost: %s\n", message.c_str());
}

std::string pathToUtf8(const std::filesystem::path& path) {
#ifdef _WIN32
  return narrow(path.wstring());
#else
  return path.string();
#endif
}

}  // namespace

bool DotNetHost::start(const std::filesystem::path& script_host_dll,
                       const std::filesystem::path& runtimeconfig,
                       const BlunderNativeAbi& abi, eastl::string& out_error) {
  out_error.clear();
  if (m_running) {
    out_error = "DotNetHost already running";
    return false;
  }

  if (!std::filesystem::exists(script_host_dll)) {
    out_error = "ScriptHost DLL not found: ";
    out_error += pathToUtf8(script_host_dll).c_str();
    logHostError(out_error);
    return false;
  }
  if (!std::filesystem::exists(runtimeconfig)) {
    out_error = "runtimeconfig not found: ";
    out_error += pathToUtf8(runtimeconfig).c_str();
    logHostError(out_error);
    return false;
  }

  get_hostfxr_parameters params{};
  params.size = sizeof(get_hostfxr_parameters);
  const auto assembly_path = toHostPath(script_host_dll);
  params.assembly_path = assembly_path.c_str();

  size_t hostfxr_path_size = 0;
  get_hostfxr_path(nullptr, &hostfxr_path_size, &params);
  if (hostfxr_path_size == 0) {
    out_error =
        "get_hostfxr_path failed (is .NET 10 runtime installed? "
        "Set DOTNET_ROOT if needed).";
    logHostError(out_error);
    return false;
  }

  std::vector<char_t> hostfxr_path(hostfxr_path_size);
  const int rc_path =
      get_hostfxr_path(hostfxr_path.data(), &hostfxr_path_size, &params);
  if (rc_path != 0) {
    out_error =
        "get_hostfxr_path failed (is .NET 10 runtime installed? "
        "Set DOTNET_ROOT if needed). code=";
    out_error += intToEastl(rc_path);
    logHostError(out_error);
    return false;
  }

  LibHandle lib = loadLibrary(hostfxr_path.data());
  if (lib == nullptr) {
    out_error = "Failed to load hostfxr library";
    logHostError(out_error);
    return false;
  }
  m_hostfxr_lib = lib;

  auto init_fn = reinterpret_cast<hostfxr_initialize_for_runtime_config_fn>(
      getExport(lib, "hostfxr_initialize_for_runtime_config"));
  auto get_delegate_fn = reinterpret_cast<hostfxr_get_runtime_delegate_fn>(
      getExport(lib, "hostfxr_get_runtime_delegate"));
  auto close_fn = reinterpret_cast<hostfxr_close_fn>(
      getExport(lib, "hostfxr_close"));
  if (init_fn == nullptr || get_delegate_fn == nullptr || close_fn == nullptr) {
    out_error = "hostfxr exports missing";
    logHostError(out_error);
    closeHandles();
    return false;
  }

  hostfxr_handle context = nullptr;
  const auto config_path = toHostPath(runtimeconfig);
  const int rc_init = init_fn(config_path.c_str(), nullptr, &context);
  // 0 = Success; 1/2 = Success_HostAlreadyInitialized /
  // Success_DifferentRuntimeProperties — still usable when context is set.
  if ((rc_init != 0 && rc_init != 1 && rc_init != 2) || context == nullptr) {
    out_error = "hostfxr_initialize_for_runtime_config failed code=";
    out_error += intToEastl(rc_init);
    logHostError(out_error);
    closeHandles();
    return false;
  }
  m_host_context = context;

  void* load_fn = nullptr;
  const int rc_del = get_delegate_fn(
      context, hdt_load_assembly_and_get_function_pointer, &load_fn);
  if (rc_del != 0 || load_fn == nullptr) {
    out_error = "hostfxr_get_runtime_delegate failed code=";
    out_error += intToEastl(rc_del);
    logHostError(out_error);
    close_fn(context);
    m_host_context = nullptr;
    closeHandles();
    return false;
  }
  m_load_assembly_and_get_fn = load_fn;

  if (!resolveExports(script_host_dll, out_error)) {
    logHostError(out_error);
    close_fn(context);
    m_host_context = nullptr;
    closeHandles();
    return false;
  }

  // Register C-ABI table before any managed Native call (hooks / game load).
  if (!registerNativeAbi(abi, out_error)) {
    logHostError(out_error);
    close_fn(context);
    m_host_context = nullptr;
    closeHandles();
    return false;
  }

  if (m_register_hooks != nullptr) {
    m_register_hooks();
  }

  m_running = true;
  return true;
}

bool DotNetHost::resolveExports(const std::filesystem::path& script_host_dll,
                                eastl::string& out_error) {
  auto load_and_get =
      reinterpret_cast<load_assembly_and_get_function_pointer_fn>(
          m_load_assembly_and_get_fn);
  if (load_and_get == nullptr) {
    out_error = "load_assembly_and_get_function_pointer is null";
    return false;
  }

  const auto dll_path = toHostPath(script_host_dll);
#ifdef _WIN32
  const char_t* type_name = L"Blunder.ScriptHost.HostExports, Blunder.ScriptHost";
  const char_t* load_name = L"LoadGameAssembly";
  const char_t* attach_name = L"AttachBehaviour";
  const char_t* apply_props_name = L"ApplyBehaviourProperties";
  const char_t* register_abi_name = L"RegisterNativeAbi";
  const char_t* register_name = L"RegisterLifecycleHooks";
  const char_t* shutdown_name = L"ShutdownCleanup";
#else
  const char_t* type_name = "Blunder.ScriptHost.HostExports, Blunder.ScriptHost";
  const char_t* load_name = "LoadGameAssembly";
  const char_t* attach_name = "AttachBehaviour";
  const char_t* apply_props_name = "ApplyBehaviourProperties";
  const char_t* register_abi_name = "RegisterNativeAbi";
  const char_t* register_name = "RegisterLifecycleHooks";
  const char_t* shutdown_name = "ShutdownCleanup";
#endif

  void* fn = nullptr;
  int rc = load_and_get(dll_path.c_str(), type_name, load_name,
                        UNMANAGEDCALLERSONLY_METHOD, nullptr, &fn);
  if (rc != 0 || fn == nullptr) {
    out_error = "Failed to resolve HostExports.LoadGameAssembly code=";
    out_error += intToEastl(rc);
    return false;
  }
  m_load_game = reinterpret_cast<LoadGameAssemblyFn>(fn);

  fn = nullptr;
  rc = load_and_get(dll_path.c_str(), type_name, attach_name,
                    UNMANAGEDCALLERSONLY_METHOD, nullptr, &fn);
  if (rc != 0 || fn == nullptr) {
    out_error = "Failed to resolve HostExports.AttachBehaviour code=";
    out_error += intToEastl(rc);
    return false;
  }
  m_attach = reinterpret_cast<AttachBehaviourFn>(fn);

  fn = nullptr;
  rc = load_and_get(dll_path.c_str(), type_name, apply_props_name,
                    UNMANAGEDCALLERSONLY_METHOD, nullptr, &fn);
  if (rc != 0 || fn == nullptr) {
    out_error = "Failed to resolve HostExports.ApplyBehaviourProperties code=";
    out_error += intToEastl(rc);
    return false;
  }
  m_apply_props = reinterpret_cast<ApplyBehaviourPropertiesFn>(fn);

  fn = nullptr;
  rc = load_and_get(dll_path.c_str(), type_name, register_abi_name,
                    UNMANAGEDCALLERSONLY_METHOD, nullptr, &fn);
  if (rc != 0 || fn == nullptr) {
    out_error = "Failed to resolve HostExports.RegisterNativeAbi code=";
    out_error += intToEastl(rc);
    return false;
  }
  m_register_abi = reinterpret_cast<RegisterNativeAbiFn>(fn);

  fn = nullptr;
  rc = load_and_get(dll_path.c_str(), type_name, register_name,
                    UNMANAGEDCALLERSONLY_METHOD, nullptr, &fn);
  if (rc != 0 || fn == nullptr) {
    out_error = "Failed to resolve HostExports.RegisterLifecycleHooks code=";
    out_error += intToEastl(rc);
    return false;
  }
  m_register_hooks = reinterpret_cast<RegisterLifecycleHooksFn>(fn);

  fn = nullptr;
  rc = load_and_get(dll_path.c_str(), type_name, shutdown_name,
                    UNMANAGEDCALLERSONLY_METHOD, nullptr, &fn);
  if (rc != 0 || fn == nullptr) {
    out_error = "Failed to resolve HostExports.ShutdownCleanup code=";
    out_error += intToEastl(rc);
    return false;
  }
  m_shutdown_cleanup = reinterpret_cast<ShutdownCleanupFn>(fn);
  return true;
}

bool DotNetHost::registerNativeAbi(const BlunderNativeAbi& abi,
                                   eastl::string& out_error) {
  if (m_register_abi == nullptr) {
    out_error = "RegisterNativeAbi export is null";
    return false;
  }
  const int rc = m_register_abi(&abi);
  if (rc != 0) {
    out_error = "RegisterNativeAbi failed code=";
    out_error += intToEastl(rc);
    return false;
  }
  return true;
}

bool DotNetHost::loadGameAssembly(const std::filesystem::path& game_dll,
                                  eastl::string& out_error) {
  out_error.clear();
  if (!m_running || m_load_game == nullptr) {
    out_error = "DotNetHost is not running";
    return false;
  }
  if (!std::filesystem::exists(game_dll)) {
    out_error = "Game assembly not found: ";
    out_error += pathToUtf8(game_dll).c_str();
    return false;
  }

  const std::string utf8 = pathToUtf8(game_dll);
  const int rc = m_load_game(utf8.c_str());
  if (rc != 0) {
    out_error = "LoadGameAssembly failed code=";
    out_error += intToEastl(rc);
    return false;
  }
  m_game_assembly_loaded = true;
  return true;
}

bool DotNetHost::attachBehaviour(ObjectId object, const char* clr_type_name,
                                 BehaviourId* out_id,
                                 eastl::string& out_error) {
  out_error.clear();
  if (!m_running || m_attach == nullptr) {
    out_error = "DotNetHost is not running";
    return false;
  }
  if (clr_type_name == nullptr || out_id == nullptr) {
    out_error = "attachBehaviour requires type name and out_id";
    return false;
  }

  // Non-zero *out_id = mount onto an existing restored slot (in/out).
  uint64_t behaviour_id = isValidBehaviourId(*out_id)
                              ? static_cast<uint64_t>(*out_id)
                              : 0;
  const int rc =
      m_attach(static_cast<uint64_t>(object), clr_type_name, &behaviour_id);
  if (rc != 0 || behaviour_id == 0) {
    *out_id = k_invalid_behaviour_id;
    out_error = "AttachBehaviour failed for type ";
    out_error += clr_type_name;
    return false;
  }
  *out_id = static_cast<BehaviourId>(behaviour_id);
  return true;
}

bool DotNetHost::applyBehaviourProperties(ObjectId object, BehaviourId behaviour_id,
                                          const char* utf8_json,
                                          eastl::string& out_error) {
  out_error.clear();
  if (!m_running || m_apply_props == nullptr) {
    out_error = "DotNetHost is not running";
    return false;
  }
  if (!isValid(object) || !isValidBehaviourId(behaviour_id)) {
    out_error = "applyBehaviourProperties requires valid object and behaviour id";
    return false;
  }
  if (utf8_json == nullptr) {
    utf8_json = "{}";
  }
  const int rc = m_apply_props(static_cast<uint64_t>(object),
                               static_cast<uint64_t>(behaviour_id), utf8_json);
  if (rc != 0) {
    out_error = "ApplyBehaviourProperties failed";
    return false;
  }
  return true;
}

bool DotNetHost::resolveProbeTickCount(const std::filesystem::path& script_host_dll,
                                       eastl::string& out_error) {
  out_error.clear();
  m_get_probe_tick = nullptr;
  m_get_probe_sibling = nullptr;
  m_get_probe_property_ok = nullptr;
  if (!m_running || m_load_assembly_and_get_fn == nullptr) {
    out_error = "DotNetHost is not running";
    return false;
  }
  if (!std::filesystem::exists(script_host_dll)) {
    out_error = "ScriptHost DLL not found: ";
    out_error += pathToUtf8(script_host_dll).c_str();
    return false;
  }

  auto load_and_get =
      reinterpret_cast<load_assembly_and_get_function_pointer_fn>(
          m_load_assembly_and_get_fn);
  const auto dll_path = toHostPath(script_host_dll);
#ifdef _WIN32
  const char_t* type_name =
      L"Blunder.ScriptHost.HostExports, Blunder.ScriptHost";
  const char_t* tick_name = L"GetProbeTickCount";
  const char_t* sibling_name = L"GetProbeSiblingFound";
  const char_t* property_ok_name = L"GetProbePropertyOk";
#else
  const char_t* type_name = "Blunder.ScriptHost.HostExports, Blunder.ScriptHost";
  const char_t* tick_name = "GetProbeTickCount";
  const char_t* sibling_name = "GetProbeSiblingFound";
  const char_t* property_ok_name = "GetProbePropertyOk";
#endif

  void* fn = nullptr;
  int rc = load_and_get(dll_path.c_str(), type_name, tick_name,
                        UNMANAGEDCALLERSONLY_METHOD, nullptr, &fn);
  if (rc != 0 || fn == nullptr) {
    out_error = "Failed to resolve HostExports.GetProbeTickCount code=";
    out_error += intToEastl(rc);
    return false;
  }
  m_get_probe_tick = reinterpret_cast<GetProbeTickCountFn>(fn);

  fn = nullptr;
  rc = load_and_get(dll_path.c_str(), type_name, sibling_name,
                    UNMANAGEDCALLERSONLY_METHOD, nullptr, &fn);
  if (rc != 0 || fn == nullptr) {
    out_error = "Failed to resolve HostExports.GetProbeSiblingFound code=";
    out_error += intToEastl(rc);
    return false;
  }
  m_get_probe_sibling = reinterpret_cast<GetProbeTickCountFn>(fn);

  fn = nullptr;
  rc = load_and_get(dll_path.c_str(), type_name, property_ok_name,
                    UNMANAGEDCALLERSONLY_METHOD, nullptr, &fn);
  if (rc != 0 || fn == nullptr) {
    out_error = "Failed to resolve HostExports.GetProbePropertyOk code=";
    out_error += intToEastl(rc);
    return false;
  }
  m_get_probe_property_ok = reinterpret_cast<GetProbeTickCountFn>(fn);
  return true;
}

int DotNetHost::getProbeTickCount() const {
  if (m_get_probe_tick == nullptr) {
    return -1;
  }
  return m_get_probe_tick();
}

int DotNetHost::getProbeSiblingFound() const {
  if (m_get_probe_sibling == nullptr) {
    return -1;
  }
  return m_get_probe_sibling();
}

int DotNetHost::getProbePropertyOk() const {
  if (m_get_probe_property_ok == nullptr) {
    return -1;
  }
  return m_get_probe_property_ok();
}

void DotNetHost::shutdown() {
  if (!m_running && m_host_context == nullptr && m_hostfxr_lib == nullptr) {
    return;
  }

  if (m_shutdown_cleanup != nullptr) {
    m_shutdown_cleanup();
  }

  m_load_game = nullptr;
  m_attach = nullptr;
  m_apply_props = nullptr;
  m_register_abi = nullptr;
  m_register_hooks = nullptr;
  m_shutdown_cleanup = nullptr;
  m_get_probe_tick = nullptr;
  m_get_probe_sibling = nullptr;
  m_get_probe_property_ok = nullptr;
  m_load_assembly_and_get_fn = nullptr;
  m_game_assembly_loaded = false;
  m_running = false;

  if (m_host_context != nullptr && m_hostfxr_lib != nullptr) {
    auto close_fn = reinterpret_cast<hostfxr_close_fn>(
        getExport(static_cast<LibHandle>(m_hostfxr_lib), "hostfxr_close"));
    if (close_fn != nullptr) {
      close_fn(static_cast<hostfxr_handle>(m_host_context));
    }
    m_host_context = nullptr;
  }

  closeHandles();
}

void DotNetHost::closeHandles() {
  if (m_hostfxr_lib != nullptr) {
    freeLibrary(static_cast<LibHandle>(m_hostfxr_lib));
    m_hostfxr_lib = nullptr;
  }
  m_host_context = nullptr;
  m_load_assembly_and_get_fn = nullptr;
  m_load_game = nullptr;
  m_attach = nullptr;
  m_apply_props = nullptr;
  m_register_abi = nullptr;
  m_register_hooks = nullptr;
  m_shutdown_cleanup = nullptr;
  m_get_probe_tick = nullptr;
  m_get_probe_sibling = nullptr;
  m_get_probe_property_ok = nullptr;
  m_game_assembly_loaded = false;
  m_running = false;
}

}  // namespace Blunder

#else  // !BLUNDER_HAS_NETHOST

namespace Blunder {
namespace {

void logHostError(const eastl::string& message) {
  std::fprintf(stderr, "DotNetHost: %s\n", message.c_str());
}

}  // namespace

bool DotNetHost::start(const std::filesystem::path&,
                       const std::filesystem::path&, const BlunderNativeAbi&,
                       eastl::string& out_error) {
  out_error =
      "nethost not available at build time (install .NET 10 SDK / set "
      "DOTNET_ROOT and reconfigure CMake)";
  logHostError(out_error);
  return false;
}

bool DotNetHost::resolveExports(const std::filesystem::path&,
                                eastl::string& out_error) {
  out_error = "nethost not available";
  return false;
}

bool DotNetHost::registerNativeAbi(const BlunderNativeAbi&,
                                   eastl::string& out_error) {
  out_error = "nethost not available";
  return false;
}

bool DotNetHost::loadGameAssembly(const std::filesystem::path&,
                                  eastl::string& out_error) {
  out_error = "nethost not available";
  return false;
}

bool DotNetHost::attachBehaviour(ObjectId, const char*, BehaviourId* out_id,
                                 eastl::string& out_error) {
  if (out_id != nullptr) {
    *out_id = k_invalid_behaviour_id;
  }
  out_error = "nethost not available";
  return false;
}

bool DotNetHost::applyBehaviourProperties(ObjectId, BehaviourId, const char*,
                                          eastl::string& out_error) {
  out_error = "nethost not available";
  return false;
}

bool DotNetHost::resolveProbeTickCount(const std::filesystem::path&,
                                       eastl::string& out_error) {
  out_error = "nethost not available";
  return false;
}

int DotNetHost::getProbeTickCount() const { return -1; }

int DotNetHost::getProbeSiblingFound() const { return -1; }

int DotNetHost::getProbePropertyOk() const { return -1; }

void DotNetHost::shutdown() { closeHandles(); }

void DotNetHost::closeHandles() {
  m_hostfxr_lib = nullptr;
  m_host_context = nullptr;
  m_load_assembly_and_get_fn = nullptr;
  m_load_game = nullptr;
  m_attach = nullptr;
  m_register_hooks = nullptr;
  m_shutdown_cleanup = nullptr;
  m_get_probe_tick = nullptr;
  m_get_probe_sibling = nullptr;
  m_get_probe_property_ok = nullptr;
  m_game_assembly_loaded = false;
  m_running = false;
}

}  // namespace Blunder

#endif  // BLUNDER_HAS_NETHOST
