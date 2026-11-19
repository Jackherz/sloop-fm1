# SLOOP JR — implementation map

Everything an implementing agent needs that is not already in `KID-MODE-PLAN.md` (the design) or
`KID-MODE-CURRICULUM.md` (the 30 missions). Line numbers were verified against `bac9c05` (SLOOP
2.8.5); they drift, so **search by function name**, not by line.

Read the invariants first. They are what keeps this project from turning into a fork you can never
rebase.

---

## 1. Invariants (do not break these)

1. **The audio path is frozen.** No edits to `eng_*.c`, `voice.c`, `fx.c`, `drums.c`, `dsp.c`,
   `phys_dsp.c`, `drum_synth.c`, `slice*`, or the ISR-side halves of `seq.c`. If `tests/golden.txt`
   changes, the patch is wrong — except for the one deliberate case: cutting sample sets (§11), which
   changes the preset list and therefore the goldens, in the kid build only.
2. **Kid logic runs in the main loop only**, never in an ISR, never inside a draw. The loop has
   `ui_input(); ui_draw(); autosave_tick();` (`main.c` ~351–356) and a 5.8 ms audio half-buffer to
   protect.
3. **No new flash map, no new format.** Progress goes in the settings object's existing side record;
   the sandbox snapshot goes in an existing section slot. Anything else breaks the promise that a
   stock SLOOP can still read her device.
4. **A gating bug must never lose data.** Locked means "unreachable from the UI", never "deleted",
   never "rewritten". If kid mode breaks completely, the project files must still be loadable by stock
   2.8.5.
5. **Text is the expensive thing.** The whole firmware has 17 KB of strings. Kid content is budgeted at
   3 KB of on-device text (the long copy lives on the printed cards).
6. **Every refusal is one line and consistent**: `NOT YET · LEVEL n` via `ui_say` (`ui.c` ~113), never
   a silent no-op and never a different message.

---

## 2. Where to put the gate

One enum, one predicate, five call sites. That is the entire locking mechanism.

```
feature bits: DRUMS, TRACK2, TRACK3, EDIT_ENV, FX_LAYER, ARP, PHYS_ENGINES, SONG, FM6_SAMPLE,
              (10 total; 16 bits reserved for later)
```

| Call site | file / function | what to add |
| --- | --- | --- |
| which pages exist | `params.c` `page_shown()` (~459) | `return kid_page_ok(pg) && <existing test>;` — this single line hides ENV/LFO/FX/SEL/EDIT/GLO/ARP/SEQ pages family by family |
| which layers open | `ui_input.c` `layer_tap()` (~777), `layers_input()` (~857) | refuse at entry when the layer's feature is locked |
| which tracks select | `ui_input.c` `panel_enc(EN_ALGO)` handling (~1279) | skip locked tracks 2/3 when cycling |
| which presets/engines list | `params.c`/`ui.c` browse list (`base_at`, `base_kind`, the `PRESETS` page) and `engines.c ENGINES[]` consumers | filter the list, do not renumber it |
| which drum kits | `drums.c DRUM_KIT_NAMES[]` (~29) consumers in `ui_studio.c` | same |

`page_shown()` is already the choke point that `open_family()` (`ui.c` ~501), `page_walk()` (~529),
`page_fix()` (~552), `page_first()` (~103) and the page button row in `ui_draw.c` (~829) all respect.
Adding the kid test there is therefore enough to make locked pages genuinely unreachable — which is why
this is a 1-line gate and not a UI rewrite.

**Trap:** `page_first(fam)` returns the first page of a family. If every page of a family is locked,
`page_first` still returns it and `open_family` will show a refused screen. Decide once: `open_family`
should refuse the *button* (with the level message) rather than land on a dead page.

---

## 3. The clubhouse screen

The device has no "home" concept to hang this on: in this build `go_home()` (`ui.c` ~566) sets
`ui.home = 0` and jumps to the first `FAM_TRK` page (the TRACKS screen), because `FELUCCA_ARRANGER` is
always 1.

