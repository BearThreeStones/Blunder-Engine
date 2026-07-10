# Blender 风格 Overlay 抗锯齿对齐 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 Blunder 编辑器 overlay 抗锯齿对齐 Blender Overlay Engine 的 shader-based AA 方案（无 MSAA），覆盖折线内禀 AA、线 MRT 合成 AA、以及（可选）轮廓方向性检测升级。

**Architecture:** Blender 使用三层 AA，互不依赖 MSAA：（1）`GPU_SHADER_3D_POLYLINE_*` 在几何阶段展开线宽并传递 `smoothline`，片元用 `(lineWidth + SMOOTH_WIDTH) * 0.5 - abs(smoothline)` 软化；（2）线 overlay（grid/wire/axes/outline detect）写入 color + `pack_line_data` 的 line MRT，全屏 `overlay_antialiasing` 做十字邻域距离投影 + `line_coverage` smoothstep + 深度感知 alpha 混合；（3）选择轮廓 detect pass 根据 8 邻域 edge case 估算方向并 `pack_line_data`，再进入同一 AA 合成。Blunder 已完成 v1 轮廓 8 邻域 kernel + transform gizmo `strokeCoord`/`fwidth`（见 `openspec/changes/outline-gizmo-anti-aliasing/`）；本计划补齐 **overlay_aa 合成空实现**、**折线公式与 Blender 常数对齐**、以及 **gizmo 细杆件 polyline 化**。

**Tech Stack:** C++20、Slang/Vulkan、`E:/Dev/Blender` 参考实现、单元测试 `engine/src/tests/`、OpenSpec delta。

**Prerequisite:** `outline-gizmo-anti-aliasing` 实现已合并（`outline_aa.*`、`gizmo_polyline_aa.*`、`transform_gizmo.slang` strokeCoord）。`OverlayLineTargets` + `OverlayAntiAliasing` C++ 管线已存在但 `overlay_aa.slang` 未消费 `lineDataTexture`。

**Blender 参考文件（本地 `E:/Dev/Blender`）：**

| Blender 路径 | 作用 |
|--------------|------|
| `source/blender/gpu/shaders/gpu_shader_3D_polyline_frag.glsl` | 折线片元 AA：`SMOOTH_WIDTH=1` |
| `source/blender/gpu/shaders/gpu_shader_3D_polyline_vert.glsl` | `smoothline` ±(lineWidth+SMOOTH_WIDTH*lineSmooth)*0.5 |
| `source/blender/draw/engines/overlay/shaders/overlay_common_lib.glsl` | `pack_line_data()` |
| `source/blender/draw/engines/overlay/shaders/overlay_antialiasing.bsl.hh` | 全屏线 AA 合成（十字邻域） |
| `source/blender/draw/engines/overlay/shaders/overlay_outline_detect_frag.glsl` | 轮廓方向性 line pack |
| `source/blender/draw/engines/overlay/overlay_shader_shared.hh` | `LINE_SMOOTH_START/END`、`DISC_RADIUS` |
| `source/blender/editors/gizmo_library/gizmo_types/dial3d_gizmo.cc` | `immUniform1f("lineWidth", …)` + POLYLINE |
| `source/blender/editors/gizmo_library/gizmo_types/arrow3d_gizmo.cc` | 箭头线框用 `POLYLINE_UNIFORM_COLOR` |

---

## 现状差距（相对 Blender）

| 区域 | Blunder 今天 | Blender | 差距 |
|------|-------------|---------|------|
| Transform gizmo 折线 | `strokeCoord` + `fwidth` smoothstep | `smoothline` + `SMOOTH_WIDTH=1` 线性衰减 | 公式/常数未对齐；无 CPU 对照 Blender 公式 |
| Transform gizmo 实体（箭头杆、平面边） | 硬边三角网格 | 细部用 POLYLINE（`lineWidth = pixelsize`） | 锯齿可见 |
| `overlay_aa.slang` | 仅 alpha 混合 scene+overlay | 十字邻域 `line_tx` + `line_coverage` | **空实现**（绑定了 lineData 未用） |
| Grid 线 | `grid.slang` 程序化（无 line MRT） | `overlay_grid` + line MRT + AA pass | 网格线未走 Blender AA 管线 |
| Axes / Wireframe line pass | `draw_line` 为 stub | 写入 line MRT | 未实现 |
| 选择轮廓 resolve | 8 邻域计数 + linear smoothstep | `overlay_outline_detect` 方向性 `pack_line_data` | 对角质量弱于 Blender |

