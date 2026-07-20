#include "runtime/core/reflection/engine_c_abi.h"

#include "runtime/core/math/math_types.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/reflection/class_db.h"
#include "runtime/core/reflection/lifecycle.h"
#include "runtime/core/reflection/variant.h"

#include "EASTL/string.h"

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

}  // extern "C"