**Recommended shape: a new page family appended at the end of `PAGES[]`** (`params.c` ~396), e.g.
`FAM_KID` with one page `{"CLUBHOUSE", FAM_KID, SC_KID, GR_NONE, {0xFF,0xFF,0xFF,0xFF}}`.

Why at the end: `PAGES[]` indices are used as `ui.page` and stored in RAM only, but every existing
page's index must stay put so `fam_last[]`, `page_walk()`'s index arrays and the drawing code behave
identically. Appending is the only index-stable option.

Then:

- `FAM_BTN[FAM_COUNT]` in `ui_input.c` (~28) needs an entry (`B_HOME`).
- `go_home()` opens it when kid mode is on: replace the `page_first(FAM_TRK)` line with a branch.
- `page_for_drum()` and `page_shown()` need a case so the new scope is never reachable through
  ALGORITHM/SELECT logic.
- `ui_draw()` (~1052) gets one early branch: `if (kid_mode && kid_owns_screen()) { kid_draw(); ... return; }`
  placed next to the existing `SC_TRK` / `on_drum_page()` branches (~1129).
- Drawing uses the existing strip canvas: `cv_begin(240, h, C_BLACK)`, `cv_text()`, `cv_blit()`
  (`gfx.c` ~184/265/197). There is **no framebuffer** — draw band by band, as `splash.c` and
  `ui_studio.c` do.

### Text metrics (measured from the generated fonts)

| font | height | digit advance | characters per 240 px line |
| --- | ---: | ---: | ---: |
| `FONT_S` | 16 px | 8 px | ~29 (proportional: expect 26–30) |
| `FONT_L` | 32 px | 16 px | ~14 |

So: heading in `FONT_L` ≤ 14 chars, instruction in `FONT_S` ≤ 28 chars per line, 2 lines. The
curriculum's screen lines were written to those limits.

### Layout

```
y   0..39   FONT_L heading: LEVEL n  +  title            (14 chars + "3/10")
y  40..74   FONT_S instruction, 2 lines
y  76..132  the band row: 4 tiles (DRUMS / BASS / KEYS / LEAD), lit = unlocked
y 134..176  the mission strip: dots for this level's 3 missions (done / current / locked)
y 178..202  four knob labels, each in its track colour (FONT_S)
y 204..238  two hints: "SEQ: HELP"   "HOME: FREE PLAY"
```

Two knobs of headroom: `Y_FOOT` in `ui.c` is 202 and the existing screens use 240−202 = 38 px for the
footer, so this layout matches the house style — and the footer is where the button reminder goes
(`SEQ: HELP` · `HOME: FREE PLAY`), because a card is read once and a footer is read every time.

---

## 4. Input interception

`ui_input()` (`ui_input.c` ~1104) reads edges once at the top:

```
pressed = fm1_input_edges(0);  notes = fm1_input_note_edges();  home = btn_hold(&ui.home_t0, B_HOME, now, 1);
```

**Corrected 20 Nov (finding 31 in `KID-MODE-STAGES.md`): do not insert it before the HOME block.** The
two lines between the HOME block and the menu are bookkeeping that the audio ISR depends on — in
particular `kb_grid` (`ui_input.c:1132`), which decides whether the white keys are drum sounds or grid
steps (`seq.c:2257`). Early-return before it, with `kb_grid` left at its last value, and the keys go
silent the moment a mission follows a visit to the DRUMS grid page. So:

```
    page_fix(); ... midi_entry();                       /* unchanged */
    kb_grid = (uint8_t)grid_keys_on();                  /* 1132: must run on every pass */
    if (home == BT_HOLD && !kid_on()) { ...open the menu... }      /* 1120, now guarded */
    if (kid_on() && kid_input(pressed, notes, home)) { ui.force = 1; return; }
    if (ui.menu) { ... }                                /* unchanged */
```

