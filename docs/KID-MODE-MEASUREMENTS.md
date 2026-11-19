# SLOOP JR — measurements

19 Nov 2026, extended 20 Nov. Companion to `docs/KID-MODE-PLAN.md`. What the firmware costs, where the
space actually is, and what still needs a machine with the JieLi toolchain.

**Start at §2b if you only read one section:** the shipped image was decrypted offline and the space
question is answered — 9,744 B free, 14.4 KB with the text logo, against an 8–12 KB estimate.

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
| **The image size** — 571,820 B used, 9,744 B free | decode of `docs/firmware/sloop-2.8.5.fwsc` (§2b) | **exact** (validated against the start stub) |
| What each feature-cut is worth, per flag | `tools/measure_flash.sh` | **needs your machine** (§7), now optional |

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

So roughly **267 KB is code + strings**.

**This table is where the plan went wrong, and §2b is the correction.** None of it is wrong as
measurement — the sample bank really is 48.7 % of the slot — but it is not the same as knowing the
image size. The 2.6-era note on record ("an image at 576,564 B, 5,000 B under the slot") is what the
0–4.6 KB estimate was extrapolated from, and 2.8.5 has moved: **the shipped image uses 571,820 B and
has 9,744 B free** (§2b, decoded from the package). So:

- the sample bank is a *big* lever, and it is **not needed**;
- the constant-data split is still the thing to consult if a future feature does need 20 KB+;
- and "code surgery" is not needed either — the kid build fits in what is already spare.

### The sample bank, per set (corrected 20 Nov)

**The first version of this table was wrong, and the error is worth keeping in the document** because it
is the kind of mistake that makes a plan confident and wrong. A zone is `{off, n, ls, le, …}` and `n` is
the **decoded sample count**, not the stored size: the blob holds 4-bit IMA output, about `n/2` bytes.
Attributing `n` bytes at `off` overlaps every neighbour and invents a "shared between sets" bucket that
does not exist (`gen_samples.py` never deduplicates — `add()` always appends).

The exact measurement is easier: the blob is appended set by set and every zone carries its stored
offset, so a set owns `[its lowest zone offset, the next set's lowest offset)`. That tiles `SMP_DATA`
exactly — 0 bytes left over, 0 shared — and it agrees with the independent check `Σ n / 2 + padding`
for every set. Reproduce with `python3 tools/measure_samples.py`:

| set | zones | bytes | of the bank | what it is | status now |
| --- | ---: | ---: | ---: | --- | --- |
| PERC | 27 | **86,144** | 30.4 % | GM drum kit + percussion | keep — drums are level 1 |
| PIANO | 4 | **44,104** | 15.6 % | grand piano | keep — her instrument, level 4 |
| BASS | 4 | **39,138** | 13.8 % | upright/electric bass | keep — level 3 |
| VIBES | 3 | 33,078 | 11.7 % | vibraphone | keep (pretty, and cheap) |
| FLUTE | 3 | 31,422 | 11.1 % | LOFI FLUTE | keep — **no longer needed as a lever** |
| HORNS | 4 | 26,462 | 9.3 % | HORN STAB | keep |
| STRGS | 3 | 19,844 | 7.0 % | string stab | keep — level 8's neighbour |
| SCRCH | 1 | 3,020 | 1.1 % | vinyl scratch | keep |
| | | **283,212** | 100 % | | |

Two numbers from the old table to correct wherever they still appear: **FLUTE is 31,422 B, not 19,846 B**,
so cutting `FLUTE + SCRCH` would free **34,442 B, not 22,866 B**. `VIBES` is the next biggest at 33 KB,
and `PERC` is the biggest of all at 86 KB.

**None of this is needed.** §2b says there are 9,744 B free plus 4,635 B of logo, against an 8–12 KB
estimate. Keep the rows above as the answer *if* a future kid feature turns out to be huge.

---

## 2b. The real image size — decoded from the shipped package (the answer)

**Reproduce it in one command:**

```
$ python3 tools/decode_fwsc.py docs/firmware/sloop-2.8.5.fwsc
package   docs/firmware/sloop-2.8.5.fwsc: 610,076 B
identity  FM-1_900
entry   0  flash.bin    602,112 B at 0x400  crc ok
entry 100  ota.bin        6,920 B at 0x93400  crc ok
stub      04818000 = FM-1 app

app slot    581,564 B
image       571,820 B    98.3 % of the slot
free          9,744 B     1.7 %
```

