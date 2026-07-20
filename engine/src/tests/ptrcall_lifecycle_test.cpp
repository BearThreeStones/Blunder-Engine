#include "runtime/core/object/object_db.h"
#include "runtime/core/reflection/class_db.h"
#include "runtime/core/reflection/lifecycle.h"

#include <cstdio>

namespace {

int g_failures = 0;
int g_tick_calls = 0;
float g_last_dt = 0.0f;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

void on_tick(void* peer, float dt) {
  ++g_tick_calls;
  g_last_dt = dt;
  *static_cast<int*>(peer) += 1;
}

class SetNameBind final : public Blunder::MethodBind {
 public:
  const char* getName() const override { return "set_name"; }
  void ptrcall(void* instance, const void** args, void* /*ret*/) override {
    auto* object = static_cast<Blunder::Object*>(instance);
    const auto* name = static_cast<const eastl::string*>(args[0]);
    object->setName(*name);
  }
};

}  // namespace

int main() {
  using namespace Blunder;

  ClassDB::clear();
  ObjectDB::clear();
  LifecycleDispatch::clear();

  ClassDB::registerClass("Object");
  ClassDB::addMethod("Object", new SetNameBind());

  ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  expect_true("object", object != nullptr);

  MethodBind* bind = ClassDB::getMethod("Object", "set_name");
  expect_true("method bind", bind != nullptr);
  if (bind != nullptr && object != nullptr) {
    eastl::string name = "Hero";
    const void* args[1] = {&name};
    bind->ptrcall(object, args, nullptr);
    expect_true("ptrcall set_name", object->getName() == "Hero");
  }

  ClassDB::addProperty(
      "Object", PropertyInfo{"enabled", VariantType::Bool},
      [](void* instance, const Variant& value) {
        static_cast<Object*>(instance)->setEnabled(value.asBool());
      },
      [](const void* instance) {
        return Variant(static_cast<const Object*>(instance)->isEnabled());
      });

  if (object != nullptr) {
    expect_true("set enabled via Variant",
                ClassDB::setProperty(object, "Object", "enabled",
                                     Variant(false)));
    expect_true("enabled false", !object->isEnabled());
    Variant out;
    expect_true("get enabled via Variant",
                ClassDB::getProperty(object, "Object", "enabled", out));
    expect_true("enabled variant false", out.asBool() == false);
    object->setEnabled(true);
  }

  int peer = 0;
  if (object != nullptr) {
    expect_true("no tick without peer", true);
    LifecycleDispatch::setTickHook("Object", on_tick);
    LifecycleDispatch::invokeTick(object, 0.016f);
    expect_true("skip tick without peer", g_tick_calls == 0);

    object->addBehaviour("Object");
    object->setBehaviourScriptPeer(object->getBehaviourIdAt(0), &peer);
    LifecycleDispatch::invokeTick(object, 0.016f);
    expect_true("tick with peer", g_tick_calls == 1);
    expect_true("dt forwarded", g_last_dt == 0.016f);
    expect_true("peer mutated", peer == 1);

    LifecycleDispatch::clear();
    LifecycleDispatch::invokeTick(object, 0.1f);
    expect_true("hooks cleared", g_tick_calls == 1);
  }

  ObjectDB::clear();
  ClassDB::clear();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
