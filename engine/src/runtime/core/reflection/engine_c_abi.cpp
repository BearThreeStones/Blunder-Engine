#include "runtime/core/reflection/engine_c_abi.h"

#include "runtime/core/math/math_types.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/reflection/class_db.h"
#include "runtime/core/reflection/lifecycle.h"
#include "runtime/core/reflection/variant.h"

#include "EASTL/string.h"

#if defined(_WIN32) || defined(_WIN64)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

using namespace Blunder;

extern "C" {

int blunder_engine_abi_version(void) { return BLUNDER_ENGINE_C_ABI_VERSION; }

BlunderObjectId blunder_object_create(void) {
  return static_cast<BlunderObjectId>(ObjectDB::create());
}

int blunder_object_destroy(BlunderObjectId id) {
  Object* object = ObjectDB::get(static_cast<ObjectId>(id));
  if (object == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  ObjectDB::destroy(static_cast<ObjectId>(id));
  return BLUNDER_ENGINE_OK;
}

int blunder_object_is_valid(BlunderObjectId id) {
  return ObjectDB::get(static_cast<ObjectId>(id)) != nullptr ? 1 : 0;
}

int blunder_object_set_bool_property(BlunderObjectId id, const char* class_name,
                                     const char* property_name, int value) {
  Object* object = ObjectDB::get(static_cast<ObjectId>(id));
  if (object == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  return ClassDB::setProperty(object, class_name, property_name,
                              Variant(value != 0))
             ? BLUNDER_ENGINE_OK
             : BLUNDER_ENGINE_ERROR;
}

int blunder_object_get_bool_property(BlunderObjectId id, const char* class_name,
                                     const char* property_name,
                                     int* out_value) {
  Object* object = ObjectDB::get(static_cast<ObjectId>(id));
  if (object == nullptr || out_value == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  Variant value;
  if (!ClassDB::getProperty(object, class_name, property_name, value)) {
    return BLUNDER_ENGINE_ERROR;
  }
  *out_value = value.asBool() ? 1 : 0;
  return BLUNDER_ENGINE_OK;
}

BlunderBehaviourId blunder_object_add_behaviour(BlunderObjectId id,
                                                const char* type_name) {
  Object* object = ObjectDB::get(static_cast<ObjectId>(id));
  if (object == nullptr || type_name == nullptr) {
    return 0;
  }
  return static_cast<BlunderBehaviourId>(
      object->addBehaviour(eastl::string(type_name)));
}

int blunder_object_remove_behaviour(BlunderObjectId id,
                                    BlunderBehaviourId behaviour_id) {
  Object* object = ObjectDB::get(static_cast<ObjectId>(id));
  if (object == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  return object->removeBehaviour(static_cast<BehaviourId>(behaviour_id))
             ? BLUNDER_ENGINE_OK
             : BLUNDER_ENGINE_ERROR;
}

int blunder_object_behaviour_count(BlunderObjectId id) {
  Object* object = ObjectDB::get(static_cast<ObjectId>(id));
  if (object == nullptr) {
    return 0;
  }
  return static_cast<int>(object->getBehaviourCount());
}

BlunderBehaviourId blunder_object_behaviour_id_at(BlunderObjectId id,
                                                 int index) {
  Object* object = ObjectDB::get(static_cast<ObjectId>(id));
  if (object == nullptr || index < 0) {
    return 0;
  }
  return static_cast<BlunderBehaviourId>(
      object->getBehaviourIdAt(static_cast<size_t>(index)));
}

int blunder_object_set_behaviour_peer(BlunderObjectId id,
                                      BlunderBehaviourId behaviour_id,
                                      void* peer) {
  Object* object = ObjectDB::get(static_cast<ObjectId>(id));
  if (object == nullptr ||
      object->getBehaviourTypeName(static_cast<BehaviourId>(behaviour_id)) ==
          nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  object->setBehaviourScriptPeer(static_cast<BehaviourId>(behaviour_id), peer);
  return BLUNDER_ENGINE_OK;
}

void* blunder_object_get_behaviour_peer(BlunderObjectId id,
                                        BlunderBehaviourId behaviour_id) {
  Object* object = ObjectDB::get(static_cast<ObjectId>(id));
  if (object == nullptr) {
    return nullptr;
  }
  return object->getBehaviourScriptPeer(
      static_cast<BehaviourId>(behaviour_id));
}

int blunder_object_set_vec3_property(BlunderObjectId id, const char* class_name,
                                     const char* property_name, float x,
                                     float y, float z) {
  Object* object = ObjectDB::get(static_cast<ObjectId>(id));
  if (object == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  return ClassDB::setProperty(object, class_name, property_name,
                              Variant(Vec3{x, y, z}))
             ? BLUNDER_ENGINE_OK
             : BLUNDER_ENGINE_ERROR;
}

int blunder_object_get_vec3_property(BlunderObjectId id, const char* class_name,
                                     const char* property_name, float* x,
                                     float* y, float* z) {
  Object* object = ObjectDB::get(static_cast<ObjectId>(id));
  if (object == nullptr || x == nullptr || y == nullptr || z == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  Variant value;
  if (!ClassDB::getProperty(object, class_name, property_name, value)) {
    return BLUNDER_ENGINE_ERROR;
  }
  if (value.getType() != VariantType::Vec3) {
    return BLUNDER_ENGINE_ERROR;
  }
  const Vec3 v = value.asVec3();
  *x = v.x;
  *y = v.y;
  *z = v.z;
  return BLUNDER_ENGINE_OK;
}

int blunder_lifecycle_set_tick_hook(const char* class_name,
                                    BlunderTickHook hook) {
  LifecycleDispatch::setTickHook(class_name, hook);
  return BLUNDER_ENGINE_OK;
}

int blunder_lifecycle_set_ready_hook(const char* class_name,
                                     BlunderReadyHook hook) {
  LifecycleDispatch::setReadyHook(class_name, hook);
  return BLUNDER_ENGINE_OK;
}

int blunder_lifecycle_clear_hooks(void) {
  LifecycleDispatch::clear();
  return BLUNDER_ENGINE_OK;
}

int blunder_lifecycle_invoke_ready(BlunderObjectId id) {
  Object* object = ObjectDB::get(static_cast<ObjectId>(id));
  if (object == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  LifecycleDispatch::invokeReady(object);
  return BLUNDER_ENGINE_OK;
}

int blunder_lifecycle_invoke_tick(BlunderObjectId id, float delta_time) {
  Object* object = ObjectDB::get(static_cast<ObjectId>(id));
  if (object == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  LifecycleDispatch::invokeTick(object, delta_time);
  return BLUNDER_ENGINE_OK;
}

int blunder_ptrcall(const char* class_name, const char* method_name,
                    BlunderObjectId id, const void** args, void* ret) {
  Object* object = ObjectDB::get(static_cast<ObjectId>(id));
  MethodBind* bind = ClassDB::getMethod(class_name, method_name);
  if (object == nullptr || bind == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  bind->ptrcall(object, args, ret);
  return BLUNDER_ENGINE_OK;
}

void blunder_native_abi_fill_from_process(BlunderNativeAbi* out) {
  if (out == nullptr) {
    return;
  }
  out->engine_abi_version = &blunder_engine_abi_version;
  out->object_create = &blunder_object_create;
  out->object_destroy = &blunder_object_destroy;
  out->object_is_valid = &blunder_object_is_valid;
  out->object_set_bool_property = &blunder_object_set_bool_property;
  out->object_get_bool_property = &blunder_object_get_bool_property;
  out->object_add_behaviour = &blunder_object_add_behaviour;
  out->object_remove_behaviour = &blunder_object_remove_behaviour;
  out->object_behaviour_count = &blunder_object_behaviour_count;
  out->object_behaviour_id_at = &blunder_object_behaviour_id_at;
  out->object_set_behaviour_peer = &blunder_object_set_behaviour_peer;
  out->object_get_behaviour_peer = &blunder_object_get_behaviour_peer;
  out->object_set_vec3_property = &blunder_object_set_vec3_property;
  out->object_get_vec3_property = &blunder_object_get_vec3_property;
  out->lifecycle_set_tick_hook = &blunder_lifecycle_set_tick_hook;
  out->lifecycle_set_ready_hook = &blunder_lifecycle_set_ready_hook;
  out->lifecycle_clear_hooks = &blunder_lifecycle_clear_hooks;
}

int blunder_native_abi_fill_from_module(BlunderNativeAbi* out, void* module) {
  if (out == nullptr || module == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }

#if defined(_WIN32) || defined(_WIN64)
  auto load = [module](const char* name) -> void* {
    return reinterpret_cast<void*>(
        ::GetProcAddress(static_cast<HMODULE>(module), name));
  };
#else
  auto load = [module](const char* name) -> void* {
    return ::dlsym(module, name);
  };
#endif

#define BLUNDER_NATIVE_ABI_LOAD(field, symbol)                          \
  do {                                                                  \
    out->field = reinterpret_cast<decltype(out->field)>(load(symbol)); \
    if (out->field == nullptr) {                                        \
      return BLUNDER_ENGINE_ERROR;                                      \
    }                                                                   \
  } while (0)

  BLUNDER_NATIVE_ABI_LOAD(engine_abi_version, "blunder_engine_abi_version");
  BLUNDER_NATIVE_ABI_LOAD(object_create, "blunder_object_create");
  BLUNDER_NATIVE_ABI_LOAD(object_destroy, "blunder_object_destroy");
  BLUNDER_NATIVE_ABI_LOAD(object_is_valid, "blunder_object_is_valid");
  BLUNDER_NATIVE_ABI_LOAD(object_set_bool_property,
                          "blunder_object_set_bool_property");
  BLUNDER_NATIVE_ABI_LOAD(object_get_bool_property,
                          "blunder_object_get_bool_property");
  BLUNDER_NATIVE_ABI_LOAD(object_add_behaviour, "blunder_object_add_behaviour");
  BLUNDER_NATIVE_ABI_LOAD(object_remove_behaviour,
                          "blunder_object_remove_behaviour");
  BLUNDER_NATIVE_ABI_LOAD(object_behaviour_count,
                          "blunder_object_behaviour_count");
  BLUNDER_NATIVE_ABI_LOAD(object_behaviour_id_at,
                          "blunder_object_behaviour_id_at");
  BLUNDER_NATIVE_ABI_LOAD(object_set_behaviour_peer,
                          "blunder_object_set_behaviour_peer");
  BLUNDER_NATIVE_ABI_LOAD(object_get_behaviour_peer,
                          "blunder_object_get_behaviour_peer");
  BLUNDER_NATIVE_ABI_LOAD(object_set_vec3_property,
                          "blunder_object_set_vec3_property");
  BLUNDER_NATIVE_ABI_LOAD(object_get_vec3_property,
                          "blunder_object_get_vec3_property");
  BLUNDER_NATIVE_ABI_LOAD(lifecycle_set_tick_hook,
                          "blunder_lifecycle_set_tick_hook");
  BLUNDER_NATIVE_ABI_LOAD(lifecycle_set_ready_hook,
                          "blunder_lifecycle_set_ready_hook");
  BLUNDER_NATIVE_ABI_LOAD(lifecycle_clear_hooks,
                          "blunder_lifecycle_clear_hooks");

#undef BLUNDER_NATIVE_ABI_LOAD
  return BLUNDER_ENGINE_OK;
}

}  // extern "C"
