#include "runtime/function/editor/selection_focus_bounds.h"

#include <cstdio>

#include <glm/gtc/quaternion.hpp>

#include "runtime/function/scene/character_controller_component.h"
#include "runtime/function/scene/collider_component.h"
#include "runtime/function/scene/scene_instance.h"

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
  const EntityId dog = scene.createEntity(
      "Chocomel", Vec3(12.0f, -4.0f, 0.0f), glm::identity<Quat>(), Vec3(1.0f));
  CharacterControllerComponent cct{};
  cct.radius = 0.75f;
  cct.height = 1.5f;
  scene.setCharacterController(dog, cct);
  scene.ensureWorldMatrices();

  AABB dog_bounds{};
  expect_true("Chocomel CCT produces focus bounds",
              computeEntityFocusBounds(scene, dog, dog_bounds));
  const Vec3 dog_size = dog_bounds.size();
  expect_true("Chocomel focus AABB is dog-scale not world-scale",
              dog_size.x < 3.0f && dog_size.y < 3.0f && dog_size.z < 3.0f);
  expect_true("Chocomel focus AABB center near entity",
              glm::length(dog_bounds.center() - Vec3(12.0f, -4.0f, 0.75f)) <
                  1.5f);

  const EntityId parent = scene.createEntity(
      "Group", Vec3(0.0f, 0.0f, 0.0f), glm::identity<Quat>(), Vec3(1.0f));
  const EntityId child = scene.createEntity(
      "ChildCollider", Vec3(3.0f, 0.0f, 1.0f), glm::identity<Quat>(), Vec3(1.0f),
      parent);
  ColliderComponent sphere{};
  sphere.shape = ColliderShapeKind::Sphere;
  sphere.sphere_radius = 0.5f;
  scene.setCollider(child, sphere);
  scene.ensureWorldMatrices();

  AABB parent_bounds{};
  expect_true("Parent selection includes descendant collider",
              computeEntityFocusBounds(scene, parent, parent_bounds));
  expect_true("Parent focus AABB covers child sphere",
              parent_bounds.contains(Vec3(3.0f, 0.0f, 1.0f)));

  AABB empty_bounds{};
  const EntityId empty = scene.createEntity(
      "Empty", Vec3(8.0f, 2.0f, 1.0f), glm::identity<Quat>(), Vec3(1.0f));
  scene.ensureWorldMatrices();
  expect_true("Transform-only entity still frames",
              computeEntityFocusBounds(scene, empty, empty_bounds));
  expect_true("Empty entity pad is small",
              empty_bounds.size().x <= kSelectionFocusEmptyHalfExtent * 2.1f);

  eastl::vector<EntityId> selected{dog};
  AABB selection{};
  expect_true("Selection helper returns dog bounds",
              computeSelectionFocusBounds(scene, selected, selection));
  expect_true("Selection AABB matches entity AABB",
              glm::length(selection.center() - dog_bounds.center()) < 1e-3f &&
                  glm::length(selection.size() - dog_bounds.size()) < 1e-3f);

  // World-scale scene must not leak into dog selection bounds.
  scene.setWorldBounds(AABB{Vec3(-200.0f), Vec3(200.0f)});
  AABB again{};
  expect_true("Selection ignores scene world bounds",
              computeSelectionFocusBounds(scene, selected, again));
  expect_true("Selection stays dog-scale with huge world bounds set",
              again.size().x < 3.0f && again.size().y < 3.0f);

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::fprintf(stderr, "selection_focus_bounds_test: all passed\n");
  return 0;
}
