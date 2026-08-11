#include "runtime/core/reflection/engine_c_abi.h"
#include "runtime/platform/input/gameplay_input.h"

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
  expect_true((std::string(label) + ": gameplay_input_get_move").c_str(),
              abi.gameplay_input_get_move != nullptr);
  expect_true((std::string(label) + ": gameplay_input_was_jump_pressed").c_str(),
              abi.gameplay_input_was_jump_pressed != nullptr);
  expect_true((std::string(label) + ": message_register").c_str(),
              abi.message_register != nullptr);
  expect_true((std::string(label) + ": message_send").c_str(),
              abi.message_send != nullptr);
  expect_true((std::string(label) + ": message_set_hook").c_str(),
              abi.message_set_hook != nullptr);
  expect_true((std::string(label) + ": message_clear_hook").c_str(),
              abi.message_clear_hook != nullptr);
  expect_true((std::string(label) + ": animation_player_play").c_str(),
              abi.animation_player_play != nullptr);
  expect_true((std::string(label) + ": animation_player_stop").c_str(),
              abi.animation_player_stop != nullptr);
  expect_true((std::string(label) + ": animation_player_set_loop").c_str(),
              abi.animation_player_set_loop != nullptr);
  expect_true((std::string(label) + ": animation_player_get_playback_position")
                  .c_str(),
              abi.animation_player_get_playback_position != nullptr);
  expect_true((std::string(label) + ": animation_player_get_clip_length").c_str(),
              abi.animation_player_get_clip_length != nullptr);
  expect_true(
      (std::string(label) + ": animation_player_add_pose_applied_listener")
          .c_str(),
      abi.animation_player_add_pose_applied_listener != nullptr);
  expect_true(
      (std::string(label) + ": animation_player_clear_pose_applied_listeners")
          .c_str(),
      abi.animation_player_clear_pose_applied_listeners != nullptr);
  expect_true((std::string(label) + ": animation_player_play_with_fade").c_str(),
              abi.animation_player_play_with_fade != nullptr);
  expect_true((std::string(label) + ": animation_player_set_slot").c_str(),
              abi.animation_player_set_slot != nullptr);
  expect_true((std::string(label) + ": animation_player_get_slot").c_str(),
              abi.animation_player_get_slot != nullptr);
  expect_true((std::string(label) + ": animation_player_set_blend_weight")
                  .c_str(),
              abi.animation_player_set_blend_weight != nullptr);
  expect_true((std::string(label) + ": animation_player_get_blend_weight")
                  .c_str(),
              abi.animation_player_get_blend_weight != nullptr);
  expect_true((std::string(label) + ": animation_player_set_time_scale").c_str(),
              abi.animation_player_set_time_scale != nullptr);
  expect_true((std::string(label) + ": animation_player_get_time_scale").c_str(),
              abi.animation_player_get_time_scale != nullptr);
  expect_true((std::string(label) + ": animation_tree_set_active").c_str(),
              abi.animation_tree_set_active != nullptr);
  expect_true((std::string(label) + ": animation_tree_get_active").c_str(),
              abi.animation_tree_get_active != nullptr);
  expect_true((std::string(label) + ": animation_tree_travel").c_str(),
              abi.animation_tree_travel != nullptr);
  expect_true((std::string(label) + ": animation_tree_start").c_str(),
              abi.animation_tree_start != nullptr);
  expect_true((std::string(label) + ": animation_tree_set_blend_space_scalar")
                  .c_str(),
              abi.animation_tree_set_blend_space_scalar != nullptr);
  expect_true((std::string(label) + ": animation_tree_get_blend_space_scalar")
                  .c_str(),
              abi.animation_tree_get_blend_space_scalar != nullptr);
  expect_true((std::string(label) + ": animation_tree_request_one_shot").c_str(),
              abi.animation_tree_request_one_shot != nullptr);
  expect_true((std::string(label) + ": animation_tree_set_add2_weight").c_str(),
              abi.animation_tree_set_add2_weight != nullptr);
  expect_true((std::string(label) + ": animation_tree_get_add2_weight").c_str(),
              abi.animation_tree_get_add2_weight != nullptr);
  expect_true((std::string(label) + ": animation_tree_set_blend_space_2d_param")
                  .c_str(),
              abi.animation_tree_set_blend_space_2d_param != nullptr);
  expect_true((std::string(label) + ": animation_tree_get_blend_space_2d_param")
                  .c_str(),
              abi.animation_tree_get_blend_space_2d_param != nullptr);
  expect_true((std::string(label) + ": animation_tree_set_asset_guid").c_str(),
              abi.animation_tree_set_asset_guid != nullptr);
  expect_true((std::string(label) + ": animation_tree_get_asset_guid").c_str(),
              abi.animation_tree_get_asset_guid != nullptr);
  expect_true((std::string(label) + ": animation_tree_set_tree_param_bool").c_str(),
              abi.animation_tree_set_tree_param_bool != nullptr);
  expect_true((std::string(label) + ": animation_tree_get_tree_param_bool").c_str(),
              abi.animation_tree_get_tree_param_bool != nullptr);
  expect_true((std::string(label) + ": animation_tree_set_tree_param_float").c_str(),
              abi.animation_tree_set_tree_param_float != nullptr);
  expect_true((std::string(label) + ": animation_tree_get_tree_param_float").c_str(),
              abi.animation_tree_get_tree_param_float != nullptr);
  expect_true((std::string(label) + ": skeleton_modifier_count").c_str(),
              abi.skeleton_modifier_count != nullptr);
  expect_true((std::string(label) + ": skeleton_modifier_set_enabled").c_str(),
              abi.skeleton_modifier_set_enabled != nullptr);
  expect_true((std::string(label) + ": skeleton_modifier_get_enabled").c_str(),
              abi.skeleton_modifier_get_enabled != nullptr);
  expect_true((std::string(label) + ": skeleton_modifier_move").c_str(),
              abi.skeleton_modifier_move != nullptr);
  expect_true((std::string(label) + ": animation_player_get_method_key_count")
                  .c_str(),
              abi.animation_player_get_method_key_count != nullptr);
  expect_true((std::string(label) + ": animation_player_get_method_key").c_str(),
              abi.animation_player_get_method_key != nullptr);
  expect_true((std::string(label) + ": sync_group_create").c_str(),
              abi.sync_group_create != nullptr);
  expect_true((std::string(label) + ": sync_group_destroy").c_str(),
              abi.sync_group_destroy != nullptr);
  expect_true((std::string(label) + ": sync_group_join").c_str(),
              abi.sync_group_join != nullptr);
  expect_true((std::string(label) + ": sync_group_leave").c_str(),
              abi.sync_group_leave != nullptr);
  expect_true((std::string(label) + ": sync_group_fire").c_str(),
              abi.sync_group_fire != nullptr);
  expect_true((std::string(label) + ": sync_group_fire_same_name").c_str(),
              abi.sync_group_fire_same_name != nullptr);
  expect_true((std::string(label) + ": sync_group_fire_same_name_seek").c_str(),
              abi.sync_group_fire_same_name_seek != nullptr);
  expect_true((std::string(label) + ": cine_enter").c_str(),
              abi.cine_enter != nullptr);
  expect_true((std::string(label) + ": cine_end").c_str(), abi.cine_end != nullptr);
  expect_true((std::string(label) + ": cine_is_in_cine").c_str(),
              abi.cine_is_in_cine != nullptr);
  expect_true((std::string(label) + ": cine_is_gameplay_input_suppressed").c_str(),
              abi.cine_is_gameplay_input_suppressed != nullptr);
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
  expect_true("abi version >= 10", BLUNDER_ENGINE_C_ABI_VERSION >= 10);

  Blunder::gameplayInputState().reset();
  float mx = 1.f;
  float my = 1.f;
  int jump = 1;
  expect_true("cabi move ok",
              blunder_gameplay_input_get_move(&mx, &my) == BLUNDER_ENGINE_OK);
  expect_true("cabi move idle", mx == 0.f && my == 0.f);
  expect_true("cabi jump ok",
              blunder_gameplay_input_was_jump_pressed(&jump) ==
                  BLUNDER_ENGINE_OK);
  expect_true("cabi jump idle", jump == 0);

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
