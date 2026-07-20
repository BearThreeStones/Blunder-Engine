#include "runtime/core/reflection/engine_c_abi.h"

#include <cstdio>
#include <filesystem>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

void expect_all_api_entries_non_null(const char* label, const BlunderNativeAbi& abi) {
  expect_true((std::string(label) + ": engine_abi_version").c_str(),
              abi.engine_abi_version != nullptr);
  expect_true((std::string(label) + ": object_create").c_str(),
              abi.object_create != nullptr);
  expect_true((std::string(label) + ": object_destroy").c_str(),
              abi.object_destroy != nullptr);
  expect_true((std::string(label) + ": object_is_valid").c_str(),
              abi.object_is_valid != nullptr);
  expect_true((std::string(label) + ": object_set_bool_property").c_str(),
              abi.object_set_bool_property != nullptr);
  expect_true((std::string(label) + ": object_get_bool_property").c_str(),
              abi.object_get_bool_property != nullptr);
  expect_true((std::string(label) + ": object_add_behaviour").c_str(),
              abi.object_add_behaviour != nullptr);
  expect_true((std::string(label) + ": object_remove_behaviour").c_str(),
              abi.object_remove_behaviour != nullptr);
  expect_true((std::string(label) + ": object_behaviour_count").c_str(),
              abi.object_behaviour_count != nullptr);
  expect_true((std::string(label) + ": object_behaviour_id_at").c_str(),
              abi.object_behaviour_id_at != nullptr);
  expect_true((std::string(label) + ": object_set_behaviour_peer").c_str(),
              abi.object_set_behaviour_peer != nullptr);
  expect_true((std::string(label) + ": object_get_behaviour_peer").c_str(),
              abi.object_get_behaviour_peer != nullptr);
  expect_true((std::string(label) + ": object_set_vec3_property").c_str(),
              abi.object_set_vec3_property != nullptr);
  expect_true((std::string(label) + ": object_get_vec3_property").c_str(),
              abi.object_get_vec3_property != nullptr);
  expect_true((std::string(label) + ": lifecycle_set_tick_hook").c_str(),
              abi.lifecycle_set_tick_hook != nullptr);
  expect_true((std::string(label) + ": lifecycle_set_ready_hook").c_str(),
              abi.lifecycle_set_ready_hook != nullptr);
  expect_true((std::string(label) + ": lifecycle_clear_hooks").c_str(),
              abi.lifecycle_clear_hooks != nullptr);
}

std::filesystem::path sharedEngineCPath() {
#if defined(BLUNDER_ENGINE_C_SHARED_PATH)
  return std::filesystem::path(BLUNDER_ENGINE_C_SHARED_PATH);
#else
  return std::filesystem::path();
#endif
}

}  // namespace

int main() {
  BlunderNativeAbi process_abi{};
  blunder_native_abi_fill_from_process(&process_abi);
  expect_all_api_entries_non_null("process", process_abi);
  expect_true("process abi version callable",
              process_abi.engine_abi_version != nullptr &&
                  process_abi.engine_abi_version() == BLUNDER_ENGINE_C_ABI_VERSION);

  const std::filesystem::path dll_path = sharedEngineCPath();
  expect_true("shared dll path defined", !dll_path.empty());
  if (dll_path.empty()) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }

#ifdef _WIN32
  HMODULE module = ::LoadLibraryW(dll_path.wstring().c_str());
  expect_true("LoadLibrary SHARED blunder_engine_c", module != nullptr);
  if (module == nullptr) {
    std::fprintf(stderr, "expected at: %s\n", dll_path.string().c_str());
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
#else
  void* module = ::dlopen(dll_path.string().c_str(), RTLD_NOW);
  expect_true("dlopen SHARED blunder_engine_c", module != nullptr);
  if (module == nullptr) {
    std::fprintf(stderr, "expected at: %s\n", dll_path.string().c_str());
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
#endif

  BlunderNativeAbi module_abi{};
  const int fill_rc =
      blunder_native_abi_fill_from_module(&module_abi, module);
  expect_true("fill_from_module OK", fill_rc == BLUNDER_ENGINE_OK);
  expect_all_api_entries_non_null("module", module_abi);
  expect_true("module abi version callable",
              module_abi.engine_abi_version != nullptr &&
                  module_abi.engine_abi_version() == BLUNDER_ENGINE_C_ABI_VERSION);

#ifdef _WIN32
  ::FreeLibrary(module);
#else
  ::dlclose(module);
#endif

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
