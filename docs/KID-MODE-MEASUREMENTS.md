# SLOOP JR — first measurement session

19 Nov 2026. Companion to `docs/KID-MODE-PLAN.md`. This is the "measure before you design" step: what
the firmware costs, where the space actually is, and what still needs a machine with the JieLi
toolchain.

---

## 0. What could and could not be measured here

This sandbox has no JieLi toolchain and no cross-compiler, and the two places they come from
(`pkgman.jieliapp.com` for the toolchain, `gitee.com` for the AC79 SDK) are not reachable from it. So:

| Measured here | Method | Confidence |
| --- | --- | --- |
| The host test suite (22 tests incl. 182 golden renders) | `cc` + the repo's own generated headers | **exact** |
| Every generated data blob, byte for byte | the generators run locally | **exact** |
| The sample bank split per set | parsed from `build/gen/felucca_samples.h` | **exact** |
| Text (string literals) per module | parsed from `firmware/src/*.c` | **exact** |
| Code size per module, ranked | DWARF line tables on host builds | **ranking only** (x86-64 ≠ pi32v2) |
| The image size, and what each feature-cut is worth | `tools/measure_flash.sh` | **needs your machine** (§7) |

Nothing below changes the sound: `tests/golden.txt` passed untouched.

---

## 1. Baseline: the fork starts green

`tests/run_tests.sh` refuses to run without `build/felucca.fwsc` (it is built by the toolchain), so
this is the same suite compiled by hand, minus the four tests that need a firmware artifact. All of
these pass on a clean checkout:

```
storage (A/B, torn writes)              recovery + boot-loop guard      arranger (order, timing, repeats)
song: 4 tracks, scene change, stop      song screen: commands, load     song screen: re-asked confirm
drum lanes, kit audio, free take        drum voices on a busy beat      re-triggered sample: no click
punch-in FX (27)                        INSERT / FILTER / ENV SYNC      FM6 (algorithms, bank)
SLICER                                  PHYS in tune                    NOISE LFSR
sequencer 2.0 (drift, ratchets, locks)  NOTES editor (LEN, MONO)        held notes / overdub
project formats FUN1–5 + autosave       user presets round trip         drum kits (all 32)
user kits USR1–3                        SYN1–4 + drum synth             font (every glyph, 1 bpp)
UNISON phase spread                     every page: one knob one value  live UI: pages, layers, fuzz
REGRESSION: 182 golden renders (0 changed), health, voices, CPU budget
DSP render to WAV                       scales                          1 min soak
8000-frame stress                       installer CLI vs simulated FM-1 web pages (node)
```

Not runnable without the target compiler (they use pi32v2 inline asm): `encoder_test`, `uac_test`,
and the build-dependent `ota_test` / `ldr_test` / `target_budget.py`.

**Consequence:** you have a known-good reference. After any kid-mode patch, this list passing plus an
unchanged `tests/golden.txt` means the sound and the sequencer are untouched.

---

## 2. The shape of the image: it is half data

The app slot is **581,564 B** (`tools/fm1pkg_make.py`). The generated constant data alone is:

| blob | bytes | of the slot |
| --- | ---: | ---: |
| `SMP_DATA` — 8 CC0 sample sets, IMA ADPCM | **283,212** | **48.7 %** |
| drum kits (`DS_KITS`, 32 kits × 16 sounds) | ~11,300 | 1.9 % |
| fonts (Terminus 8×16 + 16×32 as 2×, shared bitmap) | ~7,000 | 1.2 % |
| boot logo (RLE; `FELUCCA_LOGO_TEXT=1` drops it) | 4,635 | 0.8 % |
| tables (`felucca_tables.h`) | 4,356 | 0.7 % |
| parameter icons (86 at 1 bpp) | 3,096 | 0.5 % |
| **total constant data** | **~314,000** | **~54 %** |

So roughly **267 KB is code + strings**, and the previous measurement on record (RAM.md, 2.6-era) put
an image at 576,564 B — 5,000 B under the slot, *before* 2.7 and 2.8 added features and spent the
4,635 B logo as the overflow fix. The honest reading: **current headroom is between 0 and about 4.6 KB.**

**And the conclusion that matters:** a space recovery big enough for a kid mode does not come from
code surgery. It comes from the sample bank.

### The sample bank, per set

Measured by attributing every byte of `SMP_DATA` to the set(s) whose zones cover it (some sets share
zones, so "exclusive" is what you actually save):

