# SLOOP JR — a plan for a Christmas gift

**A kid-friendly build of SLOOP for a 10-year-old pianist/double-bassist, where finishing musical
tasks unlocks more of the instrument.**

Status: planning only, no code written yet. Written 19 Nov 2026 — 36 days to Christmas.
Base: `sloop-fm1` at 2.8.5 (`bac9c05`), GPL-3.0.

**The package:** this plan is the design. `KID-MODE-CURRICULUM.md` holds the 30 missions as a build
spec, `KID-MODE-IMPLEMENTATION.md` the engineering map (seams, gate, snapshot, tests, order of work),
`KID-MODE-MEASUREMENTS.md` the numbers, and `KID-MODE-HANDOFF.md` the index. Written to be handed to
another agent without re-deriving anything.

**Decisions locked:**

| Decision | Choice | Effect on this plan |
| --- | --- | --- |
| Companion web app | **Out of scope for now** | §9 is deferred. The paper kit plus a mission-table generator become the single source of truth for mission text, so nothing is written twice. |
| Development hardware | **One unit now, second one likely** | Ordering the second FM-1 is the first action of the project, not a nice-to-have (§13, §14). Until it arrives, develop against the WASM preview. |
| How much to hide | **Mixed: strict tutorial + free-play sandbox always open** | Needs the "two rooms" model below (§6): mission seeds must never be able to overwrite her own song. This is the biggest single addition to the design. |
| Language | **English only** | One string table. Short, concrete words; the long explanations live on paper. |
| Level 10 | **Graduation to stock SLOOP 2.8.5** (20 Nov; rationale updated 20 Nov) | Originally chosen so the kid build could be smaller than stock. Measurement says it need not be — so this is now a **ceremony, not a necessity**: she keeps projects and presets and ends on a maintained upstream build with everything unlocked. |

---

## 1. What we are actually building

Three pieces, in order of how much they matter to her:

| Piece | What it is | Why it matters |
| --- | --- | --- |
| **SLOOP JR** | A fork of SLOOP that boots into a "clubhouse" screen (or a demo song, §15): one mission at a time, plain words, big text, and only the buttons that mission needs. Finishing missions unlocks the next slice of the firmware (a track, a layer, an engine, a page). | This is the gift. It turns a 153-preset groovebox into a term-long course. |
| **The curriculum** | 10 levels × 3 missions ≈ 30 tasks, each 2–6 minutes, written around what she already knows from piano and double bass (roots, chords, pulse, bowing/legato) and then what only machines can do (timbre, loops, layering). | A locked feature is only motivating if the *reason* to unlock it is musical. |
| **The paper kit** | Printed mission cards, a sticker "band passport", button-label stickers, a taped-on cheat card. | A 240×240 screen and 14 cryptic buttons cannot carry a curriculum alone, and paper has no battery, no menus, and cannot crash. |

A companion web app is **deferred** (decision, 19 Nov). What replaces it is cheap and better for the
timeline: **one mission table** in the firmware source (id, level, one-line instruction, four knob
labels, seed recipe, unlock, hint) from which a small `tools/` script generates *both* the firmware's
strings *and* the printable cards. Write a mission once; it appears on the screen and on paper, and
they can never drift apart. See §9.

The single most important design rule follows from the hardware (§3): **the device teaches by
removing choices, not by adding screens.** Every level *hides* more than it shows.

---

## 2. What makes it work for a 10-year-old

Her situation matters: she reads music, she practises, she plays in ensembles (school orchestra,
maybe a band). She is not a beginner at *music* — she is a beginner at *machines*. So the plan is
not "synth tutorial", it is "**you are the bandleader of four players**".

1. **The band metaphor.** Track 1 = keys, track 2 = bass, track 3 = chords/lead, track 4 = drummer.
   Never call them "TRK 1–3"; the screen calls them *DRUMMER*, *BASS*, *KEYS*. She already knows how
   to be a bandleader, and she already knows what each one is supposed to do.
2. **One new idea per level, one new control per level.** Ten unlocks, no more. Every other control
   either works normally or answers "NOT YET — LEVEL 5" instead of doing something confusing.
3. **Sound first, text second.** A mission is finished when it *sounds* finished. The screen shows at
   most two short lines; the long explanation is on paper.
4. **Transfer, then extend.** Start from things she can already do — a pulse, a root note, a triad,
   a smooth bow change — and then show the electronic version of it (swing, mono glide, chord mode,
   envelope attack). Level 8 is deliberately her own instrument: physical-modelled strings.
