# 人工验收清单 — DogWalk 动画 Phase 4

对应任务 **6.2**（Chocomel 子集 Play 验收）。自动化门禁（`dogwalk_phase4_tree_gate_test`、`dogwalk_phase4_mini_play_acceptance_test`、`animation_tree_test`、`animation_preview_controller_test`、`animation_sync_group_test` 等）**不能**替代本清单。

**状态：** 未跑 — 路径已写死。人工每完成一步勾选。

英文版：`manual-checklist.md`

---

## 前置条件

| 项 | 值 |
|------|--------|
| OpenSpec 变更 | `openspec/changes/dogwalk-animation-phase-4/` |
| 引擎 worktree | `E:\Dev\Blunder-Engine\.worktrees\dogwalk-animation-phase-4` |
| 配置（一次） | 在 worktree 根目录：`cmake --preset vs2026-debug` |
| 构建（Debug） | `cmake --build build/vs2026-debug --config Debug --target engine_editor engine_player dogwalk_phase4_tree_gate_test dogwalk_phase4_mini_play_acceptance_test` |
| 二进制 | `build\vs2026-debug\bin\Debug\engine_editor.exe`、`engine_player.exe` |
| 自动化门禁 | `build\vs2026-debug\engine\src\tests\Debug\dogwalk_phase4_tree_gate_test.exe`、`dogwalk_phase4_mini_play_acceptance_test.exe` |
| Test Project | `E:\Blunder Projects\Test` |
| Chocomel 子集 | 场景内嵌 `animationTree` 的角色（BlendSpace1D  locomotion、Add2 转向、OneShot trip/SYNC） |
| Edit 策略 | Edit 预览时**不要**设 `BLUNDER_DOTNET_SCRIPTS=1`（Behaviour Tick 必须关） |

1. 用本 worktree 构建的 `engine_editor.exe` 打开 Test Project。
2. 加载带 Chocomel（或约定子集）且 AnimationTree 已激活的场景。
3. 确认角色 Object 上共置 Skeleton + AnimationPlayer + AnimationTree。

**先跑自动化门禁（工程门槛）：**

```powershell
cd E:\Dev\Blunder-Engine\.worktrees\dogwalk-animation-phase-4
cmake --build build/vs2026-debug --config Debug --target dogwalk_phase4_tree_gate_test dogwalk_phase4_mini_play_acceptance_test
.\build\vs2026-debug\engine\src\tests\Debug\dogwalk_phase4_tree_gate_test.exe
.\build\vs2026-debug\engine\src\tests\Debug\dogwalk_phase4_mini_play_acceptance_test.exe
```

---

## A. Edit 模式 — AnimationTree 擦洗（无 Behaviour Tick）

- [ ] **A1 — 激活树 + Travel**  
  Edit 下激活 AnimationTree 并 Travel/Start 到 Locomotion；无需 Play 会话即可看到骨骼更新。

- [ ] **A2 — BlendSpace 标量擦洗**  
  擦洗 BlendSpace1D 标量（速度类）；角色上可感知 locomotion 混合变化。

- [ ] **A3 — OneShot + Add2 擦洗**  
  请求 OneShot（trip 类 clip）并擦洗 Add2 权重/clip；OneShot 结束后回到 base；非零 Add2 权重时叠加可见。

- [ ] **A4 — TimeScale 擦洗**  
  树激活时擦洗 AnimationPlayer TimeScale；预览快慢变化。A1–A4 期间无 Behaviour Tick。

**通过：** Edit 树擦洗无需 DotNetHost / Behaviour Tick，也无需可视化图编辑器。

---

## B. Play — Chocomel 子集验收

工程 harness（`dogwalk_phase4_mini_play_acceptance_test`）覆盖 Play 路径 BlendSpace、Add2、OneShot 回 base、主导 base 时钟、Sync Fire→OneShot。**人工 Play** 仍须验证真实 Chocomel 内容与手感。

- [ ] **B1 — BlendSpace locomotion**  
  Play 下可感知速度类 BlendSpace 运动（idle↔walk 或约定子集 clip）。

- [ ] **B2 — 可见 additive 转向**  
  Add2 转向/吠叫叠加在 locomotion base 上可见（不要求与 Godot clip 名完全一致）。

- [ ] **B3 — OneShot 回 base**  
  Trip/打断 OneShot 播放后回到 locomotion base，树保持激活。

- [ ] **B4 — base 主导时钟的 Stepped 朝向**  
  Stepped 朝向仍跟 **base 主导 clip** 时钟（非 Add2）；位移仍由 Tick 实时驱动。

**通过：** Chocomel 子集 Play 手感符合预期；树保持激活；步进同步用 base 时钟。

---

## C. 回归 / 更早阶段门禁

- [ ] **C1 — Phase 3 mini SYNC+CINE**  
  Phase 3 `tasks.md` **5.2** 若未完成则仍 **open** — Phase 4 不能代 Mark Phase 3 Done。

- [ ] **C2 — Phase 2 Chocomel 加权条**  
  Phase 2 `tasks.md` **5.3** 若未完成则仍 **open**。

- [ ] **C3 — Phase 1 Chocomel 硬切条**  
  Phase 1 `tasks.md` **6.4** 若未完成则仍 **open**。

**通过：** Phase 4 不回归 Phase 1–3；不静默关闭更早内容门禁。

---

## 签收

| 字段 | 值 |
|-------|--------|
| 执行人 | |
| 日期 | |
| 构建 / commit | |
| 结果 | ☐ 通过 &nbsp; ☐ 失败（下方注明阻塞项） |
| 备注 | |

全部通过后，人工可将 `tasks.md` **6.2** 标为完成。本文件**不会**自动完成该任务。
