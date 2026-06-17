import sqlite3
from pathlib import Path
DB = Path(__file__).resolve().parent / "blunder_A_debug_baseline.sqlite"
c = sqlite3.connect(f"file:{DB}?mode=ro", uri=True)
cur = c.cursor()
for t in ["META_DATA_CAPTURE","META_DATA_EXPORT","META_DATA_SESSION","META_DATA_DEVICE","ProcessStreams"]:
    try:
        cur.execute(f"PRAGMA table_info({t})")
        cols = [r[1] for r in cur.fetchall()]
        if cols:
            print(f"\n{t}: {cols}")
            cur.execute(f"SELECT * FROM {t} LIMIT 5")
            for row in cur.fetchall():
                print(f"  {row}")
    except Exception as e:
        print(t, e)

# ENUM tables with time?
cur.execute("SELECT name FROM sqlite_master WHERE name LIKE '%TIME%' OR name LIKE '%time%' OR name LIKE '%Clock%'")
print("\ntime tables:", cur.fetchall())

# Maybe export has offset in ANALYSIS_DETAILS all columns
cur.execute("PRAGMA table_info(ANALYSIS_DETAILS)")
print("\nANALYSIS_DETAILS cols:", [r[1] for r in cur.fetchall()])
cur.execute("SELECT * FROM ANALYSIS_DETAILS")
print(cur.fetchone())
