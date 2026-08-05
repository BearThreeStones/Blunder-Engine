#pragma once

#include <stdint.h>

#if defined(_WIN32) || defined(_WIN64)
#  if defined(BLUNDER_ENGINE_C_EXPORTS)
#    define BLUNDER_ENGINE_C_API __declspec(dllexport)
#  elif defined(BLUNDER_ENGINE_C_DLL)
#    define BLUNDER_ENGINE_C_API __declspec(dllimport)
#  else
#    define BLUNDER_ENGINE_C_API
#  endif
#else
#  define BLUNDER_ENGINE_C_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define BLUNDER_ENGINE_C_ABI_VERSION 10

typedef uint64_t BlunderObjectId;
typedef uint64_t BlunderBehaviourId;

enum BlunderEngineResult {
  BLUNDER_ENGINE_OK = 0,
  BLUNDER_ENGINE_ERROR = 1,
};

BLUNDER_ENGINE_C_API int blunder_engine_abi_version(void);

BLUNDER_ENGINE_C_API BlunderObjectId blunder_object_create(void);
BLUNDER_ENGINE_C_API int blunder_object_destroy(BlunderObjectId id);
BLUNDER_ENGINE_C_API int blunder_object_is_valid(BlunderObjectId id);

BLUNDER_ENGINE_C_API int blunder_object_set_bool_property(BlunderObjectId id,
                                                          const char* class_name,
                                                          const char* property_name,
                                                          int value);
BLUNDER_ENGINE_C_API int blunder_object_get_bool_property(BlunderObjectId id,
                                                          const char* class_name,
                                                          const char* property_name,
                                                          int* out_value);

BLUNDER_ENGINE_C_API BlunderBehaviourId
blunder_object_add_behaviour(BlunderObjectId id, const char* type_name);
BLUNDER_ENGINE_C_API int blunder_object_remove_behaviour(
    BlunderObjectId id, BlunderBehaviourId behaviour_id);
BLUNDER_ENGINE_C_API int blunder_object_behaviour_count(BlunderObjectId id);
BLUNDER_ENGINE_C_API BlunderBehaviourId
blunder_object_behaviour_id_at(BlunderObjectId id, int index);
BLUNDER_ENGINE_C_API int blunder_object_set_behaviour_peer(
    BlunderObjectId id, BlunderBehaviourId behaviour_id, void* peer);
BLUNDER_ENGINE_C_API void* blunder_object_get_behaviour_peer(
    BlunderObjectId id, BlunderBehaviourId behaviour_id);

BLUNDER_ENGINE_C_API int blunder_object_set_vec3_property(
    BlunderObjectId id, const char* class_name, const char* property_name,
    float x, float y, float z);
BLUNDER_ENGINE_C_API int blunder_object_get_vec3_property(
    BlunderObjectId id, const char* class_name, const char* property_name,
    float* x, float* y, float* z);

typedef void (*BlunderTickHook)(void* script_peer, float delta_time);
typedef void (*BlunderReadyHook)(void* script_peer);
BLUNDER_ENGINE_C_API int blunder_lifecycle_set_tick_hook(
    const char* class_name, BlunderTickHook hook);
BLUNDER_ENGINE_C_API int blunder_lifecycle_set_ready_hook(
    const char* class_name, BlunderReadyHook hook);
BLUNDER_ENGINE_C_API int blunder_lifecycle_clear_hooks(void);
// Invoke Ready/Tick against the ObjectDB that owns this C-ABI image
// (process-linked static, or SHARED for Approach A / single-image hosts).
// In-process CoreCLR uses registered BlunderNativeAbi pointers — not DllImport.
BLUNDER_ENGINE_C_API int blunder_lifecycle_invoke_ready(BlunderObjectId id);
BLUNDER_ENGINE_C_API int blunder_lifecycle_invoke_tick(BlunderObjectId id,
                                                       float delta_time);

