import sqlite3
from pathlib import Path
DB = Path(__file__).resolve().parent / "blunder_A_debug_baseline.sqlite"
c = sqlite3.connect(f"file:{DB}?mode=ro", uri=True)
cur = c.cursor()
cur.execute("SELECT * FROM TARGET_INFO_SESSION_START_TIME LIMIT 5")
print("SESSION_START", cur.fetchall())
cur.execute("SELECT MIN(start), MAX(start), COUNT(*) FROM COMPOSITE_EVENTS")
print("COMPOSITE start", cur.fetchone())
# normalize vulkan to 0 by subtracting min
cur.execute("""
    SELECT v.start - (SELECT MIN(start) FROM VULKAN_API),
           v.end - (SELECT MIN(start) FROM VULKAN_API)
    FROM VULKAN_API v JOIN StringIds s ON s.id=v.nameId
    WHERE s.value='vkQueueSubmit' ORDER BY v.start
""")
subs = cur.fetchall()
print(f"submits {len(subs)} span {(subs[-1][0]-subs[0][0])/1e9:.2f}s")
gaps = [(subs[i+1][0]-subs[i][0])/1e6 for i in range(len(subs)-1)]
from statistics import median
gaps_s = sorted(gaps)
print(f"gap ms: min={gaps_s[0]:.1f} med={median(gaps_s):.1f} max={gaps_s[-1]:.1f}")
# per-second histogram over vulkan timeline
mx = subs[-1][0]
bins = [0]*int(mx/1e9+2)
for s,e in subs:
    bins[int(s/1e9)] += 1
print("per second (vulkan rel time):")
for i,n in enumerate(bins):
    if n: print(f"  +{i}s: {n}")