`tools/decode_fwsc.py` is committed for exactly this, and it needs no SDK. It is the inverse of
`fm1pkg_make.py` and it *proves* itself: both entries' CRC16s come out `ok`, which they cannot unless
the marker strip, the header decode and both ciphers are right. `--app out/app.bin` writes the
decrypted image, and `--strings N` lists the printable runs with offsets (that is how the string table
below was located).

**This is the number the plan was waiting for, and the toolchain is not needed to get it.**
`tools/fm1pkg_make.py` builds the app area with the JieLi SFC stream cipher under a constant key
(`KEY = 0x980F`), and `build.py` passes exactly that constant. So the shipped
`docs/firmware/sloop-2.8.5.fwsc` can be decoded offline:

1. strip the 20 identity marker bytes (as `web/fm1pkg.js logicalImage()` does),
2. decrypt the ufw entry list at `0x40` (ENC, key `0xFFFF`),
3. take `flash.bin` from the first entry (`0x400`),
4. decrypt the app area at `0x4000` (SFC, key `0x980F`, mixed with the block address),
5. the app is `0x120` into that area, padded with `0xFF` to the slot.

(The package identity is `FM-1_900` — worth knowing, because the kid build should carry a *different*
product name so an installer list can never confuse the two; the plan's `FM-1_910` proposal is still
the right idea.)

Validation that the decoding is correct: the first four bytes come out `04 81 80 00`, which is exactly
the entry stub `build.py` asserts on, and the version string `SLOOP 2.8.5` appears at offset 561,710.

| | bytes | of the slot |
| --- | ---: | ---: |
| **Image actually used** | **571,820** | **98.3 %** |
| **Headroom in the stock 2.8.5 build** | **9,744** | 1.7 % |
| App slot (`APP_SLOT`) | 581,564 | 100 % |

Three consequences, all of which change earlier assumptions in this document:

1. **The plan's premise was stale.** §2 said the build "only links because it falls back to a text boot
   logo". It does not: the shipped image **contains the bitmap splash** (its 4,635 B RLE blob is at
   offset 542,563). The 2.7-era "the flash is full" note is still true as *history* — the console was
   cut to make room then — but 2.8.5 currently has ~9.7 KB spare.
2. **There is 14.4 KB available before cutting anything.** 9,744 B now, plus 4,635 B if the kid build
   settles for the text logo that `build.py` already knows how to make. The kid mode was budgeted at
   8–12 KB. **It therefore fits without removing a single feature** — no sample-set cut, no
   fork-specific golden baseline, no preset renumbering.
3. **Anything that does need trimmming comes from the small pool**, not the sample bank: the editor's
   three cuttable pages (~1.3 % of code) plus string trimming.

A second, independent read on the same data: the source-literal count (§4) plus the measured constant
data (§2) predicts 571,820 B to within about 1 %, which is a useful cross-check on the host-proxy
estimates in §3.

**Self-check for your own build:** a stock `./build.sh` on 2.8.5 should print `image 571820 B`. If it
prints something else, the generated headers differ (fonts, samples, or the Pillow version) and every
delta measured from that baseline shifts with it.

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
| `ui_studio.c` | 3.4 % | **the device's own TRACKS / DRUMS / LAYERS / HOLD screens — the heart of the kid UI. Never cut.** |
| `phys_dsp.c` | 2.6 % | level 8's engine |
| `fm6_core.c` | 2.5 % | level 10 |
| `punch.c` | **1.2 %** | 27 punch-in FX — fun, but cuttable |
| `ui_menu.c` | 1.0 % | |
| `editor_live.c` | 0.6 % | the *editor's* remote-control page (protocol v12) — cuttable |
| `editor_fm6.c`, `editor_dsyn.c` | 0.7 % | editor-side patch/cartridge pages — cuttable |
| `arranger_scene.c` | 0.3 % | song order |
| `ui_song.c` | 0.5 % | |

Two things worth knowing that the plan guessed wrong about:

- **The arranger is small** (0.3 %), not the 2–4 KB the plan assumed. Cutting it is not worth the
  feature loss; drop it from the candidate list.
