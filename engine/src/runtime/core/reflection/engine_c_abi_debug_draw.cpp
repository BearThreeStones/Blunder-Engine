#include "runtime/core/reflection/engine_c_abi.h"

#include "runtime/core/math/math_types.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/render/overlay/debug_draw.h"
#include "runtime/function/scene/gltf_unit_scale.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_system.h"

using namespace Blunder;

namespace {

float metreToWorld(float metres) {
  if (!g_runtime_global_context.m_scene_system) {
    return metres;
  }
  SceneInstance* scene =
      g_runtime_global_context.m_scene_system->getActiveInstance();
  if (scene == nullptr) {
    return metres;
  }
  return gltfMetreQuantityInWorld(metres, sceneDrawnMeshIsCentimetre(*scene));
}

Vec3 metrePointToWorld(float x, float y, float z) {
  const float s = metreToWorld(1.0f);
  return Vec3(x * s, y * s, z * s);
}

Vec4 color4(float r, float g, float b, float a) {
  return Vec4(r, g, b, a);
}

}  // namespace

extern "C" {

int blunder_debug_draw_line(float ax, float ay, float az, float bx, float by,
                            float bz, float r, float g, float b, float a,
                            float duration_s, float width_px) {
  DebugDraw::line(metrePointToWorld(ax, ay, az), metrePointToWorld(bx, by, bz),
                  color4(r, g, b, a), duration_s, width_px);
  return BLUNDER_ENGINE_OK;
}

int blunder_debug_draw_ray(float ox, float oy, float oz, float dx, float dy,
                           float dz, float r, float g, float b, float a,
                           float duration_s, float width_px) {
  const float s = metreToWorld(1.0f);
  DebugDraw::ray(metrePointToWorld(ox, oy, oz),
                 Vec3(dx * s, dy * s, dz * s), color4(r, g, b, a), duration_s,
                 width_px);
  return BLUNDER_ENGINE_OK;
}

int blunder_debug_draw_arrow(float ax, float ay, float az, float bx, float by,
                             float bz, float r, float g, float b, float a,
                             float duration_s, float width_px) {
  DebugDraw::arrow(metrePointToWorld(ax, ay, az),
                   metrePointToWorld(bx, by, bz), color4(r, g, b, a),
                   duration_s, width_px);
  return BLUNDER_ENGINE_OK;
}

int blunder_debug_draw_wire_box(float cx, float cy, float cz, float sx,
                                float sy, float sz, float r, float g, float b,
                                float a, float duration_s, float width_px) {
  const float scale = metreToWorld(1.0f);
  DebugDraw::wireBox(metrePointToWorld(cx, cy, cz),
                     Vec3(sx * scale, sy * scale, sz * scale),
                     color4(r, g, b, a), duration_s, width_px);
  return BLUNDER_ENGINE_OK;
}

int blunder_debug_draw_wire_sphere(float cx, float cy, float cz, float radius,
                                   float r, float g, float b, float a,
                                   float duration_s, float width_px) {
  DebugDraw::wireSphere(metrePointToWorld(cx, cy, cz), metreToWorld(radius),
                        color4(r, g, b, a), duration_s, width_px);
  return BLUNDER_ENGINE_OK;
}

int blunder_debug_draw_wire_capsule(float ax, float ay, float az, float bx,
                                    float by, float bz, float radius, float r,
                                    float g, float b, float a,
                                    float duration_s, float width_px) {
  DebugDraw::wireCapsule(metrePointToWorld(ax, ay, az),
                         metrePointToWorld(bx, by, bz), metreToWorld(radius),
                         color4(r, g, b, a), duration_s, width_px);
  return BLUNDER_ENGINE_OK;
}

int blunder_debug_draw_cross(float px, float py, float pz, float size, float r,
                             float g, float b, float a, float duration_s,
                             float width_px) {
  DebugDraw::cross(metrePointToWorld(px, py, pz), metreToWorld(size),
                   color4(r, g, b, a), duration_s, width_px);
  return BLUNDER_ENGINE_OK;
}

int blunder_debug_draw_set_ingame_enabled(int enabled) {
  DebugDraw::setInGameEnabled(enabled != 0);
  return BLUNDER_ENGINE_OK;
}

int blunder_debug_draw_get_ingame_enabled(int* out_enabled) {
  if (out_enabled == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  *out_enabled = DebugDraw::inGameEnabled() ? 1 : 0;
  return BLUNDER_ENGINE_OK;
}

}  // extern "C"
