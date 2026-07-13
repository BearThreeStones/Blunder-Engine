#include "runtime/function/scene/scene_instance.h"

#include <cstdio>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

}  // namespace

int main() {
  using namespace Blunder;

  SceneInstance scene;
  const EntityId a =
      scene.createEntity("A", Vec3(1, 0, 0), glm::identity<Quat>(), Vec3(1));
  const EntityId b =
      scene.createEntity("B", Vec3(0, 1, 0), glm::identity<Quat>(), Vec3(1));
  expect_true("created two entities", scene.getEntityCount() == 2);
  expect_true("a valid", isValid(a));
  expect_true("b valid", isValid(b));

  expect_true("softDelete succeeds", scene.softDeleteEntity(a));
  expect_true("a still resolvable", scene.getEntity(a) != nullptr);
  expect_true("a is tombstoned", scene.isTombstoned(a));
  expect_true("b not tombstoned", !scene.isTombstoned(b));
  expect_true("entity count unchanged", scene.getEntityCount() == 2);
  expect_true("b id unchanged", scene.getEntityIdAtIndex(1) == b);

  expect_true("restore succeeds", scene.restoreEntity(a));
  expect_true("a not tombstoned after restore", !scene.isTombstoned(a));

  scene.softDeleteEntity(a);
  Scene exported;
  expect_true("export ok", scene.exportToScene(exported));
  expect_true("export omits tombstone", exported.getEntities().size() == 1);
  expect_true("exported entity is B",
              exported.getEntities()[0].name == eastl::string("B"));

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
