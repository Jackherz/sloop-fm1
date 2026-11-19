#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Turn docs/kid_missions.tsv into the firmware's mission table and the printed cards.

  gen_kid_missions.py OUT.h [TSV] [CARDS.md]

Input: docs/kid_missions.tsv (the source of truth, one row per mission; its own header
       explains every column). Output:

  OUT.h       KID_NMISSION, kid_mission_t, the KID_SEEDS[] array (tempo, kit, parts, the
              seeded drum lanes and bass notes), the KID_MISSIONS[] array (the text, the two
              checks and their arguments), and KID_LVL_MISSION[] (the first mission of each
              level, for the clubhouse) — everything the kid build needs, no prose.
  CARDS.md    the printed cards: one per mission, the long copy that never enters the image.

Every text field is checked against the screen it is drawn on (title <= 14 characters at
FONT_L, the lines and the hint <= 28 at FONT_S) and the build fails if a row is too long, so a
mission cannot silently truncate. The TSV stays the only place mission text is written.
"""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
W_TITLE, W_LINE = 14, 28
LANE = ["kick", "kick2", "snare", "clap", "hat", "open", "pedal", "rim",
        "snr2", "toml", "tomh", "crash", "ride", "shake", "conga", "bell"]
CHECKS = ["HONOUR", "KEYS", "LANE", "WROTE", "NOTEPC", "BARS", "PARAM", "PARAMS", "PRESET",
          "LAYER", "SAVED", "UNDONE", "ENGINE", "SAMPLE"]
COLS = ["id", "level", "title", "line1", "line2", "hint", "bpm", "kit", "parts", "drums",
        "bass", "check", "arg", "check2", "arg2", "card"]


def die(msg):
    raise SystemExit(f"gen_kid_missions: {msg}")


def text(v, w, what, mid):
    """a screen field: '-' is none, and it must fit the band it is drawn in"""
    v = v.strip()
    if v in ("-", ""):
        return ""
    if len(v) > w:
        die(f"mission {mid}: {what} is {len(v)} characters, the screen fits {w}: {v!r}")
    if '"' in v or "\\" in v:
        die(f"mission {mid}: {what} has a quote or a backslash: {v!r}")
    return v


def steps(v, mid, what):
    """'1,5,9,13' -> a 16-bit mask, bit 0 = step 1"""
    m = 0
    for s in [x for x in v.split(",") if x.strip()]:
        n = int(s)
        if not 1 <= n <= 16:
            die(f"mission {mid}: {what} step {n} is not 1..16")
        m |= 1 << (n - 1)
    return m


def pairs(v, mid, what):
    """'kick=1,5,9,13 snare=5,13' or '36=1,36=9' -> [(key, mask)]"""
    out = []
    for part in v.split():
        if part == "-":
            continue
        if "=" not in part:
            die(f"mission {mid}: {what} {part!r} is not key=steps")
        k, s = part.split("=", 1)
        out.append((k, steps(s, mid, what)))
    return out


def cparam(name, mid, what):
    name = name.strip()
    if not name or name in ("-", "0"):
        return "0u"
    if not name.replace("_", "").isalnum():
        die(f"mission {mid}: {what} {name!r} is not a C name")
    return name


def cap_arg(arg, mid, what, table):
    """'id=count' -> (id, count); 'id' -> (id, 1) for a token table (P_*, LY_*, ENGI_*)"""
    a = arg.strip()
    if not a or a == "-":
        return ("0u", 0)
    if "=" in a:
        k, c = a.split("=", 1)
        k = k.strip()
        if table is None:                       # a bare number list (NOTEPC pitch classes)
            pass
        else:
            k = cparam(k, mid, what)
        return (k if not k.isdigit() and table is not None else k, int(c))
    return (cparam(a, mid, what), 1)


def check_args(kind, arg, mid):
    """-> (a0, a1) as C source for one check"""
    a = arg.strip()
    if kind in ("KEYS", "PARAMS", "PRESET", "SAVED", "BARS", "SAMPLE", "HONOUR"):
        n = 1 if a in ("-", "") else int(a)
        return (f"{n}u", "0u")
    if kind == "WROTE":                          # WROTE track=count
        t, c = cap_arg(a, mid, "WROTE", True)
        return (t, f"{c}u")
    if kind == "NOTEPC":                         # NOTEPC track=pitchclasses
        t, rest = a.split("=", 1)
        pcs = [int(x) for x in rest.split(",") if x.strip()]
        m = 0
        for p in pcs:
            if not 0 <= p <= 11:
                die(f"mission {mid}: pitch class {p} is not 0..11")
            m |= 1 << p
        return (f"{int(t)}u", f"0x{m:03X}u")
    if kind == "LANE":                           # LANE lane=steps
        lanes = pairs(a, mid, "LANE")
        if len(lanes) != 1 or lanes[0][0] not in LANE:
            die(f"mission {mid}: LANE needs one lane of {LANE}: {a!r}")
        return (f"{LANE.index(lanes[0][0])}u", f"0x{lanes[0][1]:04X}u")
    if kind == "PARAM":                          # PARAM id or PARAM id=count
        i, c = cap_arg(a, mid, "PARAM", True)
        if i.startswith("G_"):
            i = f"(0x80u | {i})"                 # a global: the witness masks keep them apart
        return (i, f"{c}u")
    if kind in ("LAYER", "ENGINE"):
        return (cparam(a, mid, kind), "0u")
    if kind == "UNDONE":
        return ("1u", "0u")
    die(f"mission {mid}: unknown check {kind}")


def cstr(s):
    return f'"{s}"' if s else "0"


def load(tsv):
    rows = []
    for ln, line in enumerate(tsv.read_text().splitlines(), 1):
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        f = line.split("\t")
        if f[0] == "id":
            continue
        if len(f) != len(COLS):
            die(f"{tsv.name}:{ln}: {len(f)} columns, {len(COLS)} expected")
        rows.append(dict(zip(COLS, f)))
    if len(rows) != 30:
        die(f"{len(rows)} missions, 30 expected")
    for n, r in enumerate(rows):
        if int(r["id"]) != n + 1:
            die(f"mission {r['id']}: the ids are 1..30 in order")
    return rows


def emit_header(rows):
    o = []
    o.append("/* SPDX-License-Identifier: GPL-3.0-only")
    o.append(" * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */")
    o.append("/* GENERATED by tools/gen_kid_missions.py from docs/kid_missions.tsv — edit that, not this. */")
    o.append(f"#define KID_NMISSION {len(rows)}u")
    o.append("typedef struct {                        /* a seed: expanded into the working project at load */")
    o.append("    uint16_t lane[4];                   /* the seeded drum lanes, bit 0 = step 1 */")
    o.append("    uint8_t  lane_id[4];                /* which drum lane (0..15), 0xFF = none */")
    o.append("    uint8_t  bass[16];                  /* the bass track's notes, 0xFF = a rest */")
    o.append("    uint8_t  bass_n;                    /* notes set */")
    o.append("    uint8_t  bpm, kit, parts;           /* parts: bit 0 D, 1 B, 2 K, 3 L */")
    o.append("} kid_seed_t;")
    o.append("typedef struct {                        /* a mission: the screen, the seed, the two checks */")
    o.append("    const char *title, *l1, *l2, *hint; /* FONT_L <= 14, FONT_S <= 28 */")
    o.append("    uint8_t check, c0;                  /* KCHK_* and its first argument */")
    o.append("    uint16_t c1;                        /* its second: a 16-step lane mask, a count, pitch classes */")
    o.append("    uint8_t check2, d0;                 /* a second check that must also pass (0 = none) */")
    o.append("    uint16_t d1;")
    o.append("    uint8_t level;                      /* 1..10 (the clubhouse names it) */")
    o.append("} kid_mission_t;")
    o.append("")
    o.append("static const kid_seed_t KID_SEEDS[KID_NMISSION] = {")
    for r in rows:
        lanes = pairs(r["drums"], r["id"], "drums")
        if len(lanes) > 4:
            die(f"mission {r['id']}: {len(lanes)} drum lanes, 4 is the most")
        lm = [m for _, m in lanes]
        lid = []
        for k, _ in lanes:
            if k not in LANE:
                die(f"mission {r['id']}: no drum lane {k!r}")
            lid.append(LANE.index(k))
        lid += [0xFF] * (4 - len(lid))
        lm += [0] * (4 - len(lm))
        bass = [0xFF] * 16
        nb = 0
        for tok in r["bass"].replace(",", " ").split():     # note=step, note=step (1-based steps)
            if tok == "-":
                continue
            if "=" not in tok:
                die(f"mission {r['id']}: bass {tok!r} is not note=step")
            k, st = tok.split("=", 1)
            note, step = int(k), int(st)
            if not 0 <= note <= 127:
                die(f"mission {r['id']}: bass note {note} is not 0..127")
            if not 1 <= step <= 16:
                die(f"mission {r['id']}: bass step {step} is not 1..16")
            if bass[step - 1] != 0xFF:
                die(f"mission {r['id']}: two bass notes on step {step}")
            bass[step - 1] = note
            nb += 1
        if nb > 8:
            die(f"mission {r['id']}: {nb} bass notes, 8 is the most")
        parts = 0
        for ch, bit in (("D", 0), ("B", 1), ("K", 2), ("L", 3)):
            if ch in r["parts"].upper():
                parts |= 1 << bit
        if not parts:
            die(f"mission {r['id']}: no parts sound")
        kit = int(r["kit"])
        if not 0 <= kit <= 63:
            die(f"mission {r['id']}: kit {kit} is not 0..63")
        o.append("    {{%s}, {%s}, {%s}, %du, %du, %du, %du},   /* %s */" % (
            ", ".join(f"0x{m:04X}" for m in lm),
            ", ".join(f"0x{i:02X}" if i != 0xFF else "0xFF" for i in lid),
            ", ".join("0xFF" if v == 0xFF else f"{v}u" for v in bass),
            nb, int(r["bpm"]), kit & 0xFF, parts, r["title"]))
    o.append("};")
    o.append("")
    o.append("static const kid_mission_t KID_MISSIONS[KID_NMISSION] = {")
    for r in rows:
        mid = r["id"]
        title = text(r["title"], W_TITLE, "title", mid)
        l1 = text(r["line1"], W_LINE, "line1", mid)
        l2 = text(r["line2"], W_LINE, "line2", mid)
        hint = text(r["hint"], W_LINE, "hint", mid)
        kind = r["check"].strip().upper()
        if kind not in CHECKS:
            die(f"mission {mid}: check {kind!r} is not one of {CHECKS}")
        a0, a1 = check_args(kind, r["arg"], mid)
        k2 = r["check2"].strip().upper()
        if k2 in ("-", ""):
            b0, b1 = "0u", "0u"
            k2 = "0"
        else:
            if k2 not in CHECKS:
                die(f"mission {mid}: check2 {k2!r} is not one of {CHECKS}")
            b0, b1 = check_args(k2, r["arg2"], mid)
        o.append("    {%s, %s, %s, %s, KCHK_%s, %s, %s, %s, %s, %s, %du}," % (
            cstr(title), cstr(l1), cstr(l2), cstr(hint), kind, a0, a1,
            f"KCHK_{k2}" if k2 != "0" else "0", b0, b1, int(r["level"])))
    o.append("};")
    o.append("")
    lvl_first = {}
    for n, r in enumerate(rows):
        lvl_first.setdefault(int(r["level"]), n)
    lv = [lvl_first.get(l, 0xFF) for l in range(1, 11)]
    o.append("static const uint8_t KID_LVL_MISSION[11] = {%s};   /* the first mission of each level (0xFF: none) */" %
             ", ".join(f"{v}u" for v in [0] + lv))
    o.append("")
    return "\n".join(o) + "\n"


def emit_cards(rows):
    o = ["# SLOOP JR — the printed cards", "",
         "Generated by `tools/gen_kid_missions.py` from `docs/kid_missions.tsv`; edit that file, not this.",
         "One card per mission: the screen text, what she does, and the long copy.", ""]
    for r in rows:
        o.append(f"## MISSION {r['id']} — {r['title']}  (level {r['level']})")
        o.append("")
        o.append("```")
        if r["line1"].strip() not in ("-", ""):
            o.append(r["line1"])
        if r["line2"].strip() not in ("-", ""):
            o.append(r["line2"])
        o.append(f"[SEQ] {r['hint']}")
        o.append("```")
        o.append("")
        o.append(r["card"].strip())
        o.append("")
    return "\n".join(o)


def main(argv):
    if len(argv) < 2:
        die("usage: gen_kid_missions.py OUT.h [TSV] [CARDS.md]")
    out = Path(argv[1])
    tsv = Path(argv[2]) if len(argv) > 2 else ROOT / "docs" / "kid_missions.tsv"
    cards = Path(argv[3]) if len(argv) > 3 else None
    rows = load(tsv)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(emit_header(rows))
    if cards:
        cards.parent.mkdir(parents=True, exist_ok=True)
        cards.write_text(emit_cards(rows))
    print(f"kid missions: {len(rows)} missions -> {out}" + (f", {cards}" if cards else ""))


if __name__ == "__main__":
    main(sys.argv)