| set | zones | exclusive bytes | what it is | keep for SLOOP JR? |
| --- | ---: | ---: | --- | --- |
| PERC | 27 | **87,967** | GM drum kit + percussion | **yes** — drums are level 1 |
| PIANO | 4 | **43,356** | grand piano | **yes** — her instrument, level 4 |
| BASS | 4 | **36,264** | upright/electric bass | **yes** — level 3 |
| VIBES | 3 | 23,709 | vibraphone | yes (pretty, and cheap to keep) |
| FLUTE | 3 | **19,846** | LOFI FLUTE | **cut first** |
| HORNS | 4 | 15,439 | HORN STAB | cut if needed |
| STRGS | 3 | 13,230 | string stab | yes — level 8's neighbour |
| SCRCH | 1 | 3,020 | vinyl scratch | **cut first** |
| — | — | 40,381 | shared between sets | stays with whichever set needs it |

Cutting **FLUTE + SCRCH costs 22,866 B and removes exactly two presets** (`LOFI FLUTE`, `SCRATCH`).
That is 2× the ~12 KB the plan budgeted for kid mode, and it is a data change, not UI surgery.
Adding HORNS takes it to ~38 KB.

Two consequences to plan around, not to discover later:

1. Cutting sets renumbers the SAMPLE presets, so the fork gets its **own `tests/golden.txt` baseline**
   (one `GOLDEN_UPDATE=1` run, reviewed). This is expected for a fork; it must not happen for the
   stock build.
2. A kid build without those sets cannot hand her "everything" at level 10. That is fine if level 10
   is a **graduation**: install stock 2.8.5, keep her projects and presets (the project format is
   shared, and sections/presets carry over), and give her the three missing sounds as part of the
   ceremony. It also solves a second problem — a build without three presets is not a build you want
   to maintain forever.

---

## 3. Where the code is (ranking only)

Whole firmware, host x86-64, DWARF-attributed (`tools/measure_code.sh`). **Use the shares, not the
bytes** — pi32v2 `-Oz` is much tighter than this.

| module | share | note |
| --- | ---: | --- |
| `seq.c` | 16.0 % | core; untouchable |
| `fx.c` | 5.8 % | core |
| `project.c` | 5.0 % | storage/sections; core |
| `ui_layers.c` | 4.7 % | the hold-a-button layers |
| `dsp.c` | 4.7 % | core |
| `ui_draw.c` | 4.6 % | core |
| `ui.c` | 4.3 % | core |
| `editor.c` | **4.3 %** | web editor protocol — keep (it is your backup path) |
| `voice.c` | 4.2 % | core |
| `ui_vis.c` | **4.1 %** | full-screen visualiser — could go, but it is a great reward |
| `ui_input.c` | 4.0 % | core |
| `ui_studio.c` | **3.4 %** | the 2.7 LIVE show-control page — kid-irrelevant |
| `phys_dsp.c` | 2.6 % | level 8's engine |
| `fm6_core.c` | 2.5 % | level 10 |
| `punch.c` | **1.2 %** | 27 punch-in FX — fun, but cuttable |
| `ui_menu.c` | 1.0 % | |
| `editor_live.c` | 0.6 % | LIVE page support |
| `arranger_scene.c` | 0.3 % | song order |
| `ui_song.c` | 0.5 % | |

Two things worth knowing that the plan guessed wrong about:

- **The arranger is small** (0.3 %), not the 2–4 KB the plan assumed. Cutting it is not worth the
  feature loss; drop it from the candidate list.
- **The LIVE page is the biggest kid-irrelevant block** (`ui_studio.c` + `editor_live.c` ≈ 4.0 %),
  followed by the visualiser (4.1 %, but it is a reward, so keep it) and punch (1.2 %).

---

## 4. The text budget

String literals in `firmware/src/*.c` total **17,022 B** — that is the *whole* vocabulary of the
firmware, every menu label and every message.

| file | string bytes |
| --- | ---: |
| `params.c` | 2,140 |
| `ui.c` | 1,825 |
| `console.c` | 1,109 (not even built) |
| `ui_layers.c` | 1,059 |
| `icons.c` | 861 |
| `project.c` | 847 |
| `ui_menu.c` | 768 |
| `ui_studio.c` | 718 |

A 30-mission curriculum at ~60 characters of on-screen text per mission is **~1.8 KB — about 11 % of
every word in the firmware**. This is the argument for the paper kit, in one number: the long
explanation has to live on cards, because the device's entire text budget is smaller than a page of
A4. Write the missions short (≤ 2 × 18 characters) from the start.

---

