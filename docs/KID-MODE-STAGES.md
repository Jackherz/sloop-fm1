# SLOOP JR — the code sweep and the stage plan

20 Nov 2026. Companion to `KID-MODE-PLAN.md` (why), `KID-MODE-IMPLEMENTATION.md` (how), and
`KID-MODE-MEASUREMENTS.md` (what it costs). **This file is the order of work**: what will bite you, in
which order to build, and what has to be true before each stage is allowed to call itself finished.

Read §1 first even if you skip the rest. Five of those findings change decisions that are written down
elsewhere, and two of them would have cost her work or her instrument.

---

## 0. How this document was made

A read of the actual paths a kid build must touch, not a survey: the page system, the input router, the
project/section storage, the settings side record, the flash map, the RAM map, and the boot paths. Every
finding below has a `file:line` next to it so it can be re-checked in a minute. Where a number is an
upper bound (a `_Static_assert`, a region size), it says so.

**Nothing here is a reason to stop.** The space question is answered (9,744 B free, 14.4 KB with the
text logo, 8–12 KB needed), and none of these findings needs a feature dropped. Two of them, though,
would have shipped as data loss.

---

## 1. Findings: what would break, and why

Severity: **S1** = would lose her work or make the unit look broken · **S2** = a leak in the tutorial or
a wrong screen · **S3** = cost, polish, or a documented trap.

