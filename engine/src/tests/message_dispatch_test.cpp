#include "runtime/core/object/object_db.h"
#include "runtime/core/reflection/message_dispatch.h"

#include <cstdio>
#include <vector>

namespace {
int g_failures = 0;
void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

struct Call {
  void* peer;
  Blunder::MessageId id;
  int argc;
  Blunder::MessageArg args[4];
};
std::vector<Call> g_calls;

void Hook(void* peer, Blunder::MessageId id, const Blunder::MessageArg* args,
          int argc) {
  Call c{};
  c.peer = peer;
  c.id = id;
  c.argc = argc;
  for (int i = 0; i < argc && i < 4; ++i) {
    c.args[i] = args[i];
  }
  g_calls.push_back(c);
}
}  // namespace

int main() {
  Blunder::ObjectDB::clear();
  Blunder::MessageDispatch::clear();

  const Blunder::MessageId hit = Blunder::MessageDispatch::registerName("Hit");
  const Blunder::MessageId hit2 = Blunder::MessageDispatch::registerName("Hit");
  const Blunder::MessageId heal = Blunder::MessageDispatch::registerName("Heal");
  expect_true("register non-zero", hit != 0);
  expect_true("register stable", hit == hit2);
  expect_true("distinct names", hit != heal);

  Blunder::MessageDispatch::setHook(&Hook);
  const Blunder::ObjectId a = Blunder::ObjectDB::create();
  Blunder::Object* obj = Blunder::ObjectDB::get(a);
  expect_true("object", obj != nullptr);
  const Blunder::BehaviourId b0 = obj->addBehaviour("A");
  const Blunder::BehaviourId b1 = obj->addBehaviour("B");
  const Blunder::BehaviourId b2 = obj->addBehaviour("C");
  obj->setBehaviourScriptPeer(b0, reinterpret_cast<void*>(1));
  obj->setBehaviourScriptPeer(b1, nullptr);
  obj->setBehaviourScriptPeer(b2, reinterpret_cast<void*>(3));

  Blunder::MessageArg args[2];
  args[0].kind = Blunder::MessageArgKind::Int;
  args[0].i = 42;
  args[1].kind = Blunder::MessageArgKind::ObjectId;
  args[1].object_id = a;

  g_calls.clear();
  expect_true("send ok",
              Blunder::MessageDispatch::send(a, hit, args, 2));
  expect_true("two peers", g_calls.size() == 2);
  expect_true("order peer1", g_calls[0].peer == reinterpret_cast<void*>(1));
  expect_true("order peer3", g_calls[1].peer == reinterpret_cast<void*>(3));
  expect_true("argc", g_calls[0].argc == 2 && g_calls[0].args[0].i == 42);

  g_calls.clear();
  expect_true("invalid target ok",
              Blunder::MessageDispatch::send(0, hit, nullptr, 0));
  expect_true("invalid no calls", g_calls.empty());

  Blunder::MessageArg too_many[5]{};
  expect_true("argc 5 fails",
              !Blunder::MessageDispatch::send(a, hit, too_many, 5));
  expect_true("zero id fails",
              !Blunder::MessageDispatch::send(a, 0, nullptr, 0));

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
