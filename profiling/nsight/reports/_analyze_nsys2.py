#!/usr/bin/env python3
import sqlite3
from pathlib import Path
from statistics import median

DB = Path(__file__).resolve().parent / "blunder_A_debug_baseline.sqlite"
NS_MS = 1_000_000
NS_S = 1_000_000_000

def main():
    c = sqlite3.connect(f"file:{DB}?mode=ro", uri=True)
    cur = c.cursor()
    dur = cur.execute("SELECT duration FROM ANALYSIS_DETAILS").fetchone()[0] / NS_S

    print("=== Vulkan submit threads ===")
    cur.execute("""
        SELECT v.globalTid, COUNT(*), SUM(v.end-v.start)
        FROM VULKAN_API v
        JOIN StringIds s ON s.id=v.nameId
        WHERE s.value='vkQueueSubmit'
        GROUP BY v.globalTid ORDER BY COUNT(*) DESC
    """)
    for tid, cnt, tot in cur.fetchall():
        cur.execute("""
            SELECT s.value FROM ThreadNames tn
            JOIN StringIds s ON s.id=tn.nameId WHERE tn.globalTid=?
        """, (tid,))
        nm = cur.fetchone()
        name = nm[0] if nm else "?"
        print(f"  tid={tid}  submits={cnt}  total={tot/NS_MS:.2f}ms  {name}")

    print("\n=== Symbol resolution (Blunder in StringIds) ===")
    for pat in ["%blunder%", "%Blunder%", "%engine_editor%", "%RenderSystem%", "%slint_system%"]:
        cur.execute("SELECT COUNT(*) FROM StringIds WHERE value LIKE ?", (pat,))
        n = cur.fetchone()[0]
        print(f"  {pat}: {n} strings")
    cur.execute("""
        SELECT value FROM StringIds
        WHERE value LIKE '%blunder%' OR value LIKE '%Blunder%'
           OR value LIKE '%RenderSystem%' OR value LIKE '%tickVulkan%'
        LIMIT 20
    """)
    for r in cur.fetchall():
        print(f"    {r[0][:100]}")

    print("\n=== vkWaitForFences timing (GPU stall proxy) ===")
    cur.execute("""
        SELECT v.end-v.start FROM VULKAN_API v
        JOIN StringIds s ON s.id=v.nameId
        WHERE s.value='vkWaitForFences'
        ORDER BY v.end-v.start DESC LIMIT 10
    """)
    waits = [r[0]/NS_MS for r in cur.fetchall()]
    print(f"  top waits ms: {[f'{w:.1f}' for w in waits]}")
    cur.execute("""
        SELECT AVG(v.end-v.start), COUNT(*) FROM VULKAN_API v
        JOIN StringIds s ON s.id=v.nameId WHERE s.value='vkWaitForFences'
    """)
    avg, cnt = cur.fetchone()
    print(f"  avg={avg/NS_MS:.2f}ms over {cnt} calls")

    print("\n=== Submit gaps (excluding >500ms outliers) ===")
    cur.execute("""
        SELECT v.start FROM VULKAN_API v
        JOIN StringIds s ON s.id=v.nameId
        WHERE s.value='vkQueueSubmit' ORDER BY v.start
    """)
    starts = [r[0] for r in cur.fetchall()]
    gaps = [(starts[i+1]-starts[i])/NS_MS for i in range(len(starts)-1)]
    normal = [g for g in gaps if g < 500]
    if normal:
        normal.sort()
        print(f"  count={len(normal)}  min={normal[0]:.1f}  median={median(normal):.1f}  max={normal[-1]:.1f} ms")
        # bucket distribution
        buckets = {"<16ms":0, "16-33ms":0, "33-50ms":0, "50-100ms":0, "100-500ms":0}
        for g in normal:
            if g < 16: buckets["<16ms"] += 1
            elif g < 33: buckets["16-33ms"] += 1
            elif g < 50: buckets["33-50ms"] += 1
            elif g < 100: buckets["50-100ms"] += 1
            else: buckets["100-500ms"] += 1
        print(f"  gap buckets: {buckets}")
        print(f"  implied steady FPS (median gap): {1000/median(normal):.1f}")

    print("\n=== PROCESSES with most VULKAN activity ===")
    # correlate via thread - get pid from high bits?
    cur.execute("SELECT globalPid, name FROM PROCESSES")
    procs = {r[0]: r[1] for r in cur.fetchall()}
    cur.execute("""
        SELECT v.globalTid, COUNT(*) FROM VULKAN_API v GROUP BY v.globalTid ORDER BY 2 DESC LIMIT 5
    """)
    for tid, cnt in cur.fetchall():
        # try match process by tid prefix
        tid_s = str(tid)
        match = [name for pid, name in procs.items() if str(pid)[:10] in tid_s[:15]]
        print(f"  tid={tid}  vk_calls={cnt}  proc_guess={match}")

    c.close()

if __name__ == "__main__":
    main()