`kid_input()` gets `home` from line 1107, so nothing else has to move. Two rules for what it does
there: it **drains every encoder** (`panel_enc()` for all seven roles — `enc_hold` returns before the
take and so does not drain, `fm1_input.h:341`), and it sets `ui.force = 1` (`force` is a boolean,
`ui.c:63`, not a bitmask).

`kid_input()` then has two different jobs, and they must not be confused:

**(a) A kid-owned screen is up** (clubhouse, mission card). Nearly every button is free here, so use them:

| input | behaviour |
| --- | --- |
| `SEQ` tap | **HINT** — next hint line, replays the seed, then offers SKIP (§ curriculum "hint ladder") |
| `HOME` tap | back to the clubhouse / switch MISSION ↔ SANDBOX |
| `HOME` held 3 s | parent menu (§8) |
| `EDIT` tap | **UNDO** (one button — the child-facing version of stock's chord) |
| `PLAY` | start/stop the song, as stock |
| everything else | `NOT YET · LEVEL n` |

**(b) An instrument screen is up** (she is playing; gating is by omission through `page_shown()`, not by
interception). Intercept as little as possible:

| input | behaviour |
| --- | --- |
| `HOME` tap | back to the clubhouse (this is her only exit, so it must always work) |
| `HOME` held 3 s | parent menu (§8) |
| `ALGORITHM` | cycle only unlocked tracks |
| knobs 1–4 | pass through; on a mission screen the labels come from the mission's `knob[4]` |
| locked buttons/pages | disappear via `page_shown()`, not a refusal — the `NOT YET` message is for the card |
| **`OCT−` / `OCT+`** | **never intercepted.** They are the drum track's soft/hard hits and the arp octave — i.e. the instrument. The level-2 mission "hold OCT− for soft" teaches exactly this, and a gesture that changes meaning after graduation is a trap |
| **`EDIT`** | **never intercepted here**: it opens the editor pages from level 5, and undo here is stock's hold-`EDIT`-then-`OCT−` (§14) |
| note keys | pass through (always sound — never muted by gating) |

**Why the hint is not on `OCT+`** (it was, in the first draft): `OCT+`/`OCT−` are *musical* on two of the
four tracks, and the device label says so. `SEQ` only opens the sequencer family, which is locked until
level 9 anyway, and on a kid screen it is not needed at all.

**The stock hold threshold is 700 ms, not 3 s** (`btn_hold()`, `ui_input.c` ~765:
`700u * 1000u * FM1_TICKS_PER_US`). A 3 s parent-menu hold therefore needs its own timer field
(`ui.kid_t0`), not `btn_hold()` with a bigger number — that function has no duration parameter.

**Trap:** `ly_lock` (a locked layer, `ui_input.c` ~836 `layer_unlock`) and `ui.hold_kind` confirmations
already intercept input. Kid mode must clear both on entering a mission, or the first keypress gets
eaten and the child reads it as "broken".

**Trap:** `ui.entry_open` and `seq_entry()` handle the "keys held since the first press" chord entry.
If kid mode lets a keypress start an armed REC take, the mission seed can be overwritten by accident.
Decide: kid mode disarms REC when a mission loads.

---

## 5. The two rooms (must be built early, everything depends on it)

The user chose "strict tutorial + sandbox always open". The snapshot is the thing that makes that choice
safe. **Corrected 20 Nov — the first version of this section said "save it to section D", which would have
destroyed her saved section D** (`docs/KID-MODE-STAGES.md` §1, findings 3, 4 and 5):

- `proj_slot[4]` (`project.c:153`) is her **live sections A–D** in `.noinit`, and
  `sections_write()` (`project.c:1138`, called from `project.c:1215`) flushes any `sec_dirty` slot to
  flash. Using slot D as a scratch area writes the mission over her song.
- A new flash object is not available either: the "free" sector 0xFB000 is in no range that
  `FL_RANGE_OK` accepts (`fm1_flash.h:39-55`), and an appended `OBJ_*` lands in `st_sector()`'s default
  branch at 0xA5000 — inside the user sample slots (`storage.c:55`).
- And there is no RAM for a fifth `project_t`: NOINIT is 15,696 B and `proj_slot[4]` can take 15,360 of
  it (`app.ld:20`, `project.c:152`).

**So the snapshot is the autosave object, and a mission is a state that must not be persisted:**

| Step | Existing function (verified) |
| --- | --- |
| her song, as of the last quiet moment, is already in flash | `autosave_tick()` (`project.c:711`), `OBJ_AUTOSAVE` |
| put it there now if it is not yet | `proj_capture(&autosave_buf)` + `st_save(OBJ_AUTOSAVE, …)` (`project.c:723`) |
| stop autosave while a mission runs | one flag: `autosave_tick()` returns early |
| make it the live state again on the way out | `project_restore(4, …)` (`project.c:1082`) / `project_apply()` (`project.c:489`), then `autosave_hash = restored sum` |
| the pre-reset flush must write **her** song, not the seed | `persist_flush_now()` (`project.c:1157`) |

Costs: one 4 KiB erase on entering a mission (usually already paid), one flag, and a test. A power cut
during a mission brings back her song, which is the correct answer. **Do not call `project_save()`**: it
speaks to the user ("SAVED SECTION D"), refuses while playing, and sets `song_dirty`.

**Trap:** flash access silences the audio for the whole sector erase (`felucca.c fl_erase4k_quiet`), and
autosave's own rule is the template to copy — stopped, `!transport_req`, `!rec_wait`, `!ft_on`, no menu,
2.5 s since the last input, and `audio_quiet()` (`project.c:715`). The mission loader must stop the
transport before any of this, which is also what the child expects.

---

## 6. Witnesses and evaluation

One struct in RAM, cleared when a mission starts:

| field | size | set from |
| --- | ---: | --- |
| `keys` pitch mask | 16 B | the note-on path in `ui_input()` (any track) |
| `params_touched` | 4 B | `edit_param()` (`ui_input.c` ~495), plus the layer knobs |
| `steps_written` | 1 B | the record path (`seq.c`, already counts a pass) |
| `bars` | 2 B | `trk[].pass` / `song.tick` at frame rate |
| `layers_used` | 1 B | `layer_tap()` |
| `flags` | 1 B | `saved`, `undone`, `preset_changed`, `honour` |
| `params_moved_while_playing` | 4 B | `edit_param()` gated on `song.playing` |

Evaluate once per frame (cheap: a handful of mask compares). On success: save progress, unlock the
feature, show the reward, advance the mission. Because the masks persist, a mission can complete
during free play later — that is a feature, not a bug.

**Trap:** `edit_param()` is on the knob path for *every* page. Guard the witness writes with a single
`if (kid_mode)` so a normal build has exactly zero added work in that function.

---

## 7. Progress record (exact budget)

The settings object's **side record** (`storage.c` ~160): `ST_SIDE_OFF 64` → `ST_PAYLOAD_OFF 256`, so
**192 B**, of which the theme + favourites record uses 80 (`ui.c` `side_rec_t`, `_Static_assert`).
**112 B free.** Stock SLOOP reads this area but only interprets its own record, so an extra record is
invisible to it — provided it has its own magic and CRC and sits after the 80 B.

Proposed layout (24 B):

```
u32 magic "KID1"        u8  version        u8  levels_done (bitmask, 10 bits used)
u8  mission_state[8]    (30 missions x 2 bits: 0 locked, 1 open, 2 done)
u16 sessions            u16 badges         u32 crc32 over the above
```

Written only when the transport is quiet (`autosave_tick` pattern), and only when changed — a child
finishing missions in bursts is 20–40 sector writes per session at worst, which the A/B pair handles
without thought.

**Trap:** writing the settings object also rewrites `persist_t` + the SYN kits + the MIDI part
(`st_save3`, `project.c` ~958 `settings_write`). Reuse that whole path rather than a bare `st_save`, or
the next boot will restore stale kits.

---

## 8. Parent mode, volume, exits

- **Combo:** `HOME` held 3 s while in the sandbox → the existing menu code (`ui_menu.c`) with a
  reduced item list. Reuse `ui.menu`/`ui.menu_sel` rather than a new screen; `menu_input()` already
  handles knobs and its items are **all settings** — there is no project object in it, so it cannot
  wipe her work (finding 37).

  **Keep:** COLOR, ZOOM, KNOB ACCEL, LIGHTS, KEYS, NOTES, SPEAKER LOWCUT, STEP ADVANCE, DRUM PREVIEW,
  FILTER RESO, MIDI LEARN CLEAR, ABOUT. Plus the kid-only rows: `UNLOCK ALL`, `RESET PROGRESS`,
  `VOLUME LIMIT`, `BACKUP REMINDER`.

  **Hide:** `HARDWARE CALIBRATION` (`MI_PANEL`) — one OCT+ press rewrites the button map, and a
  mis-taught map also takes away the parent's UPDATE MODE combo, which reads `panel.btn[]`; only the
  raw power-on pair still works (`main.c:246`). Also `2ND CORE` (`MI_CPU1`, half the render headroom),
  `USB AUDIO` (`MI_USB`) and `USB RATE` (`MI_RATE`) — the sound path, not a kid's business. Hiding is
  a filter in the row iterator, not a table edit: `mi_sec()`/`mi_row()` index `MS_FIRST[]` directly.
- **Not a PIN.** A 10-year-old will find it in a week. The design is guidance, not security.
- **Volume ceiling:** `main.c:283` sets `song.master_q12 = (k10 * k10) >> 8` (0..4092) from the analog
  MASTER pot. Clamp the *result*, not `k10`, to ~70 %: lines 278–282 let the LIVE page's encoder
  **override** the pot (`k10 = (ed_master - 1) * 1023 / 126`), so a clamp on the pot value alone is
  bypassed by turning the knob on that page. One clamp after line 283, a runtime flag the parent menu
  lifts. Master gain is applied after the mix, so `tests/golden.txt` should be untouched — confirm it.
- **Battery:** there is no low-battery warning anywhere in the stock firmware, only the header icon
  (`ui_draw.c:26-36`). Autosave is the real protection, so her song survives a flat battery; the
  *mission* does not. One `ui_say()` on the way down is enough, with hysteresis so it cannot loop.
- **Taking it off:** keep the existing UPDATE MODE combo (OCT− + OCT+ 5 s) working, and keep
  `FELUCCA_OTA` on. You will want to push fixes in January.

---

## 9. Content pipeline (one table, two outputs)

```
kid_missions.c  (the 30 rows: id, level, title, line[2], knob[4], seed, check, unlock, hint)
        |
        +--> kid_content.h   (generated: the on-device strings only, ~3 KB)
        +--> cards.ps/.pdf   (generated by tools/kid_cards.py: 30 A6 cards, 4 per A4 sheet)
```

The long `card` copy lives **only** in the generator's input table (a separate CSV or a `#if 0` block
in the same file) so the firmware never carries it. This is the whole reason the paper kit and the
device cannot drift.