**本计划范围：** Phase 1–3（折线公式对齐、overlay AA 合成、grid 接入 line MRT）。Phase 4（轮廓 detect 升级）单独里程碑，任务保留但标为可选。

**Out of scope:** 视口 MSAA；Compositor SMAA；`navigate_gizmo.slang`（已有 disc smoothstep）；用户可配置 AA 强度。

---

## File map

| File | Responsibility |
|------|----------------|
| `engine/src/runtime/function/render/overlay/overlay_line_aa.h` | **New** — `decodeLineData`, `lineCoverage`, `neighborLineDist` CPU 镜像 |
| `engine/src/runtime/function/render/overlay/overlay_line_aa.cpp` | **New** — 实现（对照 `overlay_antialiasing.bsl.hh`） |
| `engine/shaders/overlay_aa.slang` | **Modify** — 完整 Blender 式十字邻域 AA 合成 |
| `engine/shaders/overlay_line_pack.slang` | **New** — 共享 `packLineData`（对照 `overlay_common_lib.glsl`） |
| `engine/shaders/grid_line.slang` | **Modify** — 确保 lineData 编码与 `decodeLineData` 一致 |
| `engine/src/runtime/function/render/overlay/grid_overlay.cpp` | **Modify** — 可选：grid 细线改走 `OverlayLinePass` |
| `engine/src/runtime/function/render/gizmo/gizmo_polyline_aa.h` | **Modify** — 增加 `kPolylineSmoothWidth = 1.f`、Blender 公式 |
| `engine/shaders/transform_gizmo.slang` | **Modify** — 片元 AA 改用 Blender smoothline 等价式 |
| `engine/src/tests/overlay_line_aa_test.cpp` | **New** — line decode + coverage 回归 |
| `engine/src/tests/gizmo_polyline_blender_aa_test.cpp` | **New** — Blender 折线 alpha 对照 |
| `docs/agents/render-pipeline.md` | **Modify** — 三层 AA 架构说明 |
| `openspec/changes/blender-overlay-aa-parity/` | **New** — OpenSpec delta（propose 后创建） |

---

### Task 1: Overlay line AA — CPU 镜像（TDD）

**Files:**
- Create: `engine/src/runtime/function/render/overlay/overlay_line_aa.h`
- Create: `engine/src/runtime/function/render/overlay/overlay_line_aa.cpp`
- Create: `engine/src/tests/overlay_line_aa_test.cpp`
- Modify: `engine/src/tests/CMakeLists.txt`
- Modify: `engine/src/runtime/function/render/CMakeLists.txt`

Blender 常数（`overlay_shader_shared.hh`）：

```cpp
// DISC_RADIUS = M_1_SQRTPI * 1.05f ≈ 0.5936
constexpr float kDiscRadius = 0.5936f;
constexpr float kLineSmoothStart = 0.5f - kDiscRadius;
constexpr float kLineSmoothEnd = 0.5f + kDiscRadius;
```

- [ ] **Step 1: Write the failing test**

