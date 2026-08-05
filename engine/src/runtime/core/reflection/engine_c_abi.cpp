#include "runtime/core/reflection/engine_c_abi.h"

#include "runtime/core/math/math_types.h"
#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/animation_tree.h"
#include "runtime/core/object/animation_sync_group.h"
#include "runtime/core/object/cine_segment_service.h"
#include "runtime/core/object/object.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/object_id.h"
#include "runtime/core/object/skeleton_attach_modifier.h"
#include "runtime/core/object/skeleton_look_at_modifier.h"
#include "runtime/core/object/skeleton_modifier.h"
#include "runtime/core/object/skeleton_paper_mouth_modifier.h"
#include "runtime/core/reflection/class_db.h"
#include "runtime/core/reflection/lifecycle.h"
#include "runtime/core/reflection/message_dispatch.h"
#include "runtime/core/reflection/variant.h"
#include "runtime/platform/input/gameplay_input.h"

#include "EASTL/hash_map.h"
#include "EASTL/string.h"
#include "EASTL/unique_ptr.h"
#include "EASTL/vector.h"

#include <cstring>

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

AnimationTree* animationTreeForObject(BlunderObjectId id) {
  Object* object = ObjectDB::get(static_cast<ObjectId>(id));
  if (object == nullptr) {
    return nullptr;
  }
  return object->getAnimationTree();
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
  if (eastl::string(class_name) == "AnimationTree") {
    return object->getAnimationTree();
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

int blunder_animation_player_play_with_fade(BlunderObjectId id,
                                          const char* clip_name,
                                          float fade_seconds) {
  if (clip_name == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  AnimationPlayer* player = animationPlayerForObject(id);
  if (player == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  return player->play(eastl::string(clip_name), fade_seconds)
             ? BLUNDER_ENGINE_OK
             : BLUNDER_ENGINE_ERROR;
}

int blunder_animation_player_set_slot(BlunderObjectId id, int slot_index,
                                      const char* clip_name) {
  if (clip_name == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  AnimationPlayer* player = animationPlayerForObject(id);
  if (player == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  return player->setSlot(slot_index, eastl::string(clip_name))
             ? BLUNDER_ENGINE_OK
             : BLUNDER_ENGINE_ERROR;
}

int blunder_animation_player_get_slot(BlunderObjectId id, int slot_index,
                                      char* out_name, int name_capacity) {
  if (out_name == nullptr || name_capacity <= 0) {
    return BLUNDER_ENGINE_ERROR;
  }
  AnimationPlayer* player = animationPlayerForObject(id);
  if (player == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  const eastl::string& slot_name = player->getSlotClipName(slot_index);
  out_name[0] = '\0';
  if (slot_name.empty()) {
    return BLUNDER_ENGINE_OK;
  }
  const size_t copy_len =
      static_cast<size_t>(name_capacity - 1) < slot_name.size()
          ? static_cast<size_t>(name_capacity - 1)
          : slot_name.size();
  std::memcpy(out_name, slot_name.c_str(), copy_len);
  out_name[copy_len] = '\0';
  return BLUNDER_ENGINE_OK;
}

int blunder_animation_player_set_blend_weight(BlunderObjectId id,
                                              float weight) {
  AnimationPlayer* player = animationPlayerForObject(id);
  if (player == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  player->setBlendWeight(weight);
  return BLUNDER_ENGINE_OK;
}

int blunder_animation_player_get_blend_weight(BlunderObjectId id,
                                              float* out_weight) {
  AnimationPlayer* player = animationPlayerForObject(id);
  if (player == nullptr || out_weight == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  *out_weight = player->getBlendWeight();
  return BLUNDER_ENGINE_OK;
}

int blunder_animation_player_set_time_scale(BlunderObjectId id, float scale) {
  AnimationPlayer* player = animationPlayerForObject(id);
  if (player == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  player->setTimeScale(scale);
  return BLUNDER_ENGINE_OK;
}

int blunder_animation_player_get_time_scale(BlunderObjectId id,
                                            float* out_scale) {
  AnimationPlayer* player = animationPlayerForObject(id);
  if (player == nullptr || out_scale == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  *out_scale = player->getTimeScale();
  return BLUNDER_ENGINE_OK;
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

int blunder_animation_tree_set_active(BlunderObjectId id, int active) {
  AnimationTree* tree = animationTreeForObject(id);
  if (tree == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  return tree->setActive(active != 0) ? BLUNDER_ENGINE_OK : BLUNDER_ENGINE_ERROR;
}

int blunder_animation_tree_get_active(BlunderObjectId id, int* out_active) {
  if (out_active == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  AnimationTree* tree = animationTreeForObject(id);
  if (tree == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  *out_active = tree->isActive() ? 1 : 0;
  return BLUNDER_ENGINE_OK;
}

int blunder_animation_tree_travel(BlunderObjectId id, const char* state_name) {
  if (state_name == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  AnimationTree* tree = animationTreeForObject(id);
  if (tree == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  return tree->travel(eastl::string(state_name)) ? BLUNDER_ENGINE_OK
                                                  : BLUNDER_ENGINE_ERROR;
}

int blunder_animation_tree_start(BlunderObjectId id, const char* state_name) {
  if (state_name == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  AnimationTree* tree = animationTreeForObject(id);
  if (tree == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  return tree->start(eastl::string(state_name)) ? BLUNDER_ENGINE_OK
                                                : BLUNDER_ENGINE_ERROR;
}

int blunder_animation_tree_set_blend_space_scalar(BlunderObjectId id,
                                                  const char* node_name,
                                                  float scalar) {
  if (node_name == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  AnimationTree* tree = animationTreeForObject(id);
  if (tree == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  tree->setBlendSpaceScalar(eastl::string(node_name), scalar);
  return BLUNDER_ENGINE_OK;
}

int blunder_animation_tree_get_blend_space_scalar(BlunderObjectId id,
                                                  const char* node_name,
                                                  float* out_scalar) {
  if (node_name == nullptr || out_scalar == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  AnimationTree* tree = animationTreeForObject(id);
  if (tree == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  *out_scalar = tree->getBlendSpaceScalar(eastl::string(node_name));
  return BLUNDER_ENGINE_OK;
}

int blunder_animation_tree_request_one_shot(BlunderObjectId id,
                                            const char* clip_name) {
  if (clip_name == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  AnimationTree* tree = animationTreeForObject(id);
  if (tree == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  return tree->requestOneShot(eastl::string(clip_name)) ? BLUNDER_ENGINE_OK
                                                        : BLUNDER_ENGINE_ERROR;
}

int blunder_animation_tree_set_add2_weight(BlunderObjectId id, float weight) {
  AnimationTree* tree = animationTreeForObject(id);
  if (tree == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  tree->setAdd2Weight(weight);
  return BLUNDER_ENGINE_OK;
}

int blunder_animation_tree_get_add2_weight(BlunderObjectId id,
                                           float* out_weight) {
  if (out_weight == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  AnimationTree* tree = animationTreeForObject(id);
  if (tree == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  *out_weight = tree->getAdd2Weight();
  return BLUNDER_ENGINE_OK;
}

int blunder_animation_tree_set_blend_space_2d_param(BlunderObjectId id,
                                                    const char* node_name,
                                                    float x, float y) {
  if (node_name == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  AnimationTree* tree = animationTreeForObject(id);
  if (tree == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  tree->setBlendSpace2DParam(eastl::string(node_name), x, y);
  return BLUNDER_ENGINE_OK;
}

int blunder_animation_tree_get_blend_space_2d_param(BlunderObjectId id,
                                                    const char* node_name,
                                                    float* out_x,
                                                    float* out_y) {
  if (node_name == nullptr || out_x == nullptr || out_y == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  AnimationTree* tree = animationTreeForObject(id);
  if (tree == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  const BlendSpace2DParam param =
      tree->getBlendSpace2DParam(eastl::string(node_name));
  *out_x = param.x;
  *out_y = param.y;
  return BLUNDER_ENGINE_OK;
}

int blunder_animation_tree_set_asset_guid(BlunderObjectId id, const char* guid) {
  AnimationTree* tree = animationTreeForObject(id);
  if (tree == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  tree->setAssetGuid(guid != nullptr ? eastl::string(guid) : eastl::string());
  return BLUNDER_ENGINE_OK;
}

int blunder_animation_tree_get_asset_guid(BlunderObjectId id, char* out_guid,
                                          int guid_capacity) {
  if (out_guid == nullptr || guid_capacity <= 0) {
    return BLUNDER_ENGINE_ERROR;
  }
  AnimationTree* tree = animationTreeForObject(id);
  if (tree == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  const eastl::string& guid = tree->getAssetGuid();
  out_guid[0] = '\0';
  if (guid.empty()) {
    return BLUNDER_ENGINE_OK;
  }
  const size_t copy_len =
      static_cast<size_t>(guid_capacity - 1) < guid.size()
          ? static_cast<size_t>(guid_capacity - 1)
          : guid.size();
  std::memcpy(out_guid, guid.c_str(), copy_len);
  out_guid[copy_len] = '\0';
  return BLUNDER_ENGINE_OK;
}

int blunder_skeleton_modifier_count(BlunderObjectId id, int* out_count) {
  if (out_count == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  Object* object = ObjectDB::get(static_cast<ObjectId>(id));
  if (object == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  *out_count = static_cast<int>(object->getSkeletonModifierCount());
  return BLUNDER_ENGINE_OK;
}

int blunder_skeleton_modifier_set_enabled(BlunderObjectId id, int index,
                                          int enabled) {
  Object* object = ObjectDB::get(static_cast<ObjectId>(id));
  if (object == nullptr || index < 0) {
    return BLUNDER_ENGINE_ERROR;
  }
  SkeletonModifier* modifier =
      object->getSkeletonModifierAt(static_cast<size_t>(index));
  if (modifier == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  modifier->setEnabled(enabled != 0);
  return BLUNDER_ENGINE_OK;
}

int blunder_skeleton_modifier_get_enabled(BlunderObjectId id, int index,
                                          int* out_enabled) {
  if (out_enabled == nullptr || index < 0) {
    return BLUNDER_ENGINE_ERROR;
  }
  Object* object = ObjectDB::get(static_cast<ObjectId>(id));
  if (object == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  const SkeletonModifier* modifier =
      object->getSkeletonModifierAt(static_cast<size_t>(index));
  if (modifier == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  *out_enabled = modifier->isEnabled() ? 1 : 0;
  return BLUNDER_ENGINE_OK;
}

int blunder_skeleton_modifier_move(BlunderObjectId id, int from_index,
                                   int to_index) {
  Object* object = ObjectDB::get(static_cast<ObjectId>(id));
  if (object == nullptr || from_index < 0 || to_index < 0) {
    return BLUNDER_ENGINE_ERROR;
  }
  return object->moveSkeletonModifier(static_cast<size_t>(from_index),
                                     static_cast<size_t>(to_index))
             ? BLUNDER_ENGINE_OK
             : BLUNDER_ENGINE_ERROR;
}

namespace {

SkeletonModifier* skeletonModifierAt(BlunderObjectId id, int index) {
  if (index < 0) {
    return nullptr;
  }
  Object* object = ObjectDB::get(static_cast<ObjectId>(id));
  if (object == nullptr) {
    return nullptr;
  }
  return object->getSkeletonModifierAt(static_cast<size_t>(index));
}

SkeletonPaperMouthModifier* paperMouthModifierAt(BlunderObjectId id,
                                                 int index) {
  SkeletonModifier* modifier = skeletonModifierAt(id, index);
  if (modifier == nullptr ||
      std::strcmp(modifier->getTypeName(), "PaperMouth") != 0) {
    return nullptr;
  }
  return static_cast<SkeletonPaperMouthModifier*>(modifier);
}

SkeletonAttachModifier* attachModifierAt(BlunderObjectId id, int index) {
  SkeletonModifier* modifier = skeletonModifierAt(id, index);
  if (modifier == nullptr ||
      std::strcmp(modifier->getTypeName(), "SkeletonAttachModifier") != 0) {
    return nullptr;
  }
  return static_cast<SkeletonAttachModifier*>(modifier);
}

SkeletonLookAtModifier* lookAtModifierAt(BlunderObjectId id, int index) {
  SkeletonModifier* modifier = skeletonModifierAt(id, index);
  if (modifier == nullptr ||
      std::strcmp(modifier->getTypeName(), "SkeletonLookAtModifier") != 0) {
    return nullptr;
  }
  return static_cast<SkeletonLookAtModifier*>(modifier);
}

void copyStringToBuffer(const eastl::string& value, char* out_buffer,
                        int capacity) {
  if (out_buffer == nullptr || capacity <= 0) {
    return;
  }
  out_buffer[0] = '\0';
  if (value.empty()) {
    return;
  }
  const size_t copy_len =
      static_cast<size_t>(capacity - 1) < value.size()
          ? static_cast<size_t>(capacity - 1)
          : value.size();
  std::memcpy(out_buffer, value.c_str(), copy_len);
  out_buffer[copy_len] = '\0';
}

}  // namespace

int blunder_skeleton_modifier_set_paper_mouth_open_amount(
    BlunderObjectId id, int index, float open_amount) {
  SkeletonPaperMouthModifier* modifier = paperMouthModifierAt(id, index);
  if (modifier == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  modifier->setOpenAmount(open_amount);
  return BLUNDER_ENGINE_OK;
}

int blunder_skeleton_modifier_get_paper_mouth_open_amount(
    BlunderObjectId id, int index, float* out_open_amount) {
  if (out_open_amount == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  const SkeletonPaperMouthModifier* modifier = paperMouthModifierAt(id, index);
  if (modifier == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  *out_open_amount = modifier->getOpenAmount();
  return BLUNDER_ENGINE_OK;
}

int blunder_skeleton_modifier_set_paper_mouth_bone_name(BlunderObjectId id,
                                                        int index,
                                                        const char* bone_name) {
  if (bone_name == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  SkeletonPaperMouthModifier* modifier = paperMouthModifierAt(id, index);
  if (modifier == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  modifier->setBoneName(eastl::string(bone_name));
  return BLUNDER_ENGINE_OK;
}

int blunder_skeleton_modifier_get_paper_mouth_bone_name(
    BlunderObjectId id, int index, char* out_bone_name, int name_capacity) {
  if (out_bone_name == nullptr || name_capacity <= 0) {
    return BLUNDER_ENGINE_ERROR;
  }
  const SkeletonPaperMouthModifier* modifier = paperMouthModifierAt(id, index);
  if (modifier == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  copyStringToBuffer(modifier->getBoneName(), out_bone_name, name_capacity);
  return BLUNDER_ENGINE_OK;
}

int blunder_skeleton_modifier_set_attach_bone_name(BlunderObjectId id,
                                                   int index,
                                                   const char* bone_name) {
  if (bone_name == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  SkeletonAttachModifier* modifier = attachModifierAt(id, index);
  if (modifier == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  modifier->setBoneName(eastl::string(bone_name));
  return BLUNDER_ENGINE_OK;
}

int blunder_skeleton_modifier_get_attach_bone_name(BlunderObjectId id,
                                                   int index,
                                                   char* out_bone_name,
                                                   int name_capacity) {
  if (out_bone_name == nullptr || name_capacity <= 0) {
    return BLUNDER_ENGINE_ERROR;
  }
  const SkeletonAttachModifier* modifier = attachModifierAt(id, index);
  if (modifier == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  copyStringToBuffer(modifier->getBoneName(), out_bone_name, name_capacity);
  return BLUNDER_ENGINE_OK;
}

int blunder_skeleton_modifier_set_attach_child_object_id(
    BlunderObjectId id, int index, BlunderObjectId child_object_id) {
  SkeletonAttachModifier* modifier = attachModifierAt(id, index);
  if (modifier == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  modifier->setChildObjectId(static_cast<ObjectId>(child_object_id));
  return BLUNDER_ENGINE_OK;
}

int blunder_skeleton_modifier_get_attach_child_object_id(
    BlunderObjectId id, int index, BlunderObjectId* out_child_object_id) {
  if (out_child_object_id == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  const SkeletonAttachModifier* modifier = attachModifierAt(id, index);
  if (modifier == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  *out_child_object_id =
      static_cast<BlunderObjectId>(modifier->getChildObjectId());
  return BLUNDER_ENGINE_OK;
}

int blunder_skeleton_modifier_set_look_at_target(BlunderObjectId id, int index,
                                               float x, float y, float z) {
  SkeletonLookAtModifier* modifier = lookAtModifierAt(id, index);
  if (modifier == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  modifier->setTarget(Vec3(x, y, z));
  return BLUNDER_ENGINE_OK;
}

int blunder_skeleton_modifier_get_look_at_target(BlunderObjectId id, int index,
                                                 float* out_x, float* out_y,
                                                 float* out_z) {
  if (out_x == nullptr || out_y == nullptr || out_z == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  const SkeletonLookAtModifier* modifier = lookAtModifierAt(id, index);
  if (modifier == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  const Vec3& target = modifier->getTarget();
  *out_x = target.x;
  *out_y = target.y;
  *out_z = target.z;
  return BLUNDER_ENGINE_OK;
}

int blunder_skeleton_modifier_set_look_at_bone_name(BlunderObjectId id,
                                                  int index,
                                                  const char* bone_name) {
  if (bone_name == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  SkeletonLookAtModifier* modifier = lookAtModifierAt(id, index);
  if (modifier == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  modifier->setBoneName(eastl::string(bone_name));
  return BLUNDER_ENGINE_OK;
}

int blunder_skeleton_modifier_get_look_at_bone_name(BlunderObjectId id,
                                                    int index,
                                                    char* out_bone_name,
                                                    int name_capacity) {
  if (out_bone_name == nullptr || name_capacity <= 0) {
    return BLUNDER_ENGINE_ERROR;
  }
  const SkeletonLookAtModifier* modifier = lookAtModifierAt(id, index);
  if (modifier == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  copyStringToBuffer(modifier->getBoneName(), out_bone_name, name_capacity);
  return BLUNDER_ENGINE_OK;
}

int blunder_animation_player_get_method_key_count(BlunderObjectId id,
                                                  const char* clip_name,
                                                  int* out_count) {
  if (clip_name == nullptr || out_count == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  AnimationPlayer* player = animationPlayerForObject(id);
  if (player == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  AnimationClipData clip;
  if (!player->resolveClipForName(eastl::string(clip_name), clip)) {
    return BLUNDER_ENGINE_ERROR;
  }
  *out_count = static_cast<int>(clip.method_keys.size());
  return BLUNDER_ENGINE_OK;
}

int blunder_animation_player_get_method_key(BlunderObjectId id,
                                            const char* clip_name, int index,
                                            char* out_name, int name_capacity,
                                            float* out_time) {
  if (clip_name == nullptr || out_name == nullptr || name_capacity <= 0 ||
      out_time == nullptr || index < 0) {
    return BLUNDER_ENGINE_ERROR;
  }
  AnimationPlayer* player = animationPlayerForObject(id);
  if (player == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  AnimationClipData clip;
  if (!player->resolveClipForName(eastl::string(clip_name), clip)) {
    return BLUNDER_ENGINE_ERROR;
  }
  if (static_cast<size_t>(index) >= clip.method_keys.size()) {
    return BLUNDER_ENGINE_ERROR;
  }
  const AnimationMethodKey& key = clip.method_keys[static_cast<size_t>(index)];
  *out_time = key.time;
  out_name[0] = '\0';
  const size_t copy_len =
      static_cast<size_t>(name_capacity - 1) < key.name.size()
          ? static_cast<size_t>(name_capacity - 1)
          : key.name.size();
  std::memcpy(out_name, key.name.c_str(), copy_len);
  out_name[copy_len] = '\0';
  return BLUNDER_ENGINE_OK;
}

BlunderSyncGroupId blunder_sync_group_create(void) {
  return animationSyncGroupService().create();
}

int blunder_sync_group_destroy(BlunderSyncGroupId id) {
  return animationSyncGroupService().destroy(id) ? BLUNDER_ENGINE_OK
                                                   : BLUNDER_ENGINE_ERROR;
}

int blunder_sync_group_join(BlunderSyncGroupId id,
                            BlunderObjectId player_object_id) {
  AnimationPlayer* player = animationPlayerForObject(player_object_id);
  if (player == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  return animationSyncGroupService().join(id, player) ? BLUNDER_ENGINE_OK
                                                      : BLUNDER_ENGINE_ERROR;
}

int blunder_sync_group_leave(BlunderSyncGroupId id,
                             BlunderObjectId player_object_id) {
  AnimationPlayer* player = animationPlayerForObject(player_object_id);
  if (player == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  return animationSyncGroupService().leave(id, player) ? BLUNDER_ENGINE_OK
                                                       : BLUNDER_ENGINE_ERROR;
}

int blunder_sync_group_fire(BlunderSyncGroupId id,
                            const BlunderSyncGroupFireInstruction* instructions,
                            int instruction_count) {
  if (instruction_count < 0 || (instruction_count > 0 && instructions == nullptr)) {
    return BLUNDER_ENGINE_ERROR;
  }

  eastl::vector<SyncGroupFireInstruction> native_instructions;
  native_instructions.reserve(static_cast<size_t>(instruction_count));

  for (int i = 0; i < instruction_count; ++i) {
    const BlunderSyncGroupFireInstruction& src = instructions[i];
    if (src.clip_name == nullptr) {
      return BLUNDER_ENGINE_ERROR;
    }

    AnimationPlayer* player = animationPlayerForObject(src.player_object_id);
    if (player == nullptr) {
      return BLUNDER_ENGINE_ERROR;
    }

    SyncGroupFireInstruction instruction;
    instruction.player = player;
    instruction.clip_name = eastl::string(src.clip_name);
    if (src.has_seek != 0) {
      instruction.seek_seconds = src.seek_seconds;
      instruction.has_seek = true;
    }
    native_instructions.push_back(instruction);
  }

  return animationSyncGroupService().fire(id, native_instructions)
             ? BLUNDER_ENGINE_OK
             : BLUNDER_ENGINE_ERROR;
}

int blunder_sync_group_fire_same_name(BlunderSyncGroupId id,
                                      const char* clip_name) {
  if (clip_name == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  return animationSyncGroupService().fireSameName(id, eastl::string(clip_name))
             ? BLUNDER_ENGINE_OK
             : BLUNDER_ENGINE_ERROR;
}

int blunder_sync_group_fire_same_name_seek(BlunderSyncGroupId id,
                                           const char* clip_name,
                                           float seek_seconds) {
  if (clip_name == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  return animationSyncGroupService()
             .fireSameName(id, eastl::string(clip_name), seek_seconds)
             ? BLUNDER_ENGINE_OK
             : BLUNDER_ENGINE_ERROR;
}

int blunder_cine_enter(int suppress_gameplay_input) {
  return cineSegmentService().enter(suppress_gameplay_input != 0)
             ? BLUNDER_ENGINE_OK
             : BLUNDER_ENGINE_ERROR;
}

int blunder_cine_end(void) {
  return cineSegmentService().end() ? BLUNDER_ENGINE_OK : BLUNDER_ENGINE_ERROR;
}

int blunder_cine_is_in_cine(int* out_value) {
  if (out_value == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  *out_value = cineSegmentService().isInCine() ? 1 : 0;
  return BLUNDER_ENGINE_OK;
}

int blunder_cine_is_gameplay_input_suppressed(int* out_value) {
  if (out_value == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  *out_value = cineSegmentService().isGameplayInputSuppressed() ? 1 : 0;
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
  out->animation_player_play_with_fade = &blunder_animation_player_play_with_fade;
  out->animation_player_stop = &blunder_animation_player_stop;
  out->animation_player_set_slot = &blunder_animation_player_set_slot;
  out->animation_player_get_slot = &blunder_animation_player_get_slot;
  out->animation_player_set_blend_weight =
      &blunder_animation_player_set_blend_weight;
  out->animation_player_get_blend_weight =
      &blunder_animation_player_get_blend_weight;
  out->animation_player_set_time_scale = &blunder_animation_player_set_time_scale;
  out->animation_player_get_time_scale = &blunder_animation_player_get_time_scale;
  out->animation_player_set_loop = &blunder_animation_player_set_loop;
  out->animation_player_get_playback_position =
      &blunder_animation_player_get_playback_position;
  out->animation_player_get_clip_length =
      &blunder_animation_player_get_clip_length;
  out->animation_player_add_pose_applied_listener =
      &blunder_animation_player_add_pose_applied_listener;
  out->animation_player_clear_pose_applied_listeners =
      &blunder_animation_player_clear_pose_applied_listeners;
  out->animation_tree_set_active = &blunder_animation_tree_set_active;
  out->animation_tree_get_active = &blunder_animation_tree_get_active;
  out->animation_tree_travel = &blunder_animation_tree_travel;
  out->animation_tree_start = &blunder_animation_tree_start;
  out->animation_tree_set_blend_space_scalar =
      &blunder_animation_tree_set_blend_space_scalar;
  out->animation_tree_get_blend_space_scalar =
      &blunder_animation_tree_get_blend_space_scalar;
  out->animation_tree_request_one_shot = &blunder_animation_tree_request_one_shot;
  out->animation_tree_set_add2_weight = &blunder_animation_tree_set_add2_weight;
  out->animation_tree_get_add2_weight = &blunder_animation_tree_get_add2_weight;
  out->animation_tree_set_blend_space_2d_param =
      &blunder_animation_tree_set_blend_space_2d_param;
  out->animation_tree_get_blend_space_2d_param =
      &blunder_animation_tree_get_blend_space_2d_param;
  out->animation_tree_set_asset_guid = &blunder_animation_tree_set_asset_guid;
  out->animation_tree_get_asset_guid = &blunder_animation_tree_get_asset_guid;
  out->skeleton_modifier_count = &blunder_skeleton_modifier_count;
  out->skeleton_modifier_set_enabled = &blunder_skeleton_modifier_set_enabled;
  out->skeleton_modifier_get_enabled = &blunder_skeleton_modifier_get_enabled;
  out->skeleton_modifier_move = &blunder_skeleton_modifier_move;
  out->skeleton_modifier_set_paper_mouth_open_amount =
      &blunder_skeleton_modifier_set_paper_mouth_open_amount;
  out->skeleton_modifier_get_paper_mouth_open_amount =
      &blunder_skeleton_modifier_get_paper_mouth_open_amount;
  out->skeleton_modifier_set_paper_mouth_bone_name =
      &blunder_skeleton_modifier_set_paper_mouth_bone_name;
  out->skeleton_modifier_get_paper_mouth_bone_name =
      &blunder_skeleton_modifier_get_paper_mouth_bone_name;
  out->skeleton_modifier_set_attach_bone_name =
      &blunder_skeleton_modifier_set_attach_bone_name;
  out->skeleton_modifier_get_attach_bone_name =
      &blunder_skeleton_modifier_get_attach_bone_name;
  out->skeleton_modifier_set_attach_child_object_id =
      &blunder_skeleton_modifier_set_attach_child_object_id;
  out->skeleton_modifier_get_attach_child_object_id =
      &blunder_skeleton_modifier_get_attach_child_object_id;
  out->skeleton_modifier_set_look_at_target =
      &blunder_skeleton_modifier_set_look_at_target;
  out->skeleton_modifier_get_look_at_target =
      &blunder_skeleton_modifier_get_look_at_target;
  out->skeleton_modifier_set_look_at_bone_name =
      &blunder_skeleton_modifier_set_look_at_bone_name;
  out->skeleton_modifier_get_look_at_bone_name =
      &blunder_skeleton_modifier_get_look_at_bone_name;
  out->animation_player_get_method_key_count =
      &blunder_animation_player_get_method_key_count;
  out->animation_player_get_method_key =
      &blunder_animation_player_get_method_key;
  out->sync_group_create = &blunder_sync_group_create;
  out->sync_group_destroy = &blunder_sync_group_destroy;
  out->sync_group_join = &blunder_sync_group_join;
  out->sync_group_leave = &blunder_sync_group_leave;
  out->sync_group_fire = &blunder_sync_group_fire;
  out->sync_group_fire_same_name = &blunder_sync_group_fire_same_name;
  out->sync_group_fire_same_name_seek = &blunder_sync_group_fire_same_name_seek;
  out->cine_enter = &blunder_cine_enter;
  out->cine_end = &blunder_cine_end;
  out->cine_is_in_cine = &blunder_cine_is_in_cine;
  out->cine_is_gameplay_input_suppressed =
      &blunder_cine_is_gameplay_input_suppressed;
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
  BLUNDER_NATIVE_ABI_LOAD(animation_player_play_with_fade,
                          "blunder_animation_player_play_with_fade");
  BLUNDER_NATIVE_ABI_LOAD(animation_player_stop, "blunder_animation_player_stop");
  BLUNDER_NATIVE_ABI_LOAD(animation_player_set_slot,
                          "blunder_animation_player_set_slot");
  BLUNDER_NATIVE_ABI_LOAD(animation_player_get_slot,
                          "blunder_animation_player_get_slot");
  BLUNDER_NATIVE_ABI_LOAD(animation_player_set_blend_weight,
                          "blunder_animation_player_set_blend_weight");
  BLUNDER_NATIVE_ABI_LOAD(animation_player_get_blend_weight,
                          "blunder_animation_player_get_blend_weight");
  BLUNDER_NATIVE_ABI_LOAD(animation_player_set_time_scale,
                          "blunder_animation_player_set_time_scale");
  BLUNDER_NATIVE_ABI_LOAD(animation_player_get_time_scale,
                          "blunder_animation_player_get_time_scale");
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
  BLUNDER_NATIVE_ABI_LOAD(animation_tree_set_active,
                          "blunder_animation_tree_set_active");
  BLUNDER_NATIVE_ABI_LOAD(animation_tree_get_active,
                          "blunder_animation_tree_get_active");
  BLUNDER_NATIVE_ABI_LOAD(animation_tree_travel, "blunder_animation_tree_travel");
  BLUNDER_NATIVE_ABI_LOAD(animation_tree_start, "blunder_animation_tree_start");
  BLUNDER_NATIVE_ABI_LOAD(animation_tree_set_blend_space_scalar,
                          "blunder_animation_tree_set_blend_space_scalar");
  BLUNDER_NATIVE_ABI_LOAD(animation_tree_get_blend_space_scalar,
                          "blunder_animation_tree_get_blend_space_scalar");
  BLUNDER_NATIVE_ABI_LOAD(animation_tree_request_one_shot,
                          "blunder_animation_tree_request_one_shot");
  BLUNDER_NATIVE_ABI_LOAD(animation_tree_set_add2_weight,
                          "blunder_animation_tree_set_add2_weight");
  BLUNDER_NATIVE_ABI_LOAD(animation_tree_get_add2_weight,
                          "blunder_animation_tree_get_add2_weight");
  BLUNDER_NATIVE_ABI_LOAD(animation_tree_set_blend_space_2d_param,
                          "blunder_animation_tree_set_blend_space_2d_param");
  BLUNDER_NATIVE_ABI_LOAD(animation_tree_get_blend_space_2d_param,
                          "blunder_animation_tree_get_blend_space_2d_param");
  BLUNDER_NATIVE_ABI_LOAD(animation_tree_set_asset_guid,
                          "blunder_animation_tree_set_asset_guid");
  BLUNDER_NATIVE_ABI_LOAD(animation_tree_get_asset_guid,
                          "blunder_animation_tree_get_asset_guid");
  BLUNDER_NATIVE_ABI_LOAD(skeleton_modifier_count,
                          "blunder_skeleton_modifier_count");
  BLUNDER_NATIVE_ABI_LOAD(skeleton_modifier_set_enabled,
                          "blunder_skeleton_modifier_set_enabled");
  BLUNDER_NATIVE_ABI_LOAD(skeleton_modifier_get_enabled,
                          "blunder_skeleton_modifier_get_enabled");
  BLUNDER_NATIVE_ABI_LOAD(skeleton_modifier_move,
                          "blunder_skeleton_modifier_move");
  BLUNDER_NATIVE_ABI_LOAD(skeleton_modifier_set_paper_mouth_open_amount,
                          "blunder_skeleton_modifier_set_paper_mouth_open_amount");
  BLUNDER_NATIVE_ABI_LOAD(skeleton_modifier_get_paper_mouth_open_amount,
                          "blunder_skeleton_modifier_get_paper_mouth_open_amount");
  BLUNDER_NATIVE_ABI_LOAD(skeleton_modifier_set_paper_mouth_bone_name,
                          "blunder_skeleton_modifier_set_paper_mouth_bone_name");
  BLUNDER_NATIVE_ABI_LOAD(skeleton_modifier_get_paper_mouth_bone_name,
                          "blunder_skeleton_modifier_get_paper_mouth_bone_name");
  BLUNDER_NATIVE_ABI_LOAD(skeleton_modifier_set_attach_bone_name,
                          "blunder_skeleton_modifier_set_attach_bone_name");
  BLUNDER_NATIVE_ABI_LOAD(skeleton_modifier_get_attach_bone_name,
                          "blunder_skeleton_modifier_get_attach_bone_name");
  BLUNDER_NATIVE_ABI_LOAD(skeleton_modifier_set_attach_child_object_id,
                          "blunder_skeleton_modifier_set_attach_child_object_id");
  BLUNDER_NATIVE_ABI_LOAD(skeleton_modifier_get_attach_child_object_id,
                          "blunder_skeleton_modifier_get_attach_child_object_id");
  BLUNDER_NATIVE_ABI_LOAD(skeleton_modifier_set_look_at_target,
                          "blunder_skeleton_modifier_set_look_at_target");
  BLUNDER_NATIVE_ABI_LOAD(skeleton_modifier_get_look_at_target,
                          "blunder_skeleton_modifier_get_look_at_target");
  BLUNDER_NATIVE_ABI_LOAD(skeleton_modifier_set_look_at_bone_name,
                          "blunder_skeleton_modifier_set_look_at_bone_name");
  BLUNDER_NATIVE_ABI_LOAD(skeleton_modifier_get_look_at_bone_name,
                          "blunder_skeleton_modifier_get_look_at_bone_name");
  BLUNDER_NATIVE_ABI_LOAD(animation_player_get_method_key_count,
                          "blunder_animation_player_get_method_key_count");
  BLUNDER_NATIVE_ABI_LOAD(animation_player_get_method_key,
                          "blunder_animation_player_get_method_key");
  BLUNDER_NATIVE_ABI_LOAD(sync_group_create, "blunder_sync_group_create");
  BLUNDER_NATIVE_ABI_LOAD(sync_group_destroy, "blunder_sync_group_destroy");
  BLUNDER_NATIVE_ABI_LOAD(sync_group_join, "blunder_sync_group_join");
  BLUNDER_NATIVE_ABI_LOAD(sync_group_leave, "blunder_sync_group_leave");
  BLUNDER_NATIVE_ABI_LOAD(sync_group_fire, "blunder_sync_group_fire");
  BLUNDER_NATIVE_ABI_LOAD(sync_group_fire_same_name,
                          "blunder_sync_group_fire_same_name");
  BLUNDER_NATIVE_ABI_LOAD(sync_group_fire_same_name_seek,
                          "blunder_sync_group_fire_same_name_seek");
  BLUNDER_NATIVE_ABI_LOAD(cine_enter, "blunder_cine_enter");
  BLUNDER_NATIVE_ABI_LOAD(cine_end, "blunder_cine_end");
  BLUNDER_NATIVE_ABI_LOAD(cine_is_in_cine, "blunder_cine_is_in_cine");
  BLUNDER_NATIVE_ABI_LOAD(cine_is_gameplay_input_suppressed,
                          "blunder_cine_is_gameplay_input_suppressed");

#undef BLUNDER_NATIVE_ABI_LOAD
  return BLUNDER_ENGINE_OK;
}

}  // extern "C"
