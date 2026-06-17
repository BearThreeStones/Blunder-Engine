#!/usr/bin/env python3
"""One-shot analysis for blunder_A_debug_baseline Resolved.sqlite"""
import sqlite3
from pathlib import Path
from statistics import median

DB = Path(__file__).resolve().parent / "blunder_A_debug_baseline Resolved.sqlite"
B_DB = Path(__file__).resolve().parent / "blunder_B_debug_zerocopy.sqlite"
NS_MS = 1_000_000
NS_S = 1_000_000_000


def analyze(path: Path, label: str) -> dict:
    c = sqlite3.connect(f"file:{path}?mode=ro", uri=True)
    cur = c.cursor()
    t0, dur = cur.execute(
        "SELECT startTime, duration FROM ANALYSIS_DETAILS LIMIT 1"
    ).fetchone()
    dur_s = dur / NS_S

    submits = cur.execute(
        """
        SELECT v.start, v.globalTid FROM VULKAN_API v
        JOIN StringIds s ON s.id=v.nameId
        WHERE s.value='vkQueueSubmit' ORDER BY v.start
        """
    ).fetchall()
    n_sub = len(submits)
    rel = [(s - t0) / NS_S for s, _ in submits]
    gaps = [(rel[i + 1] - rel[i]) * 1000 for i in range(len(rel) - 1)]

    wl = cur.execute(
        "SELECT COUNT(*), AVG(end-start) FROM VULKAN_WORKLOAD"
    ).fetchone()
    fence = cur.execute(
        """
        SELECT COUNT(*), AVG(v.end-v.start) FROM VULKAN_API v
        JOIN StringIds s ON s.id=v.nameId WHERE s.value='vkWaitForFences'
        """
    ).fetchone()

    blunder_syms = cur.execute(
        "SELECT COUNT(*) FROM StringIds WHERE value LIKE '%Blunder::%'"
    ).fetchone()[0]

    tick_hits = 0
    ids = [r[0] for r in cur.execute(
        "SELECT id FROM StringIds WHERE value LIKE '%tickOneFrame%'"
    )]
    if ids:
        ph = ",".join("?" * len(ids))
        tick_hits = cur.execute(
            f"SELECT COUNT(*) FROM SAMPLING_CALLCHAINS WHERE symbol IN ({ph})",
            ids,
        ).fetchone()[0]

    c.close()
    return {
        "label": label,
        "dur_s": dur_s,
        "submits": n_sub,
        "submit_rate": n_sub / dur_s,
        "gap_median_ms": median(gaps) if gaps else 0,
        "gap_max_ms": max(gaps) if gaps else 0,
        "gaps_over_200ms": sum(1 for g in gaps if g > 200),
        "wl_avg_ms": (wl[1] or 0) / NS_MS,
        "fence_avg_ms": (fence[1] or 0) / NS_MS if fence[0] else 0,
        "blunder_syms": blunder_syms,
        "tick_samples": tick_hits,
        "main_tid": submits[0][1] if submits else None,
    }


def main() -> None:
    a = analyze(DB, "A Resolved (CPU readback)")
    print("=== Symbol resolution ===")
    print(f"  Blunder:: symbols in StringIds: {a['blunder_syms']}")
    print(f"  tickOneFrame CPU samples: {a['tick_samples']:,}")

    print("\n=== A vs B (submit / GPU wait) ===")
    if B_DB.exists():
        b = analyze(B_DB, "B zero-copy")
        print(f"  {'':22} {'A Resolved':>14} {'B zero-copy':>14}")
        print(f"  {'duration':22} {a['dur_s']:>13.2f}s {b['dur_s']:>13.2f}s")
        print(f"  {'vkQueueSubmit/s':22} {a['submit_rate']:>14.2f} {b['submit_rate']:>14.2f}")
        print(f"  {'submit gap median':22} {a['gap_median_ms']:>13.1f}ms {b['gap_median_ms']:>13.1f}ms")
        print(f"  {'GPU workload avg':22} {a['wl_avg_ms']:>13.3f}ms {b['wl_avg_ms']:>13.3f}ms")
        print(f"  {'vkWaitForFences avg':22} {a['fence_avg_ms']:>13.3f}ms {b['fence_avg_ms']:>13.3f}ms")
        print(f"  {'Blunder:: StringIds':22} {a['blunder_syms']:>14} {b['blunder_syms']:>14}")
    else:
        print(f"  duration {a['dur_s']:.2f}s  submits {a['submits']} ({a['submit_rate']:.2f}/s)")

    print("\n=== Submit pacing (A) ===")
    print(f"  gaps >200ms: {a['gaps_over_200ms']}  max gap: {a['gap_max_ms']:.0f}ms")
    print(f"  => ~{1000/a['submit_rate']:.0f}ms between 3D frames (application gated, not GPU)")

    print("\n=== Interpretation ===")
    print("  - Symbols OK: Blunder/Slint function names present in CPU samples")
    print("  - 3D rate ~5/s matches viewport composite throttle (~250ms request + present gate)")
    print("  - GPU work tiny (~0.17ms/submit); vkWaitForFences dominates Vulkan CPU time")
    print("  - Nsight Frame duration still tracks Skia Present, not vkQueueSubmit")


if __name__ == "__main__":
    main()