BLUNDER_ENGINE_C_API int blunder_gameplay_input_get_move(float* out_x,
                                                         float* out_y);
BLUNDER_ENGINE_C_API int blunder_gameplay_input_was_jump_pressed(
    int* out_pressed);

typedef uint32_t BlunderMessageId;

typedef struct BlunderMessageArg {
  uint8_t kind;
  uint8_t _padding[7];
  union {
    uint8_t b;
    int64_t i;
    float f;
    BlunderObjectId object_id;
  };
} BlunderMessageArg;

typedef void (*BlunderMessageHook)(void* peer, BlunderMessageId id,
                                   const BlunderMessageArg* args, int argc);

BLUNDER_ENGINE_C_API int blunder_message_register(const char* name,
                                                  BlunderMessageId* out_id);
BLUNDER_ENGINE_C_API int blunder_message_send(BlunderObjectId target,
                                              BlunderMessageId id,
                                              const BlunderMessageArg* args,
                                              int argc);
BLUNDER_ENGINE_C_API int blunder_message_set_hook(BlunderMessageHook hook);
BLUNDER_ENGINE_C_API int blunder_message_clear_hook(void);

typedef void (*BlunderPoseAppliedHook)(BlunderObjectId object_id, void* userdata);

BLUNDER_ENGINE_C_API int blunder_animation_player_play(BlunderObjectId id,
                                                       const char* clip_name);
BLUNDER_ENGINE_C_API int blunder_animation_player_play_with_fade(
    BlunderObjectId id, const char* clip_name, float fade_seconds);
BLUNDER_ENGINE_C_API int blunder_animation_player_stop(BlunderObjectId id);
BLUNDER_ENGINE_C_API int blunder_animation_player_set_slot(BlunderObjectId id,
                                                           int slot_index,
                                                           const char* clip_name);
BLUNDER_ENGINE_C_API int blunder_animation_player_get_slot(BlunderObjectId id,
                                                           int slot_index,
                                                           char* out_name,
                                                           int name_capacity);
BLUNDER_ENGINE_C_API int blunder_animation_player_set_blend_weight(
    BlunderObjectId id, float weight);
BLUNDER_ENGINE_C_API int blunder_animation_player_get_blend_weight(
    BlunderObjectId id, float* out_weight);
BLUNDER_ENGINE_C_API int blunder_animation_player_set_time_scale(
    BlunderObjectId id, float scale);
BLUNDER_ENGINE_C_API int blunder_animation_player_get_time_scale(
    BlunderObjectId id, float* out_scale);
BLUNDER_ENGINE_C_API int blunder_animation_player_set_loop(BlunderObjectId id,
                                                           int loop);
BLUNDER_ENGINE_C_API int blunder_animation_player_get_playback_position(
    BlunderObjectId id, float* out_position);
BLUNDER_ENGINE_C_API int blunder_animation_player_get_clip_length(
    BlunderObjectId id, float* out_length);
BLUNDER_ENGINE_C_API int blunder_animation_player_add_pose_applied_listener(
    BlunderObjectId id, BlunderPoseAppliedHook hook, void* userdata);
BLUNDER_ENGINE_C_API int blunder_animation_player_clear_pose_applied_listeners(
    BlunderObjectId id);

BLUNDER_ENGINE_C_API int blunder_animation_tree_set_active(BlunderObjectId id,
                                                           int active);
BLUNDER_ENGINE_C_API int blunder_animation_tree_get_active(BlunderObjectId id,
                                                           int* out_active);
BLUNDER_ENGINE_C_API int blunder_animation_tree_travel(BlunderObjectId id,
                                                       const char* state_name);
BLUNDER_ENGINE_C_API int blunder_animation_tree_start(BlunderObjectId id,
                                                      const char* state_name);
BLUNDER_ENGINE_C_API int blunder_animation_tree_set_blend_space_scalar(
    BlunderObjectId id, const char* node_name, float scalar);
