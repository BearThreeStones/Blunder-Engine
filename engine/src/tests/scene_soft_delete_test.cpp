#include "runtime/function/scene/scene.h"
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

void softDeleteOmitsTombstonesOnExport() {
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
}

/// Instantiated mesh Asset Reference (GUID) must round-trip through exportToScene
/// so editor saveActiveScene can persist mesh GUID refs.
void exportPreservesMeshGuidReference() {
  using namespace Blunder;

  const char* kMeshGuid = "11111111-2222-4333-8444-555555555555";

  Scene source;
  SceneEntityDefinition definition;
  definition.name = "Cube";
  definition.mesh_virtual_path = kMeshGuid;
  source.getEntities().push_back(eastl::move(definition));

  SceneInstance instance;
  instance.instantiate(source);

  Scene exported;
  expect_true("export with mesh guid ok", instance.exportToScene(exported));
  expect_true("export has one entity", exported.getEntities().size() == 1);
  expect_true("export preserves mesh guid",
              exported.getEntities()[0].mesh_virtual_path == kMeshGuid);
}

/// Legacy path mesh refs must also survive instantiate → export.
void exportPreservesMeshPathReference() {
  using namespace Blunder;

  const char* kMeshPath = "assets/Meshes/Cube.mesh.yaml";

  Scene source;
  SceneEntityDefinition definition;
  definition.name = "Cube";
  definition.mesh_virtual_path = kMeshPath;
  source.getEntities().push_back(eastl::move(definition));

  SceneInstance instance;
  instance.instantiate(source);

  Scene exported;
  expect_true("export with mesh path ok", instance.exportToScene(exported));
  expect_true("export path entity count", exported.getEntities().size() == 1);
  expect_true("export preserves mesh path",
              exported.getEntities()[0].mesh_virtual_path == kMeshPath);
}

}  // namespace

int main() {
  softDeleteOmitsTombstonesOnExport();
  exportPreservesMeshGuidReference();
  exportPreservesMeshPathReference();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::fprintf(stdout, "scene_soft_delete_test: all passed\n");
  return 0;
}
