#!/usr/bin/env python3
"""Ad-hoc Nsight SQLite explorer for blunder_A_debug_baseline.sqlite"""
import sqlite3
import sys
from pathlib import Path

DB = Path(__file__).resolve().parent / "blunder_A_debug_baseline.sqlite"


def main() -> None:
    if not DB.exists():
        print(f"Missing: {DB}", file=sys.stderr)
        sys.exit(1)

    con = sqlite3.connect(f"file:{DB}?mode=ro", uri=True)
    cur = con.cursor()

    cur.execute("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name")
    tables = [r[0] for r in cur.fetchall()]
    print("=== TABLES ===")
    for t in tables:
        cur.execute(f"SELECT COUNT(*) FROM [{t}]")
        n = cur.fetchone()[0]
        print(f"  {t}: {n:,} rows")

    print("\n=== SCHEMA (first 15 tables) ===")
    for t in tables[:15]:
        cur.execute(f"PRAGMA table_info([{t}])")
        cols = [f"{r[1]}:{r[2]}" for r in cur.fetchall()]
        print(f"\n{t}:")
        print("  " + ", ".join(cols[:12]))
        if len(cols) > 12:
            print(f"  ... +{len(cols)-12} more")

    con.close()


if __name__ == "__main__":
    main()
