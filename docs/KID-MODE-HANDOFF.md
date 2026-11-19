# SLOOP JR — handoff

Read me first. This is the index to a plan that is deliberately more detailed than the code that will
follow it.

**Project:** a kid-friendly build of SLOOP for a 10-year-old (pianist, double-bassist), given as a
Christmas gift, where finishing musical missions unlocks more of the firmware.

**State:** planning complete, **no firmware source modified**, nothing built for the device yet. All
work so far is documentation plus two measurement tools, committed on this branch.

---

## 1. The four documents

| read | when |
| --- | --- |
| `KID-MODE-PLAN.md` | first. The design: audience, constraints, the 10 levels, the two rooms, gating philosophy, timeline, risks, licensing. |
| `KID-MODE-MEASUREMENTS.md` | second. What the firmware actually costs, with numbers. Explains why the sample bank, not the UI, pays for the project. |
| `KID-MODE-IMPLEMENTATION.md` | before writing any code. Exact seams (file/function), the gate, the clubhouse screen, the snapshot, the progress record, the test plan, the order of work, and a list of traps. |
| `KID-MODE-CURRICULUM.md` | before writing mission content. All 30 missions with screen text, seed recipes, machine checks, printed-card copy, and the reward design with its research basis. |

Plus: `docs/firmware/sloop-2.8.5.fwsc` (the stock package to install on the device as the base and the
graduation target), and `tools/measure_flash.sh` / `tools/measure_code.sh` (the measurement harnesses).

---

## 2. Decisions already made (do not re-litigate without a reason)

| # | Decision | Date |
| --- | --- | --- |
| 1 | 10 levels × 3 missions, unlocking one new capability per level, ending in free play | 19 Nov |
| 2 | Strict tutorial **plus a free-play sandbox always available** = the two-rooms model | 19 Nov |
| 3 | Mission seeds are recipes expanded at runtime, never stored projects | 19 Nov |
| 4 | Progress lives in the settings' existing 192-byte side record (112 B free, ~24 B needed) | 19 Nov |
| 5 | Locking is UI-level only, via `page_shown()` + four small call sites; no data format changes | 19 Nov |
| 6 | No companion web app for v1; one mission table generates both on-device strings and printed cards | 19 Nov |
| 7 | English only; short on-screen text; long copy on paper | 19 Nov |
| 8 | **Level 10 is graduation to stock SLOOP 2.8.5** — now a **ceremony, not a necessity** (the measured headroom removes the need to shrink the kid build) | 19 Nov, revised 20 Nov |
| 9 | The audio path is frozen; `tests/golden.txt` must not move | 19 Nov |
| 10 | Parent mode is a combo, not a PIN; gaming it is harmless | 19 Nov |
| 11 | **Kid mode owns four buttons, and only on its own screens:** `SEQ` = hint, `HOME` = back, `EDIT` = undo, `PLAY`/`REC` as stock. `OCT−/OCT+` are never taken (they are the instrument). On instrument screens the only interception is `HOME` | 20 Nov |
| 12 | **No sample sets are cut**, so the preset bank stays identical to stock and the kid build shares the stock golden baseline | 20 Nov |

---

## 3. Where the project stands

**Done**

- Design and curriculum, fully written.
- The engineering map, with every seam verified against the source.
- Baseline verified green: 22 host tests including **182 golden renders, 0 changed**.
- Budget measured: image is ~54 % constant data; sample bank is 283,212 B (48.7 % of the 581,564 B
  slot); firmware string literals are 17,022 B; progress needs ~24 of the 112 free bytes.
- Measurement tooling committed and working, and none of it needs the toolchain:
  `tools/measure_code.sh` (source ranking), `tools/measure_samples.py` (the sample bank, per set),
  **`tools/decode_fwsc.py`** (decrypts any shipped `.fwsc` — image size, free flash, string table), and
  `tools/measure_flash.sh` (per-flag image deltas; the only one that wants a build).

- **The image size, without the toolchain.** The shipped `.fwsc` decrypts under the constant key in
  `tools/fm1pkg_make.py`: the stock 2.8.5 image uses **571,820 of 581,564 B → 9,744 B free**, and it
  ships with the bitmap splash, so the text-logo fallback (4,635 B) is still in reserve. **14.4 KB
  available against an 8–12 KB need.** No feature has to be cut. See `KID-MODE-MEASUREMENTS.md` §2b.

**Three earlier figures were wrong, and are now corrected — use these, not the older text:**