BLUNDER_ENGINE_C_API int blunder_animation_tree_get_blend_space_scalar(
    BlunderObjectId id, const char* node_name, float* out_scalar);
BLUNDER_ENGINE_C_API int blunder_animation_tree_request_one_shot(
    BlunderObjectId id, const char* clip_name);
BLUNDER_ENGINE_C_API int blunder_animation_tree_set_add2_weight(
    BlunderObjectId id, float weight);
BLUNDER_ENGINE_C_API int blunder_animation_tree_get_add2_weight(
    BlunderObjectId id, float* out_weight);
BLUNDER_ENGINE_C_API int blunder_animation_tree_set_blend_space_2d_param(
    BlunderObjectId id, const char* node_name, float x, float y);
BLUNDER_ENGINE_C_API int blunder_animation_tree_get_blend_space_2d_param(
    BlunderObjectId id, const char* node_name, float* out_x, float* out_y);
BLUNDER_ENGINE_C_API int blunder_animation_tree_set_asset_guid(
    BlunderObjectId id, const char* guid);
BLUNDER_ENGINE_C_API int blunder_animation_tree_get_asset_guid(
    BlunderObjectId id, char* out_guid, int guid_capacity);

BLUNDER_ENGINE_C_API int blunder_skeleton_modifier_count(BlunderObjectId id,
                                                         int* out_count);
BLUNDER_ENGINE_C_API int blunder_skeleton_modifier_set_enabled(
    BlunderObjectId id, int index, int enabled);
BLUNDER_ENGINE_C_API int blunder_skeleton_modifier_get_enabled(
    BlunderObjectId id, int index, int* out_enabled);
BLUNDER_ENGINE_C_API int blunder_skeleton_modifier_move(BlunderObjectId id,
                                                        int from_index,
                                                        int to_index);

BLUNDER_ENGINE_C_API int blunder_skeleton_modifier_set_paper_mouth_open_amount(
    BlunderObjectId id, int index, float open_amount);
BLUNDER_ENGINE_C_API int blunder_skeleton_modifier_get_paper_mouth_open_amount(
    BlunderObjectId id, int index, float* out_open_amount);
BLUNDER_ENGINE_C_API int blunder_skeleton_modifier_set_paper_mouth_bone_name(
    BlunderObjectId id, int index, const char* bone_name);
BLUNDER_ENGINE_C_API int blunder_skeleton_modifier_get_paper_mouth_bone_name(
    BlunderObjectId id, int index, char* out_bone_name, int name_capacity);
BLUNDER_ENGINE_C_API int blunder_skeleton_modifier_set_attach_bone_name(
    BlunderObjectId id, int index, const char* bone_name);
BLUNDER_ENGINE_C_API int blunder_skeleton_modifier_get_attach_bone_name(
    BlunderObjectId id, int index, char* out_bone_name, int name_capacity);
BLUNDER_ENGINE_C_API int blunder_skeleton_modifier_set_attach_child_object_id(
    BlunderObjectId id, int index, BlunderObjectId child_object_id);
BLUNDER_ENGINE_C_API int blunder_skeleton_modifier_get_attach_child_object_id(
    BlunderObjectId id, int index, BlunderObjectId* out_child_object_id);
BLUNDER_ENGINE_C_API int blunder_skeleton_modifier_set_look_at_target(
    BlunderObjectId id, int index, float x, float y, float z);
BLUNDER_ENGINE_C_API int blunder_skeleton_modifier_get_look_at_target(
    BlunderObjectId id, int index, float* out_x, float* out_y, float* out_z);
BLUNDER_ENGINE_C_API int blunder_skeleton_modifier_set_look_at_bone_name(
    BlunderObjectId id, int index, const char* bone_name);
BLUNDER_ENGINE_C_API int blunder_skeleton_modifier_get_look_at_bone_name(
    BlunderObjectId id, int index, char* out_bone_name, int name_capacity);

