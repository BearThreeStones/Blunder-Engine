#!/usr/bin/env python3
"""Compare Group A vs B sqlite exports."""
import sqlite3
from pathlib import Path
from statistics import median

REPORTS = Path(__file__).resolve().parent
NS_MS = 1_000_000
NS_S = 1_000_000_000


def analyze(path: Path, label: str):
    c = sqlite3.connect(f"file:{path}?mode=ro", uri=True)
    cur = c.cursor()
    dur = cur.execute("SELECT duration FROM ANALYSIS_DETAILS").fetchone()[0] / NS_S

    cur.execute("""
        SELECT COUNT(*) FROM VULKAN_API v
        JOIN StringIds s ON s.id=v.nameId WHERE s.value='vkQueueSubmit'
    """)
    submits = cur.fetchone()[0]

    cur.execute("""
        SELECT v.start FROM VULKAN_API v
        JOIN StringIds s ON s.id=v.nameId
        WHERE s.value='vkQueueSubmit' ORDER BY v.start
    """)
    starts = [r[0] for r in cur.fetchall()]
    gaps = [(starts[i+1]-starts[i])/NS_MS for i in range(len(starts)-1)] if len(starts) > 1 else []
    normal = sorted(g for g in gaps if g < 500)

    cur.execute("SELECT COUNT(*), AVG(end-start) FROM VULKAN_WORKLOAD")
    wl_cnt, wl_avg = cur.fetchone()

    cur.execute("""
        SELECT AVG(v.end-v.start), COUNT(*) FROM VULKAN_API v
        JOIN StringIds s ON s.id=v.nameId WHERE s.value='vkWaitForFences'
    """)
    fence_avg, fence_cnt = cur.fetchone()

    # engine tid from submits
    cur.execute("""
        SELECT v.globalTid, COUNT(*) FROM VULKAN_API v
        JOIN StringIds s ON s.id=v.nameId
        WHERE s.value='vkQueueSubmit' GROUP BY v.globalTid
    """)
    tid_info = cur.fetchall()

    print(f"\n{'='*50}")
    print(f"{label}  ({path.name})")
    print(f"  duration: {dur:.2f}s")
    print(f"  vkQueueSubmit: {submits}  ({submits/dur:.2f}/s avg)")
    if normal:
        print(f"  submit gap (excl >500ms): median={median(normal):.1f}ms  max={normal[-1]:.1f}ms")
    if wl_cnt:
        print(f"  VULKAN_WORKLOAD: {wl_cnt}  GPU avg={wl_avg/NS_MS:.3f}ms")
    if fence_cnt:
        print(f"  vkWaitForFences: {fence_cnt}x  avg={fence_avg/NS_MS:.3f}ms")
    print(f"  submit thread: {tid_info}")
    c.close()


def main():
    analyze(REPORTS / "blunder_A_debug_baseline.sqlite", "Group A (CPU readback baseline)")
    analyze(REPORTS / "blunder_B_debug_zerocopy.sqlite", "Group B (zero-copy)")


if __name__ == "__main__":
    main()