| # | finding | sev | evidence | what happens | fix | stage |
|---|---|---|---|---|---|---|
| 1 | **`page_first()` ignores the gate** | S1 | `ui.c:103` (no `page_shown` check), `ui.c:550` `page_fix()` | Hide a family's *first* page and `page_fix()` can never settle: it walks back while the predecessor is in the family, then assigns `page_first(fam)` — the hidden page — and `page_entered()` fires. Every frame. A hidden page is displayed permanently, with a redraw loop. | make `page_first()` gate-aware and give it a **safe** fallback; assert in the gate test that every family keeps ≥ 1 visible page | S1 |
| 2 | **`ui.page` has no bounds check** | S1 | `ui.c:96` `cur_page()` → `&PAGES[ui.page]` | If a family ever has no visible page and `page_first()` returns `NPAGES` (or 0 with an empty family), `cur_page()` reads past `PAGES[]`: garbage `params[]` ids → writes to the wrong places. | never let a family be fully gated; `page_first()` returns a page that exists; a `_Static_assert`-style host test over the matrix | S1 |
| 3 | **Snapshotting into "section D" is data loss** | S1 | `project.c:153` `proj_slot[4]` is live A–D in `.noinit`; `project.c:1130` `section_store()`; `project.c:1138` `sections_write()`, called from `project.c:1215` | `proj_slot[3]` *is* her section D, and `sec_dirty` + `sections_write()` will flush the kid snapshot **over her saved section D in flash**. The model meant to protect her song would overwrite it. | see §2.1 — snapshot lives in the **autosave object**, not a section | S4 |
| 4 | **A new flash object is not possible where it looks possible** | S1 | `fm1_flash.h:39-55` `FL_RANGE_OK`; `storage.c:55` `st_sector()` | The "free" sector 0xFB000 is in **no** writable range, so a save there fails. An appended `OBJ_*` falls into `st_sector()`'s default branch → `0xA5000`, **inside the user sample slots** → would corrupt her recorded samples. | do not add an object; if one is ever needed, add a branch to both `st_sector()` and `FL_RANGE_OK` first | S4 |
| 5 | **There is no RAM for a fifth project buffer** | S2 | `app.ld:20` NOINIT 0x3D50 = 15,696 B; `project.c:153` `proj_slot[4]`; `project.c:152` assert ≤ 3,840 B each | 4 × 3,840 = 15,360 B of 15,696. Even at the assert's upper bound there are ~336 B left, so a `project_t` snapshot buffer does not fit in `.noinit` (`autosave_buf` sits in `.pool`, where the build needs ≥ 8 KiB spare: `build.py:265`). | snapshot through the autosave object (§2.1); no new buffers | S4 |
| 6 | **`autosave_tick()` will eat the mission** | S1 | `project.c:711-724`, `AUTOSAVE_IDLE` 2500 ms, `GAP` 20 s | While she plays a mission, the working project is autosaved every ~20 s — so "her song" in flash becomes the mission by the time she leaves. | suspend autosave during a mission, flush hers before the seed loads | S4 |
| 7 | **`G_NEWPRJ` + one-slot undo** | S1 | `ui_input.c:462` `project_new()`, `ui_input.c:610`; `seq.c:449-471` `undo` struct; `ui.c:259` `undo_swap()` | NEW LOOP wipes all four tracks with no confirmation. The undo slot holds **one track's steps + length** only — not parameters, not engine/preset, not globals — so it cannot bring the project back. | hide the TOOLS page from the gate; teach undo as "the last pattern change on this track" | S1, S6 |
| 8 | **MIDI Program Change bypasses the whole gate** | S2 | `ui.c:964-982` `pc_poll()` writes `trk[i].p[P_E0]` (kit) and loads presets directly | With a MIDI keyboard attached (likely — she is a piano student), a PC message selects a kit or preset regardless of level and regardless of what the screen shows. | ignore PC (and Bank Select) in kid mode, or filter it through the same allow-lists | S1 |
| 9 | **Kits and presets are values, not pages** | S2 | kit: `ui_input.c:1275` (encoder), `ui.c:977` (PC), `params.c:412` (`EDIT 1` exposes `P_E0` as KNOB 1), `project.c` `proj_apply()`; presets: `ui.c:997` `preset_go()`, `ui.c:673` `set_engine_of()` | A page gate alone does nothing for `P_E0` or a preset index: they are reachable from an encoder, from MIDI, from a project/seed load, and from a parameter page. | gate the **values** at their chokepoints (§2.2) as well as the pages | S1 |
| 10 | **OCT− + OCT+ held 5 s while stopped enters UBOOT** | S2 | `main.c:286-300` (countdown shows from 2 s, cancels if `song.playing`) | On the drum track those two buttons are the soft/hard modifiers, so this is a *playable* pair. A child holding both while the transport is stopped gets an update-mode countdown and a dark screen. Recoverable, but it reads as "broken" on Christmas morning. | in kid mode, require the parent menu (or a longer hold) for this combo | S3 |
| 11 | **Power-on combos are child-reachable** | S2 | `main.c:246` OCT−+OCT+ at power-on → `panel_setup()`; `bootguard.h:27` OCT− alone → recovery; `recovery.c:48-58` (50 ms window) | OCT−+OCT+ at power-on starts **button calibration**, which rewrites the learned button map from whatever she presses next → a unit whose labels do nothing. OCT− alone at power-on enters recovery instead of booting. | leave the combos (they are the parent's tools) but put both on the printed card, and keep a spare of the panel table in the settings so calibration can be undone | S3 |
| 12 | **The kid screen cannot be 240×240** | S2 | `gfx.c:30` `CV_MAX = 240*124`, `gfx.c:184` `cv_begin()` clamps silently, one `cv_px` buffer | A layout written as "y 0..238" is not drawable: the canvas silently becomes 240×124 and the lower half of the design lands on top of the upper half. | draw in the house bands (§2.3): header 0..19, graph 74..197, footer 202..239 | S3 |
| 13 | **`ui.menu` and the layer locks swallow input** | S2 | `ui_input.c:1122-1131` (menu early-returns *after* `page_fix`), `ui_input.c:1141` `home_eat`, `ly_lock`/`ui.hold_kind` | If the parent menu is open, or a layer lock is in force, the kid handler never runs — the screens look frozen and the first keypress is eaten. | force `ui.menu = 0`, clear `hold_kind`/layer locks and `ui.home_t0` on mission entry *and* on leaving the parent menu | S3 |
| 14 | **Flash writes stop the audio ~50 ms** | S2 | `project.c:691-724` (autosave waits for `audio_quiet()`), `felucca.c fl_erase4k_quiet` | Writing progress at the wrong moment glitches the sound she is playing — the one thing that must never stutter. | piggyback on the settings save, deferred exactly like autosave (stopped, quiet, 2.5 s idle) | S2 |
| 15 | **The settings object is shared with the web editor** | S3 | `project.c:772-773` assert: `web/editor.html` writes bytes 48..83 (`ARR`) | Any change to `persist_t` breaks the web editor's Song page silently. | do not touch `persist_t`; kid data goes only in the **side record** | S2 |
| 16 | **The stock side record is 80 B and self-checking** | S3 | `ui.c:888-905` (`side_rec_t`: theme 40 B, then `fmagic`+`fav`+`fcrc`), `project.c:767` | A kid build that grows `sizeof(side_rec_t)` naively changes where stock 2.8.5 expects its CRC — it would then reject the record and drop her theme and favourites at graduation. | append a **separate** block after `fcrc` with its own magic and CRC; stock keeps validating its own 80 bytes | S2 |
| 17 | **`side_fill()` memsets the whole record first** | S3 | `ui.c:897-904` | A kid block filled by an earlier call is zeroed by the next `side_fill()`. | fill the kid block *inside* `side_fill()`, after its `memset`, from a RAM copy | S2 |
| 18 | **A FREE TAKE overwrites the pattern** | S2 | `seq.c:1093-1124`, `seq.c:1345` `ft_on`; `autosave_tick` also refuses while `ft_on` | REC on an empty part starts a free take that replaces the pattern — a one-button way for a child to destroy the mission seed (and the undo slot only has one track). | keep REC out of level 1, and disarm/refuse it while a mission is active | S5 |
| 19 | **`page_entered()` has global side effects** | S3 | `ui.c:124-142` sets `song.seq_mode`, clears `ui.entry_open`/`hot_t`, raises `ui.force` | A kid page appended to `PAGES[]` inherits them; forgetting `seq_mode` leaves the sequencer armed behind the clubhouse. | the kid page sets `song.seq_mode = 0` in its own enter path; test it | S3 |
| 20 | **Drawing cost is one canvas at a time** | S3 | `gfx.c:184-215`, `ui_draw.c:52-120` (several `cv_begin`/`cv_blit` bands per frame) | A bespoke kid screen that fills the display in one pass cannot exist; and doing it in many small bands has a per-band `lcd_blit` cost that is not measured here. | design in 3 bands (header / 124 px body / footer), measure on the device before adding a fourth | S3 |

Two smaller ones, recorded so they are not rediscovered: the `undo` slot is keyed by `sess` and a
*session* is one mark (`seq.c:456-471`), so a mission's seed load must not mark undo for her (she would
"undo" into the seed and think the mission vanished); and `page_walk()`'s index array is `uint8_t
idx[16]` (`ui.c:527`) — `FAM_GLO` has 9 pages today, so a kid build that *adds* GLO pages would overflow
it. Add pages to a **new** family, never to GLO.

---

## 1b. Second sweep: the parts that are not pages at all

The first sweep followed the page system. This one followed **raw button bits** — because the FM-1 gives a
button three different meanings depending on how long it is held and what is held with it, and those
paths do not consult `PAGES[]` at all. Ten more findings, and the first three are the worst in the whole
document for the *experience* (a 10-year-old does exactly these things).

| # | finding | sev | evidence | what happens | fix | stage |
|---|---|---|---|---|---|---|
| 21 | **The layer holds bypass the page gate completely** | S1 | `ui_layers.c:1-24` (the layer table), `:43` (`ly_bit[]`), `seq.c:153-163` (`layer_buttons()`/`layer_now()`) | Hold SAVE + key 5–8 = **store the loop over section A–D**; hold EDIT + key = **erase that sound/note**; hold GLO + keys = mutes/solo/fills; hold SEQ + keys = step editing, locks, fill conditions; hold ARP = rolls. None of it is a page. And the keys' half runs **in the audio ISR** from `fm1_in.buttons & ly_bit[l]`, so intercepting the press in `ui_input()` does nothing. | zero `ly_bit[l]` for every locked layer — that is the ISR's whole lookup table | S1 |
| 22 | **REC held ~0.7 s clears the track, from the raw bit** | S1 | `ui_input.c:1026`, `:1038-1061` (`rec_down` reads `fm1_in.buttons`, not the consumed `pressed`) | A child resting a finger on REC wipes the track. It does mark undo first, but undo is one track and she will not know. Consuming the press in the kid handler does **not** stop it. | mask the bit for the hold path too (a `kid_btn_blocked()` check in `holds_input`), or gate `ui.hold_kind` | S1 |
| 23 | **MIDI REMOTE can press any panel button** | S2 | `ui_input.c:1073-1101`, `fm1_remote_btn`; the code's own comment: "a let-go never coming: REC would clear a track" | With a controller on the REMOTE channel, CCs become SAVE/EDIT/REC presses — the same three dangerous holds, without touching the instrument. | kid mode sets `G_RMCH = 0` (REMOTE off) | S1 |
| 24 | **REC on an empty pattern is a FREE TAKE, not grid recording** | S2 | `seq.c:1093-1110` (the comment), `track_empty()`, `rec_tempo` | The curriculum's level-1 "REC, THEN PLAY" assumes a tempo grid. On an empty pattern stock starts a *free take* (no tempo, no grid, up to 24 s). | kid mode forces `rec_tempo = 1` (TEMPO) — and note it is a **persisted** setting (bits 9–10 of `lights_word`) | S5 |
| 25 | **A crash is a red hex screen for ten seconds** | S2 | `main.c:129-172` `fm1_fault()` | If a kid-build bug traps, the screen goes red with vector/PC/SP hex for 10 s, then reboots. To a child that is a broken present. | do not remove the diagnostic (it is the parent's only evidence); put "red screen = photo it, then off and on" on the card | S3 |
| 26 | **Rapid power cycling does *not* trip the boot guard** | — | `main.c:384` (`bootguard_clear` only on a real power-on, not a watchdog/soft reset) | **Verified non-issue**, recorded because it looks alarming: only *warm* resets escalate (two → recovery via `bootguard_begin`, `bootguard.h:16-26`). Switching off and on is safe. | — | — |
| 27 | **A missing `FAM_BTN[]` entry silently means `B_FX`** | S2 | `ui_input.c:28-29`; `B_FX` is 0 in `panel.c:9` | Add `FAM_KID` to the enum and forget the initialiser, and the family is reached by the FX button instead of HOME. C zero-fills the missing entry; it does not fail to compile. | extend the array in the same edit; a static assert on `sizeof FAM_BTN` is free | S3 |
| 28 | **Header messages are capped at 24 bytes** | S3 | `ui.c:86` `char msg[24]`, `ui_say()` | "NOT YET · LEVEL 6" fits (17), "HOLD EDIT, THEN OCT-" fits (21). A longer mission hint silently truncates. | keep every `ui_say`/`ui_message` under 24 and let the card carry the rest | S3 |
| 29 | **The button LEDs default to OFF** | S3 | `panel.c:146-156` (`settings_init` sets palette/lowcut/zoom, not lights); `lights_word()` bit 0–3 | A fresh unit has no background glow on the buttons. Not a bug, but it is half of "this looks like a toy that lights up". | set `LIGHTS_LOW` and a key-light mode once in the gift build's settings | S11 |
| 30 | **Every `ui.force` is a full 240×124 blit** | S3 | `ui_draw.c:52-120`, `gfx.c:184-215` (one canvas, three bands) | A kid screen that raises `ui.force` while it animates redraws the strip and the footer every frame: CPU, battery, and a flicker risk that is invisible on a desk. | redraw on state change only; measure with the CPU readout on the SYSTEM page | S3 |
| 31 | **An early return before `kb_grid` makes the keys silent** | S1 | `ui_input.c:1132` (`kb_grid = grid_keys_on()`), `seq.c:2257` (`is_drum && layer == LY_PLAY && kb_grid` → a step key, no sound), `ui_studio.c:515` | `kb_grid` is a **volatile** flag the audio ISR reads. Insert the kid handler "before the HOME block" as first written, and the refresh never runs while a kid screen is up — so the last value sticks. Come from the DRUMS grid page into a mission and every white key is swallowed as a step: the instrument goes silent and looks broken. | put the kid handler **after** line 1132 (it only needs `home`, computed at 1107) and guard the menu block with `!kid_on()`; or set `kb_grid = 0` inside the kid handler | S1 |
| 32 | **Kid screens hide the messages, including the parent's** | S3 | `ui_draw.c:52-56` (`ui.msg` in the header), `main.c:296-306` (UPDATE MODE countdown via `ui_say`) | Every message — `NOT YET · LEVEL n`, "SAVED SECTION D", the update-mode countdown — is drawn by the stock header. A kid screen that replaces the header hides all of them, so the parent's countdown would be invisible and a save would look like nothing happened. | keep a 20 px header band on kid screens and draw `ui.msg` in it, exactly as the house does | S3 |

**The shape this gives the gate** (revised in §2.2): pages, values, and raw bits. Three levels, and a
button has to be blocked at all three or it is not blocked. Finding 31 is the reminder that the gate must
also *not* skip the bookkeeping lines around it.

## 2. Decisions these force (changes to the other docs)

### 2.1 The snapshot is the autosave object, not section D

`KID-MODE-IMPLEMENTATION.md` §5 said "save it to slot D". Findings 3, 4 and 5 rule that out: slot D is
her own live section, a new flash object cannot be placed without editing the flash map, and there is no
RAM for another `project_t`. The design that needs no new storage and no new RAM:

```
enter a mission (stopped, quiet):
    autosave_tick() has already put her working project in OBJ_AUTOSAVE
    if not: st_save(OBJ_AUTOSAVE, &autosave_buf)   -- one save, while stopped
    kid_mission_active = 1                         -- autosave_tick() returns early from now on
    load the seed into the working project
