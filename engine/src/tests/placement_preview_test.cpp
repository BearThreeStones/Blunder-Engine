#include <cmath>
#include <cstdio>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "runtime/core/math/coordinate_system.h"
#include "runtime/core/math/geometry.h"
#include "runtime/function/editor/ground_placement.h"
#include "runtime/function/editor/placement_preview_controller.h"
#include "runtime/function/render/viewport_unproject.h"
#include "runtime/resource/content_browser/content_browser_drop.h"

namespace {

int g_failures = 0;

void expect_true(const char* label, bool value) {
  if (!value) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

void expect_near(float actual, float expected, const char* label,
                 float eps = 1e-4f) {
  if (std::fabs(actual - expected) > eps) {
    std::fprintf(stderr, "FAIL %s: expected %.6f got %.6f\n", label, expected,
                 actual);
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

  {
    const float vp_w = 800.0f;
    const float vp_h = 450.0f;
    const Vec2 top = viewportLocalToClipNdc(Vec2(400.0f, 0.0f), vp_w, vp_h);
    const Vec2 bottom = viewportLocalToClipNdc(Vec2(400.0f, vp_h), vp_w, vp_h);
    expect_near(top.x, 0.0f, "top-center NDC x");
    expect_near(top.y, -1.0f, "Vulkan top of image is NDC y=-1");
    expect_near(bottom.y, 1.0f, "Vulkan bottom of image is NDC y=+1");

    const Vec3 eye(12.0f, -12.0f, 9.0f);
    const Vec3 target(0.0f);
    const Vec3 world_up(0.0f, 0.0f, 1.0f);
    Mat4 projection =
        glm::perspectiveZO(glm::radians(45.0f), vp_w / vp_h, 0.1f, 1000.0f);
    projection[1][1] *= -1.0f;
    const Mat4 view = glm::lookAt(eye, target, world_up);

    const Vec3 ground_point(1.5f, -2.0f, 0.0f);
    const Vec4 clip = projection * view * Vec4(ground_point, 1.0f);
    expect_true("ground point is in front of camera", clip.w > 1e-4f);
    const Vec2 projected_ndc(clip.x / clip.w, clip.y / clip.w);
    const Vec2 vulkan_pixel((projected_ndc.x * 0.5f + 0.5f) * vp_w,
                            (projected_ndc.y * 0.5f + 0.5f) * vp_h);
    const Vec2 unproject_ndc = viewportLocalToClipNdc(vulkan_pixel, vp_w, vp_h);
    expect_near(unproject_ndc.x, projected_ndc.x,
                "unproject NDC x matches projected clip");
    expect_near(unproject_ndc.y, projected_ndc.y,
                "unproject NDC y matches Vulkan framebuffer");
  }

  {
    const float vp_w = 800.0f;
    const float vp_h = 450.0f;
    const Vec3 eye(12.0f, -12.0f, 9.0f);
    const Vec3 target(0.0f);
    const Vec3 world_up(0.0f, 0.0f, 1.0f);
    Mat4 projection =
        glm::perspectiveZO(glm::radians(45.0f), vp_w / vp_h, 0.1f, 1000.0f);
    projection[1][1] *= -1.0f;
    const Mat4 view = glm::lookAt(eye, target, world_up);
    const Vec3 forward = glm::normalize(target - eye);
    const Vec3 right = glm::normalize(glm::cross(forward, world_up));
    const Vec3 cam_up = glm::cross(right, forward);

    const auto hit_at = [&](float x, float y) {
      const Vec2 ndc = viewportLocalToClipNdc(Vec2(x, y), vp_w, vp_h);
      return groundPlacementFromRay(
          unprojectViewportRay(ndc, view, projection, eye, false));
    };

    const Vec3 hit_center = hit_at(400.0f, 225.0f);
    const Vec3 hit_right = hit_at(500.0f, 225.0f);
    const Vec3 hit_down = hit_at(400.0f, 325.0f);
    expect_true("mouse right moves ground hit along camera right",
                glm::dot(hit_right - hit_center, right) > 0.05f);
    expect_true("mouse down moves ground hit opposite camera up (not inverted)",
                glm::dot(hit_down - hit_center, cam_up) < -0.05f);

    const Vec2 origin(180.0f, 64.0f);
    const Vec2 logical_center = origin + Vec2(400.0f, 225.0f);
    const Vec3 hit_from_logical = hit_at(
        logicalToViewportLocal(logical_center, origin).x,
        logicalToViewportLocal(logical_center, origin).y);
    expect_true("logical pointer subtracts origin without a second HiDPI map",
                vecNear(hit_from_logical, hit_center));
  }

  {
    const Mat4 gltf_translate =
        glm::translate(Mat4(1.0f), Vec3(0.0f, 2.0f, 0.0f));
    const Mat4 engine = similarityGltfToEngine(gltf_translate);
    expect_true("glTF +Y node translation becomes engine +Z",
                vecNear(Vec3(engine[3]), Vec3(0.0f, 0.0f, 2.0f)));
  }

  if (g_failures == 0) {
    std::printf("placement_preview_test: all passed\n");
    return 0;
  }
  std::fprintf(stderr, "placement_preview_test: %d failure(s)\n", g_failures);
  return 1;
}