| earlier claim | now | why it matters |
| --- | --- | --- |
| "the image is full, 0–4.6 KB headroom" | **9,744 B free** (14.4 KB with the text logo) | the flash risk is retired, and no feature must be cut |
| "the build only fits with a text boot logo" | the shipped image **contains the bitmap splash** | the text-logo 4.6 KB is still in reserve |
| "cutting FLUTE+SCRCH frees 22,866 B; 40,381 B is shared between sets" | **34,442 B, and nothing is shared** (`tools/measure_samples.py`) | a zone's `n` is the decoded sample count, not stored bytes; the old table double-counted |

**Not done**

- No device build has been produced (this sandbox cannot reach the JieLi toolchain server or the AC79
  SDK). `tools/measure_flash.sh` is now *optional*: it confirms per-flag deltas and checks that your
  generated headers match the shipped ones (the baseline must read 571,820 B).

**Not decided (small, and cheap to settle later)**

1. First boot: the clubhouse, or a preloaded demo song with the clubhouse behind one press.
   (Recommendation on the table: the demo song — the first interaction should be hearing something.)
2. Whether one of the two preloaded demo songs is the gift-giver's own composition with a card saying so.
3. Her name on the boot splash (cheap; keeps the build private).
4. Stickers vs printed badges for the passport.

---

## 4. If you are the agent picking this up

1. **Read `KID-MODE-PLAN.md` §2–§8 and `KID-MODE-IMPLEMENTATION.md` §1 and §14 first.** The invariants
   and the traps are the accumulated cost of this plan; skipping them recreates it.
2. **Do not start by measuring.** The space question is answered offline: 9,744 B free, 14.4 KB with
   the text logo, 8–12 KB needed (`KID-MODE-MEASUREMENTS.md` §2b). Run `tools/measure_flash.sh` when you
   have the toolchain, but only to confirm per-flag deltas and that a stock build prints `image 571820 B`.
3. **Then start at task 1 of `KID-MODE-IMPLEMENTATION.md` §13** and go down the list. Do not build
   content before the gate and the two-rooms snapshot exist: those two decide whether a child can lose
   her work, and that is the one unrecoverable failure in this project.
4. **Write `tests/kid_test.c` early**, before the UI. The gate matrix is a table, and a table is the
   only way to keep 10 levels × 40 pages × 16 layers honest.
5. **Keep the audio path untouched.** After every change: `tests/regress.c` must report 182 renders,
   0 changed. Anything else means the patch drifted into the DSP.
6. **Respect the two UX findings from late planning.** Undo is a two-button chord (hold `EDIT`, then
   `OCT−`) with only one slot — so a card offers `EDIT` alone, the instrument keeps the chord, and the
   safety mission spells the chord out. And `OCT−`/`OCT+` are musical (soft/hard drum hits, arp octave),
   so they stay native and the hint lives on `SEQ`. Details in `KID-MODE-IMPLEMENTATION.md` §4 and §14.
7. **Playtest at level 2, not at level 10.** The first six missions are the experiment; the other 24
   are content. If a 10-year-old cannot finish "kick on 1 and 3" without help, the level design is
   wrong and it is cheap to know that in week 2.

**What "done" means for v1**

- Ten levels completable on the device with `PLAY`, `REC`, `SEQ`, `HOME` and `EDIT`, and no external help.
- Her song survives mission loads, power cycles and a reinstall (proven by test 3 in
  `KID-MODE-IMPLEMENTATION.md` §12).
- `tests/golden.txt` unchanged, and identical for stock and kid: no sample cuts, no DSP change.
- The kid image fits under 581,564 B **without** falling back to the text logo (if it only fits with
  it, treat that as a budget overrun and trim strings first).
- The printed kit exists and matches the on-screen text (both generated from one table).
- Recovery is documented on paper in the box, and the stock `.fwsc` is in the box too.

---

## 5. Honest risks at handoff

| risk | state |
| --- | --- |
| Flash budget | **Retired.** 9,744 B free today, 14.4 KB with the text logo, against an 8–12 KB estimate — and no sample sets need cutting, so the preset bank and the golden renders stay identical to stock. |
| A child losing her work | Designed for (two rooms, snapshot, tests) but **not yet built**. This is the top implementation risk. |
| Content written too long for a 240 px screen | Budgeted (28 chars/line at FONT_S) and pre-written to fit, but unverified on hardware. |
| Playtesting slipping past mid-December | Real. Mitigation is in the plan's §13: decide the fallback on 15 Dec, not 24 Dec. The fallback (stock SLOOP + printed cards + hand-unlocking) is a good gift on its own. |
| Upstream releases during December | Low: kid files are separate and the audio path is frozen. |
| One development FM-1 | Open. The plan makes ordering a second unit the first action. |
