import sqlite3
from pathlib import Path
DB = Path(__file__).resolve().parent / "blunder_A_debug_baseline.sqlite"
c = sqlite3.connect(f"file:{DB}?mode=ro", uri=True)
cur = c.cursor()
cur.execute("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name")
tables = [r[0] for r in cur.fetchall()]
print("tables:", len(tables))
for t in tables:
    if any(x in t.lower() for x in ("thread", "process", "pid", "tid")):
        cur.execute(f"PRAGMA table_info({t})")
        cols = [r[1] for r in cur.fetchall()]
        print(f"  {t}: {cols}")