```cpp
#include <cstdio>
#include <cmath>
#include "runtime/function/render/overlay/overlay_line_aa.h"

int main() {
  using namespace Blunder;
  int failures = 0;
  auto expect_near = [&](const char* label, float a, float b) {
    if (std::fabs(a - b) > 1e-3f) {
      std::fprintf(stderr, "FAIL %s (got %f want %f)\n", label, a, b);
      ++failures;
    }
  };

  // pack_line_data default: sin_theta=0, dist=0.5 → decode dist=0
  const LineData center = decodeLineData(float2{0.0f, 0.5f});
  expect_near("center dist", center.dist, 0.0f);

  // line_coverage at kernel edge — smoothstep
  const float cov = lineCoverage(0.0f, 0.0f, true);
  if (cov <= 0.99f) {
    std::fprintf(stderr, "FAIL center coverage full (got %f)\n", cov);
    ++failures;
  }
  const float edge = lineCoverage(1.5f, 0.0f, true);
  if (edge > 0.01f) {
    std::fprintf(stderr, "FAIL far edge coverage zero (got %f)\n", edge);
    ++failures;
  }

  if (failures == 0) {
    std::printf("overlay_line_aa_test: all passed\n");
    return 0;
  }
  return 1;
}
```

- [ ] **Step 2: Run test — verify RED**

```powershell
cmake --build build/vs2026-debug --target overlay_line_aa_test --config Debug
```

Expected: FAIL — header not found

- [ ] **Step 3: Implement CPU helper**

`overlay_line_aa.h`:

```cpp
#pragma once

#include <glm/vec2.hpp>

namespace Blunder {

struct LineData {
  glm::vec2 dir;
  float dist;
  float dist_raw;
  bool is_valid() const { return dist_raw != 0.0f; }
};

LineData decodeLineData(glm::vec2 packed);
float lineCoverage(float distance_to_line, float line_kernel_size, bool smooth);
float neighborLineDist(const LineData& neighbor, glm::ivec2 offset);

}  // namespace Blunder
```

`overlay_line_aa.cpp` — 对照 `overlay_antialiasing.bsl.hh` `Line::decode` 与 `line_coverage`：

```cpp
#include "runtime/function/render/overlay/overlay_line_aa.h"
#include <algorithm>
#include <cmath>

namespace Blunder {

namespace {
constexpr float kDiscRadius = 0.5936f;
constexpr float kLineSmoothStart = 0.5f - kDiscRadius;
constexpr float kLineSmoothEnd = 0.5f + kDiscRadius;

float smoothstep(float edge0, float edge1, float x) {
  const float t = std::clamp((x - edge0) / std::max(edge1 - edge0, 1e-6f), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}
}  // namespace

LineData decodeLineData(const glm::vec2 packed) {
  const float dist = (packed.y - 0.5f) * 2.5f;
  const float sin_theta = (packed.x - 0.5f) * 2.0f;
  const float cos_theta = std::sqrt(std::max(1.0f - sin_theta * sin_theta, 0.0f));
  glm::vec2 perp{sin_theta, cos_theta};
  const float len = glm::length(perp);
  if (len > 1e-6f) {
    perp /= len;
  }
  return LineData{perp, dist, packed.y};
}

float lineCoverage(const float distance_to_line, const float line_kernel_size,
                   const bool smooth) {
  if (smooth) {
    return smoothstep(kLineSmoothEnd, kLineSmoothStart,
                      std::fabs(distance_to_line) - line_kernel_size);
  }
  return (line_kernel_size - std::fabs(distance_to_line)) >= -0.5f ? 1.0f : 0.0f;
}

float neighborLineDist(const LineData& neighbor, const glm::ivec2 offset) {
  const bool is_dir_horizontal = std::fabs(neighbor.dir.x) > std::fabs(neighbor.dir.y);
  const bool is_ofs_horizontal = offset.x != 0;
  if (!neighbor.is_valid() || is_ofs_horizontal != is_dir_horizontal) {
    return 1e10f;
  }
  return neighbor.dist +
         static_cast<float>(offset.x) * (-neighbor.dir.x) +
         static_cast<float>(offset.y) * (-neighbor.dir.y);
}

}  // namespace Blunder
```

- [ ] **Step 4: Run test — verify GREEN**

- [ ] **Step 5: Commit**

```bash
git add engine/src/runtime/function/render/overlay/overlay_line_aa.* \
        engine/src/tests/overlay_line_aa_test.cpp \
        engine/src/tests/CMakeLists.txt \
        engine/src/runtime/function/render/CMakeLists.txt
git commit -m "test: overlay line AA CPU mirror (Blender antialiasing pass)"
```