5. **Never a fail state.** No timers, no scores, no losing. Missions can be left half-done, undone,
   or done "wrong" — there is always UNDO, and nothing destructive happens without a confirmation
   (the firmware already does this).
6. **Reward = more music, not more tokens.** Expected prizes for things children already enjoy reliably
   reduce their motivation, and the effect is strongest at her age; surprise and mastery do not. So the
   unlock is a door she walks through, the band growing is the progress display, and there is never a
   streak. See §5.
7. **Ask for creative work honestly.** Some missions are machine-verifiable ("kick on beats 1 and 3");
   some cannot be ("write a melody you like"). Those use a *show-and-tell* key: she decides it is good,
   presses it, and the machine celebrates. Pretending to grade creativity is worse than admitting it.
8. **She can always escape.** A hidden combo unlocks everything in 3 seconds (yours to know). The
   instrument must never become a locked door she resents.
9. **Hands.** Detented knobs are good for kids (no accidental values). Use one knob per idea, big
   value readouts (ZOOM ON exists), and the 27 keys as a piano — she already has piano fingers.
10. **Session shape.** Christmas morning gets levels 1–3 (playable immediately). The rest is a term of
    Sunday afternoons. Design for 20-minute sessions, not for one big finish.

---

## 3. What the hardware and firmware actually allow (verified in this tree)

This is the part that constrains everything, so it is worth being precise.

| Constraint | Reality in this repo | Consequence for the plan |
| --- | --- | --- |
| Screen | 240×240 ST7789, RGB565, **no framebuffer** — draws into a 240×124 strip (`gfx.c`) | No scrolling prose, no icons-with-captions. Two short lines max. Use `FONT_L` (16×32) for the mission line. |
| Controls | 14 buttons (FX, SEL, ENV, LFO, EDIT, GLO, HOME, SAVE, ARP, SEQ, PLAY, REC, OCT−, OCT+), 7 encoders (SELECT, ALGORITHM, PRESETS, 4 knobs), 27 note keys (`panel.c`, `fm1_input.h`) | Everything advanced is a *hold-a-button* layer. That is the single biggest accessibility problem for a child, so kid mode must replace layers with visibility. |
| Sound | 3 synth tracks + drums, 8 voices, 12 engines, 153 presets, 37 kits, 64 steps, 4 sections | Plenty of reward material to unlock — that is the upside. |
| **Flash** | App slot is **581,564 B** (`tools/fm1pkg_make.py`). History: the USB console was cut at 2.7 "because its 6–8 KB: the flash is full" (`felucca.c`), and `build.py` ~341 still has an automatic text-logo fallback on link overflow — so the codebase *expects* to be tight. | **Measured, and not the constraint it looked like.** The shipped 2.8.5 image uses **571,820 B of 581,564 → 9,744 B free**, and it ships *with* the bitmap splash (so the text-logo trick is still unspent: 14.4 KB available in total). Kid mode was budgeted at 8–12 KB. See `docs/KID-MODE-MEASUREMENTS.md` §2b. |
| RAM | 96 KiB region, 58–74 KB already used depending on branch; build fails if the pool headroom drops under 8 KiB (`build.py`) | Kid state and screens must be small and reused; no second framebuffer. |
| Data flash | A/B sector pairs (`storage.c`). Free space is essentially **one 4 KiB sector** (0xFB000); USR4 samples take 80 KB. Settings copy has a **192-byte "side record"** (`ST_SIDE_MAX`) already used for theme + favourites. | Progress record goes in the existing side record — no new flash map, no risk to her samples/presets. |
| Audio timing | Everything UI-side must stay out of the audio ISR; the main loop has a 5.8 ms budget per half-buffer | All kid logic lives in the main loop. Progress writes defer to "quiet", like autosave already does (`project.c autosave_tick`). |
| Quality gates | Golden renders (every engine × preset hashed), CPU budget, 60+ host tests, WASM preview parity | Kid mode must change **zero** audio. Any fork that trips `tests/golden.txt` is wrong. |
| Flash chip variants | 2.8.5 fixed units with the newer UC25HQ80 flash (mostly late-2026 AliExpress stock) | **Build on ≥ 2.8.5**, or her unit may silently refuse to save. |
| Install / recovery | Browser installer (Chrome/Edge, Web MIDI) or `tools/fm1_install.py`; USB RESCUE = hold OCT− at power-on; brick recovery = FM-1-transporter | Keep `FELUCCA_OTA` in the kid build. You need a way to push fixes after Christmas. |

