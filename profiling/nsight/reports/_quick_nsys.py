#!/usr/bin/env python3
import sqlite3
from pathlib import Path

DB = Path(__file__).resolve().parent / "blunder_A_debug_baseline.sqlite"
NS_MS = 1_000_000
NS_S = 1_000_000_000

def q(cur, sql, params=()):
    cur.execute(sql, params)
    return cur.fetchall()

def main():
    c = sqlite3.connect(f"file:{DB}?mode=ro", uri=True)
    cur = c.cursor()
    t0, t1, dur = q(cur, "SELECT startTime, stopTime, duration FROM ANALYSIS_DETAILS LIMIT 1")[0]
    dur_s = dur / NS_S
    print(f"Capture: {dur_s:.2f}s")

    print("\n-- VULKAN_WORKLOAD rows --")
    n = q(cur, "SELECT COUNT(*) FROM VULKAN_WORKLOAD")[0][0]
    print(f"total rows: {n}")
    if n:
        row = q(cur, "SELECT COUNT(*), AVG(end-start), MIN(end-start), MAX(end-start) FROM VULKAN_WORKLOAD")[0]
        print(row)

    print("\n-- COMPOSITE_EVENTS rate --")
    n = q(cur, "SELECT COUNT(*) FROM COMPOSITE_EVENTS")[0][0]
    print(f"total: {n:,}  rate: {n/dur_s:.0f}/s")

    print("\n-- Top threads by composite events --")
    for r in q(cur, """
        SELECT c.globalTid, COUNT(*) cnt
        FROM COMPOSITE_EVENTS c
        GROUP BY c.globalTid ORDER BY cnt DESC LIMIT 10
    """):
        tid, cnt = r
        name = q(cur, """
            SELECT s.value FROM ThreadNames tn
            JOIN StringIds s ON s.id=tn.nameId WHERE tn.globalTid=?
        """, (tid,))
        nm = name[0][0] if name else "?"
        print(f"  {cnt:8,}/s~{cnt/dur_s:6.0f}  tid={tid}  {nm}")

    print("\n-- symbol column sample --")
    for r in q(cur, "SELECT symbol, typeof(symbol), COUNT(*) FROM SAMPLING_CALLCHAINS GROUP BY symbol LIMIT 5"):
        print(r)

    print("\n-- StringIds: render keywords --")
    for kw in ["tickVulkan", "compositeFrame", "SkiaRenderer", "copyColor", "QueueSubmit", "memcpy"]:
        rows = q(cur, "SELECT id, value FROM StringIds WHERE value LIKE ? LIMIT 5", (f"%{kw}%",))
        if rows:
            print(f"  {kw}: {len(rows)} matches, e.g. {rows[0][1][:80]}")

    print("\n-- VULKAN_API submit --")
    rows = q(cur, """
        SELECT s.value, COUNT(*), AVG(v.end-v.start)
        FROM VULKAN_API v JOIN StringIds s ON s.id=v.nameId
        WHERE s.value LIKE '%Submit%'
        GROUP BY s.value
    """)
    for r in rows:
        print(f"  {r[1]:5} avg={r[2]/NS_MS:.3f}ms  {r[0]}")

    c.close()

if __name__ == "__main__":
    main()
