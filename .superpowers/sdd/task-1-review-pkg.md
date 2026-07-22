HEAD=fb8bd741942027faa53e02bd90d2ec43c31fa0ee
diff --git a/engine/src/runtime/CMakeLists.txt b/engine/src/runtime/CMakeLists.txt
index 823345d..13342a0 100644
--- a/engine/src/runtime/CMakeLists.txt
+++ b/engine/src/runtime/CMakeLists.txt
@@ -82,20 +82,22 @@ add_library(engine_runtime STATIC
     "core/layer/layer_stack.h"
     "core/layer/layer_stack.cpp"
     "core/reflection/export_macros.h"
     "core/reflection/variant.h"
     "core/reflection/variant.cpp"
     "core/reflection/type_info.h"
     "core/reflection/class_db.h"
     "core/reflection/class_db.cpp"
     "core/reflection/lifecycle.h"
     "core/reflection/lifecycle.cpp"
+    "core/reflection/message_dispatch.h"
+    "core/reflection/message_dispatch.cpp"
     "core/reflection/engine_c_abi.h"
     "core/reflection/generated/register_generated.h"
     "core/reflection/generated/object.gen.cpp"
     "core/object/object_id.h"
     "core/object/behaviour_id.h"
     "core/object/entity_store.h"
     "core/object/object.h"
     "core/object/object.cpp"
     "core/object/object_db.h"
     "core/object/object_db.cpp"
diff --git a/engine/src/runtime/core/reflection/message_dispatch.cpp b/engine/src/runtime/core/reflection/message_dispatch.cpp
new file mode 100644
index 0000000..32af235
--- /dev/null
+++ b/engine/src/runtime/core/reflection/message_dispatch.cpp
@@ -0,0 +1,91 @@
+#include "runtime/core/reflection/message_dispatch.h"
+
+#include "EASTL/string.h"
+#include "EASTL/unordered_map.h"
+#include "EASTL/vector.h"
+
+#include "runtime/core/object/behaviour_id.h"
+#include "runtime/core/object/object.h"
+#include "runtime/core/object/object_db.h"
+
+namespace Blunder {
+namespace {
+
+eastl::unordered_map<eastl::string, MessageId>& nameToId() {
+  static eastl::unordered_map<eastl::string, MessageId> s_map;
+  return s_map;
+}
+
+MessageId& nextId() {
+  static MessageId s_next = 1;
+  return s_next;
+}
+
+MessageHookFn& hook() {
+  static MessageHookFn s_hook = nullptr;
+  return s_hook;
+}
+
+}  // namespace
+
+void MessageDispatch::clear() {
+  nameToId().clear();
+  nextId() = 1;
+  hook() = nullptr;
+}
+
+MessageId MessageDispatch::registerName(const char* name) {
+  if (name == nullptr) {
+    return k_invalid_message_id;
+  }
+  const eastl::string key(name);
+  const auto it = nameToId().find(key);
+  if (it != nameToId().end()) {
+    return it->second;
+  }
+  const MessageId id = nextId()++;
+  nameToId()[key] = id;
+  return id;
+}
+
+void MessageDispatch::setHook(MessageHookFn fn) { hook() = fn; }
+
+bool MessageDispatch::send(ObjectId target, MessageId id,
+                           const MessageArg* args, int argc) {
+  if (argc < 0 || argc > 4 || id == k_invalid_message_id) {
+    return false;
+  }
+
+  Object* object = ObjectDB::get(target);
+  if (object == nullptr) {
+    return true;
+  }
+
+  MessageHookFn fn = hook();
+  if (fn == nullptr) {
+    return true;
+  }
+
+  const size_t n = object->getBehaviourCount();
+  eastl::vector<BehaviourId> behaviour_ids;
+  behaviour_ids.reserve(n);
+  for (size_t i = 0; i < n; ++i) {
+    behaviour_ids.push_back(object->getBehaviourIdAt(i));
+  }
+
+  for (BehaviourId behaviour_id : behaviour_ids) {
+    object = ObjectDB::get(target);
+    if (object == nullptr) {
+      break;
+    }
+    void* peer = object->getBehaviourScriptPeer(behaviour_id);
+    if (peer == nullptr) {
+      continue;
+    }
+    fn(peer, id, args, argc);
+  }
+
+  return true;
+}
+
+}  // namespace Blunder
diff --git a/engine/src/runtime/core/reflection/message_dispatch.h b/engine/src/runtime/core/reflection/message_dispatch.h
new file mode 100644
index 0000000..4a085d6
--- /dev/null
+++ b/engine/src/runtime/core/reflection/message_dispatch.h
@@ -0,0 +1,43 @@
+#pragma once
+
+#include <cstdint>
+
+#include "runtime/core/object/object_id.h"
+
+namespace Blunder {
+
+using MessageId = uint32_t;
+inline constexpr MessageId k_invalid_message_id = 0;
+
+enum class MessageArgKind : uint8_t {
+  Nil = 0,
+  Bool = 1,
+  Int = 2,
+  Float = 3,
+  ObjectId = 4,
+};
+
+struct MessageArg {
+  MessageArgKind kind{MessageArgKind::Nil};
+  union {
+    bool b;
+    int64_t i;
+    float f;
+    ObjectId object_id;
+  };
+};
+
+using MessageHookFn = void (*)(void* script_peer, MessageId id,
+                               const MessageArg* args, int argc);
+
+class MessageDispatch {
+ public:
+  static void clear();
+  static MessageId registerName(const char* name);
+  static void setHook(MessageHookFn fn);
+  /// Returns false if id==0 or argc not in [0,4]. Invalid ObjectId is success no-op.
+  static bool send(ObjectId target, MessageId id, const MessageArg* args,
+                   int argc);
+};
+
+}  // namespace Blunder
diff --git a/engine/src/tests/CMakeLists.txt b/engine/src/tests/CMakeLists.txt
index 5695597..7c89cc1 100644
--- a/engine/src/tests/CMakeLists.txt
+++ b/engine/src/tests/CMakeLists.txt
@@ -161,20 +161,39 @@ if(MSVC)
     target_compile_options(ptrcall_lifecycle_test PRIVATE /Zc:preprocessor)
 endif()
 
 target_precompile_headers(ptrcall_lifecycle_test
     REUSE_FROM engine_runtime
 )
 
 add_test(NAME ptrcall_lifecycle_test
          COMMAND ptrcall_lifecycle_test)
 
+add_executable(message_dispatch_test
+    "message_dispatch_test.cpp"
+)
+
+target_link_libraries(message_dispatch_test
+    PRIVATE engine_runtime
+)
+
+if(MSVC)
+    target_compile_options(message_dispatch_test PRIVATE /Zc:preprocessor)
+endif()
+
+target_precompile_headers(message_dispatch_test
+    REUSE_FROM engine_runtime
+)
+
+add_test(NAME message_dispatch_test
+         COMMAND message_dispatch_test)
+
 add_executable(engine_c_abi_test
     "engine_c_abi_test.cpp"
 )
 
 target_link_libraries(engine_c_abi_test
     PRIVATE blunder_engine_c_static
 )
 
 if(MSVC)
     target_compile_options(engine_c_abi_test PRIVATE /Zc:preprocessor)
diff --git a/engine/src/tests/message_dispatch_test.cpp b/engine/src/tests/message_dispatch_test.cpp
new file mode 100644
index 0000000..5e8485e
--- /dev/null
+++ b/engine/src/tests/message_dispatch_test.cpp
@@ -0,0 +1,89 @@
+#include "runtime/core/object/object_db.h"
+#include "runtime/core/reflection/message_dispatch.h"
+
+#include <cstdio>
+#include <vector>
+
+namespace {
+int g_failures = 0;
+void expect_true(const char* label, bool ok) {
+  if (!ok) {
+    std::fprintf(stderr, "FAIL %s\n", label);
+    ++g_failures;
+  }
+}
+
+struct Call {
+  void* peer;
+  Blunder::MessageId id;
+  int argc;
+  Blunder::MessageArg args[4];
+};
+std::vector<Call> g_calls;
+
+void Hook(void* peer, Blunder::MessageId id, const Blunder::MessageArg* args,
+          int argc) {
+  Call c{};
+  c.peer = peer;
+  c.id = id;
+  c.argc = argc;
+  for (int i = 0; i < argc && i < 4; ++i) {
+    c.args[i] = args[i];
+  }
+  g_calls.push_back(c);
+}
+}  // namespace
+
+int main() {
+  Blunder::ObjectDB::clear();
+  Blunder::MessageDispatch::clear();
+
+  const Blunder::MessageId hit = Blunder::MessageDispatch::registerName("Hit");
+  const Blunder::MessageId hit2 = Blunder::MessageDispatch::registerName("Hit");
+  const Blunder::MessageId heal = Blunder::MessageDispatch::registerName("Heal");
+  expect_true("register non-zero", hit != 0);
+  expect_true("register stable", hit == hit2);
+  expect_true("distinct names", hit != heal);
+
+  Blunder::MessageDispatch::setHook(&Hook);
+  const Blunder::ObjectId a = Blunder::ObjectDB::create();
+  Blunder::Object* obj = Blunder::ObjectDB::get(a);
+  expect_true("object", obj != nullptr);
+  const Blunder::BehaviourId b0 = obj->addBehaviour("A");
+  const Blunder::BehaviourId b1 = obj->addBehaviour("B");
+  const Blunder::BehaviourId b2 = obj->addBehaviour("C");
+  obj->setBehaviourScriptPeer(b0, reinterpret_cast<void*>(1));
+  obj->setBehaviourScriptPeer(b1, nullptr);
+  obj->setBehaviourScriptPeer(b2, reinterpret_cast<void*>(3));
+
+  Blunder::MessageArg args[2];
+  args[0].kind = Blunder::MessageArgKind::Int;
+  args[0].i = 42;
+  args[1].kind = Blunder::MessageArgKind::ObjectId;
+  args[1].object_id = a;
+
+  g_calls.clear();
+  expect_true("send ok",
+              Blunder::MessageDispatch::send(a, hit, args, 2));
+  expect_true("two peers", g_calls.size() == 2);
+  expect_true("order peer1", g_calls[0].peer == reinterpret_cast<void*>(1));
+  expect_true("order peer3", g_calls[1].peer == reinterpret_cast<void*>(3));
+  expect_true("argc", g_calls[0].argc == 2 && g_calls[0].args[0].i == 42);
+
+  g_calls.clear();
+  expect_true("invalid target ok",
+              Blunder::MessageDispatch::send(0, hit, nullptr, 0));
+  expect_true("invalid no calls", g_calls.empty());
+
+  Blunder::MessageArg too_many[5]{};
+  expect_true("argc 5 fails",
+              !Blunder::MessageDispatch::send(a, hit, too_many, 5));
+  expect_true("zero id fails",
+              !Blunder::MessageDispatch::send(a, 0, nullptr, 0));
+
+  if (g_failures != 0) {
+    std::fprintf(stderr, "%d failure(s)\n", g_failures);
+    return 1;
+  }
+  return 0;
+}