---

## 10. Files and rough budget

| file | contents | est. code |
| --- | --- | ---: |
| `firmware/src/kid.h` | feature enum, record layout, API | ~40 lines |
| `firmware/src/kid_state.c` | progress load/save/CRC, unlock, parent-menu actions | ~150 lines |
| `firmware/src/kid_ui.c` | clubhouse draw, band row, mission strip, reward, hint ladder | ~300 lines |
| `firmware/src/kid_missions.c` | the 30-row table, witnesses, predicates, seed expansion | ~250 lines |
| `firmware/src/kid_rooms.c` | snapshot/restore, mission load/unload | ~80 lines |
| `firmware/src/kid_content.h` | generated strings | ~3 KB data |
| `firmware/src/kid_cards.tsv` | the printable copy (never compiled) | 30 rows |
| `tests/kid_test.c` | host test (see §12) | ~300 lines |
| `tools/kid_cards.py` | TSV → printable cards | ~120 lines |

Total new code ≈ 1,000 lines + 3 KB of strings. In the measured terms of this repo that is roughly
**8–12 KB of image**, which is why §11 exists.

---

## 11. Build, release, and paying for it

- **Flag:** `FELUCCA_KID` in `felucca.c` (`#ifndef`-guarded, and add it to the whitelist in
  `tools/build.py` ~183 — the flag list there is explicit, so a new flag that is not added is silently
  ignored; that trap cost time during the measurement session).
