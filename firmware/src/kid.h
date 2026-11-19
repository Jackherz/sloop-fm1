/* SPDX-License-Identifier: GPL-3.0-only */
/* SLOOP JR (docs/KID-MODE-*.md): the kid build's gate. One predicate per kind of
 * thing a level unlocks (pages, engines, tracks, layers, kits, buttons), all
 * answered from the level (docs/KID-MODE-PLAN.md §4: one new control per level).
 * Every call site is #if FELUCCA_KID-guarded, so a stock build carries none of
 * this; the definitions live in kid.c, included at the end of felucca.c. */
#include <stdint.h>
#ifndef FELUCCA_KID
#define FELUCCA_KID 0            /* the kid build (SLOOP JR); 0 = stock behaviour */
#endif
#ifndef KID_LEVEL
#define KID_LEVEL 1              /* the level a fresh kid build starts at (tests override at runtime) */
#endif
#if FELUCCA_KID
#define KID_MAGIC 0x3144494Bu    /* "KID1" */
/* the progress record (docs/KID-MODE-STAGES.md §2.4): 24 B appended after the
 * stock side record's fcrc, with its own magic and CRC, so stock 2.8.5 still
 * validates its own 80 bytes and graduation keeps her theme and favourites */
typedef struct {
    uint32_t magic;      /* KID_MAGIC (0 = no record) */
    uint8_t version;     /* 1 */
    uint8_t level;       /* current level 1..10 (0 = no record) */
    uint8_t mission[8];  /* 30 missions x 2 bits (S5) */
    uint16_t sessions;
    uint16_t badges;
    uint8_t reserved[2];
    uint32_t crc;        /* over everything before it */
} kid_rec_t;
extern uint8_t kid_dirty;   /* the record changed since the settings were written */
extern uint8_t kid_mission_active;   /* a mission is up: autosave is suspended (S4) */
uint8_t kid_on(void);            /* 1: the kid build (the gate is on) */
uint8_t kid_level(void);         /* 1..10 */
void kid_set_level(uint8_t n);   /* set the level (saved when quiet, like autosave) */
void kid_reset(void);            /* parent: wipe progress (back to level 1) */
void kid_fill(kid_rec_t *r);     /* the record as it stands (CRC computed) */
void kid_take(kid_rec_t *r);     /* a record read back: what checks out */
uint8_t kid_page_ok(uint32_t idx);    /* PAGES[idx] reachable at this level */
uint8_t kid_engine_ok(uint32_t e);    /* engine id allowed (NENGINES = user presets: L9+) */
uint8_t kid_track_ok(uint32_t i);     /* track index allowed (the drum track always) */
uint8_t kid_layer_ok(uint32_t l);     /* LY_* layer allowed */
uint32_t kid_kit_max(void);           /* highest drum kit index allowed */
uint8_t kid_btn_blocked(uint32_t b);  /* a B_* button's hold / layer action is blocked */
void kid_refuse(void);           /* "NOT YET · LEVEL n" (ui.msg is 24 B) */
void kid_apply_layers(void);     /* zero ly_bit for locked layers (the ISR's table) */
void kid_init(void);             /* boot: REMOTE off, the drums first, layers */
/* the clubhouse (S3): one page, three bands (docs/KID-MODE-STAGES.md §2.3) */
uint8_t kid_on_screen(void);     /* the clubhouse page is up */
void kid_enter(void);            /* go to the clubhouse (menu/holds/locks cleared, seq_mode 0) */
void kid_draw(void);             /* the clubhouse screen (header / body / footer) */
int kid_input(uint32_t pressed, uint32_t notes, uint32_t home);  /* 1: the input was consumed */
/* the two rooms (S4): the snapshot is the autosave object (STAGES §2.1) */
/* 0: the transport is stopping, call again (nothing written yet); 1: done; -1: refused
 * (her song cannot be saved / the seed is not a project / nothing to leave) */
int kid_mission_enter(const void *seed, uint32_t n);   /* flush her song, suspend autosave, load the seed */
int kid_mission_leave(void);                           /* restore her song from OBJ_AUTOSAVE */

/* ---- the mission engine (S5): one table, seeds, witnesses, checks ---------- */
/* docs/kid_missions.tsv -> tools/gen_kid_missions.py -> build/gen/kid_missions.h:
 * the text, the seeds, the checks. No prose in the image. */
enum {                          /* what completes a mission (the curriculum's §4 notation) */
    KCHK_NONE = 0,
    KCHK_HONOUR,                /* she pressed SEQ: "I am happy with this" */
    KCHK_KEYS,                  /* this many different keys, since the mission started */
    KCHK_LANE,                  /* drum lane c0 has steps c1 (bit 0 = step 1) */
    KCHK_WROTE,                 /* track c0 has c1 steps written */
    KCHK_NOTEPC,                /* track c0's notes cover pitch classes c1 (12 bits) */
    KCHK_BARS,                  /* c0 bars played with two parts or more sounding */
    KCHK_PARAM,                 /* parameter c0 moved while playing (0x80 | G_*: a global) */
    KCHK_PARAMS,                /* this many different parameters moved while playing */
    KCHK_PRESET,                /* the sound changed c0 times */
    KCHK_LAYER,                 /* LY_* c0 held */
    KCHK_SAVED,                 /* a section saved (c0 times) */
    KCHK_UNDONE,                /* she undid something */
    KCHK_ENGINE,                /* a part runs engine c0 */
    KCHK_SAMPLE                 /* she stored a sound of her own (c0 of them) */
};
extern uint8_t kid_ms_id;                       /* the mission up: 0..29, 0xFF = none */
int kid_mission_start(uint8_t id);              /* expand the seed and enter (as enter: 0 stopping, 1 up, -1 refused) */
int kid_mission_end(void);                      /* leave a mission (her song back) */
uint8_t kid_mission_pass(void);                 /* 1: the checks are true now */
uint8_t kid_mission_finish(void);               /* pass + record + badge the level when its missions are done */
void kid_mission_tick(uint32_t notes, uint32_t ms);   /* witnesses, once a UI frame */
uint8_t kid_mission_done(uint8_t id);           /* 0 untouched, 1 started, 2 done */
const char *kid_mission_title(uint8_t id), *kid_mission_line(uint8_t id, uint32_t n),
           *kid_mission_hint(uint8_t id), *kid_mission_done_line(uint8_t id);
uint8_t kid_mission_of_level(uint8_t level, uint8_t nth);   /* 0..29, 0xFF = none */
/* S6: the clubhouse plays it */
uint8_t kid_mission_current(void);              /* the mission to offer (0..29, 0xFF = all done) */
void kid_mission_skip(void);                    /* park it for now: the others stay open */
uint8_t kid_mission_hint_rung(void);            /* 0 the card, 1 the hint, 2 a demo, 3 skip */
void kid_mission_hint_next(void);               /* SEQ: the next rung of the ladder */
uint8_t kid_mission_demo(void);                 /* 1: a demonstration is running (listen) */
/* the witnesses the stock paths feed (they do nothing unless a mission is up) */
void kid_wit_param(uint32_t id);
void kid_wit_preset(void);
void kid_wit_layer(uint32_t l);
void kid_wit_saved(void);
void kid_wit_undone(void);
void kid_wit_sample(void);
#endif