## 4b. Storage budget (exact, read off the code)

Progress does not need a new flash map. The settings object already carries a **side record** between
its header and its payload: `ST_SIDE_OFF 64` to `ST_PAYLOAD_OFF 256` = **192 B**
(`storage.c`), and the theme + favourites record currently uses 80 of them
(`_Static_assert(sizeof(side_rec_t) == 80u)`). That leaves **112 B**.

A 30-mission campaign needs, at 2 bits a mission: 60 bits of mission state, 1 bitmask of levels,
1 byte of format version, 4 bytes of CRC ⇒ **~24 B**, comfortably inside the 112 B, with room for a
couple of counters (sessions played, "first seen" date) later.

The "her song is safe" snapshot in the two-rooms model (§6 of the plan) needs nothing at all: it uses
one of the four existing section slots (2 sectors each, already allocated) and the existing autosave
object. **Total new flash wear for the whole gamification layer: one extra settings write per mission
completed.** That matters — a 10-year-old will complete missions in bursts, and the A/B sector pair
tolerates that easily.

---

## 5. What this does to the plan

Recommended order of attack for the space, now that the numbers exist:

| Priority | Move | Gives | Costs |
| --- | --- | --- | --- |
| 1 | Cut sample sets **FLUTE + SCRCH** in the kid build | **+22.9 KB** | 2 presets; own golden baseline; graduation at level 10 |
| 2 | Cut `ui_studio.c` + `editor_live.c` (LIVE page) | ~4 % of code (measure on device) | the show-control page (kid-irrelevant, you keep the rest of the editor) |
| 3 | Cut `punch.c` | ~1.2 % of code | 27 punch-in FX — but they are level 6's fun, so do this last |
| 4 | Add **HORNS** to the cut sets | +15.4 KB | 1 preset |
| — | ~~arranger~~ | 0.3 % | not worth it — remove from the candidate list |
| — | ~~`FELUCCA_CDC`~~ | already out since 2.7 | — |

With (1) alone the plan's ~12 KB requirement is met twice over, which means **the risky part of the
project is now the UI work, not the flash.** That changes the risk table: the flash risk drops from
"medium-high" to "low", and the schedule risk (evenings, playtests) becomes the top risk.

---

## 6. Source changes: none

The plan listed "cut the arranger" as a candidate. It cannot be done with a flag today:
`#define FELUCCA_ARRANGER 1` is hardcoded in `felucca.c` and — more importantly — `ui_song.c` uses
`arrangement_enabled` / `arrangement_clock` **without** the `#if` guard that `seq.c` and `project.c`
use. I tried the guard, confirmed with a preprocessor run that it leaves ~40 unguarded references, and
reverted both that and the matching `tools/build.py` whitelist entry rather than leave a switch that
breaks the build.

At 0.3 % of the code (measured) the arranger was never worth this. **The tree is unmodified apart from
the new files listed by `git status`.**

---

## 7. What still needs your machine (10 minutes of setup, ~15 minutes of building)

```sh
tools/get_toolchain.sh                     # ~/.jieli/toolchain  (JieLi's server)
git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 \
    https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
pip3 install Pillow

tools/measure_flash.sh --quick             # baseline + logo + arranger  (~6 min)
tools/measure_flash.sh                     # + UAC, UART, icons, combined, CDC  (~15 min)
```

It writes `build/measurements.md` with a row per variant (image bytes, delta, RAM, pool headroom) and
prints the exact data-blob sizes next to it. **Sanity check built into the script:** the
`cdc_built` row should read **6,000–8,000 B**, because SLOOP 2.7 recorded the USB console as "its
6–8 KB: the flash is full". If it lands there, the flag plumbing and the measurement are honest. If
it does not, trust the numbers less and tell me.

What I expect (and want to be wrong about): baseline at or within a few KB of 581,564 with the text
logo already in play, `logo_text` at 0 delta (already spent), `no_icons` in the low hundreds of bytes, `no_uac` a few KB. If `logo_text` reads
0, the baseline is already using the text logo and that trick is spent.

Send me `build/measurements.md` and I will fold the real numbers into §8 of the plan and set the kid
build's flag set.

---

## 8. Reproducing the host-side numbers

```sh
tools/measure_code.sh        # builds 3 host proxies, prints the module ranking
```

The sample-set attribution is a 20-line script over `build/gen/felucca_samples.h` (zone offset + length
per set, de-duplicated for shared zones); it is deterministic and needs no build. If it becomes a
recurring question, it should become `tools/measure_samples.py` — say the word and I will add it.
