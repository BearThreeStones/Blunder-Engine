#!/usr/bin/env python3
import sqlite3
from pathlib import Path

ROOT = Path(__file__).resolve().parent

def probe(db_path: Path) -> None:
    print(f"\n=== {db_path.name} ({db_path.stat().st_size // 1024 // 1024} MB) ===")
    c = sqlite3.connect(f"file:{db_path.as_posix()}?mode=ro", uri=True)
    cur = c.cursor()
    total = cur.execute("SELECT COUNT(*) FROM SAMPLING_CALLCHAINS").fetchone()[0]
    null_sym = cur.execute(
        "SELECT COUNT(*) FROM SAMPLING_CALLCHAINS WHERE symbol IS NULL"
    ).fetchone()[0]
    print(f"SAMPLING_CALLCHAINS rows: {total:,}, symbol IS NULL: {null_sym:,}")
    print("symbol column types (top):")
    for row in cur.execute(
        "SELECT typeof(symbol), COUNT(*) c FROM SAMPLING_CALLCHAINS "
        "GROUP BY typeof(symbol) ORDER BY c DESC LIMIT 5"
    ):
        print(f"  {row}")
    print("sample symbol values:")
    for row in cur.execute(
        "SELECT symbol, COUNT(*) c FROM SAMPLING_CALLCHAINS "
        "GROUP BY symbol ORDER BY c DESC LIMIT 6"
    ):
        sid, cnt = row
        txt = cur.execute("SELECT value FROM StringIds WHERE id=?", (sid,)).fetchone()
        label = (txt[0][:100] if txt else "?")
        print(f"  id={sid} hits={cnt} -> {label}")
    for kw in ("tickVulkan", "SlintSystem", "compositeFrame", "engine_editor"):
        n = cur.execute(
            "SELECT COUNT(*) FROM StringIds WHERE value LIKE ?", (f"%{kw}%",)
        ).fetchone()[0]
        print(f"  StringIds LIKE %{kw}%: {n}")
    print("  StringIds with '(' (likely demangled fn):", cur.execute(
        "SELECT COUNT(*) FROM StringIds WHERE value LIKE '%(%'"
    ).fetchone()[0])
    print("  StringIds 0x7... user addresses:", cur.execute(
        "SELECT COUNT(*) FROM StringIds WHERE value LIKE '0x7%'"
    ).fetchone()[0])
    print("  StringIds 0xffff... kernel addresses:", cur.execute(
        "SELECT COUNT(*) FROM StringIds WHERE value LIKE '0xffff%'"
    ).fetchone()[0])
    rows = cur.execute(
        "SELECT value FROM StringIds WHERE value LIKE '%(%' LIMIT 5"
    ).fetchall()
    if rows:
        print("  sample fn names:", [r[0][:80] for r in rows])
    c.close()

for name in ("blunder_B_debug_zerocopy.sqlite", "blunder_A_debug_baseline.sqlite"):
    p = ROOT / name
    if p.exists():
        probe(p)
    else:
        print(f"\n=== {name}: MISSING ===")
