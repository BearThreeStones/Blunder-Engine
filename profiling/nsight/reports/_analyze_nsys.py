#!/usr/bin/env python3
"""Full analysis of blunder_A_debug_baseline.sqlite for viewport FPS."""
import sqlite3
from pathlib import Path
from statistics import median

DB = Path(__file__).resolve().parent / "blunder_A_debug_baseline.sqlite"
NS_MS = 1_000_000
NS_S = 1_000_000_000

ENGINE_PID = None  # filled at runtime


def connect():
    return sqlite3.connect(f"file:{DB}?mode=ro", uri=True)


def fmt_ms(ns):
    if ns is None:
        return "n/a"
    return f"{ns / NS_MS:.3f}ms"


def engine_tid_prefix(cur):
    global ENGINE_PID
    cur.execute(
        "SELECT globalPid FROM PROCESSES WHERE name LIKE '%engine_editor%'"
    )
    row = cur.fetchone()
    if not row:
        return None
    ENGINE_PID = row[0]
    # Nsight encodes pid in high bits of globalTid on Windows
    return str(ENGINE_PID)[:12]


def is_engine_tid(tid, prefix):
    return str(tid).startswith(prefix)


def symbol_ids(cur, pattern):
    cur.execute("SELECT id, value FROM StringIds WHERE value LIKE ?", (pattern,))
    return cur.fetchall()


def count_symbol_hits(cur, sym_ids):
    if not sym_ids:
        return 0
    ph = ",".join("?" * len(sym_ids))
    cur.execute(
        f"SELECT COUNT(*) FROM SAMPLING_CALLCHAINS WHERE symbol IN ({ph})",
        sym_ids,
    )
    return cur.fetchone()[0]


