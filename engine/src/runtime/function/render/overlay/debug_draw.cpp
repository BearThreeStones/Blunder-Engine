#include "runtime/function/render/overlay/debug_draw.h"

#include <algorithm>
#include <cstdint>

#include "EASTL/vector.h"

namespace Blunder {

namespace {

enum class DebugDrawPrimitive : uint8_t {
  line = 0,
  ray = 1,
  arrow = 2,
  wire_box = 3,
  wire_sphere = 4,
  wire_capsule = 5,
  cross = 6,
};

struct DebugDrawCommand {
  DebugDrawPrimitive kind{DebugDrawPrimitive::line};
  Vec3 a{0.0f};
  Vec3 b{0.0f};
  Vec3 extra{0.0f};
  float radius{0.0f};
  Vec4 color{1.0f};
  float width_px{DebugDraw::k_default_width_px};
  float remaining_s{0.0f};
  bool one_shot{true};
};

eastl::vector<DebugDrawCommand> g_commands;
bool g_ingame_enabled = false;

float sanitizeWidth(float width_px) {
  return width_px > 0.0f ? width_px : DebugDraw::k_default_width_px;
}

void push(DebugDrawPrimitive kind, const Vec3& a, const Vec3& b,
          const Vec3& extra, float radius, const Vec4& color, float duration_s,
          float width_px) {
  DebugDrawCommand cmd{};
  cmd.kind = kind;
  cmd.a = a;
  cmd.b = b;
  cmd.extra = extra;
  cmd.radius = radius;
  cmd.color = color;
  cmd.width_px = sanitizeWidth(width_px);
  cmd.one_shot = duration_s <= 0.0f;
  cmd.remaining_s = cmd.one_shot ? 0.0f : duration_s;
  g_commands.push_back(cmd);
}

}  // namespace

void DebugDraw::line(const Vec3& a, const Vec3& b, const Vec4& color,
                     float duration_s, float width_px) {
  push(DebugDrawPrimitive::line, a, b, Vec3(0.0f), 0.0f, color, duration_s,
       width_px);
}

void DebugDraw::ray(const Vec3& origin, const Vec3& direction,
                    const Vec4& color, float duration_s, float width_px) {
  push(DebugDrawPrimitive::ray, origin, direction, Vec3(0.0f), 0.0f, color,
       duration_s, width_px);
}

void DebugDraw::arrow(const Vec3& from, const Vec3& to, const Vec4& color,
                      float duration_s, float width_px) {
  push(DebugDrawPrimitive::arrow, from, to, Vec3(0.0f), 0.0f, color, duration_s,
       width_px);
}

void DebugDraw::wireBox(const Vec3& center, const Vec3& size, const Vec4& color,
                        float duration_s, float width_px) {
  push(DebugDrawPrimitive::wire_box, center, Vec3(0.0f), size, 0.0f, color,
       duration_s, width_px);
}

void DebugDraw::wireSphere(const Vec3& center, float radius, const Vec4& color,
                           float duration_s, float width_px) {
  push(DebugDrawPrimitive::wire_sphere, center, Vec3(0.0f), Vec3(0.0f), radius,
       color, duration_s, width_px);
}

void DebugDraw::wireCapsule(const Vec3& a, const Vec3& b, float radius,
                            const Vec4& color, float duration_s,
                            float width_px) {
  push(DebugDrawPrimitive::wire_capsule, a, b, Vec3(0.0f), radius, color,
       duration_s, width_px);
}

void DebugDraw::cross(const Vec3& point, float size, const Vec4& color,
                      float duration_s, float width_px) {
  push(DebugDrawPrimitive::cross, point, Vec3(0.0f), Vec3(0.0f), size, color,
       duration_s, width_px);
}

void DebugDraw::endFrame(float delta_time) {
  const float dt = std::max(delta_time, 0.0f);
  size_t write = 0;
  for (size_t read = 0; read < g_commands.size(); ++read) {
    DebugDrawCommand cmd = g_commands[read];
    if (cmd.one_shot) {
      continue;
    }
    cmd.remaining_s -= dt;
    if (cmd.remaining_s <= 0.0f) {
      continue;
    }
    g_commands[write++] = cmd;
  }
  g_commands.resize(write);
}

void DebugDraw::reset() {
  g_commands.clear();
  g_ingame_enabled = false;
}

uint32_t DebugDraw::commandCount() {
  return static_cast<uint32_t>(g_commands.size());
}

void DebugDraw::setInGameEnabled(bool enabled) {
  g_ingame_enabled = enabled;
}

bool DebugDraw::inGameEnabled() {
  return g_ingame_enabled;
}

void DebugDraw::expandLines(GizmoLineCommandStream& out,
                            const OverlayState& state) {
  auto emit = [&](const Vec3& a, const Vec3& b, const Vec4& color,
                  float width_px) {
    out.push(GizmoLineDrawStyle::line, a, b, Vec3(0.0f), color, width_px);
  };

  for (const DebugDrawCommand& cmd : g_commands) {
    switch (cmd.kind) {
      case DebugDrawPrimitive::line:
        emit(cmd.a, cmd.b, cmd.color, cmd.width_px);
        break;
      case DebugDrawPrimitive::ray:
        emit(cmd.a, cmd.a + cmd.b, cmd.color, cmd.width_px);
        break;
      case DebugDrawPrimitive::arrow:
        debugDrawEmitArrow(cmd.a, cmd.b, cmd.color, cmd.width_px, emit);
        break;
      case DebugDrawPrimitive::wire_box:
        debugDrawEmitWireBox(cmd.a, cmd.extra, cmd.color, cmd.width_px, emit);
        break;
      case DebugDrawPrimitive::wire_sphere: {
        const int steps =
            debugDrawStepsForWorldPoint(cmd.a, cmd.radius, state);
        debugDrawEmitWireSphere(cmd.a, cmd.radius, steps, cmd.color,
                                cmd.width_px, emit);
        break;
      }
      case DebugDrawPrimitive::wire_capsule: {
        const Vec3 mid = (cmd.a + cmd.b) * 0.5f;
        const int steps =
            debugDrawStepsForWorldPoint(mid, cmd.radius, state);
        debugDrawEmitWireCapsule(cmd.a, cmd.b, cmd.radius, steps, cmd.color,
                                 cmd.width_px, emit);
        break;
      }
      case DebugDrawPrimitive::cross:
        debugDrawEmitCross(cmd.a, cmd.radius, cmd.color, cmd.width_px, emit);
        break;
    }
  }
}

}  // namespace Blunder
