#include <cstdio>

#include <glm/geometric.hpp>

#include "runtime/core/math/geometry.h"
#include "runtime/function/editor/ground_placement.h"
#include "runtime/function/editor/placement_preview_controller.h"
#include "runtime/resource/content_browser/content_browser_drop.h"

namespace {

int g_failures = 0;

void expect_true(const char* label, bool value) {
  if (!value) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

bool vecNear(const Blunder::Vec3& a, const Blunder::Vec3& b, float eps = 1e-4f) {
  return glm::length(a - b) <= eps;
}

}  // namespace

int main() {
  using namespace Blunder;

  {
    const Ray ray{Vec3(0.0f, 0.0f, 10.0f), Vec3(0.0f, 0.0f, -1.0f)};
    expect_true("downward ray hits origin on Z=0",
                vecNear(groundPlacementFromRay(ray), Vec3(0.0f)));
  }

  {
    const Ray ray{Vec3(3.0f, -2.0f, 8.0f), Vec3(0.0f, 0.0f, -1.0f)};
    expect_true("downward ray hits XY on Z=0",
                vecNear(groundPlacementFromRay(ray), Vec3(3.0f, -2.0f, 0.0f)));
  }

  {
    const Ray ray{Vec3(0.0f, 0.0f, 1.0f), Vec3(1.0f, 0.0f, 0.0f)};
    expect_true("parallel miss uses origin",
                vecNear(groundPlacementFromRay(ray), Vec3(0.0f)));
  }

  {
    const Ray ray{Vec3(0.0f, 0.0f, 1.0f), Vec3(0.0f, 0.0f, 1.0f)};
    expect_true("away-from-ground miss uses origin",
                vecNear(groundPlacementFromRay(ray), Vec3(0.0f)));
  }

  expect_true("mesh.yaml is mesh",
              classifyContentBrowserDrop("assets/Meshes/box.mesh.yaml") ==
                  ContentBrowserDropKind::mesh);
  expect_true("mesh.asset is mesh",
              classifyContentBrowserDrop("assets/Meshes/box.mesh.asset") ==
                  ContentBrowserDropKind::mesh);
  expect_true("scene.asset is scene",
              classifyContentBrowserDrop("assets/Scenes/root.scene.asset") ==
                  ContentBrowserDropKind::scene);
  expect_true("texture is other",
              classifyContentBrowserDrop("assets/Textures/a.texture.yaml") ==
                  ContentBrowserDropKind::other);
  expect_true("empty path is other",
              classifyContentBrowserDrop("") == ContentBrowserDropKind::other);

  expect_true(
      "idle drag keeps default cursor",
      resolveContentBrowserDragCursor(false, true, false,
                                      ContentBrowserDropKind::mesh) ==
          ContentBrowserDragCursorKind::default_arrow);
  expect_true(
      "mesh over viewport is pointer",
      resolveContentBrowserDragCursor(true, true, false,
                                      ContentBrowserDropKind::mesh) ==
          ContentBrowserDragCursorKind::pointer);
  expect_true(
      "scene over viewport is pointer",
      resolveContentBrowserDragCursor(true, true, false,
                                      ContentBrowserDropKind::scene) ==
          ContentBrowserDragCursorKind::pointer);
  expect_true(
      "texture over viewport is not-allowed",
      resolveContentBrowserDragCursor(true, true, false,
                                      ContentBrowserDropKind::other) ==
          ContentBrowserDragCursorKind::not_allowed);
  expect_true(
      "folder hover is move",
      resolveContentBrowserDragCursor(true, false, true,
                                      ContentBrowserDropKind::mesh) ==
          ContentBrowserDragCursorKind::move);
  expect_true(
      "inspector chrome is not-allowed",
      resolveContentBrowserDragCursor(true, false, false,
                                      ContentBrowserDropKind::mesh) ==
          ContentBrowserDragCursorKind::not_allowed);
  expect_true(
      "viewport wins over folder for mesh",
      resolveContentBrowserDragCursor(true, true, true,
                                      ContentBrowserDropKind::mesh) ==
          ContentBrowserDragCursorKind::pointer);

  {
    PlacementPreviewController preview;
    preview.setSourcePath("assets/Meshes/box.mesh.yaml");
    expect_true("preview hidden until over viewport", !preview.isVisible());
    preview.setPointerOverViewport(true);
    preview.setGroundPosition(Vec3(4.0f, 1.0f, 0.0f));
    expect_true("mesh over viewport is visible", preview.isVisible());
    expect_true("preview pose tracks ground placement",
                vecNear(preview.groundPosition(), Vec3(4.0f, 1.0f, 0.0f)));
  }

  {
    PlacementPreviewController preview;
    preview.setSourcePath("assets/Scenes/root.scene.asset");
    preview.setPointerOverViewport(true);
    expect_true("scene drag has no placement preview", !preview.isVisible());
  }

  {
    PlacementPreviewController preview;
    preview.setSourcePath("assets/Meshes/box.mesh.yaml");
    preview.setPointerOverViewport(true);
    expect_true("preview visible before leave", preview.isVisible());
    preview.setPointerOverViewport(false);
    expect_true("preview hides over browser", !preview.isVisible());
  }

  {
    PlacementPreviewController preview;
    preview.setSourcePath("assets/Meshes/box.mesh.yaml");
    preview.setPointerOverViewport(true);
    preview.clear();
    expect_true("clear hides preview", !preview.isVisible());
    expect_true("clear drops source path", preview.sourcePath().empty());
  }

  if (g_failures == 0) {
    std::printf("placement_preview_test: all passed\n");
    return 0;
  }
  std::fprintf(stderr, "placement_preview_test: %d failure(s)\n", g_failures);
  return 1;
}