- **Correction to an earlier assumption in this session:** `ui_studio.c` is not the editor's LIVE page.
  It is the FM-1's own performance screens (TRACKS, the DRUM grid and kit, the layers, the HOLD
  prompts) — the surface the whole kid experience sits on. The editor's LIVE page is
  `editor_live.c` (0.6 %), which is small.
- What is genuinely cuttable without touching anything she uses: `editor_live.c` + `editor_fm6.c` +
  `editor_dsyn.c` (1.3 % together: the editor's remote-control page and the two patch-editing pages,
  all parent tools), `ui_menu.c` (1.0 %, but the parent menu needs some of it) and `punch.c` (1.2 %,
  which is level 6's reward, so it goes last). That is **~3 % of code, roughly 8 KB** — enough only if
  the sample cut happens first.

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
A4. Write the missions short (≤ 2 × 28 characters on screen; the measured line budget is in
`KID-MODE-IMPLEMENTATION.md` §3) from the start.

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

Revised with the real headroom (9,744 B, plus 4,635 B if the text logo is acceptable = **14.4 KB**):

| Priority | Move | Gives | Costs |
| --- | --- | --- | --- |
| 0 | **Build kid mode lean and see if it fits** | up to 9,744 B available today | nothing |
| 1 | Text logo instead of the bitmap splash (one flag, automatic) | +4,635 B | the pretty boot picture — and she gets a nicer one in the graduation build |
| 2 | `editor_live.c` + `editor_fm6.c` + `editor_dsyn.c` (editor remote-control and patch pages) | ~1.3 % of code (~3–4 KB) | nothing she uses |
| 3 | Trim mission strings (the curriculum's screen text) | 0.5–1 KB | wording work |
| — | ~~cut sample sets~~ | was the plan's main lever (34.4 KB for FLUTE+SCRCH, as measured) | **no longer needed.** Cutting sets renumbers SAMPLE presets, which would break user presets and require a fork-only golden baseline — a real cost for space we do not need |
| — | ~~cut `ui_studio.c`~~ | — | it is the device's TRACKS/DRUMS screens; never |
| — | ~~cut the arranger~~ | 0.3 % | not worth it, and not flag-able today |

**So the flash risk is retired, not just downgraded**, and the graduation stops being a necessity. It is
still a good ending (she moves to a maintained stock build with everything unlocked, keeps her projects
and presets), but the kid build no longer *has* to be smaller than stock. That is the user's call, not a
constraint — see `KID-MODE-HANDOFF.md` §2.

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

## 7. What still needs your machine (optional now, but useful)

The image size is answered above. What a real build still gives you is the **per-flag** deltas for the
cuts in §5, and a check that your generated headers match the shipped ones.

```sh
tools/get_toolchain.sh                     # ~/.jieli/toolchain  (JieLi's server)
git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 \
    https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
pip3 install Pillow

tools/measure_flash.sh --quick             # baseline + logo + arranger  (~6 min)
tools/measure_flash.sh                     # + UAC, UART, icons, combined, CDC  (~15 min)
```

It writes `build/measurements.md` with a row per variant (image bytes, delta, RAM, pool headroom) and
prints the exact data-blob sizes next to it. Two checks are built in: the baseline row should read
**571,820 B** (the decoded figure above), and the `cdc_built` row should read **6,000–8,000 B**, because
SLOOP 2.7 recorded the USB console at "its 6–8 KB: the flash is full" when it was cut.

What I expect (and want to be wrong about): baseline at or within a few KB of 581,564 with the text
logo already in play, `logo_text` at 0 delta (already spent), `no_icons` in the low hundreds of bytes, `no_uac` a few KB. If `logo_text` reads
0, the baseline is already using the text logo and that trick is spent.

Send me `build/measurements.md` and I will fold the real numbers into §8 of the plan and set the kid
build's flag set.

---

## 8. Reproducing the host-side numbers

```sh
tools/measure_code.sh                  # builds 3 host proxies, prints the module ranking
python3 tools/measure_samples.py       # the sample-bank table in §2, from the generated header
python3 tools/measure_samples.py --cut FLUTE,SCRCH   # what that cut would free
python3 tools/decode_fwsc.py docs/firmware/sloop-2.8.5.fwsc   # image size + free flash, no SDK
```

`measure_samples.py` reads `build/gen/felucca_samples.h`, which is checked in, so it needs no audio
sources and no build. `decode_fwsc.py` needs only the package. Between the three, every number in this
document can be re-derived on any machine.
