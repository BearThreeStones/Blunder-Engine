#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BLUNDER_ENGINE_C_ABI_VERSION 1

typedef uint64_t BlunderObjectId;

enum BlunderEngineResult {
  BLUNDER_ENGINE_OK = 0,
  BLUNDER_ENGINE_ERROR = 1,
};

int blunder_engine_abi_version(void);

BlunderObjectId blunder_object_create(void);
int blunder_object_destroy(BlunderObjectId id);
int blunder_object_is_valid(BlunderObjectId id);

int blunder_object_set_bool_property(BlunderObjectId id, const char* class_name,
                                     const char* property_name, int value);
int blunder_object_get_bool_property(BlunderObjectId id, const char* class_name,
                                     const char* property_name, int* out_value);

typedef void (*BlunderTickHook)(void* script_peer, float delta_time);
int blunder_lifecycle_set_tick_hook(const char* class_name, BlunderTickHook hook);
int blunder_lifecycle_clear_hooks(void);

typedef void (*BlunderPtrCallFn)(void* instance, const void** args, void* ret);
int blunder_ptrcall(const char* class_name, const char* method_name,
                    BlunderObjectId id, const void** args, void* ret);

#ifdef __cplusplus
}
#endif
