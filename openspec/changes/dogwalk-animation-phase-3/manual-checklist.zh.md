# 人工验收清单 — DogWalk 动画 Phase 3

对应任务 **6.2**。自动化门禁（`dogwalk_phase3_sync_cine_gate_test`、`dogwalk_phase3_mini_play_acceptance_test`、`animation_sync_group_test`、`cine_segment_service_test` 等）**不能**替代本清单。

**状态：** 未跑 — 路径已写死。人工每完成一步勾选。

英文版：`manual-checklist.md`  
场景说明：`E:\Blunder Projects\Test\Assets\Scenes\PHASE3_SYNC_CINE.md`

---

## 前置条件

| 项 | 值 |
|------|--------|
| OpenSpec 变更 | `openspec/changes/dogwalk-animation-phase-3/` |
| 引擎 worktree | `E:\Dev\Blunder-Engine\.worktrees\dogwalk-animation-phase-3` |
| 配置（一次） | 在 worktree 根目录：`cmake --preset vs2026-debug` |
| 构建（Debug） | `cmake --build build/vs2026-debug --config Debug --target engine_editor engine_player dogwalk_phase3_sync_cine_gate_test dogwalk_phase3_mini_play_acceptance_test` |
| 二进制 | `build\vs2026-debug\bin\Debug\engine_editor.exe`、`engine_player.exe` |
| 自动化门禁 | `build\vs2026-debug\engine\src\tests\Debug\dogwalk_phase3_sync_cine_gate_test.exe`、`dogwalk_phase3_mini_play_acceptance_test.exe` |
| Test Project | `E:\Blunder Projects\Test` |
| 入口场景 | `E:\Blunder Projects\Test\Assets\Scenes\phase3_sync_cine.scene.asset`（Character + Partner） |
| Edit 策略 | Edit 预览时**不要**设 `BLUNDER_DOTNET_SCRIPTS=1`（Behaviour Tick 必须关） |

1. 用本 worktree 构建的 `engine_editor.exe` 打开 Test Project。
2. 打开 Phase 3 小验收场景：`Assets/Scenes/phase3_sync_cine.scene.asset`。
3. 确认至少两个 Object 各自有共置 Skeleton + AnimationPlayer（`Character`、`Partner`）。

**先跑自动化门禁（工程门槛）：**

```powershell
cd E:\Dev\Blunder-Engine\.worktrees\dogwalk-animation-phase-3
cmake --build build/vs2026-debug --config Debug --target dogwalk_phase3_sync_cine_gate_test dogwalk_phase3_mini_play_acceptance_test
.\build\vs2026-debug\engine\src\tests\Debug\dogwalk_phase3_sync_cine_gate_test.exe
.\build\vs2026-debug\engine\src\tests\Debug\dogwalk_phase3_mini_play_acceptance_test.exe
```

---

## A. Edit 模式 — Sync Group + CINE 标记（无 Behaviour Tick）

- [ ] **A1 — 多 Player Fire**  
  Edit 下对 Sync Group 做 Fire（成员可用不同 clip 名）。两个 Skeleton 硬切对齐开播。  
  *提示：工具栏多选 Fire 走同名 `fireSameName`（S0）；异名 Fire 以 Play 段 Behaviour 为主。*

- [ ] **A2 — in-CINE 可见**  
  Enter CINE；确认 in-CINE（及若有的输入抑制标记）可见。End CINE；标记清除。

- [ ] **A3 — 无 Behaviour / 无自动 TRS**  
  A1–A2 期间：无 Play 会话、无 DotNetHost Behaviour Tick；仅 Enter/End **不会**自动吸附 Object 位置。

**通过标准：** Edit 下多 Skeleton Fire + CINE 标记可用，且不依赖玩法脚本。

---

## B. Play 小验收 — SYNC + CINE

工程 harness（`dogwalk_phase3_mini_play_acceptance_test`）已覆盖同步开播、CINE 输入抑制/恢复、显式 End。**产品手感仍需人工 Play**（Behaviour、视口、真实内容）。

场景 `phase3_sync_cine`：进 Play 后 `Phase3SyncCineDemo` 会 Fire（Character=`walk`、Partner=`idle`）→ Enter CINE（约 2s 禁 WASD）→ End 交回控制；Space 可再触发。

- [ ] **B1 — 同步开播**  
  Play 下角色 + 道具/搭档经 Sync Group 同时开播（不同 clip 名可接受）。

- [ ] **B2 — CINE 交接**  
  Enter CINE 按设计抑制或移交玩法控制；End 后控制交回玩家。

- [ ] **B3 — 显式 End**  
  确认结束段必须走 End（或脚本调用 End）；仅某一 clip 播完**不会**卡在 in-CINE。

**通过标准：** 迷你 DogWalk 式同步 + 交接手感明确；控制干净交回。

---

## C. 回归 / 并行门禁

- [ ] **C1 — Phase 1/2 单 Player 路径**  
  单角色硬切，以及（若有）双槽 blend 仍可用。

- [ ] **C2 — Phase 2 Chocomel 加权门槛**  
  Phase 2 `tasks.md` **5.3**（Chocomel idle↔walk 加权 Play 验收）仍为 **open** — Phase 3 小验收**不得**顺手勾掉 Phase 2 Done。

**通过标准：** Phase 3 不回退 Phase 1/2 播放；Phase 2 内容门禁不被静默关闭。

---

## 签字

| 字段 | 值 |
|-------|--------|
| 执行人 | |
| 日期 | |
| 构建 / commit | |
| 结果 | ☐ 通过 &nbsp; ☐ 失败（下方记阻塞） |
| 备注 | |

全部章节通过后，人工才可勾 `tasks.md` **6.2**。本文件**不会**自动完成该任务。