---

### Task 2: `overlay_aa.slang` — Blender 十字邻域合成

**Files:**
- Modify: `engine/shaders/overlay_aa.slang`
- Modify: `engine/src/runtime/function/render/overlay/overlay_anti_aliasing.cpp`（如需 depth 纹理绑定）

Blender 参考：`overlay_antialiasing.bsl.hh` `frag_main`（`texelFetch` 十字 4 邻域 + `neighbor_blend`）。

- [ ] **Step 1: Write failing test extension** — 在 `overlay_line_aa_test.cpp` 追加 `neighborLineDist` 水平/垂直 case（RED）

- [ ] **Step 2: Replace `overlay_aa.slang` fragment**

核心逻辑（Slang 伪代码，须用 `Load` 代替 `texelFetch`）：

```slang
static const float kDiscRadius = 0.5936;
static const float kLineSmoothStart = 0.5 - kDiscRadius;
static const float kLineSmoothEnd = 0.5 + kDiscRadius;

struct LineData {
    float2 dir;
    float dist;
    float dist_raw;
};

LineData decodeLine(float2 data) {
    float dist = (data.y - 0.5) * 2.5;
    float sin_theta = (data.x - 0.5) * 2.0;
    float cos_theta = sqrt(max(1.0 - sin_theta * sin_theta, 0.0));
    float2 perp = normalize(float2(sin_theta, cos_theta));
    return { perp, dist, data.y };
}

float lineCoverage(float d, float kernel, bool smooth) {
    if (smooth) {
        return smoothstep(kLineSmoothEnd, kLineSmoothStart, abs(d) - kernel);
    }
    return step(-0.5, kernel - abs(d));
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target {
    int2 texel = int2(input.uv * /* viewport size from ubo */);
    float4 centerColor = overlayColorTexture.Load(int3(texel, 0));
    float2 centerLine = lineDataTexture.Load(int3(texel, 0)).rg;
    LineData center = decodeLine(centerLine);

    float line_kernel = 0.5 - 0.5; // theme.sizes.pixel * 0.5 - 0.5; use 0 for 1px lines
    bool do_smooth = true;

    if (!do_smooth && center.dist <= 1.0) {
        float4 scene = sceneColorTexture.Sample(sceneColorSampler, input.uv);
        return float4(lerp(scene.rgb, centerColor.rgb, centerColor.a), scene.a);
    }

    // Fetch cross neighbors, compute coverage, blend (mirror neighbor_blend)
    // ... full port from overlay_antialiasing.bsl.hh ...

    float4 scene = sceneColorTexture.Sample(sceneColorSampler, input.uv);
    return float4(lerp(scene.rgb, centerColor.rgb, centerColor.a), scene.a);
}
```

- [ ] **Step 3: Build `engine_runtime` + 启用 `BLUNDER_EDITOR_OVERLAY_AA=1` 手动验证 grid**

```powershell
$env:BLUNDER_EDITOR_OVERLAY_AA="1"
cmake --build build/vs2026-debug --target editor --config Debug
```

Expected: 编译成功；grid 线（若已接 line MRT）边缘软化

- [ ] **Step 4: Run `overlay_line_aa_test` — GREEN**

- [ ] **Step 5: Commit**

```bash
git add engine/shaders/overlay_aa.slang
git commit -m "feat: Blender-style overlay line anti-aliasing composite pass"
```

---

### Task 3: 共享 `packLineData` + grid 接入 line MRT

**Files:**
- Create: `engine/shaders/overlay_line_pack.slang`
- Modify: `engine/shaders/grid_line.slang`
- Modify: `engine/src/runtime/function/render/overlay/grid_overlay.cpp`（或新建 grid line draw 路径）

Blender `pack_line_data`（`overlay_common_lib.glsl:30-54`）：