/// Method-track query surface (Edit markers / tooling). Product dispatch uses Message.
BLUNDER_ENGINE_C_API int blunder_animation_player_get_method_key_count(
    BlunderObjectId id, const char* clip_name, int* out_count);
BLUNDER_ENGINE_C_API int blunder_animation_player_get_method_key(
    BlunderObjectId id, const char* clip_name, int index, char* out_name,
    int name_capacity, float* out_time);

typedef uint64_t BlunderSyncGroupId;

typedef struct BlunderSyncGroupFireInstruction {
  BlunderObjectId player_object_id;
  const char* clip_name;
  float seek_seconds;
  uint8_t has_seek;
  uint8_t _padding[3];
} BlunderSyncGroupFireInstruction;

BLUNDER_ENGINE_C_API BlunderSyncGroupId blunder_sync_group_create(void);
BLUNDER_ENGINE_C_API int blunder_sync_group_destroy(BlunderSyncGroupId id);
BLUNDER_ENGINE_C_API int blunder_sync_group_join(BlunderSyncGroupId id,
                                                 BlunderObjectId player_object_id);
BLUNDER_ENGINE_C_API int blunder_sync_group_leave(BlunderSyncGroupId id,
                                                 BlunderObjectId player_object_id);
BLUNDER_ENGINE_C_API int blunder_sync_group_fire(
    BlunderSyncGroupId id, const BlunderSyncGroupFireInstruction* instructions,
    int instruction_count);
BLUNDER_ENGINE_C_API int blunder_sync_group_fire_same_name(BlunderSyncGroupId id,
                                                          const char* clip_name);
BLUNDER_ENGINE_C_API int blunder_sync_group_fire_same_name_seek(
    BlunderSyncGroupId id, const char* clip_name, float seek_seconds);

BLUNDER_ENGINE_C_API int blunder_cine_enter(int suppress_gameplay_input);
BLUNDER_ENGINE_C_API int blunder_cine_end(void);
BLUNDER_ENGINE_C_API int blunder_cine_is_in_cine(int* out_value);
BLUNDER_ENGINE_C_API int blunder_cine_is_gameplay_input_suppressed(int* out_value);

typedef void (*BlunderPtrCallFn)(void* instance, const void** args, void* ret);
BLUNDER_ENGINE_C_API int blunder_ptrcall(const char* class_name,
                                         const char* method_name,
                                         BlunderObjectId id, const void** args,
                                         void* ret);

