#!/usr/bin/env python3
"""Submit burst pattern + engine thread CPU on tid 282068487539252."""
import sqlite3
from pathlib import Path

DB = Path(__file__).resolve().parent / "blunder_A_debug_baseline.sqlite"
ENGINE_TID = 282068487539252
NS_MS = 1_000_000
NS_S = 1_000_000_000

def main():
    c = sqlite3.connect(f"file:{DB}?mode=ro", uri=True)
    cur = c.cursor()
    t0, t1, dur = cur.execute(
        "SELECT startTime, stopTime, duration FROM ANALYSIS_DETAILS"
    ).fetchone()
    dur_s = dur / NS_S

    cur.execute("""
        SELECT v.start FROM VULKAN_API v
        JOIN StringIds s ON s.id=v.nameId
        WHERE s.value='vkQueueSubmit' ORDER BY v.start
    """)
    starts = [r[0] for r in cur.fetchall()]
    rel = [(s - t0) / NS_S for s in starts]
    print(f"vkQueueSubmit: {len(starts)} over {dur_s:.1f}s = {len(starts)/dur_s:.2f}/s effective")
    print(f"first submit @ {rel[0]:.2f}s  last @ {rel[-1]:.2f}s  active span {rel[-1]-rel[0]:.2f}s")

    # idle gaps > 100ms
    gaps = [(rel[i+1]-rel[i]) for i in range(len(rel)-1)]
    long_idle = [g for g in gaps if g > 0.1]
    print(f"idle gaps >100ms: {len(long_idle)}  total idle {sum(long_idle):.1f}s")
    if long_idle:
        print(f"  longest idle: {max(long_idle):.1f}s")

    # timeline buckets 1s
    buckets = [0] * int(dur_s + 2)
    for r in rel:
        bi = int(r)
        if bi < len(buckets):
            buckets[bi] += 1
    print("submits per second:")
    for i, n in enumerate(buckets[:int(dur_s)+1]):
        if n:
            bar = "#" * n
            print(f"  {i:2d}s: {n:2d} {bar}")

    # Engine thread composite event rate
    cur.execute(
        "SELECT COUNT(*) FROM COMPOSITE_EVENTS WHERE globalTid=?",
        (ENGINE_TID,),
    )
    comp = cur.fetchone()[0]
    print(f"\nengine main-ish thread composite events: {comp:,} ({comp/dur_s:.0f}/s)")

    # Thread name
    cur.execute("""
        SELECT s.value FROM ThreadNames tn
        JOIN StringIds s ON s.id=tn.nameId WHERE tn.globalTid=?
    """, (ENGINE_TID,))
    nm = cur.fetchone()
    print(f"thread name: {nm[0] if nm else 'unnamed'}")

    # SCHED blocking on engine thread
    cur.execute("PRAGMA table_info(SCHED_EVENTS)")
    sched_cols = [r[1] for r in cur.fetchall()]
    print(f"SCHED_EVENTS cols: {sched_cols}")

    if "globalTid" in sched_cols:
        cur.execute(
            "SELECT COUNT(*) FROM SCHED_EVENTS WHERE globalTid=?",
            (ENGINE_TID,),
        )
        print(f"SCHED events on engine tid: {cur.fetchone()[0]:,}")

    # MapMemory / memcpy in VULKAN or sampling - check StringIds for Map
    cur.execute("""
        SELECT value, COUNT(*) FROM StringIds s
        WHERE value LIKE '%MapMemory%' OR value LIKE '%Read%'
           OR value LIKE '%copy%' OR value LIKE '%staging%'
        LIMIT 5
    """)
    # wrong - need join sampling

    cur.execute("""
        SELECT s.value, COUNT(*) cnt FROM SAMPLING_CALLCHAINS sc
        JOIN StringIds s ON s.id=sc.symbol
        WHERE s.value LIKE '%MapMemory%' OR s.value LIKE '%vkMap%'
           OR s.value LIKE '%memcpy%' OR s.value LIKE '%ReadPixels%'
        GROUP BY s.value ORDER BY cnt DESC LIMIT 15
    """)
    rows = cur.fetchall()
    print("\nCPU samples (resolved strings only):")
    if rows:
        for name, cnt in rows:
            print(f"  {cnt:7,}  {name[:80]}")
    else:
        print("  (no resolved symbols — PDB not applied to call stacks)")

    # Check ANALYSIS / debug symbols metadata
    cur.execute("SELECT name FROM sqlite_master WHERE name LIKE 'META%'")
    print("\nMETA tables:", [r[0] for r in cur.fetchall()])

    c.close()

if __name__ == "__main__":
    main()
