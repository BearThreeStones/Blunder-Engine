#include "runtime/core/reflection/engine_c_abi.h"

#include "runtime/core/math/math_types.h"
#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/object.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/object_id.h"
#include "runtime/core/reflection/class_db.h"
#include "runtime/core/reflection/lifecycle.h"
#include "runtime/core/reflection/message_dispatch.h"
#include "runtime/core/reflection/variant.h"
#include "runtime/platform/input/gameplay_input.h"

#include "EASTL/hash_map.h"
#include "EASTL/string.h"
#include "EASTL/unique_ptr.h"
#include "EASTL/vector.h"

#if defined(_WIN32) || defined(_WIN64)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

using namespace Blunder;

namespace {

BlunderMessageHook g_blunder_message_hook = nullptr;

struct PoseAppliedCAbiBinding {
  BlunderObjectId object_id{0};
  BlunderPoseAppliedHook hook{nullptr};
  void* userdata{nullptr};
};

eastl::hash_map<BlunderObjectId,
                eastl::vector<eastl::unique_ptr<PoseAppliedCAbiBinding>>>
    g_pose_applied_bindings;

AnimationPlayer* animationPlayerForObject(BlunderObjectId id) {
  Object* object = ObjectDB::get(static_cast<ObjectId>(id));
  if (object == nullptr) {
    return nullptr;
  }
  return object->ensureAnimationPlayer();
}

void pose_applied_c_abi_bridge(AnimationPlayer& /*player*/, void* userdata) {
  auto* binding = static_cast<PoseAppliedCAbiBinding*>(userdata);
  if (binding == nullptr || binding->hook == nullptr) {
    return;
  }
  binding->hook(binding->object_id, binding->userdata);
}

void removePoseAppliedBindingsForObject(BlunderObjectId id) {
  g_pose_applied_bindings.erase(id);
}

void* propertyInstance(Object* object, const char* class_name) {
  if (object == nullptr || class_name == nullptr) {
    return nullptr;
  }
  if (eastl::string(class_name) == "AnimationPlayer") {
    return object->getAnimationPlayer();
  }
  return object;
}

BlunderMessageArg toBlunderMessageArg(const MessageArg& arg) {
  BlunderMessageArg out{};
  out.kind = static_cast<uint8_t>(arg.kind);
  switch (arg.kind) {
    case MessageArgKind::Bool:
      out.b = arg.b ? 1 : 0;
      break;
    case MessageArgKind::Int:
      out.i = arg.i;
      break;
    case MessageArgKind::Float:
      out.f = arg.f;
      break;
    case MessageArgKind::ObjectId:
      out.object_id = static_cast<BlunderObjectId>(arg.object_id);
      break;
    case MessageArgKind::Nil:
      break;
  }
  return out;
}

MessageArg fromBlunderMessageArg(const BlunderMessageArg& arg) {
  MessageArg out{};
  out.kind = static_cast<MessageArgKind>(arg.kind);
  switch (out.kind) {
    case MessageArgKind::Bool:
      out.b = arg.b != 0;
      break;
    case MessageArgKind::Int:
      out.i = arg.i;
      break;
    case MessageArgKind::Float:
      out.f = arg.f;
      break;
    case MessageArgKind::ObjectId:
      out.object_id = static_cast<ObjectId>(arg.object_id);
      break;
    case MessageArgKind::Nil:
      break;
  }
  return out;
}

void message_hook_adapter(void* script_peer, MessageId id, const MessageArg* args,
                          int argc) {
  if (g_blunder_message_hook == nullptr) {
    return;
  }
  BlunderMessageArg c_args[4]{};
  const int n = argc < 0 ? 0 : (argc > 4 ? 4 : argc);
  if (args != nullptr) {
    for (int i = 0; i < n; ++i) {
      c_args[i] = toBlunderMessageArg(args[i]);
    }
  }
  g_blunder_message_hook(script_peer, id, c_args, argc);
}

}  // namespace

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
  AnimationPlayer* player = object->getAnimationPlayer();
  if (player != nullptr) {
    player->clearPoseAppliedListeners();
  }
  removePoseAppliedBindingsForObject(id);
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
  void* instance = propertyInstance(object, class_name);
  if (instance == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  return ClassDB::setProperty(instance, class_name, property_name,
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
  void* instance = propertyInstance(object, class_name);
  if (instance == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  Variant value;
  if (!ClassDB::getProperty(instance, class_name, property_name, value)) {
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

int blunder_gameplay_input_get_move(float* out_x, float* out_y) {
  if (out_x == nullptr || out_y == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  const GameplayInputSnapshot snap = gameplayInputState().current();
  *out_x = snap.move_x;
  *out_y = snap.move_y;
  return BLUNDER_ENGINE_OK;
}

int blunder_gameplay_input_was_jump_pressed(int* out_pressed) {
  if (out_pressed == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  *out_pressed = gameplayInputState().current().jump_pressed ? 1 : 0;
  return BLUNDER_ENGINE_OK;
}

int blunder_message_register(const char* name, BlunderMessageId* out_id) {
  if (out_id == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  const MessageId id = MessageDispatch::registerName(name);
  if (id == k_invalid_message_id) {
    return BLUNDER_ENGINE_ERROR;
  }
  *out_id = id;
  return BLUNDER_ENGINE_OK;
}

int blunder_message_send(BlunderObjectId target, BlunderMessageId id,
                         const BlunderMessageArg* args, int argc) {
  MessageArg native_args[4]{};
  const int n = argc < 0 ? 0 : (argc > 4 ? 4 : argc);
  if (args != nullptr) {
    for (int i = 0; i < n; ++i) {
      native_args[i] = fromBlunderMessageArg(args[i]);
    }
  }
  return MessageDispatch::send(static_cast<ObjectId>(target), id, native_args,
                               argc)
             ? BLUNDER_ENGINE_OK
             : BLUNDER_ENGINE_ERROR;
}

int blunder_message_set_hook(BlunderMessageHook hook) {
  g_blunder_message_hook = hook;
  MessageDispatch::setHook(hook != nullptr ? &message_hook_adapter : nullptr);
  return BLUNDER_ENGINE_OK;
}

int blunder_message_clear_hook(void) {
  g_blunder_message_hook = nullptr;
  MessageDispatch::setHook(nullptr);
  return BLUNDER_ENGINE_OK;
}

int blunder_animation_player_play(BlunderObjectId id, const char* clip_name) {
  if (clip_name == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  AnimationPlayer* player = animationPlayerForObject(id);
  if (player == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  return player->play(eastl::string(clip_name)) ? BLUNDER_ENGINE_OK
                                                : BLUNDER_ENGINE_ERROR;
}

int blunder_animation_player_stop(BlunderObjectId id) {
  AnimationPlayer* player = animationPlayerForObject(id);
  if (player == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  player->stop();
  return BLUNDER_ENGINE_OK;
}

int blunder_animation_player_set_loop(BlunderObjectId id, int loop) {
  AnimationPlayer* player = animationPlayerForObject(id);
  if (player == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  player->setLoop(loop != 0);
  return BLUNDER_ENGINE_OK;
}

int blunder_animation_player_get_playback_position(BlunderObjectId id,
                                                   float* out_position) {
  AnimationPlayer* player = animationPlayerForObject(id);
  if (player == nullptr || out_position == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  *out_position = player->getPlaybackPosition();
  return BLUNDER_ENGINE_OK;
}

int blunder_animation_player_get_clip_length(BlunderObjectId id,
                                             float* out_length) {
  AnimationPlayer* player = animationPlayerForObject(id);
  if (player == nullptr || out_length == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  *out_length = player->getClipLength();
  return BLUNDER_ENGINE_OK;
}

int blunder_animation_player_add_pose_applied_listener(
    BlunderObjectId id, BlunderPoseAppliedHook hook, void* userdata) {
  if (hook == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  AnimationPlayer* player = animationPlayerForObject(id);
  if (player == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }

  auto binding = eastl::make_unique<PoseAppliedCAbiBinding>();
  binding->object_id = id;
  binding->hook = hook;
  binding->userdata = userdata;
  player->addPoseAppliedListener(&pose_applied_c_abi_bridge, binding.get());
  g_pose_applied_bindings[id].push_back(eastl::move(binding));
  return BLUNDER_ENGINE_OK;
}

int blunder_animation_player_clear_pose_applied_listeners(BlunderObjectId id) {
  AnimationPlayer* player = animationPlayerForObject(id);
  if (player == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  player->clearPoseAppliedListeners();
  removePoseAppliedBindingsForObject(id);
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
  out->gameplay_input_get_move = &blunder_gameplay_input_get_move;
  out->gameplay_input_was_jump_pressed =
      &blunder_gameplay_input_was_jump_pressed;
  out->message_register = &blunder_message_register;
  out->message_send = &blunder_message_send;
  out->message_set_hook = &blunder_message_set_hook;
  out->message_clear_hook = &blunder_message_clear_hook;
  out->animation_player_play = &blunder_animation_player_play;
  out->animation_player_stop = &blunder_animation_player_stop;
  out->animation_player_set_loop = &blunder_animation_player_set_loop;
  out->animation_player_get_playback_position =
      &blunder_animation_player_get_playback_position;
  out->animation_player_get_clip_length =
      &blunder_animation_player_get_clip_length;
  out->animation_player_add_pose_applied_listener =
      &blunder_animation_player_add_pose_applied_listener;
  out->animation_player_clear_pose_applied_listeners =
      &blunder_animation_player_clear_pose_applied_listeners;
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
  BLUNDER_NATIVE_ABI_LOAD(gameplay_input_get_move,
                          "blunder_gameplay_input_get_move");
  BLUNDER_NATIVE_ABI_LOAD(gameplay_input_was_jump_pressed,
                          "blunder_gameplay_input_was_jump_pressed");
  BLUNDER_NATIVE_ABI_LOAD(message_register, "blunder_message_register");
  BLUNDER_NATIVE_ABI_LOAD(message_send, "blunder_message_send");
  BLUNDER_NATIVE_ABI_LOAD(message_set_hook, "blunder_message_set_hook");
  BLUNDER_NATIVE_ABI_LOAD(message_clear_hook, "blunder_message_clear_hook");
  BLUNDER_NATIVE_ABI_LOAD(animation_player_play, "blunder_animation_player_play");
  BLUNDER_NATIVE_ABI_LOAD(animation_player_stop, "blunder_animation_player_stop");
  BLUNDER_NATIVE_ABI_LOAD(animation_player_set_loop,
                          "blunder_animation_player_set_loop");
  BLUNDER_NATIVE_ABI_LOAD(animation_player_get_playback_position,
                          "blunder_animation_player_get_playback_position");
  BLUNDER_NATIVE_ABI_LOAD(animation_player_get_clip_length,
                          "blunder_animation_player_get_clip_length");
  BLUNDER_NATIVE_ABI_LOAD(animation_player_add_pose_applied_listener,
                          "blunder_animation_player_add_pose_applied_listener");
  BLUNDER_NATIVE_ABI_LOAD(animation_player_clear_pose_applied_listeners,
                          "blunder_animation_player_clear_pose_applied_listeners");

#undef BLUNDER_NATIVE_ABI_LOAD
  return BLUNDER_ENGINE_OK;
}

}  // extern "C"