**Implication in one line:** the firmware is full, the screen is tiny, and the input model is
expert-oriented. The plan therefore puts *content* on paper/web, keeps *state* in 192 bytes, and
spends its scarce kilobytes on **gating + one good screen**, not on a lesson engine.

---

## 4. The curriculum

Unlocks are ordered so each one is immediately useful for the loop she just made, and so the first
three happen on Christmas morning.

| # | Level | What she does (mission seeds) | Unlocks | Musical idea | Ties to her instruments |
| --- | --- | --- | --- | --- | --- |
| 1 | **THE DRUMMER** | White keys = 16 drum sounds. Record a 1-bar beat with kicks on 1 and 3. Press PLAY. | Drums track, REC/PLAY, tempo, 3 kits | pulse, backbeat, bars | counting rests in orchestra |
| 2 | **THE GROOVE** | Add snare on 2 and 4, hats in between, one ghost hit (hold OCT−), swing it. | SWING, ratchet/roll, 6 more kits | subdivision, accents, feel | bowing accents, off-beats |
| 3 | **THE BASS PLAYER** | Track 2: one octave, root notes only, mono, one slide. | **track 2** + bass presets/engines | root motion, register, legato | her actual instrument, one octave down |
| 4 | **THE CHORD PLAYER** | Track 3: chord mode (TRIAD, then 7TH), key locked to the bass root. | **track 3**, CHORD, key/scale | triads → 7ths, voicing | piano chords she already plays |
| 5 | **THE SOUND SHAPER** | One knob: filter cutoff (open/close), then resonance, then attack (pluck ↔ pad). | EDIT + ENV layers, all tracks | timbre, brightness, envelope shape | pizzicato vs arco, mute vs open |
| 6 | **THE SPACE MAKER** | Delay send, reverb send, then one punch-in effect while playing. | FX layer, punch-ins | space, echo, tension/release | hall vs practice room |
| 7 | **THE MELODY** | Lead part over her chords; scale-locked; then arpeggiate a chord. | ARP layer, lead presets | melody over harmony, sequence | scales she already practises |
| 8 | **THE STRINGS** | PHYS engine: plucked string, bowed string (CELLO BOW), then sympathetic strings (SITAR/TANPURA). | PHYS + GRAIN + NOISE engines | physical modelling, resonance | **direct bridge: a double bass in a box** |
| 9 | **THE COMPOSER** | Sections A–D, SONG order, save it, give it a name. Make a 2-minute piece: intro / A / B / A. | SEQ + SONG pages, saving | form, arrangement | playing a piece start to finish |
| 10 | **THE INVENTOR** | FM6 (DX7-style) sound design; sample her own voice or her double bass into USR1 and play it on the keys. | FM6, SAMPLE, everything — then **graduation to stock SLOOP 2.8.5** (decided 20 Nov; see §8) | synthesis families | her instrument as raw material |

**Mission structure (3 per level):** `COPY` (match the example) → `CHANGE` (one thing, hear the
difference) → `MAKE` (a tiny piece of her own). Roughly 3–5 hours of structured play across the term,
which is the right dose: long enough to matter, short enough to finish.

**Side quests** (never gate anything, always available): "make it rain" (NOISE), "make a 3-note
melody", "the slowest beat you can dance to", "make the bass sound angry", "record 4 seconds of your
voice". These are the ones she will show her friends.

### How a mission is verified

Mixed model, by design:

| Kind | Example | Detection | Cost |
| --- | --- | --- | --- |
| **Do** | "play every white key once", "turn the filter all the way down and back up" | counters/bitmasks of keys and parameters touched while transport runs | trivial |
| **Make** | "kicks on 1 and 3, snare on 2 and 4" | predicate over the drum step lanes (they are already in RAM) | trivial |
| **Change** | "close the filter while the loop plays" | "parameter moved during playback" witness | trivial |
| **Keep** | "let 8 bars play with 3 parts unmuted" | bar counter + mute state | trivial |
| **Show and tell** | "make a melody you like" | she presses SEQ on the mission card | trivial, but honest |

Everything above reads state that already exists. **No audio analysis, no pitch detection, no
recording buffer** — those would be expensive in code, RAM, and CPU headroom, and they would be the
first things to glitch her audio.

---

## 5. The reward system (revised after the research)