```slang
float4 packLineData(float2 frag_co, float2 edge_start, float2 edge_pos) {
    float2 edge = edge_start - edge_pos;
    float len = length(edge);
    if (len > 0.0) {
        edge /= len;
        float2 perp = float2(-edge.y, edge.x);
        if (perp.y < 0.0) perp = -perp;
        float sin_theta = perp.x;
        float dist = dot(perp, frag_co - edge_start);
        return float4(sin_theta * 0.5 + 0.5, dist * 0.4 + 0.5, 0.0, 1.0);
    }
    return float4(0.0, 0.5, 0.0, 1.0);
}
```

- [ ] **Step 1: 统一 `grid_line.slang` lineData 编码为 `packLineData` 格式**（替换现有 `lineDir/signedDist` 若不一致）

- [ ] **Step 2: 将 grid 细线绘制挂到 `OverlaySystem::draw_overlay_lines`**（参照 `overlay_system.cpp:154-157`）

- [ ] **Step 3: 手动 QA** — `BLUNDER_EDITOR_OVERLAY_AA=1`，网格线与 Blender Viewport > Smooth Wires > Overlay 对比

- [ ] **Step 4: Commit**

```bash
git add engine/shaders/overlay_line_pack.slang engine/shaders/grid_line.slang \
        engine/src/runtime/function/render/overlay/grid_overlay.cpp
git commit -m "feat: route grid lines through overlay line MRT for AA"
```

---

### Task 4: Transform gizmo 折线 AA — 对齐 Blender `smoothline` 公式

**Files:**
- Modify: `engine/src/runtime/function/render/gizmo/gizmo_polyline_aa.h`
- Modify: `engine/src/runtime/function/render/gizmo/gizmo_polyline_aa.cpp`
- Modify: `engine/shaders/transform_gizmo.slang`
- Create: `engine/src/tests/gizmo_polyline_blender_aa_test.cpp`

Blender 片元（`gpu_shader_3D_polyline_frag.glsl:19-21`）：

```glsl
if (lineSmooth) {
  fragColor.a *= clamp((lineWidth + SMOOTH_WIDTH) * 0.5 - abs(smoothline), 0.0, 1.0);
}
```

`SMOOTH_WIDTH = 1.0`（`gpu_shader_3D_polyline_infos.hh`）。

- [ ] **Step 1: Write failing test**

```cpp
// stroke_coord maps to smoothline / halfWidth; lineWidth_px=2, fw≈0 → at edge alpha→0
expect_near("blender edge",
            polylineStrokeAlphaBlender(/*smoothline=*/1.0f, /*lineWidth=*/2.0f, /*lineSmooth=*/true),
            0.0f);
expect_near("blender center",
            polylineStrokeAlphaBlender(0.0f, 2.0f, true),
            1.0f);
```

- [ ] **Step 2: Implement `polylineStrokeAlphaBlender`**

```cpp
constexpr float kPolylineSmoothWidth = 1.0f;

float polylineStrokeAlphaBlender(float smoothline, float line_width, bool line_smooth) {
  const float half_w = (line_width + kPolylineSmoothWidth * (line_smooth ? 1.0f : 0.0f)) * 0.5f;
  return std::clamp(half_w - std::fabs(smoothline), 0.0f, 1.0f);
}
```

- [ ] **Step 3: Update `transform_gizmo.slang`** — `strokeCoord * lineWidthHalf` 作为 `smoothline` 传入上式（替换 `fwidth` 路径，或保留 `fwidth` 仅作 fallback 并文档说明差异）

- [ ] **Step 4: Run `transform_gizmo_aa_test` + `gizmo_polyline_blender_aa_test` — GREEN**

- [ ] **Step 5: Commit**

---

### Task 5: Gizmo 箭头杆件 — POLYLINE 化（Blender `arrow3d_gizmo.cc`）

**Files:**
- Modify: `engine/shaders/transform_gizmo.slang` (`vsArrow` stem 段)
- Modify: `engine/src/runtime/function/render/gizmo/transform_gizmo_overlay.cpp`（若需改 draw call）

Blender 箭头 stem 用 `GPU_SHADER_3D_POLYLINE_UNIFORM_COLOR`，`lineWidth = U.pixelsize + select_bias`，非三角条带。