- **Identity:** `FELUCCA_ID "FM-1_910"`, version string `SLOOP JR 1.0`, package name
  `sloop-jr-1.0.fwsc`, so it can never be confused with stock in an installer list.
- **Paying for it (measured 20 Nov — nothing needs cutting):** stock 2.8.5 uses 571,820 B of the
  581,564 B slot, so there are **9,744 B free**, and the bitmap wordmark is 4,635 B of that image
  (`splash.c:36-50`). The kid build replaces the splash with a text card, so **build it with
  `FELUCCA_LOGO_TEXT=1` from the start and the budget is 14,379 B** — not an emergency fallback but the
  working number. Note that *not calling* `sloop_logo_draw()` reclaims nothing: `tools/build.py:40`
  passes `-ffunction-sections` without `-fdata-sections`, so the unreferenced array stays in `.rodata`;
  the flag drops it at preprocessing time (`build.py:343` says the same: "4.6 KB less"). If the kid
  build still overruns, cut the editor's LIVE/FM6/DSYN pages (§ plan §8). **Do not cut sample sets** —
  it buys room we do not need, renumbers the SAMPLE presets, and forces a kid-only `tests/golden.txt`.
- **Check the size without a toolchain:** `python3 tools/decode_fwsc.py PKG.fwsc` prints the image and
  the free bytes for any package (`docs/KID-MODE-MEASUREMENTS.md` §2b), and a stock build must print
  `image 571820 B`.