leave a mission:
    kid_mission_active = 0
    project_restore(4, ...) from OBJ_AUTOSAVE, i.e. project.c's "load the working project"
    autosave_hash = restored sum                   -- so it is not rewritten pointlessly
```

Why this is the right shape: OBJ_AUTOSAVE already *is* "her song, as of the last quiet moment". Missions
are a temporary state that must not be persisted, and a power cut during a mission should bring back
**her song**, not the mission. Costs: one extra 4 KiB erase per mission entry (already paid by autosave
in the common case), and one flag in `autosave_tick()`.

Two edges to write tests for: `persist_flush_now()` (`project.c:1157`, called from `main.c:306/323/334`) writes `autosave_buf` before an
update or reset — if that runs mid-mission it must write **her** song, not the seed; and leaving a
mission while `song.playing` must stop the transport first (an erase under a running transport is the
one thing that glitches).

### 2.2 The gate has three levels, and a button must be blocked at all three

Pages are the smallest part of it. A kid build that only gates pages still hands a child SAVE + key
(section store), REC held (clear track) and EDIT + key (erase), because those live in the layer table and
in raw button bits (findings 21–23).

**Level 1 — pages.** `page_shown()` plus `page_first()`/`page_fix()`, `open_family()`, the draw row.

**Level 2 — values.** These are not pages and are reachable from an encoder, a MIDI message, or a
project load:

| what | chokepoint | gate |
|---|---|---|
| engine | `select_engine()` `ui.c:701` | engine id in the level's allowed set |
| preset | `preset_go()` `ui.c:997` | list index mapped through an allowed list, not just clamped |
| drum kit | `TDRUM->p[P_E0]`: `ui_input.c:1275`, `ui.c:977`, `params.c:412` (`EDIT 1` KNOB 1) | `clamp(0, kid_kit_max())` at each writer |
| track | `track_select()` `ui.c:1034` | skip tracks above `kid_track_max()` |
| MIDI PC | `pc_poll()` `ui.c:964` | ignored while kid mode is on |

**Level 3 — raw button bits.** The ISR and the hold paths read `fm1_in.buttons` directly:

| what | where | gate |
|---|---|---|
| every layer (FX, EDIT, ARP, SEQ, SEL, GLO, SAVE) | `seq.c:153-163` via `ly_bit[]` (`ui_layers.c:43`) | zero `ly_bit[l]` for locked layers; that is the entire lookup |
| REC held = clear track | `ui_input.c:1026-1061` | a `kid_btn_blocked()` check beside `rec_down` |
| free take buttons | `ui_layers.c:46-47` `ft_btn_mask`/`ft_drop_mask` | irrelevant once `rec_tempo` is forced, but mask them if not |
| buttons over MIDI | `fm1_remote_btn`, `ui_input.c:1073-1101` | `G_RMCH = 0` |

Deliberately **not** gated: `proj_apply()`/`proj_import()`. A project restore must reproduce what she
made. If a restore could put a locked engine back, the gate would be lying about what she owns.

### 2.3 The kid screen is three bands, not one canvas

`cv_begin(240, 124)` is the biggest canvas that exists (`CV_MAX`), and the house layout already uses
three bands: header 0..19 (`H_HEAD` 20), graph strip 74..197 (`Y_GRAPH` 74, `H_GRAPH` 124), footer
202..239 (`Y_FOOT` 202). The clubhouse is therefore:

```
band 1  240 x 20    LEVEL n  ·  title                     (FONT_S, one line, 28 chars)
band 2  240 x 124   instruction (2 x FONT_S) + tiles + mission dots   (the graph strip's slot)
footer  lcd_fill + a 240 x 38 band                          SEQ: HELP · HOME: FREE PLAY
```

That is also the cheapest thing to draw, which matters because there is no framebuffer.

### 2.4 Progress record: appended side block, never a new object

`OBJ_*` cannot grow (finding 4) and the settings object is shared with the web editor (finding 15). So:
`ST_SIDE_OFF`..`ST_SIDE_MAX` = 192 B, of which stock uses 80 → **112 B free**. The kid block is
magic + version + level + 60 mission bits + sessions + badges + CRC32 = **24 B**, appended after `fcrc`,
filled inside `side_fill()` after its `memset`, validated by its own CRC (finding 16, 17). Stock still
reads and validates its own 80 bytes, so graduation keeps her theme and favourites.

### 2.5 Record the two dangerous combos on paper

Findings 10 and 11 are not fixable without taking away the parent's tools. The card taped under the unit
gets: "hold **nothing** at power-on. If the screen is black or asks you to press buttons one at a time,
switch it off and on again."

---

## 3. The stages

Each stage is a stopping point: the build works, the tests pass, and if Christmas arrived tomorrow the
gift would still be good. Sizes are estimates from the measurement doc's shares; the gate is
`image ≤ 581,564 B` **without** falling back to the text logo.

### S0 — Scaffolding, so the flag exists before the code does  (½ day)

| | |
|---|---|
| **Goal** | A `FELUCCA_KID` build that is byte-identical to stock in behaviour. |
| **Work** | Add `FELUCCA_KID` to `felucca.c` (`#ifndef`-guarded) *and* to the `tools/build.py` whitelist (~183 — a flag that is not listed is silently ignored). Add `firmware/src/kid.c` + `kid.h` with `kid_on()` returning 0 and no call sites yet. Second FM-1 ordered. |
| **Exit** | `./build.sh` prints `image 571820 B`; `tests/regress.c` reports 182 renders, 0 changed; `FELUCCA_KID=1 ./build.sh` builds and behaves identically. |
| **Tests** | the existing suite, unchanged. |
| **Size** | ~0 B. |
| **Risk** | none — this stage exists to make the whitelist trap impossible to hit later. |

### S1 — The gate, both halves  (3 days)

| | |
|---|---|
| **Goal** | Level 1 is a real level: no TOOLS page, no step editor, no locked engine or kit — and no way to get stuck. |
| **Work** | `kid_gate.c` (or the top of `kid.c`): `kid_level()`, `kid_page_allowed()`, `kid_engine_allowed()`, `kid_kit_max()`, `kid_track_max()`, `kid_btn_blocked()`. Wire in **all three levels of §2.2**: pages (`page_shown()`, `page_first()`, `page_fix()`, `open_family()`, the draw row), values (`select_engine()`, `preset_go()`, the three `P_E0` writers, `track_select()`, `pc_poll()`), and **raw bits** — zero `ly_bit[]` for locked layers, block the REC hold, force `G_RMCH = 0`; the handler goes **after** `kb_grid = grid_keys_on()` (`ui_input.c:1132`), not before the HOME block — see finding 31. |
| **Exit** | The gate matrix test exists and passes for levels 1..10 with the level forced by a build-time `-DKID_LEVEL=n`. Manual: every front-panel button and every encoder, from every screen, at level 1 — nothing reaches a locked page, nothing sticks, nothing flickers, and holding SAVE/EDIT/REC/GLO/SEQ with keys down changes nothing. |
| **Tests** | `tests/kid_test.c` test 1 (gate matrix) + a new test: **every family has ≥ 1 visible page at every level**, and `page_first()` returns a visible index for each. Plus a "button table" test: for each level and each physical button, hold it 2 s with a key down and with nothing else down, and assert the *allowed* set — the layer table and the REC hold are asserted here, not by hand. |
| **Size** | ~900 B (the mask table is a few dozen bytes; the wiring is the cost). |
| **Risk** | the first place a wrong gate becomes a stuck unit. Do the matrix test before the wiring, not after. |

### S2 — The progress record  (2 days)

| | |
|---|---|
| **Goal** | Level survives a power cycle; a torn write costs one level, not the file. |
| **Work** | The 24 B block after `fcrc` (`ui.c:888`), filled in `side_fill()`, validated in `side_take()`; loaded at boot; written through `settings_save()` deferred like autosave (finding 14) — never `st_save` directly. Parent-mode reset (level 1 / level 10 / clear). |
| **Exit** | Round trip at every level; simulated torn write keeps the previous copy (the A/B pair does this already — prove it); a kid-written settings object is still accepted by a **stock** 2.8.5 (`theme` + `favourites` intact); first boot with no record = level 1. |
| **Tests** | kid test 2, and a stock-compat test that loads a kid-written side record through the stock `side_take()` logic. |
| **Size** | ~400 B. |
| **Risk** | the one thing that must not corrupt is the settings object that also holds her theme. Test against stock *before* touching the record again. |

### S3 — Clubhouse, input map, and the screen  (3 days)

| | |
|---|---|
| **Goal** | She can see where she is and the four buttons work. |
| **Work** | `FAM_KID` **appended** to `PAGES[]`; the three-band draw (§2.3); `kid_input()` inserted after `page_fix()`/`midi_entry()` and before `if (home == BT_HOLD)` (`ui_input.c:1120`); `SEQ` = hint, `EDIT` = undo, `HOME` tap = rooms, `HOME` held 3 s = parent (own timer — `btn_hold()` is fixed at 700 ms, `ui_input.c:765`); force `ui.menu = 0`, clear `hold_kind`/layer locks/`ui.home_t0` on entry (finding 13); `song.seq_mode = 0` on the kid page (finding 19); kid-mode handling of the UBOOT combo (finding 10). |
| **Work (also)** | extend `FAM_BTN[]` in the same edit as the enum (finding 27); keep every header message under 24 bytes (finding 28); redraw on state change only (finding 30); **keep the 20 px header band and draw `ui.msg` in it** so the parent's messages and the update countdown are not hidden (finding 32). |
| **Exit** | On the device: the screen is legible at arm's length, the four buttons do what the footer says, and a 5-minute button mash cannot leave the clubhouse or open a menu. |
| **Tests** | kid test: input consumed map (a table of button → expected effect, driven through `kid_input()` directly). Manual: mash test, power-on test with palms on the panel. |
| **Size** | ~900 B. |
| **Risk** | the first stage that needs the hardware to judge. If the second unit has not arrived, use the WASM preview for layout and accept that the feel is unverified. |

### S4 — Two rooms / the snapshot  (3 days)

| | |
|---|---|
| **Goal** | A mission can never damage her song. This is the stage that makes the whole design safe. |
| **Work** | §2.1 exactly: `kid_mission_active` gating `autosave_tick()`; the pre-mission flush; restore on exit; `persist_flush_now()` made mission-aware; transport stopped before every write. |
| **Exit** | **kid test 3**: build a song → enter mission 1 → play → leave → the song is byte-identical (`proj_hash`) — 30 cycles, and once with a power cut in the middle of the mission. |
| **Tests** | kid test 3, both variants, in the suite (not by hand). |
| **Size** | ~300 B. |
| **Risk** | the highest-consequence stage in the project. Do not start S5 until test 3 passes 30/30. |

### S5 — The mission engine  (2 days)

| | |
|---|---|
| **Goal** | One table drives the device and the paper. A mission can be loaded, evaluated, completed, and remembered. |
| **Work** | `docs/kid_missions.tsv` (source of truth) → `tools/gen_kid_missions.py` → `build/gen/kid_missions.h` + the printed cards. Seed expansion (`seed → proj_apply`), witnesses (the masks in the curriculum), predicates, completion, unlock, badge. REC disarmed and FREE TAKE refused while a mission is active (finding 18); **`rec_tempo` forced to TEMPO so level 1's "REC, then play" is a grid, not a free take** (finding 24); seed load does not mark undo. |
| **Exit** | All 30 seeds expand and pass `proj_ok()`; a synthetic witness sequence completes each check at its boundary and not one step before; a mission left and re-entered resumes at the same state. |
| **Tests** | kid tests 4 and 5, plus a "null mission" (complete = press SEQ) to prove the plumbing without content. |
| **Size** | ~1,500 B + content (30 × ~120 B = 3.6 KB). |
| **Risk** | content size. Write the 30 missions short from the start (2 × 28 chars); the printed card is where the words go. |

### S6 — Levels 1–3 and the reward vocabulary  (2 days)

| | |
|---|---|
| **Goal** | Six missions you can play, and the reward design is real, not described. |
| **Work** | Missions 1–9 from the curriculum; the band row lighting up; the badge/jingle; the hint ladder (demo → do it for her → skip); the safety mission that fires on the first undo. |
| **Exit** | You play levels 1–3 on the device twice, without reading a doc, and the third mission of each level feels earned. |
| **Size** | ~600 B + content. |
| **Risk** | this is where over-rewarding creeps in. The research (plan §5) says unexpected, informational praise; no streaks, no points. |

### S7 — Levels 4–7  (2 days) · S8 — Levels 8–10 and graduation  (2 days)

Same shape as S6, three levels at a time. S8 adds: the graduation itself (a card + the stock `.fwsc` in
the box + the "same machine as everybody else" message), and the level-10 check that **stock 2.8.5 boots
on top of a kid-written settings object with her theme, favourites and projects intact** — the test that
makes the whole "no fork to be stranded on" promise true.

### S9 — Size, strings, and the printed kit  (1–2 days)

| | |
|---|---|
| **Goal** | The image fits without the logo fallback, and the paper exists. |
| **Work** | The strings pass (short, plain, caps); `tools/measure_flash.sh` on a toolchain machine for the per-flag deltas; the card generator's paper output; the cheat card (§2.5). |
| **Exit** | `image` under 581,564 B **with** the bitmap splash; every on-screen line measured with `text_w()` against 240 px; 30 cards printed and matched to the table by one command. |
| **Tests** | kid test 7 (image size, printed by `build.py` at the end of every build). |
| **Risk** | the only stage where "it stopped fitting" is possible, and it has three fallbacks before anything she uses is touched (plan §8). |

### S10 — Playtest week  (5 days, the important one)

| | |
|---|---|
| **Goal** | Learn what a real 10-year-old does, before the wrap. |
| **Work** | Two sessions, 20 minutes, video the screen and her hands. Fix the top five confusions. Nothing else. |
| **Exit** | Levels 1–3 completed by her, with no adult help beyond reading a card aloud. |
| **Risk** | the schedule. Decide the fallback on **15 Dec**, not 24 Dec (plan §13). |

### S11 — Freeze, install, print, pack  (3 days) · S12 — After Christmas (ongoing)

Freeze; build `9.0-kid`; install on the gift unit; preload the demo songs; print; set the theme colours;
pack the stock `.fwsc` and the recovery card. S12 is: watch her play, keep the parent menu's graduation
ready for when she reaches it, and sync the kid branch when upstream 2.9 lands (the audio path is frozen,
so a rebase is a build + a golden run — that is the payoff of *not* cutting sample sets).

---

## 4. Gates between stages

Do not move on until these are true. They are cheap and they are all mechanical.

| gate | check | after |
|---|---|---|
| G1 | stock build prints `image 571820 B`; 182 renders, 0 changed | S0, every stage after |
| G2 | gate matrix test green for levels 1..10; every family has ≥ 1 visible page; **the button table passes** (no locked layer, no REC-hold clear, no REMOTE press) | S1 |
| G3 | side-record round trip + stock 2.8.5 accepts a kid-written record | S2 |
| G4 | 30 × "song survives a mission", 1 × "song survives a power cut" | S4 |
| G4b | enter a mission **from the DRUMS grid page** with a drum track selected: the white keys still sound | S1/S3 |
| G5 | all 30 seeds expand and pass `proj_ok()`; every predicate passes at its boundary | S5 |
| G6 | kid image ≤ 581,564 B **without** `FELUCCA_LOGO_TEXT` | S9 |
| G7 | stock 2.8.5 boots over her data with theme, favourites and projects intact | S8 |
| G8 | a 10-year-old finishes level 3 unaided | S10 |

---

## 5. Calendar (36 days, mapped)

| when | stages | the honest state at that date |
|---|---|---|
| Nov 19–20 | S0 | stock build reproducible; flag scaffolding in place |
| Nov 21–23 | S1 | level 1 is a real level; nothing can be broken from the panel |
| Nov 24–25 | S2 | progress survives a power cycle |
| Nov 26–27 | S3 | the clubhouse exists and the four buttons work |
| Nov 28–Dec 1 | S4 | **a mission cannot damage her song** — the project is now safe to build content on |
| Dec 2–3 | S5 | the engine and the table; a null mission completes |
| Dec 3–5 | S6 | levels 1–3 playable |
| Dec 6–7 | S7 | levels 4–7 |
| Dec 8–9 | S8 | levels 8–10 + graduation proven |
| Dec 9–10 | S9 | fits, prints, packs |
| Dec 10–16 | S10 | playtested twice |
| Dec 17–19 | S11 | box closed |
| Dec 20–24 | — | buffer |

If S4 slips past **3 Dec**, take the fallback (stock SLOOP + printed cards + hand-unlocking): the two
rooms are the part that cannot be faked, and a kid build without them is a kid build that can eat her
song on Christmas Day.

---

## 6. What to cut, in order, if the calendar slips

1. **Levels 8–10** (the engines, the sampler, the graduation) — levels 1–7 are a complete gift.
2. **The badge/jingle vocabulary** — informational praise in the header costs nothing.
3. **Side quests** — they are the first thing at risk in any plan and the least missed.
4. **The printed passport** — cards alone are enough.
5. **Never**: the gate (S1), the snapshot (S4), or the "song survives" test.

---

## 7. What still needs the hardware (and cannot be answered here)

1. Real pool headroom with kid code linked (`build.py:265` fails under 8 KiB — it will tell you).
2. Per-flag image deltas (`tools/measure_flash.sh`, now a confirmation, not a gate).
3. The felt duration of a 4 KiB erase while stopped (~50 ms by comment, unmeasured here).
4. Whether a 3-band clubhouse redraws fast enough to feel instant.
5. Whether `FONT_S` at 8 px is readable at arm's length on the actual panel.
6. Whether a child can reach the UBOOT combo (5 s, two adjacent buttons) in practice.
7. Whether the `SEQ`-as-hint mapping survives contact with a real child, or whether the label needs a
   sticker that says HELP.
8. The volume ceiling and the speaker's low cut, by ear, in a living room.
