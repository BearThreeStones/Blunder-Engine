#pragma once

#include <cstdint>

#include "runtime/core/object/object_id.h"

namespace Blunder {

using MessageId = uint32_t;
inline constexpr MessageId k_invalid_message_id = 0;

enum class MessageArgKind : uint8_t {
  Nil = 0,
  Bool = 1,
  Int = 2,
  Float = 3,
  ObjectId = 4,
};

struct MessageArg {
  MessageArgKind kind{MessageArgKind::Nil};
  union {
    bool b;
    int64_t i;
    float f;
    ObjectId object_id;
  };
};

using MessageHookFn = void (*)(void* script_peer, MessageId id,
                               const MessageArg* args, int argc);

class MessageDispatch {
 public:
  static void clear();
  static MessageId registerName(const char* name);
  static void setHook(MessageHookFn fn);
  /// Returns false if id==0 or argc not in [0,4]. Invalid ObjectId is success no-op.
  static bool send(ObjectId target, MessageId id, const MessageArg* args,
                   int argc);
};

}  // namespace Blunder