// Function-pointer table mirroring Blunder.Api Native.cs C-ABI v4 entry points.
// Hosts register this into ScriptHost so managed code shares one ObjectDB image.
typedef struct BlunderNativeAbi {
  int (*engine_abi_version)(void);
  BlunderObjectId (*object_create)(void);
  int (*object_destroy)(BlunderObjectId id);
  int (*object_is_valid)(BlunderObjectId id);
  int (*object_set_bool_property)(BlunderObjectId id, const char* class_name,
                                  const char* property_name, int value);
  int (*object_get_bool_property)(BlunderObjectId id, const char* class_name,
                                  const char* property_name, int* out_value);
  BlunderBehaviourId (*object_add_behaviour)(BlunderObjectId id,
                                             const char* type_name);
  int (*object_remove_behaviour)(BlunderObjectId id,
                                 BlunderBehaviourId behaviour_id);
  int (*object_behaviour_count)(BlunderObjectId id);
  BlunderBehaviourId (*object_behaviour_id_at)(BlunderObjectId id, int index);
  int (*object_set_behaviour_peer)(BlunderObjectId id,
                                   BlunderBehaviourId behaviour_id, void* peer);
  void* (*object_get_behaviour_peer)(BlunderObjectId id,
                                     BlunderBehaviourId behaviour_id);
  int (*object_set_vec3_property)(BlunderObjectId id, const char* class_name,
                                  const char* property_name, float x, float y,
                                  float z);
  int (*object_get_vec3_property)(BlunderObjectId id, const char* class_name,
                                  const char* property_name, float* x, float* y,
                                  float* z);
  int (*lifecycle_set_tick_hook)(const char* class_name, BlunderTickHook hook);
  int (*lifecycle_set_ready_hook)(const char* class_name, BlunderReadyHook hook);
  int (*lifecycle_clear_hooks)(void);
  int (*gameplay_input_get_move)(float* out_x, float* out_y);
  int (*gameplay_input_was_jump_pressed)(int* out_pressed);
  int (*message_register)(const char* name, BlunderMessageId* out_id);
  int (*message_send)(BlunderObjectId target, BlunderMessageId id,
                      const BlunderMessageArg* args, int argc);
  int (*message_set_hook)(BlunderMessageHook hook);
  int (*message_clear_hook)(void);
  int (*animation_player_play)(BlunderObjectId id, const char* clip_name);
  int (*animation_player_play_with_fade)(BlunderObjectId id,
                                         const char* clip_name,
                                         float fade_seconds);
  int (*animation_player_stop)(BlunderObjectId id);
  int (*animation_player_set_slot)(BlunderObjectId id, int slot_index,
                                   const char* clip_name);
  int (*animation_player_get_slot)(BlunderObjectId id, int slot_index,
                                   char* out_name, int name_capacity);
  int (*animation_player_set_blend_weight)(BlunderObjectId id, float weight);
  int (*animation_player_get_blend_weight)(BlunderObjectId id,
                                           float* out_weight);
  int (*animation_player_set_time_scale)(BlunderObjectId id, float scale);
  int (*animation_player_get_time_scale)(BlunderObjectId id, float* out_scale);
  int (*animation_player_set_loop)(BlunderObjectId id, int loop);
  int (*animation_player_get_playback_position)(BlunderObjectId id,
                                              float* out_position);
  int (*animation_player_get_clip_length)(BlunderObjectId id,
                                          float* out_length);
  int (*animation_player_add_pose_applied_listener)(
      BlunderObjectId id, BlunderPoseAppliedHook hook, void* userdata);
  int (*animation_player_clear_pose_applied_listeners)(BlunderObjectId id);
  int (*animation_tree_set_active)(BlunderObjectId id, int active);
  int (*animation_tree_get_active)(BlunderObjectId id, int* out_active);
  int (*animation_tree_travel)(BlunderObjectId id, const char* state_name);
  int (*animation_tree_start)(BlunderObjectId id, const char* state_name);
  int (*animation_tree_set_blend_space_scalar)(BlunderObjectId id,
                                               const char* node_name,
                                               float scalar);
  int (*animation_tree_get_blend_space_scalar)(BlunderObjectId id,
                                               const char* node_name,
                                               float* out_scalar);
  int (*animation_tree_request_one_shot)(BlunderObjectId id,
                                         const char* clip_name);
  int (*animation_tree_set_add2_weight)(BlunderObjectId id, float weight);
  int (*animation_tree_get_add2_weight)(BlunderObjectId id, float* out_weight);
  int (*animation_tree_set_blend_space_2d_param)(BlunderObjectId id,
                                                 const char* node_name, float x,
                                                 float y);
  int (*animation_tree_get_blend_space_2d_param)(BlunderObjectId id,
                                                 const char* node_name,
                                                 float* out_x, float* out_y);
  int (*animation_tree_set_asset_guid)(BlunderObjectId id, const char* guid);
  int (*animation_tree_get_asset_guid)(BlunderObjectId id, char* out_guid,
                                       int guid_capacity);
  int (*skeleton_modifier_count)(BlunderObjectId id, int* out_count);
  int (*skeleton_modifier_set_enabled)(BlunderObjectId id, int index,
                                       int enabled);
  int (*skeleton_modifier_get_enabled)(BlunderObjectId id, int index,
                                       int* out_enabled);
  int (*skeleton_modifier_move)(BlunderObjectId id, int from_index,
                                int to_index);
  int (*skeleton_modifier_set_paper_mouth_open_amount)(BlunderObjectId id,
                                                       int index,
                                                       float open_amount);
  int (*skeleton_modifier_get_paper_mouth_open_amount)(BlunderObjectId id,
                                                       int index,
                                                       float* out_open_amount);
  int (*skeleton_modifier_set_paper_mouth_bone_name)(BlunderObjectId id,
                                                     int index,
                                                     const char* bone_name);
  int (*skeleton_modifier_get_paper_mouth_bone_name)(BlunderObjectId id,
                                                     int index,
                                                     char* out_bone_name,
                                                     int name_capacity);
  int (*skeleton_modifier_set_attach_bone_name)(BlunderObjectId id, int index,
                                                const char* bone_name);
  int (*skeleton_modifier_get_attach_bone_name)(BlunderObjectId id, int index,
                                                char* out_bone_name,
                                                int name_capacity);
  int (*skeleton_modifier_set_attach_child_object_id)(
      BlunderObjectId id, int index, BlunderObjectId child_object_id);
  int (*skeleton_modifier_get_attach_child_object_id)(
      BlunderObjectId id, int index, BlunderObjectId* out_child_object_id);
  int (*skeleton_modifier_set_look_at_target)(BlunderObjectId id, int index,
                                            float x, float y, float z);
  int (*skeleton_modifier_get_look_at_target)(BlunderObjectId id, int index,
                                              float* out_x, float* out_y,
                                              float* out_z);
  int (*skeleton_modifier_set_look_at_bone_name)(BlunderObjectId id, int index,
                                                 const char* bone_name);
  int (*skeleton_modifier_get_look_at_bone_name)(BlunderObjectId id, int index,
                                                 char* out_bone_name,
                                                 int name_capacity);
  int (*animation_player_get_method_key_count)(BlunderObjectId id,
                                               const char* clip_name,
                                               int* out_count);
  int (*animation_player_get_method_key)(BlunderObjectId id,
                                         const char* clip_name, int index,
                                         char* out_name, int name_capacity,
                                         float* out_time);
  BlunderSyncGroupId (*sync_group_create)(void);
  int (*sync_group_destroy)(BlunderSyncGroupId id);
  int (*sync_group_join)(BlunderSyncGroupId id, BlunderObjectId player_object_id);
  int (*sync_group_leave)(BlunderSyncGroupId id, BlunderObjectId player_object_id);
  int (*sync_group_fire)(BlunderSyncGroupId id,
                         const BlunderSyncGroupFireInstruction* instructions,
                         int instruction_count);
  int (*sync_group_fire_same_name)(BlunderSyncGroupId id, const char* clip_name);
  int (*sync_group_fire_same_name_seek)(BlunderSyncGroupId id,
                                        const char* clip_name,
                                        float seek_seconds);
  int (*cine_enter)(int suppress_gameplay_input);
  int (*cine_end)(void);
  int (*cine_is_in_cine)(int* out_value);
  int (*cine_is_gameplay_input_suppressed)(int* out_value);
} BlunderNativeAbi;

// Fill from process-linked C-ABI symbols (editor / blunder_engine_c_static).
BLUNDER_ENGINE_C_API void blunder_native_abi_fill_from_process(
    BlunderNativeAbi* out);

// Fill from a LoadLibrary'd / dlopen'd SHARED blunder_engine_c module (tests).
// module is HMODULE on Windows, void* handle elsewhere. Returns BLUNDER_ENGINE_OK
// when every Api entry resolves; otherwise BLUNDER_ENGINE_ERROR.
BLUNDER_ENGINE_C_API int blunder_native_abi_fill_from_module(
    BlunderNativeAbi* out, void* module);

#ifdef __cplusplus
}
#endif