The first draft of this section was a badge per level. The evidence says that is the risky pattern:
**expected, contingent, tangible rewards for an activity a child already enjoys reliably reduce her
intrinsic motivation**, the effect is strongest between roughly 5 and 12, and streaks are the worst
offender; *unexpected* rewards behave much more like praise than like payment
[1](https://hiwavemakers.com/blog/gamification-learning-apps-backfire-kids-research/). The three needs
that sustain motivation are autonomy, competence and relatedness
[5](https://thelearningrooms.com/beyond-badges-what-modern-gamification-does-to-the-brain/), which maps
straight onto: a sandbox she controls (autonomy), a band she builds (competence), and playing it to you
(relatedness). So:

| Element | What it is | Why it survives the research |
| --- | --- | --- |
| **The band grows** | four tiles — DRUMS, BASS, KEYS, LEAD — lighting up as levels finish | a true statement about work she did, not a token; informational, not controlling |
| **`3 / 10`** | levels finished | progress indicators help at 10+ *when they mean something* |
| **Surprise sound** | each level ends with one unnamed preset ("a sound from 1983") | unexpected > expected; this is the only "prize" in the design |
| **Sticker passport** (paper) | a page per level, sticker when she plays it to you | relatedness — the reward is the audience, not the sticker |
| **A jingle** | 4 notes on completion, the only new audio in the project | tiny, and it must be routed so `tests/golden.txt` does not move |
| **Nothing lost, ever** | no streaks, no daily counters, no scores, no leaderboards, nothing that can be downgraded | removes the entire class of "punished for missing a day" |

And in the wording on the device: never "well done". Say what happened — **"SNARE ON 2 AND 4 — THAT'S
A BACKBEAT"**. Informational, specific, true.

Full detail, including the per-mission reward moments, in `KID-MODE-CURRICULUM.md` §6.

---

## 6. The on-device experience

### The clubhouse screen

```
+------------------------------------------+
|  LEVEL 3        THE BASS PLAYER    3/10  |   <- big, always the same place
|                                          |
|  Play the lowest white key               |   <- one instruction, 2 lines max
|  four times.                             |
|                                          |
|  [ DRUMMER ] [ BASS ] [ KEYS ] [ DRUMS ] |   <- the band, only unlocked parts lit
|                                          |
|  TONE     LEVEL    SPACE    ---          |   <- 4 knobs, plain-word labels
|  SEQ = hint    HOME = back               |
+------------------------------------------+
```

Rules that make it kind:

- **Four active buttons on a card, and only four:** PLAY, REC, `SEQ` (hint / next), `HOME` (back, and
  switch between the MISSION room and the SANDBOX room), plus `EDIT` = undo while a card is up. On the
  *instrument* screens almost nothing is intercepted — locked things are simply absent, and HOME is the
  only hijacked key, because it is her exit. Every refusal reads as one line — "NOT YET · LEVEL 6" —
  and never silently does something unexpected.
- **`OCT−`/`OCT+` keep their musical meaning** (they are soft/hard drum hits and the arp octave; the
  level-2 mission teaches them). Stealing them for navigation would teach a gesture that stops working
  at graduation. The hint therefore lives on `SEQ`, which only opens the sequencer family — locked
  until level 9, and never needed from a card.
- **Contextual knob names.** KNOB 1–4 are relabelled per mission ("TONE", "WARMTH", "SPACE"). That is a
  4-string table per mission, a few dozen bytes each, and it removes the worst part of the machine:
  values called `P_ED_FLT` and `RESO` shown as `-64..63`.
- **Hint always available.** `SEQ` replays the mission's demo (a 1-bar seed) or shows the next line.
  There is no penalty for needing it, which is the whole point.
- **The band row is the map.** As levels finish, DAZZ: DRUMMER lights, then BASS, then KEYS. By level 9
  she sees the four-piece band she built.
- **ZOOM ON by default** (large value readout), **SPEAKER LOWCUT ON**, and a **volume ceiling** (see
  §11) — a 240 px screen is not where you want a 10-year-old's ears surprised.

### Two rooms: MISSION and SANDBOX

Because the sandbox is always open, the machine needs **two places to be**, and the second one only
exists to protect her work:

| | MISSION room | SANDBOX room |
| --- | --- | --- |
| What is loaded | a seed recipe expanded into the working project | **her own song** — the live project, autosaved |
| What buttons do | 4 active, everything else refused | PLAY, REC, and everything she has unlocked so far |
| Mission checks | armed | off; nothing to complete, nothing to fail |
| Saved to | nothing permanent — a mission is disposable by design | autosave, plus a **reserved slot** (see below) |
| Why it exists | one idea at a time, no wrong answers | "I want to just make something" — the reason she keeps playing |

The protection rule: **entering a mission takes a snapshot first.** If her sandbox project differs
from the last snapshot, it is written to a reserved section slot (slot D, which is not yet part of
song mode before level 9) and the clubhouse shows "YOUR SONG IS SAFE" for a moment. Leaving the
mission room restores it automatically. So the worst case is one button press to get her music back,
and it cannot be lost by a mission load, a seed, or a crash.

This reservation dissolves at level 9, when sections A–D become the real feature and she is old enough
(mechanically) to understand saving deliberately. Everything above works with the existing project
slots and the autosave object — no new flash map, no new format.

Why this matters more than it looks: "aggressive gating + a free sandbox" is the best combination for
a child *only if* the sandbox is genuinely safe. A kid who loses a song once stops trusting the
machine, and that ends the project faster than any locked button would.

### Entering and leaving

- Boots into the clubhouse by default (`main.c` boot branch).
- **Parent mode:** hold HOME + SAVE for 3 s. Four items: UNLOCK ALL, RESET PROGRESS, VOLUME LIMIT,
  BACKUP. Not a PIN — she will find it, and it does not matter: the point is guidance, not security.
- Progress is stored in the settings' existing **192-byte side record** (a few bytes: 30 missions ×
  2 bits, a level bitmask, a version byte, a CRC), saved only when the transport is quiet, exactly like
  autosave does today. It travels with backups and survives updates.

### What "locked" means, precisely

Locked = **not reachable from the UI**. Nothing is deleted, no data format changes, and unlocking
never rewrites a project. Concretely there is one gate function and one place per surface:

| Surface | Seam in this tree | Effect when locked |
| --- | --- | --- |
| Parameter pages (the big one) | `params.c page_shown()` — already the single choke point for which pages `SELECT`/buttons can reach | page skipped; button answers "NOT YET · LEVEL n" |
| Layers (hold FX/SEL/EDIT/GLO…) | `ui_input.c open_family()` / `layer_*` | held button does nothing + hint line |
| Tracks | `ALGORITHM` handling in `ui_input.c` | track 2/3 unreachable until unlocked |
| Presets, engines, kits | the browse list built in `params.c`/`ui.c` (`base_at`, `base_kind`) | only unlocked engines/presets listed — this is what makes the machine feel small |
| Saving / sections | `FAM_SAVE` pages | slot D is reserved as "her song" (auto-snapshot, one button to restore) until level 9, when sections A–D become the real feature |
| Menu | `ui_menu.c` | only COLOR / LIGHTS / ZOOM |

Because it is all UI-level, a bug can never lose her music: worst case she sees a page she shouldn't.

---

## 7. Architecture (what gets built, no code yet)

### Build shape

- A fork with a build flag, `FELUCCA_KID=1`, compiled by `tools/build.py` like the existing flags
  (`FELUCCA_CDC`, `FELUCCA_UAC`, `FELUCCA_ARRANGER`, `FELUCCA_LOGO_TEXT`). Package identity
  `FM-1_9xx`, version string `SLOOP JR 1.0`, so it can never be confused with stock SLOOP in an
  installer list.
- **Zero changes to the audio path.** No file under `eng_*`, `voice.c`, `fx.c`, `drums.c`, `seq.c`
  except read-only hooks. This is what keeps `tests/golden.txt` green and keeps you able to rebase on
  upstream releases (which land often — 2.8.5 is days old).

### New files (small, self-contained)

| File | Job | Est. size |
| --- | --- | --- |
| `firmware/src/kid.h` | feature enum (10 unlocks), progress record layout, the one gate API | ~1 KB of code, no data |
| `firmware/src/kid_state.c` | progress load/save in the side record, level completion, badges, parent-menu actions | ~1.5 KB |
| `firmware/src/kid_ui.c` | clubhouse screen, mission card, band row, unlock animation, hint lines | ~3–5 KB |
| `firmware/src/kid_missions.c` | mission table, witnesses (counters/bitmasks), predicates, seed loader | ~2–4 KB |
| `firmware/src/kid_content.h` | generated: mission strings + tiny seed recipes | ~1.5–2.5 KB of const data |
| `tests/kid_test.c` | host test: gate matrix, mission predicates, progress round-trip, torn write | (host only) |

### Hooks into existing files (target: under ~150 changed lines total)

- `felucca.c` — include the kid files under the flag.
- `main.c` — boot into clubhouse; volume ceiling; parent-mode combo; call `kid_tick()` in the loop.
- `ui_input.c` — consult the gate before opening a family/layer/track; route the 4 active buttons;
  emit witnesses (param touched, key pressed, transport, save).
- `params.c` — `page_shown()` consults the gate; filter the preset/engine/kits list.
- `ui_draw.c` — one early branch: if kid mode, draw the clubhouse instead of the page.
- `project.c` — persist the kid record in the side record next to theme/favourites.
- `tools/build.py`, `web/make_site.py` — build variant + a separate install page.

### The witness model (the clever bit, and it is cheap)

One small struct in RAM: "keys played" (128-bit pitch mask), "parameters touched" (32-bit mask over
the page ids she has moved), "steps written" counter, "bars played" counter, "last save", "layers
opened". Updated from existing UI events, cleared per mission. Every mission check is then a few
comparisons over that struct plus, for pattern missions, a read of the step lanes that are already in
RAM. This is why 30 missions fit in a couple of kilobytes rather than a scripting engine.

### Seeds without storing projects

A "seed" is not a saved project (each one would cost two 4 KiB flash sectors). It is a recipe —
8–24 bytes: which parts are on, a 16-step bitmask per drum lane, a root note, a scale, a preset index,
a tempo — expanded into the working project in RAM when the mission starts. 30 seeds ≈ under 1 KB of
const data, and she can mangle any of them without touching her own saved song.

---

## 8. The size budget (measured — and the answer is comfortable)

**The stock 2.8.5 image uses 571,820 B of the 581,564 B slot: 9,744 B free.** It ships with the bitmap
boot splash, so the text-logo fallback (4,635 B) is still in reserve. **Available: 14.4 KB. Needed:
8–12 KB.** `docs/KID-MODE-MEASUREMENTS.md` §2b has the method (the package decrypts under the constant
key in `fm1pkg_make.py`) and the validation.

So the order is simply:

1. **Build kid mode lean, target ≤ 8 KB, and see if it fits.** Most of the estimate is text, and text is
   the easiest thing to trim.
2. If it overflows: accept the text logo (+4,635 B, one flag, automatic).
3. If it still overflows: cut the editor's three remote-control/patch pages (~3–4 KB).
4. If it still overflows: trim mission strings. That is now the last resort, not the plan.

What this **retires**:

- ~~Cutting sample sets.~~ Measured properly, FLUTE+SCRCH is worth 34.4 KB — the plan's main lever.
  It is not needed, and it has real
  costs: cutting sets renumbers the SAMPLE presets, which breaks user presets she saves later and
  forces a fork-only golden baseline. **Leave the sample bank alone.**
- ~~`ui_studio.c`.~~ It is the device's TRACKS/DRUMS/LAYERS screens, not the editor's LIVE page.
- ~~The arranger.~~ 0.3 % of code, and not flag-able without edits to `ui_song.c`.

**What happens to the graduation (decided 20 Nov, rationale now changed):** it was chosen because the
kid build would have to be smaller than stock. It does not. The graduation is therefore **optional** —
still a nice ending (she moves to a maintained stock build with everything unlocked, keeping her
projects and presets), no longer a necessity. Keeping it costs nothing and gives the course a proper
finish; dropping it costs nothing either. Recommendation: keep it as ceremony.

## 9. Paper, not pixels: the kit and the mission generator

The companion app is out of scope (decision, 19 Nov), so the long-form teaching has to live
somewhere. It lives on paper — and the paper is generated from the same mission table the firmware
uses, so the two can never disagree.

**One mission table, two outputs.** Each mission in `kid_content.h` carries: id, level, a screen line
(≤ 2 × 28 characters), four knob labels, a seed recipe, the unlock it completes, a hint line, and a
"what this teaches" paragraph. A `tools/` script renders that table into A6 cards (one per mission,
four per A4 sheet) plus the passport. If a mission's wording changes in code, the cards change the
moment you reprint. This is the whole benefit the web app would have given, without a second device,
a second codebase, or a version-skew bug.

**The physical kit (build in week 1, it cannot fail):**

| Item | Content |
| --- | --- |
| 30 mission cards | level colour, 2-line instruction, "what to listen for", a tick box |
| Band passport (A5, folded) | one page per level: sticker space, "what I learned", "what I made" |
| Button-label stickers | printed names over the cryptic ones: SEL → SOUND, GLO → SETUP, SEQ → HELP, ALGORITHM → WHICH PLAYER (leave OCT−/OCT+ labelled: they *are* octave/soft-hard, and she will use them) |
| Cheat card (taped under the unit) | the four buttons (`PLAY`, `REC`, `SEQ`, `HOME`) plus `EDIT`, the grown-up combo, the rescue combo |
| A note from you | why you made it, what you love about her playing |

**Still true from the firmware side:** `build/preview/sloop-preview.wasm` exists and lets you click
through the clubhouse screens in a browser without flashing — that stays useful for layout work and as
a demo for you, and it is not part of her gift.

**Deferred, not abandoned (v2):** an installable page with the level map, long mission text, a demo
player, and progress sync over a SysEx command pair. If she takes to it, that is the natural next
project in the new year — and by then the mission table already exists to feed it.

---

## 10. Build, flash, test

**Environment (do this before anything else, it is the usual bottleneck):**
JieLi pi32v2 toolchain (`tools/get_toolchain.sh`) + the AC79NN SDK (`gitee`, 1.2.1) + Python/Pillow,
and Docker if you are not on Linux x86-64. Then `./build.sh` → `build/felucca.fwsc`.

**Hardware:** order the second FM-1 **today**. One unit means every flash risks the gift, and shipping
from AliExpress late in November is the kind of thing that quietly eats two weeks. Until it arrives,
do layout and flow work in the WASM preview and keep the single unit for save/audio/recovery checks
only. If the second unit has not arrived by 10 Dec, switch to developing on the gift unit and accept
that it gets flashed repeatedly (install stock 2.8.5 again and reflash before you wrap it).

**Test gates before every flash:**
1. `tests/run_tests.sh` — **golden renders must be unchanged**. If `tests/golden.txt` moves, something
   touched the audio path: stop and fix it.
2. `tests/kid_test.c` (new) — the gate matrix: for each level, assert exactly which pages/layers/
   tracks/presets are reachable; assert progress survives a simulated torn write; assert a seed
   expands into a project that passes the existing project checksum; and the sandbox round trip
   (song → mission → song, byte-identical).
3. `python3 tools/build_preview.py` — the WASM preview lets you click through the clubhouse screens in
   a browser, which is far faster than flashing for layout work.
4. On device: a scripted walkthrough of all 10 levels (there is precedent: `tests/knob_walk_test.c`,
   `tests/hostsim.c`), plus a real 20-minute session with a child if you can borrow one.

**Release hygiene:** build `--release 9.0-kid`, keep the `.fwsc`, and keep `docs/firmware/sloop-2.8.5.fwsc`
plus M-UPGRADE's stock `FM-1.fwsc` (V16) on the same laptop you will use on Christmas morning.

---

## 11. Safety, ears, and the boring things that ruin gifts

- **Volume ceiling.** MASTER is an analog pot read by the ADC (`main.c` → `song.master_q12`). Kid mode
  can scale that down to ~70 % so headphones cannot be turned up to full by accident; the parent menu
  can lift it. Verify it does not touch the golden renders (it applies after the mix, so it should not).
- **Speaker lowcut ON** and **LIGHTS at MID** by default (she will use it in the car).
- **Nothing destructive without a confirmation** — the firmware already arms ERASE/NEW PROJECT; keep the
  confirmations in kid mode and add one for "reset my song".
- **Backup ritual:** you take a full editor backup after each session (that is also how you save her
  best pieces to keep). Put a printed card in the box that says exactly this.
- **Battery + cable:** check the charge, include a decent USB-C data cable (a charge-only cable is a
  20-minute Christmas-morning debugging session).
- **Beta software.** SLOOP is labelled beta upstream. Write the recovery steps on a card that lives in
  the box: hold OCT− at power-on = USB RESCUE; install again; last resort FM-1-transporter.

---

## 12. Licensing and the honest small print

- SLOOP is **GPL-3.0**. If you give her the device with modified firmware, you are conveying GPL code:
  include the source (a USB stick, or a URL + a printed note) and keep the copyright notices and the
  ABOUT credits. Do not strip the attribution — Leo Kuroshita and isod89 built the thing this is
  standing on.
- Assets are separately licensed (`LICENSING.md`): CC0 samples, Terminus font, FUKIAI font, Apache-2.0
  MSFA. Do not add a font or sample without checking its licence — a gift can be shared with a photo on
  the internet, which is distribution.
- If you later publish SLOOP JR, use a new package identity and a clear "not official SLOOP" line, so
  nobody flashes it expecting upstream behaviour.
- M-VAVE's warranty does not cover this. That is fine — it is recoverable — but it is worth knowing
  before you flash the present.

---

## 13. Timeline to Christmas (36 days)

| When | What | Done when |
| --- | --- | --- |
| **Nov 19–25** | **Order the second FM-1 (first action).** Toolchain + SDK; build and install stock 2.8.5 on a dev unit; run the full test suite. The size question is answered offline (571,820 / 581,564 B — `docs/KID-MODE-MEASUREMENTS.md` §2b), so `tools/measure_flash.sh` is now a *confirmation* when the toolchain is up, not a gate. Write all 30 missions here, on paper, before any UI code — and build the mission table + card generator. | Stock build prints `image 571820 B`; `build/measurements.md` has a row per variant; the curriculum is frozen and printable. |
| **Nov 26–Dec 2** | Kid shell: gate function, progress record, clubhouse screen, parent menu, 4 active buttons; levels 1–2 (6 missions). | You can play levels 1–2 on the device without touching a documentation file. |
| **Dec 3–9** | Levels 3–10, seeds, witnesses/predicates, badges, jingle, hint lines; image-size check (expect it to fit — 9.7 KB spare); strings pass (short, plain, caps). | All 30 missions reachable and completable. |
| **Dec 10–16** | **The playtest week.** A real 10-year-old, twice, 20 minutes each, video the screen. Fix what she ignores, misreads or finds boring. Accessibility pass; safety pass; backup/restore. | Two playtests done, top five confusions fixed. |
| **Dec 17–19** | Freeze. Build `9.0-kid`, install on the gift unit, preload 2 demo songs (one yours), set colours/lights, print cards + passport + stickers, pack. | Box closed. |
| **Dec 20–24** | Buffer, and the fallback if something broke. | — |

**Fallback that still makes a great gift:** stock SLOOP 2.8.5 + the printed mission cards + the sticker
passport, with you unlocking features by hand over the first weeks ("today you get the bass player").
That delivers 80 % of the experience with 0 % of the firmware risk. Decide this on **Dec 15**, not on
Dec 24.

---

## 14. Risks

| Risk | Likelihood | Mitigation |
| --- | --- | --- |
| Kid mode does not fit in flash | **Retired** | Measured: 9,744 B free, 14.4 KB with the text logo, against an 8–12 KB estimate. Three fallbacks before anything she uses is touched |
| The fork drifts from upstream | **Low** | No sample sets cut, so the preset bank is identical to stock and the same `tests/golden.txt` applies. Kid files are separate and the audio path is frozen |
| She finds the layer/hold model confusing anyway | Medium | Four buttons plus `EDIT` in kid mode; playtest in week 4; paper cards as backup |
| Playtest impossible (no child available) | Medium | Sit with the rules for 10 minutes as if you were 10; watch where *you* hesitate; ask a friend's kid for 20 minutes — this is worth a phone call |
| Gift unit has the newer flash chip | Low (fixed in 2.8.5) | Build on ≥ 2.8.5 and test SAVE on the actual unit |
| Second FM-1 arrives late, or not at all | High (single unit today) | Order in week 1; preview for layout until it lands; hard fallback on 10 Dec = develop on the gift unit and reflash before wrapping |
| A mission load wipes her sandbox song | Medium without care | The two-rooms snapshot rule (§6); a host test that forces the sequence "make song → enter mission → leave mission" and asserts the song comes back byte-identical |
| Upstream releases during December | High | Keep the audio path untouched, kid files separate, rebase only if a fix matters |
| You run out of evenings | High | The fallback in §13 is real, and it is still a good present |
| She outgrows it | Certain, and fine | Level 10 hands her the whole instrument; the parent combo hands her stock SLOOP whenever she wants |

---

## 15. Decisions taken, and what is still open

Settled: no companion app for now; mixed gating with an always-open sandbox; English only; level 10 as
graduation to stock SLOOP; one unit today with a second on order. The full list with dates is in
`KID-MODE-HANDOFF.md` §2.

Still open, all small and none blocking the first week of work:

1. **First boot: clubhouse or demo song?** (Recommendation: the demo song — the first interaction should
   be hearing something, with the clubhouse one press away.)
2. **Is one of the preloaded songs yours, with a card saying so?** (Recommended yes. It costs nothing
   and it is the thing she will remember.)
3. **Her name on the boot splash?** (Cheap and private-build only.)
4. **Stickers or printed badges for the passport?**

---

## Appendix: the one-page summary for the box

- Ten levels, one new control each. The band grows: drummer → bass → keys → everything.
- A sticker for each level, awarded when she plays it to you.
- **HOME** switches between the mission and a free-play room where she can just make things; her own
  song is snapshotted before any mission loads and comes back when she returns.
- Nothing can be broken: UNDO always works, nothing deletes without asking.
- Hold **HOME + SAVE** for 3 s for the grown-up menu (unlock everything, reset, volume).
- If it will not start: hold **OCT−** while switching on, then install again from the laptop.
- Keep the laptop and the two `.fwsc` files (SLOOP JR and stock) in the box.
