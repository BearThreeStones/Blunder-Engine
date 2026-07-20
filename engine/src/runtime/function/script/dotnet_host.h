#pragma once

#include "EASTL/string.h"

#include "runtime/core/object/behaviour_id.h"
#include "runtime/core/object/object_id.h"
#include "runtime/core/reflection/engine_c_abi.h"

#include <filesystem>

namespace Blunder {

/// In-process CoreCLR host (nethost / hostfxr) that loads Blunder.ScriptHost.
/// Start failure is non-fatal: returns false and fills out_error (caller logs).
class DotNetHost {
 public:
  DotNetHost() = default;
  ~DotNetHost() { shutdown(); }

  DotNetHost(const DotNetHost&) = delete;
  DotNetHost& operator=(const DotNetHost&) = delete;

  /// Start ScriptHost and register `abi` into Blunder.Api before lifecycle hooks.
  /// Editor: fill via `blunder_native_abi_fill_from_process`.
  /// Approach A tests: fill via `blunder_native_abi_fill_from_module` (SHARED).
  bool start(const std::filesystem::path& script_host_dll,
             const std::filesystem::path& runtimeconfig,
             const BlunderNativeAbi& abi, eastl::string& out_error);

  bool loadGameAssembly(const std::filesystem::path& game_dll,
                        eastl::string& out_error);

  bool attachBehaviour(ObjectId object, const char* clr_type_name,
                       BehaviourId* out_id, eastl::string& out_error);

  /// Test seam: resolve `HostExports.GetProbeTickCount` / sibling FoundSibling
  /// via reflection on the already-loaded game assembly.
  bool resolveProbeTickCount(const std::filesystem::path& script_host_dll,
                             eastl::string& out_error);
  int getProbeTickCount() const;
  int getProbeSiblingFound() const;

  void shutdown();
  bool isRunning() const { return m_running; }

 private:
  bool resolveExports(const std::filesystem::path& script_host_dll,
                      eastl::string& out_error);
  bool registerNativeAbi(const BlunderNativeAbi& abi, eastl::string& out_error);
  void closeHandles();

  bool m_running{false};
  void* m_hostfxr_lib{nullptr};
  void* m_host_context{nullptr};  // hostfxr_handle
  void* m_load_assembly_and_get_fn{nullptr};

  using LoadGameAssemblyFn = int (*)(const char* utf8_path);
  using AttachBehaviourFn = int (*)(uint64_t object_id, const char* utf8_type,
                                    uint64_t* out_behaviour_id);
  using RegisterNativeAbiFn = int (*)(const BlunderNativeAbi* abi);
  using RegisterLifecycleHooksFn = void (*)();
  using ShutdownCleanupFn = void (*)();
  using GetProbeTickCountFn = int (*)();

  LoadGameAssemblyFn m_load_game{nullptr};
  AttachBehaviourFn m_attach{nullptr};
  RegisterNativeAbiFn m_register_abi{nullptr};
  RegisterLifecycleHooksFn m_register_hooks{nullptr};
  ShutdownCleanupFn m_shutdown_cleanup{nullptr};
  GetProbeTickCountFn m_get_probe_tick{nullptr};
  GetProbeTickCountFn m_get_probe_sibling{nullptr};
};

}  // namespace Blunder
