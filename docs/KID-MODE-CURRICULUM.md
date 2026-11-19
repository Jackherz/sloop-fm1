# SLOOP JR — the curriculum

30 missions in 10 levels, written as a build spec: what she sees, what she does, what the machine
checks, what unlocks, and what the printed card says. This is the content half of the handoff; the
engineering half is `KID-MODE-IMPLEMENTATION.md`.

Written for a 10-year-old who reads well, plays piano and double bass, and has never used a groovebox.

---

## 1. Why it is sequenced this way

Three rules, each with a reason:

**Sound before symbol.** Music-teaching practice for beginners converges on experience first: hear it,
copy it, name it, then create with it, and only then read or write it down. Kodály-influenced teaching
puts the same order plainly — "do it, recognise it, learn the symbol for it, then meet it in another
context" — and warns that starting from notation disconnects the symbol from the sound
[3](https://dacapomusicfoundation.wordpress.com/wp-content/uploads/2011/11/sound-before-symbol2.pdf).
So every level is: **hear the seed → copy it → change one thing → make your own**, and the *name* of
the thing (FILTER, ATTACK) appears only after she has used it twice. Nothing in the firmware is taught
by reading a manual.

**One axis at a time.** The same teaching tradition keeps technique a few lessons ahead of theory, and
theory ahead of repertoire, rather than mixing them
[2](https://www.reddit.com/r/MusicEd/comments/1i5np11/sound_before_symbol_yay_or_nay/). Practically: a
mission changes **one** control, and the previous level's controls stay available so she can still be
expressive while learning.

**Map the machine onto things she already owns.** A double bass *is* a monophonic low instrument with
expressive slides; the piano *is* chords and melody. Levels 3, 4 and 8 deliberately borrow her existing
physical knowledge so the new tool has somewhere to land. Where a teacher of 10-year-olds was asked for
advice on synthesis, the consensus was: get them making something they can show, keep instructions
short, fill everything with sound, and let them twiddle knobs early
[4](https://gearspace.com/board/electronic-music-instruments-and-electronic-music-production/799636-10-year-old-synthesis-student-any-advice.html).

**What the research says about the rewards** (this changed the reward design — see §6):
expected, contingent, tangible rewards for activities children already enjoy reliably *reduce* their
intrinsic motivation, and the effect is strongest between roughly 5 and 12 years old; the same review
notes that *unexpected* rewards carry far less risk, and that progress indicators help when they
represent real mastery and backfire when they are arbitrary
[1](https://hiwavemakers.com/blog/gamification-learning-apps-backfire-kids-research/). Streaks are the
worst offender and are banned from this design outright. So: the *band growing* is the progress display
(informational, true), the unlock is the surprise (unexpected), and the sticker belongs to the ritual of
playing it for you (relatedness, not payment).

---

## 2. Mission anatomy

Every mission is one row in `docs/kid_missions.tsv` (the source of truth: `tools/gen_kid_missions.py`
turns it into the firmware's table and the printed cards):

| field | what it is | constraint |
| --- | --- | --- |
| `id` | 0–29, order of play | 1 byte |
| `level` | 1–10 | 1 byte |
| `title` | the FONT_L heading | **≤ 14 characters** (16 px advance × 14 = 224 px) |
| `line[2]` | the instruction at FONT_S | **≤ 28 characters each** (~8 px advance) |
| `knob[4]` | plain-word knob labels | ≤ 6 characters each |
| `seed` | what is loaded when the mission starts (§3) | ≤ 24 bytes |
| `check` | the predicate that completes it (§4) | a small struct, not a script |
| `unlock` | feature bit(s) granted | 1 byte |
| `hint` | the second hint, after the first ("SEQ") | ≤ 28 characters |
| `card` | long copy, printed only | ≤ 220 characters |

Text totals: 30 × (14 + 2×28 + 4×6 + 28 + 220) ≈ 30 × 350 = **10.5 KB of prose** — which is exactly
why the long `card` field never enters the firmware. Only `title`/`line`/`knob`/`hint` (~30 × 100 =
**3 KB**) do.

**Those 3 KB are an editorial choice, not a constraint.** For calibration: all the firmware's string
literals come to 17,022 B (3 % of the image), the strings sit at the tail of the image around offset
561 K, and there are 9,744 B of flash free. So the real limit on mission text is **what fits on a
240 px screen and what a ten-year-old will read** — two 28-character lines — not bytes. Spend the
savings on clarity, not on more words: the temptation is to explain in `line[2]`, and the answer is
always the printed card.

### The buttons in every mission (revised: OCT stays musical)

`PLAY`, `REC`, **`SEQ` = HINT** (and "next" on the card), `HOME` = back / clubhouse / sandbox, and
**`EDIT` = UNDO** (one button). `OCT−`/`OCT+` are **not** intercepted: they are the drum track's soft and
hard hits (`ui_input.c` — held on the drum track) and the arp octave, i.e. *playing the instrument*, and
the level-2 mission "hold OCT− for soft" teaches exactly that. Taking them for UI would teach her a
gesture that stops working the day she graduates.

So the only intercepted buttons are `SEQ` (hint, on a kid screen only — in the sandbox it still opens the
sequencer), `HOME` (always: rooms and back), and `EDIT` (undo, on a kid screen only). Everything else
responds as stock, except the features that are still locked, which answer `NOT YET · LEVEL n`.

---

## 3. Seed notation

A seed is *not* a stored project (that would cost 8 KiB of flash each). It is a recipe expanded into
the working project at load time. Written here as a compact line so it can be transcribed directly:

```
L1 A  bpm=92 kit=808  D[kick=x...x...x...x...]                       parts=D
L3 A  bpm=92 kit=808  D[kick=x...x...x...x... snare=....x.......x...]
                     B[up bass C2 x=x.......x....... root C]          parts=D,B holes=B
```

- `parts=` which tracks sound (`D` drums, `B` bass, `K` keys, `L` lead); the rest are muted by the seed.
- `[...]` a 16-step lane, `x` a hit, `.` a rest, `X` a hard hit, `o` a ghost.
- `holes=` a part the seed leaves empty for her to fill (the mission's job).
- Everything not named takes its default (root C, scale minor pentatonic, quant to the grid).

---

## 4. Check notation

| token | means | how it is evaluated |
| --- | --- | --- |
| `keys⊇{…}` | she played these piano keys | 128-bit mask, union since the mission started |
| `lane(S)⊇{…}` | these step numbers of drum sound S are on | read the drum lane's `dstep_t` bits, 1-based steps |
| `wrote≥n` | she wrote n steps (any track) | counter on the record path |
| `bars≥n` | n bars ran with ≥2 parts unmuted | `trk[].pass` / transport counter |
| `param(P)⇄` | she moved parameter P while playing | 32-bit "parameters touched" mask, set only while `song.playing` |
| `params≥n` | n different parameters touched | popcount of the same mask |
| `preset≠` | she changed the sound at least once | engine/preset change counter |
| `layers⊇{…}` | she held these function buttons | layer-touched mask |
| `saved` | a section was saved | flag from `project_save` |
| `undone` | she undid something | flag from the undo path |
| `honour` | she pressed SEQ on the card | explicit "I'm happy with this" |
| `engine=E` | the part uses engine E | `trk[].eng_req` |

Every check is a comparison over state that already exists. **No audio analysis, no recording buffer,
no timers.** A mission can also be completed *later* — the masks persist until the mission is finished.

---

## 5. The 30 missions

Level colours follow the firmware's own (blue / green / yellow / orange / white).

### Level 1 — THE DRUMMER (unlocks: the drum kit, REC, tempo)

| # | Title | Screen | She does | Check | Card |
| --- | --- | --- | --- | --- | --- |
| 1 | FIRST HIT | `PRESS THE WHITE KEYS` / `ONE AT A TIME` | plays 8 different drum lanes | `keys⊇` 8 distinct lanes | "The white keys are a drum kit, in order: kick, kick 2, snare, clap, hat, open hat, pedal, rim. Hit them all — nothing here can break." |
| 2 | THE BEAT | `PLAY KICK ON 1 AND 3` / `REC, THEN PLAY` | REC, hits kick four times in a bar | `lane(kick)⊇{1,5,9,13}` | "Kick on 1 and 3 is the heartbeat of nearly every dance record ever made. Count 1-2-3-4 and hit on 1 and 3." |
| 3 | YOUR KIT | `PICK A KIT YOU LIKE` / `THEN PLAY IT` | turns PRESETS through 3 kits, plays | `preset≠` and `bars≥2` | "ACOUSTIC, TIGHT, DEEP, DUST… same beat, different drum kit. Which one sounds like the music you like?" |

### Level 2 — THE GROOVE (unlocks: swing, fills, more kits)

| # | Title | Screen | She does | Check | Card |
| --- | --- | --- | --- | --- | --- |
| 4 | BACKBEAT | `ADD SNARE ON 2 AND 4` | snare on 2 and 4 | `lane(snare)⊇{5,13}` | "The snare on 2 and 4 is the clap you hear in every pop song. Now it's a real beat." |
| 5 | IN BETWEEN | `HATS BETWEEN` / `HOLD OCT- FOR SOFT` | hats on the off-beats, one ghost | `lane(hat)⊇{3,7,11,15}` | "Hats fill the space between the beats. A quiet one (hold OCT−) makes it breathe." |
| 6 | SWING IT | `TURN SWING UP` / `LISTEN TO THE LIMP` | KNOB with SWING | `param(P_SWING)⇄` | "Straight is 0. Push it up and the off-beats lean late — that's swing, and it's the difference between a drum machine and a drummer." |

### Level 3 — THE BASS PLAYER (unlocks: track 2, bass sounds)

| # | Title | Screen | She does | Check | Card |
| --- | --- | --- | --- | --- | --- |
| 7 | LOW END | `BASS: PLAY THE ROOT` / `LOWEST WHITE KEY` | plays track 2 lowest keys | `keys⊇{lowest}` and track 2 has ≥4 notes | "This is your instrument. Track 2 is a bass: one note at a time, low, and it holds the band up." |
| 8 | ROOT MOVING | `MOVE THE ROOT` / `C, THEN F, THEN G` | seeds a I–IV–V | `lane`-equivalent on track 2 notes `{C,F,G}` | "In a lot of songs the bass doesn't play melodies — it walks between three notes. C, F, G: try it under your drums." |
| 9 | SLIDE | `SLIDE INTO A NOTE` / `TURN GLIDE UP` | turns GLIDE, plays | `param(P_GLIDE)⇄` | "On a double bass a slide is a real thing your hand does. Here it's one knob." |

### Level 4 — THE CHORD PLAYER (unlocks: track 3, CHORD modes, key/scale)

| # | Title | Screen | She does | Check | Card |
| --- | --- | --- | --- | --- | --- |
| 10 | TRIADS | `CHORDS: ONE FINGER` / `PLAY WHITE KEYS` | turns CHORD to TRIAD, plays | `params touched P_CHORD` and 3 keys | "One white key now plays a whole triad — the same chord shape your left hand makes, spelled out by the machine." |
| 11 | SEVENTHS | `NOW TRY 7TH` / `SOUNDS JAZZY?` | CHORD → 7TH, plays | `param(P_CHORD)⇄` twice | "Add the 7th and a plain chord becomes the sound of jazz, neo-soul and lo-fi beats." |

### Level 5 — THE SOUND SHAPER (unlocks: EDIT + ENV pages on every part)

| # | Title | Screen | She does | Check | Card |
| --- | --- | --- | --- | --- | --- |
| 12 | BRIGHTNESS | `TURN TONE DOWN` / `WHILE IT PLAYS` | turns the filter cutoff during playback | `param(P_TFLT)⇄` | "This knob is the single most useful thing on the machine. Everything you have ever heard in electronic music does this." |
| 13 | PLUCK OR PAD | `ATTACK: CLICK TO SWELL` | turns attack up and down | `param(P_ATK)⇄` | "Fast attack = plucked. Slow attack = a pad that fades in. Same note, different instrument." |
| 14 | RESONANCE | `PUSH RESONANCE UP` / `HEAR IT SING` | turns resonance | `param(P_TFLT)` + resonance param | "Turn this up and the filter starts to whistle at one note. That's how acid basslines work." |

### Level 6 — THE SPACE MAKER (unlocks: FX pages + the 27 punch-ins)

| # | Title | Screen | She does | Check | Card |
| --- | --- | --- | --- | --- | --- |
| 15 | ECHO | `SEND SOME DELAY` / `ON THE SNARE` | raises the drum delay send | `param(G_DRDLY)⇄` | "An echo on one drum only — that's how dub and techno get that huge space out of a tiny room." |
| 16 | ROOM | `ADD REVERB` / `MAKE IT BIG` | raises reverb send | `param(P_REV)⇄` | "Reverb is the room. Small room = tight. Big room = cathedral." |
| 17 | PUNCH IN | `HOLD FX` / `HIT A KEY` | uses one punch-in effect | `layers⊇{LY_FX}` | "Hold FX and hit a key: the music gets destroyed for a second. Let go and it's back. This is what DJs do with their hands." |

### Level 7 — THE MELODY (unlocks: ARP + lead sounds)

| # | Title | Screen | She does | Check | Card |
| --- | --- | --- | --- | --- | --- |
| 18 | ONE FINGER MELODY | `HOLD ONE KEY` / `HEAR IT MOVE` | turns the arpeggiator on | `param(P_AMODE)⇄` | "Hold one note and the machine plays it over and over in a pattern. Chords become melodies." |
| 19 | SCALE SAFE | `LOCK TO C MINOR` / `NOTHING SOUNDS WRONG` | sets ROOT and SCALE | `params touched P_ROOT/P_SCALE` | "Lock the keys to a scale and every note you press is in tune with the rest. You cannot play a wrong note." |
| 20 | LEAD | `PLAY A TUNE ON TOP` | plays a melody on track 3 | `wrote≥4` on track 3 and `bars≥4` | "Everything is playing — now the top line. Four notes is a melody. That's all a hook is." |

### Level 8 — THE STRINGS (unlocks: PHYS, GRAIN, NOISE engines)

| # | Title | Screen | She does | Check | Card |
| --- | --- | --- | --- | --- | --- |
| 21 | PLUCKED | `PHYS: STR PLUCK` / `LIKE A PIZZICATO` | picks PHYS, plays | `engine=ENGI_PHYS` + 4 keys | "This engine doesn't play a recording. It computes a string being plucked, right now, sample by sample. Pizzicato, basically." |
| 22 | BOWED | `NOW CELLO BOW` / `HOLD THE KEY` | picks CELLO BOW, holds | `preset` change to a PHYS preset + a held note | "Hold the key and it bows. Same maths as your bass, different hand." |
| 23 | SYMPATHETIC | `SITAR: PLAY C` / `HEAR THE OTHERS RING` | plays SITAR/TANPURA | `engine=PHYS` + `param(P_E*)⇄` | "Strike one note and other strings ring along without being touched. That's a sitar, and it's also why the piano pedal sounds the way it does." |

### Level 9 — THE COMPOSER (unlocks: SEQ + SONG pages, saving sections A–D)

| # | Title | Screen | She does | Check | Card |
| --- | --- | --- | --- | --- | --- |
| 24 | KEEP IT | `SAVE THIS` / `IT IS YOURS NOW` | saves to a section | `saved` | "Saving is the difference between playing and composing. This one's kept." |
| 25 | TWO PARTS | `MAKE A SECOND PART` / `SAME BAND, NEW TUNE` | builds a B section | `saved` twice with different content | "A verse and a chorus, or an A and a B. Two ideas that answer each other — that's a song." |
| 26 | THE ORDER | `PUT A B A` / `PLAY THE WHOLE THING` | arranges sections | `bars≥8` with section changes | "Order is a choice. A-B-A is the oldest shape in music and it still works." |

### Level 10 — THE INVENTOR (unlocks: FM6 + SAMPLE, then graduation)

| # | Title | Screen | She does | Check | Card |
| --- | --- | --- | --- | --- | --- |
| 27 | SIX OPERATORS | `FM6: ONE KEY` / `TURN THE MACROS` | picks FM6, turns macros | `engine=ENGI_FM6` + `params≥2` | "This is how a DX7 made every bell, electric piano and bass sound of the 1980s. Six little oscillators, borrowing each other's wobble." |
| 28 | YOUR VOICE | `RECORD 4 SECONDS` / `NOW PLAY IT LOW` | samples her voice into USR1 | `wav` present in a user slot | "Whatever you put in, you can play at any pitch. Record your voice, then play it as a bassline." |
| 29 | SHOW AND TELL | `PLAY ME YOUR SONG` / `SEQ WHEN DONE` | performs for you | `honour` | "This one has no test. You decide when it's finished, and you play it to someone." |
| 30 | GRADUATE | `YOU BUILT A BAND` / `NOW THE WHOLE ONE` | — | `honour` | "Ten levels. Drummer, bass, chords, melody, and a song. Now the whole machine, with nothing locked and nothing to learn next — the same one that gets updated by itself." |

**The safety mission.** Somewhere in level 2 or 3, insert a short one: `MAKE A MISTAKE` / `NOW UNDO IT`
(`undone`). It is not on the card list as a numbered mission — it is the "you cannot break it" lesson, and
it fires the first time the undo path runs, at any level, as a small unprompted "see? gone." confirmation.

**Where undo lives matters.** On a mission card, `EDIT` is a one-button undo. On the *instrument* screens
(where she actually fixes a wrong beat) undo is stock's two-button gesture — **hold `EDIT`, then press
`OCT−`** (`ui_input.c` ~949) — because `EDIT` must still open the editor pages from level 5, and `OCT−/OCT+`
must still play soft/hard drum hits. So the safety card says both: `EDIT ON A CARD` / `OR HOLD EDIT+OCT-`.
Do not put a child in front of an unlabelled two-button chord and expect discovery.

**Level 10 is the graduation** (decided 19 Nov; rationale updated 20 Nov). It was originally going to
ship without two or three sampled sounds to pay for its code — **that is no longer necessary**: the
measured headroom is 9,744 B, or 14.4 KB with the text logo, against an 8–12 KB need
(`docs/KID-MODE-MEASUREMENTS.md` §2b). So nothing is held back, and the graduation becomes a *ceremony*:
at level 30 she keeps her projects and presets, you install stock SLOOP 2.8.5, and the gift of that moment
is that the machine is now **the same one everybody else has** — no fork to be stranded on, and every
future upstream update applies. Keep it for that reason.

---

## 6. The reward design (revised after the research)

| Old idea | Problem | What replaces it |
| --- | --- | --- |
| A badge for each level (expected, contingent) | exactly the pattern that reduces intrinsic motivation in 5–12s [1](https://hiwavemakers.com/blog/gamification-learning-apps-backfire-kids-research/) | **the band grows** — a true statement about her own work, no tokens |
| "Unlock the next thing" as a reward | fine as *structure*, dangerous as *payment* | keep the unlock as a door, never as a prize for compliance; every level also ends with a **surprise** (an unnamed sound) |
| Sticker per level | ok *only* as a social ritual | the sticker is for playing it to you — relatedness, the third need in self-determination theory [5](https://thelearningrooms.com/beyond-badges-what-modern-gamification-does-to-the-brain/) |
| Streaks / daily counters | worst offender, and a missed day is punishment | **never**, for any reason |
| Progress bar | helps at 10+ *if it is meaningful* | `3 / 10` only, tied to levels actually finished |
| Points, scores, leaderboards | shifts attention to the reward | none |

Nothing in the firmware should say "well done" for pressing a button. It should say what happened:
"SNARE ON 2 AND 4 — THAT'S A BACKBEAT". Informational praise, not payment.

**As built (S6)** — what the machine actually does when a mission completes, in this order:

1. the check reads true, and 600 ms pass (so she hears what she made before the screen changes);
2. the header shows the mission's own line, the `done` column of `docs/kid_missions.tsv` — "EIGHT
   DRUMS", "THAT'S A BACKBEAT", "THE WHOLE MACHINE" — nothing about her;
3. if the level opened, that is said next, as the second message: `LEVEL 2 IS OPEN`;
4. the clubhouse's band row lights the player she just added (`kid_track_ok()`), and the level's three
   dots fill in, one for each mission done.

There is no score, no streak, no counter, no badge shown as payment, and no "well done" for pressing a
button anywhere in the kid build. `3 / 10` is the only number, and it is the levels.

---

## 7. When she is stuck

A fixed ladder, no dead ends:

1. The mission line is already the shortest version of the instruction.
2. `SEQ` replays the seed pattern (hear it again) and shows one more line (`hint`).
3. `SEQ` again: the machine *does* the step for her while she watches, then reverses it (a demonstration,
   not a skip).
4. `SEQ` a third time offers "SKIP FOR NOW" — the mission parks, the level's other missions stay
   open, and it returns later. Nothing is lost, nothing is marked failed.
5. The card in her hand has the long version, written for her, with a bad drawing.

A kid who is stuck for 90 seconds has stopped learning; the ladder exists so that window never opens.

**As built (S6)** — `SEQ` on the clubhouse, one rung a press, and the card always shows where she is:

| rung | what she sees / hears |
| --- | --- |
| 1 | the `hint` line replaces the card's third line (no message to dismiss) |
| 2 | `LISTEN`: the transport starts; for a drum mission the steps she is missing are lent to that lane for 2 s and then taken back — a demonstration, and it **cannot** complete the mission for her |
| 3 | `TAP SEQ: SKIP` |
| 4 | `SKIPPED`: the mission parks and she is back on her song. It is still the card (it comes first), and it is never marked failed |

Steps 2 and 3 of the ladder above collapse into rung 2 on the device (the transport replays the seed
*and* the drum step is done for her, which is the same 2 seconds); rung 1 is the hint alone. Rungs
reset when a mission starts or ends, so the ladder never starts half-way up.

---

## 8. Side quests (never gate anything)

| Quest | Card line |
| --- | --- |
| MAKE IT RAIN | "NOISE engine → RAIN, then a filter sweep. Congrats, you made weather." |
| THREE NOTES | "A whole melody, three notes only. Beethoven did this." |
| THE SLOWEST BEAT | "How slow can it go before it stops being a beat?" |
| ANGRY BASS | "Distortion on the bass. What emotion does that sound like?" |
| YOUR NAME | "Spell your name with drum hits. Is it a good beat?" |
| THE QUIET ONE | "Make a track where almost nothing happens. That's ambient, and it's harder than it looks." |