- **Installer:** a separate page under `web/` built by `web/make_site.py`, branded, with one button.
  Do not host it on the public SLOOP page.

---

## 12. Tests (write these before the UI, they are the safety net)

`tests/kid_test.c`, host only, no hardware — model it on `tests/ui_pages_test.c` (which drives the real
UI functions) and `tests/knob_walk_test.c` (which enumerates every page):

1. **Gate matrix.** For each level 1..10, assert exactly which of the ~40 `PAGES[]` entries are
   reachable, which layers open, which tracks `ALGORITHM` can select, and which kits/presets appear.
   This is one table-driven test and it catches every off-by-one in the unlock progression.
2. **Progress round trip.** Save at each level, reload, assert identical; simulate a torn write (the
   A/B pair) and assert the previous level survives; assert stock-2.8.5-shaped settings still boot.
3. **Sandbox safety.** Drive: make a song → enter mission 1 → play → leave → assert the song is
   byte-identical. Repeat for mission 30 (after slot D is released at level 9).
4. **Seed expansion.** For all 30 seeds: expand, then assert the result passes the existing project
   checksum and that every lane matches the seed's declared bits.
5. **Predicates.** For each check type, feed a synthetic witness sequence and assert complete /
   not-complete at the boundary (this is where the curriculum's "≥" off-by-ones live).
6. **No audio change.** Run the existing `tests/regress.c` in the same suite; `tests/golden.txt` must
   be untouched for the *stock* configuration — with no sample cuts the kid build shares it, which is a
   free regression net the earlier plan had to give up.
7. **Image size (per build, printed by `build.py`).** Stock must read **571,820 B**; the kid build must
   stay under **581,564 B** *without* `FELUCCA_LOGO_TEXT` — if it only fits with the text logo, that is a
   budget overrun to look at, not a success.

On device (manual checklist, printed): the four buttons she uses (`PLAY`, `REC`, `SEQ`, `HOME`) plus
`EDIT`, a full level 1–3 playthrough, save/load a section, the parent menu, a power cycle mid-mission,
a stock-SLOOP reinstall to prove her files still open, and one deliberate mistake to check that she can
find the undo (`EDIT` on a card, hold `EDIT` + `OCT−` on an instrument screen).

---

## 13. Order of work

**(20 Nov) This list is the dependency order; `KID-MODE-STAGES.md` §3 is the same work with dates,
entry/exit criteria and a gate between each stage. Where they differ, the stages doc wins — it was
written after a code sweep that found two ways to lose her work.**

| # | Task | Depends on | Why this order |
| --- | --- | --- | --- |
| 0 | Order the second FM-1; toolchain + a stock build (`image 571820 B`); `FELUCCA_KID` in the flag whitelist | — | the measurement no longer blocks anything, but the hardware does |
| 1 | The gate, both halves (§1 and §2.2 of the stages doc) | 0 | proves the choke point before there is content |
| 2 | `kid_test.c` gate matrix, incl. "every family keeps a visible page" | 1 | fixes the progression table in code, not in prose |
| 3 | Progress record in the side record + parent menu reset | 1 | so testing a level twice is easy |
| 4 | Clubhouse screen + the button map (§4) + the refusal line | 1, 3 | first thing a child can actually touch |
| 5 | Two rooms (§5: the autosave snapshot) | 1, 2 | everything else assumes it; nothing is safe until it passes |
| 6 | Witnesses + predicates + seeds for levels 1–2 (6 missions) | 4, 5 | a playable slice to test the feel with |
| 7 | **Playtest #1** with a real 10-year-old, 20 minutes | 6 | before writing 24 more missions |
| 8 | Missions 7–30, one level at a time, with a playtest after level 4 and level 8 | 6 | content is cheap to change before it is a system |
| 9 | Reward pass (band row, surprise sound, jingle), hint ladder | 8 | polish that needs the whole course present |
| 10 | Volume ceiling, lights defaults, parent docs + the power-on card | 9 | safety last, because it does not block testing |
| 11 | Release build (no sample cuts), install on the gift unit, stock `.fwsc` in the box | 0, 10 | the point of no return, do it late |
| 12 | Print the cards from the mission table, assemble the box | 9 | can run in parallel from day one |

---

## 14. Traps, collected

- `PAGES[]` order is load-bearing; append, never insert.
- `page_first()` returns a page even when every page of the family is gated.
- `project_save()` talks to the user and refuses while playing; the kid snapshot must not call it.
- Flash writes silence audio for a whole 4 KiB erase: snapshot only while stopped.
- `settings_write()` writes three parts (`persist_t`, SYN kits, MIDI part); do not hand-roll a flash
  write for the kid record.
- `ly_lock`, `ui.hold_kind`, `ui.entry_open` can each swallow the first keypress.
- `ui.home` is only meaningful when `FELUCCA_ARRANGER` is 0 — it is always 1 here, so "home" is a page.
- The `tools/build.py` flag whitelist is explicit; a new `-D` is ignored unless listed.
- Font advance is proportional: budget 26–30 characters per line at `FONT_S`, then check on hardware.
- Cutting sample sets renumbers SAMPLE presets: her saved projects keep engine+preset indices, so cut
  them **before** she starts, not after.
- `FELUCCA_LOGO_TEXT` is an **automatic fallback on link overflow** (`build.py` ~341: it catches
  `overflow` in the linker output, sets the flag and rebuilds). So an overrun does not fail the build — it
  silently trades the boot picture for a text logo. Watch the printed image size, and note that the
  fallback is the *first* thing the budget spends, not a last resort.
- A child pressing PLAY while a mission loads is the normal case, not the edge case: every load path
  must stop the transport first.
- **Undo is a two-button chord and there is only one undo slot.** `ui_input.c` ~949: `held == LY_ERASE`
  (EDIT) *and* `OCT−`/`OCT+`, edge-detected; the `undo` struct keeps **one step, not per-track**. So
  (a) it cannot rescue an accidental NEW LOOP, which is why `G_NEWPRJ` must be *absent* rather than
  guarded, and (b) the safety mission has to spell the chord out, because a child will not discover it.
- **`OCT−`/`OCT+` are instrument controls, not navigation.** On the drum track they are soft/hard hits
  (`core.h` ~203), on the arp they are the octave. Stealing them for "hint" teaches a gesture that
  vanishes at graduation. The hint went to `SEQ` on kid screens instead.
- **Drum lanes are not kick/snare/hat.** `drums.c` ~85, white keys F3 upward:
  `KICK, KICK 2, SNARE, CLAP, HAT, OPEN HAT, PEDAL, RIM, SNARE 2, LOW TOM, HI TOM, CRASH, RIDE, SHAKER,
  CONGA, COWBELL`. So "kick and snare together" is the 1st and 3rd white keys, not the 1st and 2nd —
  mission copy and any witness check on lane index must use the real order.
- **`·` is safe in `FONT_S`** (index 151, 2 ink pixels, 8 px advance), so the `KICK·SNARE` notation the
  cards use really renders. `-`, `:`, `/`, `!`, `?` and `'` are all 8 px too.
- `G_NEWPRJ` calls `project_new()` with no confirmation and only refuses while the song plays
  (`ui_input.c` ~600). Omit it from kid mode; do not try to intercept it after the fact.
- **A page gate is not a gate.** The layer holds (SAVE+key stores sections, EDIT+key erases, GLO+keys
  mute) live in `ly_bit[]` and run in the audio ISR; REC held clears the track from the *raw* button bit;
  MIDI REMOTE presses panel buttons. See `KID-MODE-STAGES.md` §1b (findings 21–23) before writing the
  gate.
- **Never early-return before `kb_grid = grid_keys_on()`** (`ui_input.c:1132`): the drums go silent when
  a mission follows a visit to the grid page (finding 31).
- **Drain the encoders** (`fm1_input.h:341`): `enc_steps[]` accumulates until taken, and `enc_hold` does
  not drain — a kid screen that ignores them makes the next value jump by everything she spun
  (finding 33). `ui.force` is a boolean, not a bitmask (finding 34).
- **`FAM_BTN[]` zero-fills a missing entry** to `B_FX` (`ui_input.c:28`), so a new family needs its
  initialiser in the same edit — C will not warn.
- **Header messages are 24 bytes** (`ui.c:86`), and a kid screen that replaces the header hides them —
  including the parent's UPDATE MODE countdown (finding 32).
- **`page_first()` is gate-blind and `ui.page` is unbounded.** Hiding a family's first page makes
  `page_fix()` re-enter the gate every frame (`ui.c:103`, `:550`), and `cur_page()` indexes `PAGES[]`
  with no bounds check (`ui.c:96`). Gate `page_first()` too, and assert every family keeps a visible page.
- **A page gate does not gate a value.** Kits are `TDRUM->p[P_E0]` with three writers
  (`ui_input.c:1273`, `ui.c:977`, the parameter editor), and engines/presets change through
  `select_engine()`/`preset_go()`. `pc_poll()` (`ui.c:964`) changes both from MIDI with no UI involved —
  ignore Program Change in kid mode.
- **The screen is 240×124 at most** (`gfx.c:30` `CV_MAX`), and `cv_begin()` clamps silently. Draw the
  clubhouse as the house's three bands, not as one 240×240 layout.
- **Power-on and 5-second combos are child-reachable**: OCT−+OCT+ at power-on starts button calibration
  with whatever she presses (`main.c:246`), OCT− alone enters recovery (`bootguard.h:27`), and OCT−+OCT+
  held 5 s while stopped enters UBOOT (`main.c:286`). Leave the combos alone and put them on the card.
