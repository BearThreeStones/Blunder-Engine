import sqlite3
from pathlib import Path
from statistics import median

DB = Path(__file__).resolve().parent / "blunder_A_debug_baseline.sqlite"
ENGINE_TID = 282068487539252
NS = 1_000_000_000

c = sqlite3.connect(f"file:{DB}?mode=ro", uri=True)
cur = c.cursor()
dur = cur.execute("SELECT duration FROM ANALYSIS_DETAILS").fetchone()[0] / NS

# Composite timeline is 0..15s (starts at 100ns)
print("=== Engine thread COMPOSITE_EVENTS per second ===")
cur.execute("""
    SELECT (start / 1000000000) AS sec, COUNT(*)
    FROM COMPOSITE_EVENTS WHERE globalTid=?
    GROUP BY sec ORDER BY sec
""", (ENGINE_TID,))
rows = cur.fetchall()
for sec, n in rows:
    print(f"  {sec:2.0f}s: {n:5,}")

# Compare all threads during sec 5-7 (vulkan burst window - approximate)
# Map vulkan time to composite: need offset
vk_min = cur.execute("SELECT MIN(start) FROM VULKAN_API").fetchone()[0]
sub_min, sub_max = cur.execute("""
    SELECT MIN(v.start), MAX(v.start) FROM VULKAN_API v
    JOIN StringIds s ON s.id=v.nameId WHERE s.value='vkQueueSubmit'
""").fetchone()
print(f"\nVulkan submit window (raw): {sub_min} .. {sub_max} ({(sub_max-sub_min)/NS:.2f}s)")
print(f"Vulkan API min: {vk_min}")

# CPU cycles on engine thread - threadState distribution
cur.execute("PRAGMA table_info(COMPOSITE_EVENTS)")
print("cols", [r[1] for r in cur.fetchall()])
cur.execute("""
    SELECT threadState, COUNT(*) FROM COMPOSITE_EVENTS
    WHERE globalTid=? GROUP BY threadState ORDER BY 2 DESC
""", (ENGINE_TID,))
print("\nEngine thread states:")
for st, n in cur.fetchall():
    cur.execute("SELECT name FROM ENUM_SAMPLING_THREAD_STATE WHERE id=?", (st,))
    nm = cur.fetchone()
    print(f"  {nm[0] if nm else st}: {n:,}")

# NVTX if present
cur.execute("SELECT name FROM sqlite_master WHERE name LIKE '%NVTX%' OR name LIKE '%Nvtx%'")
print("\nNvtx tables:", [r[0] for r in cur.fetchall()])

c.close()
