#include "runtime/core/reflection/message_dispatch.h"

#include "EASTL/string.h"
#include "EASTL/unordered_map.h"
#include "EASTL/vector.h"

#include "runtime/core/object/behaviour_id.h"
#include "runtime/core/object/object.h"
#include "runtime/core/object/object_db.h"

namespace Blunder {
namespace {

eastl::unordered_map<eastl::string, MessageId>& nameToId() {
  static eastl::unordered_map<eastl::string, MessageId> s_map;
  return s_map;
}

MessageId& nextId() {
  static MessageId s_next = 1;
  return s_next;
}

MessageHookFn& hook() {
  static MessageHookFn s_hook = nullptr;
  return s_hook;
}

}  // namespace

void MessageDispatch::clear() {
  nameToId().clear();
  nextId() = 1;
  hook() = nullptr;
}

MessageId MessageDispatch::registerName(const char* name) {
  if (name == nullptr) {
    return k_invalid_message_id;
  }
  const eastl::string key(name);
  const auto it = nameToId().find(key);
  if (it != nameToId().end()) {
    return it->second;
  }
  const MessageId id = nextId()++;
  nameToId()[key] = id;
  return id;
}

void MessageDispatch::setHook(MessageHookFn fn) { hook() = fn; }

bool MessageDispatch::send(ObjectId target, MessageId id,
                           const MessageArg* args, int argc) {
  if (argc < 0 || argc > 4 || id == k_invalid_message_id) {
    return false;
  }

  Object* object = ObjectDB::get(target);
  if (object == nullptr) {
    return true;
  }

  MessageHookFn fn = hook();
  if (fn == nullptr) {
    return true;
  }

  const size_t n = object->getBehaviourCount();
  eastl::vector<BehaviourId> behaviour_ids;
  behaviour_ids.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    behaviour_ids.push_back(object->getBehaviourIdAt(i));
  }

  for (BehaviourId behaviour_id : behaviour_ids) {
    object = ObjectDB::get(target);
    if (object == nullptr) {
      break;
    }
    void* peer = object->getBehaviourScriptPeer(behaviour_id);
    if (peer == nullptr) {
      continue;
    }
    fn(peer, id, args, argc);
  }

  return true;
}

}  // namespace Blunder