def main():
    con = connect()
    cur = con.cursor()
    t0, t1, dur = cur.execute(
        "SELECT startTime, stopTime, duration FROM ANALYSIS_DETAILS LIMIT 1"
    ).fetchone()
    dur_s = dur / NS_S

    print("=" * 60)
    print("BLUNDER Group A (debug baseline) — Nsight SQLite 分析")
    print("=" * 60)
    print(f"时长: {dur_s:.2f}s")

    prefix = engine_tid_prefix(cur)
    print(f"engine_editor globalPid: {ENGINE_PID}")
    print(f"engine thread id 前缀: {prefix}")

    # Engine threads
    print("\n--- engine_editor 线程 (composite 事件) ---")
    cur.execute(
        """
        SELECT c.globalTid, COUNT(*) cnt
        FROM COMPOSITE_EVENTS c
        GROUP BY c.globalTid
        """
    )
    engine_threads = []
    for tid, cnt in cur.fetchall():
        if prefix and is_engine_tid(tid, prefix):
            cur.execute(
                """
                SELECT s.value FROM ThreadNames tn
                JOIN StringIds s ON s.id = tn.nameId
                WHERE tn.globalTid = ?
                """,
                (tid,),
            )
            nm = cur.fetchone()
            name = nm[0] if nm else "(unnamed)"
            engine_threads.append((tid, cnt, name))
    engine_threads.sort(key=lambda x: -x[1])
    if not engine_threads:
        print("  未按前缀匹配到线程，列出所有命名线程:")
        cur.execute(
            """
            SELECT tn.globalTid, s.value FROM ThreadNames tn
            JOIN StringIds s ON s.id = tn.nameId
            WHERE s.value LIKE '%engine%' OR s.value LIKE '%Blunder%'
               OR s.value LIKE '%Slint%' OR s.value LIKE '%Vulkan%'
            """
        )
        for tid, name in cur.fetchall():
            cur.execute(
                "SELECT COUNT(*) FROM COMPOSITE_EVENTS WHERE globalTid=?",
                (tid,),
            )
            cnt = cur.fetchone()[0]
            print(f"  tid={tid}  events={cnt:,}  {name}")
    else:
        for tid, cnt, name in engine_threads[:12]:
            print(f"  {cnt/dur_s:6.0f} ev/s  tid={tid}  {name}")

    # Vulkan
    print("\n--- Vulkan GPU workload ---")
    cur.execute(
        "SELECT COUNT(*), AVG(end-start), MIN(end-start), MAX(end-start) FROM VULKAN_WORKLOAD"
    )
    cnt, avg, mn, mx = cur.fetchone()
    print(f"  workload 数: {cnt}  ({cnt/dur_s:.2f}/s)")
    print(f"  GPU 时长: avg={fmt_ms(avg)}  min={fmt_ms(mn)}  max={fmt_ms(mx)}")
    cur.execute(
        """
        SELECT s.value, COUNT(*), AVG(v.end-v.start)
        FROM VULKAN_WORKLOAD v JOIN StringIds s ON s.id=v.nameId
        GROUP BY s.value ORDER BY COUNT(*) DESC
        """
    )
    for name, c, a in cur.fetchall():
        print(f"    {c:3} x avg={fmt_ms(a)}  {name}")

    print("\n--- vkQueueSubmit 频率 (3D tick 代理) ---")
    cur.execute(
        """
        SELECT v.start, v.end FROM VULKAN_API v
        JOIN StringIds s ON s.id=v.nameId
        WHERE s.value = 'vkQueueSubmit'
        ORDER BY v.start
        """
    )
    submits = cur.fetchall()
    print(f"  vkQueueSubmit: {len(submits)} 次  ({len(submits)/dur_s:.2f}/s)")
    if len(submits) >= 2:
        gaps = [(submits[i + 1][0] - submits[i][0]) / NS_MS for i in range(len(submits) - 1)]
        gaps_sorted = sorted(gaps)
        print(
            f"  间隔: min={gaps_sorted[0]:.1f}ms  "
            f"median={median(gaps_sorted):.1f}ms  max={gaps_sorted[-1]:.1f}ms"
        )

    # Top Vulkan API by total time
    print("\n--- Vulkan API (按总耗时 top 15) ---")
    cur.execute(
        """
        SELECT s.value, COUNT(*), SUM(v.end-v.start), AVG(v.end-v.start)
        FROM VULKAN_API v JOIN StringIds s ON s.id=v.nameId
        GROUP BY s.value
        ORDER BY SUM(v.end-v.start) DESC
        LIMIT 15
        """
    )
    for name, c, tot, avg in cur.fetchall():
        print(f"  total={fmt_ms(tot):>10}  {c:4}x avg={fmt_ms(avg):>8}  {name}")

    # CPU sampling hotspots — resolve symbol ids first
    print("\n--- CPU 采样热点 (Blunder / 渲染相关) ---")
    patterns = [
        ("engine tick", "%tickOneFrame%"),
        ("RenderSystem::tickVulkan", "%tickVulkan%"),
        ("compositeFrame", "%compositeFrame%"),
        ("SkiaRenderer", "%SkiaRenderer%"),
        ("copyColor/readback", "%copyColor%"),
        ("pollAndPresent", "%pollAndPresent%"),
        ("setViewportImage", "%setViewportImage%"),
        ("SlintSystem", "%SlintSystem%"),
        ("memcpy", "%memcpy%"),
        ("MapMemory", "%MapMemory%"),
        ("ForwardRender", "%ForwardRender%"),
        ("BlunderEngine", "%BlunderEngine%"),
        ("endFrame", "%endFrame%"),
        ("Present", "%Present%"),
        ("slint", "%slint%"),
        ("skia", "%Skia%"),
    ]
    for label, pat in patterns:
        rows = symbol_ids(cur, pat)
        if not rows:
            continue
        ids = [r[0] for r in rows]
        hits = count_symbol_hits(cur, ids)
        if hits == 0:
            continue
        top = rows[0][1][:90]
        extra = f" (+{len(rows)-1} symbols)" if len(rows) > 1 else ""
        print(f"  {label:28} {hits:9,} samples  e.g. {top}{extra}")

    print("\n--- CPU 采样全局 Top 20 (按 symbol id 聚合) ---")
    cur.execute(
        """
        SELECT sc.symbol, COUNT(*) hits
        FROM SAMPLING_CALLCHAINS sc
        GROUP BY sc.symbol
        ORDER BY hits DESC
        LIMIT 20
        """
    )
    top_syms = cur.fetchall()
    for sym_id, hits in top_syms:
        cur.execute("SELECT value FROM StringIds WHERE id=?", (sym_id,))
        row = cur.fetchone()
        name = row[0][:85] if row else f"id={sym_id}"
        print(f"  {hits:9,}  {name}")

    print("\n--- 结论摘要 ---")
    submit_rate = len(submits) / dur_s if submits else 0
    print(f"  • 捕获为全系统 trace；engine 线程通过 globalTid 前缀 {prefix} 过滤")
    print(f"  • Vulkan submit ~{submit_rate:.1f}/s → 3D 离屏渲染约每 {1000/submit_rate:.0f}ms 一帧" if submit_rate else "  • 无 vkQueueSubmit 数据")
    print(f"  • Nsight UI「Frame duration」≈ Skia Present，不是 engine tick")
    print(f"  • Group A 使用 CPU readback；viewport 刷新受 m_viewport_frame_ready + present 节流 (~50ms)")

    con.close()


if __name__ == "__main__":
    main()