- [ ] **Step 1: 将 `vsArrow` stem 从 quad 三角改为 `emitPolyline` 两点线段**（local Z 轴 0→stemLen）

- [ ] **Step 2: `line_width_px` 设为 `1.0 * pixelsize`（与 Blender `U.pixelsize` 对齐）**

- [ ] **Step 3: 手动 QA** — Translate 模式箭头杆对比 Blender，边缘应软化

- [ ] **Step 4: Run `transform_gizmo_hover_test` — 无回归**

- [ ] **Step 5: Commit**

```bash
git commit -m "feat: draw transform gizmo arrow stems as smooth polylines (Blender parity)"
```

---

### Task 6: 文档 + OpenSpec

**Files:**
- Modify: `docs/agents/render-pipeline.md`
- Create: `openspec/changes/blender-overlay-aa-parity/`（`/opsx:propose blender-overlay-aa-parity`）

- [ ] **Step 1: 文档三层 AA 架构**（polyline 内禀 / line MRT 合成 / outline detect）

- [ ] **Step 2: OpenSpec delta** — `overlay-line-aa` capability；`transform-gizmo` 折线公式要求

- [ ] **Step 3: `openspec validate blender-overlay-aa-parity`**

- [ ] **Step 4: Commit docs + openspec**

---

### Task 7（可选）: 轮廓 detect — Blender 方向性 `pack_line_data`

**Files:**
- Modify: `engine/shaders/outline_resolve.slang` 或拆分为 detect + composite 两 pass
- Reference: `E:/Dev/Blender/source/blender/draw/engines/overlay/shaders/overlay_outline_detect_frag.glsl`

**范围大、单独里程碑。** 仅在 Task 1–3 手动 QA 仍不满意时启动。步骤概要：

- [ ] **Step 1: CPU 测试 edge case 枚举**（`XPOS|XNEG|YPOS|YNEG` 与 Blender `edge_case` switch 对照）
- [ ] **Step 2: Port `straight_line_dir` / `diag_dir` / `pack_line_data` 到 Slang**
- [ ] **Step 3: 输出 line MRT 而非直接 color**（需改 `OutlineOverlay` 为两 pass 或并入 overlay AA）
- [ ] **Step 4: 与 `overlay_aa.slang` 合成**

不在 v1 阻塞 Task 1–6。

---

## Self-review

| Spec / 需求 | 对应 Task |
|-------------|-----------|
| Blender polyline `SMOOTH_WIDTH` | Task 4 |
| Blender overlay antialiasing 全屏合成 | Task 2 |
| `pack_line_data` 编码一致 | Task 3 |
| Gizmo 细杆 polyline | Task 5 |
| 轮廓方向性 detect | Task 7（可选） |
| CPU 可测回归 | Task 1, 4 |
| 无 MSAA | 全计划 |

**Placeholder scan:** 无 TBD；Task 2 fragment 需实现时逐行 port `neighbor_blend`（实施阶段展开完整 Slang，不省略）。

**Type consistency:** `decodeLineData` / `packLineData` / `polylineStrokeAlphaBlender` 命名在 CPU 与 shader 间一致。

---

## 与已完成工作的关系

`docs/superpowers/plans/2026-07-06-outline-gizmo-anti-aliasing.md` 已完成 Task 1–4（轮廓 kernel、gizmo `strokeCoord`）。**本计划是其 Blender 对齐后续**，不重复已实现部分；Task 4 可选择性替换 v1 `fwidth` 实现。

---

## Manual QA checklist（全计划完成后）

1. 选中 mesh — 轮廓对角线（Task 7 前：8 邻域 kernel；Task 7 后：Blender 级）
2. `BLUNDER_EDITOR_OVERLAY_AA=1` — 网格线、（未来）axes/wireframe 软化
3. Transform R/S — 圆环 + 箭头杆（Task 4–5）
4. `BLUNDER_EDITOR_RENDER_SCALE=0.75` — 无严重退化
5. 与 Blender 4.x 同场景并排截图对比
