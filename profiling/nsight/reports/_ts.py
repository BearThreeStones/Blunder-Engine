import sqlite3
from pathlib import Path
DB = Path(__file__).resolve().parent / "blunder_A_debug_baseline.sqlite"
c = sqlite3.connect(f"file:{DB}?mode=ro", uri=True)
cur = c.cursor()
t0, t1, dur = cur.execute("SELECT startTime, stopTime, duration FROM ANALYSIS_DETAILS").fetchone()
print("ANALYSIS", t0, t1, dur/1e9)
cur.execute("SELECT MIN(start), MAX(end), COUNT(*) FROM VULKAN_API")
print("VULKAN_API", cur.fetchone())
cur.execute("SELECT MIN(start), MAX(cpu), COUNT(*) FROM COMPOSITE_EVENTS")
print("COMPOSITE", cur.fetchone())
cur.execute("SELECT MIN(start), MAX(start) FROM VULKAN_API v JOIN StringIds s ON s.id=v.nameId WHERE s.value='vkQueueSubmit'")
print("SUBMIT range", cur.fetchone())
