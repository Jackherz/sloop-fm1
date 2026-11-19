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
| 2 | Strict tutorial (five buttons only) **plus a free-play sandbox always available** = the two-rooms model | 19 Nov |
| 3 | Mission seeds are recipes expanded at runtime, never stored projects | 19 Nov |
| 4 | Progress lives in the settings' existing 192-byte side record (112 B free, ~24 B needed) | 19 Nov |
| 5 | Locking is UI-level only, via `page_shown()` + four small call sites; no data format changes | 19 Nov |
| 6 | No companion web app for v1; one mission table generates both on-device strings and printed cards | 19 Nov |
| 7 | English only; short on-screen text; long copy on paper | 19 Nov |
| 8 | **Level 10 is graduation to stock SLOOP 2.8.5** — the kid build may cut sample sets to pay for its code | 20 Nov |
| 9 | The audio path is frozen; `tests/golden.txt` must not move (except the kid build's own baseline) | 19 Nov |
| 10 | Parent mode is a combo, not a PIN; gaming it is harmless | 19 Nov |

---

## 3. Where the project stands

**Done**

- Design and curriculum, fully written.
- The engineering map, with every seam verified against the source.
- Baseline verified green: 22 host tests including **182 golden renders, 0 changed**.
- Budget measured: image is ~54 % constant data; sample bank is 283,212 B (48.7 % of the 581,564 B
  slot); cutting FLUTE + SCRCH is worth **22,866 B** against a ~12 KB need; firmware text is 17 KB
  total so kid content is capped at ~3 KB; progress needs ~24 of the 112 free bytes.
- Measurement tooling committed and working (`tools/measure_code.sh` reproduces the ranking here).

**Not done, and it is the only blocking unknown**

- `tools/measure_flash.sh` has never been run against a real image: this sandbox has no JieLi
  toolchain and cannot reach `pkgman.jieliapp.com` or `gitee.com`. It needs a machine that can run
  `./build.sh`. It returns the baseline image size and the per-flag deltas, and self-checks against the
  known "USB console = 6–8 KB" figure from SLOOP 2.7.

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
2. **Run the measurement.** It is one command on a machine with the toolchain, and the whole space
   budget depends on its output. Fold the real numbers back into §8 of the plan.
3. **Then start at task 1 of `KID-MODE-IMPLEMENTATION.md` §13** and go down the list. Do not build
   content before the gate and the two-rooms snapshot exist: those two decide whether a child can lose
   her work, and that is the one unrecoverable failure in this project.
4. **Write `tests/kid_test.c` early**, before the UI. The gate matrix is a table, and a table is the
   only way to keep 10 levels × 40 pages × 16 layers honest.
5. **Keep the audio path untouched.** After every change: `tests/regress.c` must report 182 renders,
   0 changed. Anything else means the patch drifted into the DSP.
6. **Playtest at level 2, not at level 10.** The first six missions are the experiment; the other 24
   are content. If a 10-year-old cannot finish "kick on 1 and 3" without help, the level design is
   wrong and it is cheap to know that in week 2.

**What "done" means for v1**

- Ten levels completable on the device with the five buttons and no external help.
- Her song survives mission loads, power cycles and a reinstall (proven by test 3 in
  `KID-MODE-IMPLEMENTATION.md` §12).
- `tests/golden.txt` unchanged for stock; the kid build has its own reviewed baseline.
- The printed kit exists and matches the on-screen text (both generated from one table).
- Recovery is documented on paper in the box, and the stock `.fwsc` is in the box too.

---

## 5. Honest risks at handoff

| risk | state |
| --- | --- |
| Flash budget | **Largely retired.** Data measurement found a 22.9 KB lever for a ~12 KB need. Only the per-flag confirmation is missing. |
| A child losing her work | Designed for (two rooms, snapshot, tests) but **not yet built**. This is the top implementation risk. |
| Content written too long for a 240 px screen | Budgeted (28 chars/line at FONT_S) and pre-written to fit, but unverified on hardware. |
| Playtesting slipping past mid-December | Real. Mitigation is in the plan's §13: decide the fallback on 15 Dec, not 24 Dec. The fallback (stock SLOOP + printed cards + hand-unlocking) is a good gift on its own. |
| Upstream releases during December | Low: kid files are separate and the audio path is frozen. |
| One development FM-1 | Open. The plan makes ordering a second unit the first action. |
