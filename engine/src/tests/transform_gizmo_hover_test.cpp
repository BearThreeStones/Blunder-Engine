#include <cstdio>
#include <cmath>
#include <optional>

#include <glm/vec3.hpp>

#include "runtime/core/math/geometry.h"
#include "runtime/function/render/gizmo/transform_gizmo_pick.h"
#include "runtime/function/render/gizmo/transform_gizmo_types.h"

namespace {

int g_failures = 0;

void expect_true(const char* label, const bool value) {
  if (!value) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

void expect_axis(const char* label,
                 const std::optional<Blunder::ManipulatorAxis>& actual,
                 const Blunder::ManipulatorAxis expected) {
  if (!actual || *actual != expected) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

Blunder::TransformGizmoPickContext makeTranslatePickCtx() {
  Blunder::TransformGizmoPickContext ctx{};
  ctx.ray = Blunder::Ray{glm::vec3(-2.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f)};
  ctx.basis.origin = glm::vec3(0.0f);
  ctx.basis.axis_x = glm::vec3(1.0f, 0.0f, 0.0f);
  ctx.basis.axis_y = glm::vec3(0.0f, 1.0f, 0.0f);
  ctx.basis.axis_z = glm::vec3(0.0f, 0.0f, 1.0f);
  ctx.group_scale = 1.0f;
  ctx.gizmo_pixel_size = 0.01f;
  ctx.camera_forward = glm::vec3(0.0f, 0.0f, -1.0f);
  ctx.camera_position = glm::vec3(0.0f, 0.0f, 5.0f);
  return ctx;
}

}  // namespace

int main() {
  using namespace Blunder;

  expect_true("drag highlights active axis",
              gizmoHandleHighlighted(ManipulatorAxis::trans_y, ManipulatorAxis::trans_x,
                                     ManipulatorAxis::trans_y));
  expect_true("hover highlights when not dragging",
              gizmoHandleHighlighted(std::nullopt, ManipulatorAxis::trans_z,
                                     ManipulatorAxis::trans_z));
  expect_true("no highlight when idle",
              !gizmoHandleHighlighted(std::nullopt, std::nullopt, ManipulatorAxis::trans_x));

  {
    const float idot[3] = {1.0f, 1.0f, 1.0f};
    const GizmoAxisColor colors = gizmoAxisColor(ManipulatorAxis::trans_x, idot);
    expect_true("blender normal alpha 0.6",
                std::fabs(colors.color.a - 0.6f) < 1e-4f);
    expect_true("blender highlight alpha 1.0",
                std::fabs(colors.color_hi.a - 1.0f) < 1e-4f);
    const glm::vec4 hovered = gizmoColorGet(ManipulatorAxis::trans_x, idot, true);
    expect_true("gizmoColorGet uses color_hi",
                std::fabs(hovered.a - 1.0f) < 1e-4f);
  }

  {
    const float idot[3] = {1.0f, 1.0f, 1.0f};
    const GizmoAxisColor rot_t = gizmoAxisColor(ManipulatorAxis::rot_t, idot);
    expect_true("rot_t normal alpha 0.03",
                std::fabs(rot_t.color.a - 0.03f) < 1e-4f);
    expect_true("rot_t hover keeps normal color (WM_GIZMO_DRAW_HOVER decor)",
                std::fabs(gizmoColorGet(ManipulatorAxis::rot_t, idot, true).a - rot_t.color.a) <
                    1e-6f);
  }
  {
    const float idot_edge[3] = {0.06f, 1.0f, 1.0f};
    const GizmoAxisColor x_faded = gizmoAxisColor(ManipulatorAxis::trans_x, idot_edge);
    expect_true("faded axis normal alpha < 0.6",
                x_faded.color.a > 0.0f && x_faded.color.a < 0.6f);
    expect_true("faded axis hover alpha > normal",
                gizmoColorGet(ManipulatorAxis::trans_x, idot_edge, true).a > x_faded.color.a);
  }
  {
    const float idot[3] = {0.0f, 0.0f, 1.0f};
    const GizmoAxisColor rot_x = gizmoAxisColor(ManipulatorAxis::rot_x, idot);
    expect_true("rotation dial ignores idot fade",
                std::fabs(rot_x.color.a - 0.6f) < 1e-4f);
    expect_true("rotation dial hover full alpha",
                std::fabs(gizmoColorGet(ManipulatorAxis::rot_x, idot, true).a - 1.0f) < 1e-4f);
  }
  {
    const float idot[3] = {1.0f, 1.0f, 1.0f};
    expect_true("rot_c decor ignores hover color_hi",
                std::fabs(gizmoColorGet(ManipulatorAxis::rot_c, idot, true).a -
                          gizmoAxisColor(ManipulatorAxis::rot_c, idot).color.a) < 1e-6f);
    expect_true("scale annulus decor ignores hover color_hi",
                std::fabs(gizmoColorGet(ManipulatorAxis::scale_c_outer, idot, true).a -
                          gizmoAxisColor(ManipulatorAxis::scale_c_outer, idot).color.a) < 1e-6f);
  }
  {
    const float idot[3] = {1.0f, 1.0f, 1.0f};
    const GizmoAxisColor x = gizmoAxisColor(ManipulatorAxis::trans_x, idot);
    expect_true("blender theme X red channel",
                std::fabs(x.color.r - 1.0f) < 1e-4f);
    expect_true("blender theme X green channel",
                std::fabs(x.color.g - (51.0f / 255.0f)) < 1e-4f);
    expect_true("blender theme X blue channel",
                std::fabs(x.color.b - (82.0f / 255.0f)) < 1e-4f);
    const GizmoAxisColor center = gizmoAxisColor(ManipulatorAxis::trans_c, idot);
    expect_true("blender TH_GIZMO_VIEW_ALIGN is white",
                center.color.r > 0.99f && center.color.g > 0.99f &&
                    center.color.b > 0.99f);
  }

  const auto ctx = makeTranslatePickCtx();
  expect_axis("pick wrapper hits X arrow",
              pickTransformGizmoHandle(TransformGizmoMode::translate, ctx),
              ManipulatorAxis::trans_x);
  {
    Blunder::TransformGizmoPickContext plane_ctx{};
    plane_ctx.basis.origin = glm::vec3(0.0f);
    plane_ctx.basis.axis_x = glm::vec3(1.0f, 0.0f, 0.0f);
    plane_ctx.basis.axis_y = glm::vec3(0.0f, 1.0f, 0.0f);
    plane_ctx.basis.axis_z = glm::vec3(0.0f, 0.0f, 1.0f);
    plane_ctx.group_scale = 1.0f;
    plane_ctx.gizmo_pixel_size = 0.01f;
    const float plane_center =
        Blunder::TransformGizmoMetrics::k_mesh_plane_center_offset;
    plane_ctx.ray = Blunder::Ray{
        glm::vec3(plane_center, plane_center, 5.0f), glm::vec3(0.0f, 0.0f, -1.0f)};
    expect_axis("pick translate XY plane handle",
                pickTransformGizmoHandle(TransformGizmoMode::translate, plane_ctx),
                ManipulatorAxis::trans_xy);
    expect_axis("pick scale XY plane handle",
                pickTransformGizmoHandle(TransformGizmoMode::scale, plane_ctx),
                ManipulatorAxis::trans_xy);
  }
  {
    const auto hit = pickTransformGizmoHandle(TransformGizmoMode::none, ctx);
    expect_true("pick wrapper misses in none mode", !hit.has_value());
  }

  if (g_failures == 0) {
    std::printf("transform_gizmo_hover_test: all passed\n");
    return 0;
  }
  std::fprintf(stderr, "transform_gizmo_hover_test: %d failure(s)\n", g_failures);
  return 1;
}
