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

#define BLUNDER_ENGINE_C_ABI_VERSION 2

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
// (SHARED blunder_engine_c for managed DllImport / Approach A e2e tests).
BLUNDER_ENGINE_C_API int blunder_lifecycle_invoke_ready(BlunderObjectId id);
BLUNDER_ENGINE_C_API int blunder_lifecycle_invoke_tick(BlunderObjectId id,
                                                       float delta_time);

typedef void (*BlunderPtrCallFn)(void* instance, const void** args, void* ret);
BLUNDER_ENGINE_C_API int blunder_ptrcall(const char* class_name,
                                         const char* method_name,
                                         BlunderObjectId id, const void** args,
                                         void* ret);

#ifdef __cplusplus
}
#endif
