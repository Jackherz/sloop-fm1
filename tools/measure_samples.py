#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Attribute the sample bank (SMP_DATA) to the sample sets, and say what cutting one would really save.

Reads `build/gen/felucca_samples.h` (produced by `tools/gen_samples.py`, so no source audio is needed),
and answers the question "how many bytes do I actually get if I drop a set?":

  tools/measure_samples.py                       # the table in docs/KID-MODE-MEASUREMENTS.md §2
  tools/measure_samples.py --cut FLUTE,SCRCH     # what that cut would free, exclusive bytes only
  tools/measure_samples.py --header PATH

Method — and the trap in it: a `smp_zone_t` is `{off, n, ls, le, ...}`, and **`n` is the decoded sample
count, not the stored size** (`gen_samples.py` keeps `n=len(smp)` in decoded samples; the blob gets the
4-bit IMA output, about `n/2` bytes). Attributing `n` bytes at `off` therefore overlaps its neighbours
and invents a "shared between sets" bucket that does not exist — `add()` never deduplicates, it always
appends.

The exact measurement is one step simpler: the blob is appended in set order, and every zone carries the
stored byte offset it starts at. So a set owns `[its lowest zone offset, the next set's lowest offset)`
and the last set runs to the end of `SMP_DATA`. That tiles the bank exactly, with no sharing — which is
also why cutting a set is worth more than the old table said.
"""
import argparse
import re
import sys
from pathlib import Path

ZONE_RE = re.compile(r"^\s*\{(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+)\},\s*$")
SET_RE = re.compile(r'^\s*\{"([A-Z0-9_]+)",\s*(\d+),\s*(\d+)\},')


def parse(header):
    text = Path(header).read_text()
    total = int(re.search(r"SMP_DATA\[(\d+)\]", text).group(1))
    zones, sets = [], []
    # the two arrays are emitted in order, so a section marker is enough context
    section = None
    for line in text.splitlines():
        if "SMP_ZONES[]" in line:
            section = "zones"
            continue
        if "SMP_SETS[]" in line:
            section = "sets"
            continue
        if "SMP_PRESET_TABLE[]" in line:
            section = None
            continue
        if section == "zones":
            m = ZONE_RE.match(line)
            if m:
                g = [int(x) for x in m.groups()]
                zones.append({"off": g[0], "n": g[1], "looped": g[10]})
        elif section == "sets":
            m = SET_RE.match(line)
            if m:
                sets.append({"name": m.group(1), "z0": int(m.group(2)), "nz": int(m.group(3))})
    return total, zones, sets


def extents(zones, sets, total):
    """[(name, zones, start, end)] — the blob tiles exactly, in the order the sets were appended"""
    rows = []
    for s in sets:
        mine = zones[s["z0"]:s["z0"] + s["nz"]]
        if not mine:
            continue
        rows.append({"name": s["name"], "zones": len(mine),
                     "start": min(z["off"] for z in mine)})
    rows.sort(key=lambda r: r["start"])
    for i, r in enumerate(rows):
        r["end"] = rows[i + 1]["start"] if i + 1 < len(rows) else total
    # a zone of another set must not start inside this set's extent (that would mean the order is wrong)
    return rows


def main():
    ap = argparse.ArgumentParser(description="what the sample bank costs, per set")
    ap.add_argument("--header", type=Path, default=Path("build/gen/felucca_samples.h"))
    ap.add_argument("--cut", help="comma-separated set names: what dropping them would free")
    a = ap.parse_args()
    if not a.header.exists():
        sys.exit(f"{a.header}: missing. Run `python3 tools/gen_samples.py {a.header}` first.")

    total, zones, sets = parse(a.header)
    if len(zones) != sum(s["nz"] for s in sets):
        sys.exit(f"{a.header}: parsed {len(zones)} zones but SMP_SETS[] names "
                 f"{sum(s['nz'] for s in sets)} — the header is not what this tool expects")
    rows = extents(zones, sets, total)
    covered = sum(r["end"] - r["start"] for r in rows)
    print(f"SMP_DATA {total:,} B; {covered:,} B attributed to sets "
          f"({100.0 * covered / total:.1f} %), gaps: {total - covered:,} B")
    print()
    print(f"{'set':<8}{'zones':>7}{'bytes':>12}  {'of the bank':>11}")
    for r in sorted(rows, key=lambda r: -(r["end"] - r["start"])):
        n = r["end"] - r["start"]
        print(f"{r['name']:<8}{r['zones']:>7}{n:>12,}  {100.0 * n / total:>10.1f} %")

    if a.cut:
        names = [n.strip().upper() for n in a.cut.split(",") if n.strip()]
        have = [r["name"] for r in rows]
        unknown = [n for n in names if n not in have]
        if unknown:
            sys.exit(f"unknown set(s): {', '.join(unknown)} (have: {', '.join(have)})")
        freed = sum(r["end"] - r["start"] for r in rows if r["name"] in names)
        left = total - freed
        print()
        print(f"cutting {', '.join(names)} frees {freed:,} B of SMP_DATA ({100.0 * freed / total:.1f} %)")
        print(f"the bank becomes {left:,} B; presets that point at the cut sets disappear and the "
              f"remaining SAMPLE preset indices shift")
        print("note: docs/KID-MODE-MEASUREMENTS.md §2b says the kid build does not need this.")


if __name__ == "__main__":
    main()
