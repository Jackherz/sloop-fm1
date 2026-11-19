/* SPDX-License-Identifier: GPL-3.0-only */
/* SLOOP JR: the gate (docs/KID-MODE-STAGES.md S1). The level -> unlock tables
 * are the plan's §4 (one new control per level); every predicate is a pure
 * function of the level, so the gate matrix test (tests/kid_test.c) can pin
 * them. Locked means unreachable, never deleted, never rewritten: a project
 * restore (proj_apply) is deliberately not gated. */
#if FELUCCA_KID
#include "kid_missions.h"   /* kid_seed_t, kid_mission_t, KID_SEEDS[], KID_MISSIONS[] (generated) */
/* the level each page unlocks at (PAGES[] index; 0 = never in kid mode: TOOLS,
 * whose NEW LOOP would wipe her song with no confirmation, finding 7) */
static const uint8_t KID_PAGE_LVL[] = {
    5, 5,                          /* ENV, ENV DEST */
    10, 10, 10,                    /* LFO, LFO 2, LFO DEST */
    6, 6, 6, 6, 6, 6, 6,           /* FX, FILTER, SLICER, INSERT, INSERT 2, DLY, REV/CHO */
    4, 4,                          /* SEL, SEL 2 */
    5, 5, 5, 5, 5,                 /* EDIT 1, EDIT 2, FILTER, VOICE, VOICE 2 */
    1, 10, 10, 1, 10, 10, 10, 10, 10,  /* GLOBAL, MASTER, SYSTEM, DRUMS, MIDI, MIDI 2, MIDI 3, TEXTURE, TEXTURE 2 */
    3, 9, 9, 0,                    /* PRESETS, USER, SECTION, TOOLS (never) */
    7, 7,                          /* ARP, ARP 2 */
    9, 9, 9,                       /* NOTES, PATTERN, SONG */
    1, 1,                          /* TRACKS, DRUMS */
    1,                             /* SLOOP JR (the clubhouse: always reachable, it is home) */
};
_Static_assert(sizeof(KID_PAGE_LVL) / sizeof(KID_PAGE_LVL[0]) == sizeof(PAGES) / sizeof(PAGES[0]),
               "one level per page (extend the table when PAGES grows)");
/* the level each engine unlocks at (ENGINES[] index) */
static const uint8_t KID_ENG_LVL[] = {
    3, 3, 3, 3,   /* ANALOG, DIGITAL, PHASE, LOFI (bass) */
    10,           /* SAMPLE */
    7, 7, 7,      /* FORMANT, TRIO, DRAWBAR (lead / keys) */
    8,            /* GRAIN */
    10,           /* FM6 */
    8, 8,         /* PHYS, NOISE */
    10,           /* SLICE */
};
static kid_rec_t kid_ram;   /* the progress record as it stands in RAM (level 0 = none) */
uint8_t kid_dirty;          /* changed since the settings were written (project.c) */
static uint32_t kid_parked;     /* missions parked with SKIP this session (a bit each) */
static uint8_t  kid_rung;       /* the ladder's rung for the mission on the card */
static uint8_t  kid_demo_lane;  /* a demonstration: the lane and the steps it lent her */
static uint64_t kid_demo_mask;  /* (the steps it added: NSTEP is 64) */
static uint32_t kid_demo_t;     /* when it started (ms, 0 = not running) */
static uint32_t kid_pass_t;     /* when the check first read true (a settle before the reward) */
static uint8_t  kid_safety;     /* the one-shot "see? gone." */
static uint8_t  kid_req;        /* 1: PLAY asked for the mission, 2: HOME asked to leave it */
static char     kid_say2[24];   /* the second message (the level that opened) */
static uint8_t  kid_safety_m;   /* (the safety lesson is waiting for the header) */

_Static_assert(sizeof(kid_rec_t) == 24u, "the progress record is 24 B (of 112 free)");
uint8_t kid_on(void) { return 1; }
uint8_t kid_level(void)
{
    uint8_t n = kid_ram.level ? kid_ram.level : KID_LEVEL;
    return n < 1u ? 1u : n > 10u ? 10u : n;
}
void kid_set_level(uint8_t n)
{
    kid_ram.level = n;
    kid_dirty = 1;
    settings_later = 1;   /* saved when quiet, like autosave (finding 14) */
}
void kid_reset(void)
{
    memset(&kid_ram, 0, sizeof kid_ram);
    kid_dirty = 1;
    settings_later = 1;
    kid_parked = 0;                 /* S6: a fresh record offers the first mission again */
    kid_rung = 0;
    kid_safety = 0;
    kid_safety_m = 0;
    kid_say2[0] = 0;
}
void kid_fill(kid_rec_t *r)
{
    uint32_t i, any = (uint32_t)kid_ram.level | kid_ram.sessions | kid_ram.badges;
    for (i = 0; i < 8u; i++)
        any |= kid_ram.mission[i];
    if (!any) {
        memset(r, 0, sizeof *r);
        return;
    }
    *r = kid_ram;
    r->magic = KID_MAGIC;
    r->version = 1;
    r->crc = st_crc32(r, __builtin_offsetof(kid_rec_t, crc));
}
void kid_take(kid_rec_t *r)
{
    if (r->magic == KID_MAGIC && r->version == 1u &&
        r->crc == st_crc32(r, __builtin_offsetof(kid_rec_t, crc)))
        kid_ram = *r;   /* a torn write fails the CRC: the previous copy (the A/B pair) stands */
}
uint8_t kid_page_ok(uint32_t idx)
{
    return idx < sizeof(KID_PAGE_LVL) / sizeof(KID_PAGE_LVL[0]) &&
           KID_PAGE_LVL[idx] != 0u && kid_level() >= KID_PAGE_LVL[idx];
}
uint8_t kid_engine_ok(uint32_t e)
{
    if (e == NENGINES)
        return kid_level() >= 9u;   /* user presets, with the USER page */
    return e < sizeof(KID_ENG_LVL) / sizeof(KID_ENG_LVL[0]) && kid_level() >= KID_ENG_LVL[e];
}
uint8_t kid_track_ok(uint32_t i)
{
    if (i == TRK_DRUM)
        return 1;
    if (i == 0u)
        return kid_level() >= 3u;
    if (i == 1u)
        return kid_level() >= 4u;
    if (i == 2u)
        return kid_level() >= 7u;
    return 0;
}
uint8_t kid_layer_ok(uint32_t l)
{
    switch (l) {
    case LY_PLAY: return 1;
    case LY_MIX: return kid_level() >= 2u;    /* mutes, solos, fills, tap tempo */
    case LY_SCALE: return kid_level() >= 4u;  /* the key of the song */
    case LY_ERASE: return kid_level() >= 5u;  /* erase, shift, transpose, undo */
    case LY_FX: return kid_level() >= 6u;     /* punch-ins */
    case LY_ROLL: return kid_level() >= 7u;   /* the arp roll */
    case LY_STEP: return kid_level() >= 9u;   /* the step editor */
    case LY_SONG: return kid_level() >= 9u;   /* sections A..D */
    }
    return 0;
}
uint32_t kid_kit_max(void)
{
    uint8_t l = kid_level();
    return l <= 1u ? 2u : l == 2u ? 8u : DRUM_KITS - 1u;   /* 3 kits, then 9, then all */
}
uint8_t kid_btn_blocked(uint32_t b)
{
    switch (b) {
    case B_REC: return 1;   /* held: clears the track (finding 22); the tap still records */
    case B_FX: return !kid_layer_ok(LY_FX);
    case B_EDIT: return !kid_layer_ok(LY_ERASE);
    case B_ARP: return !kid_layer_ok(LY_ROLL);
    case B_SEQ: return !kid_layer_ok(LY_STEP);
    case B_SCL: return !kid_layer_ok(LY_SCALE);
    case B_GLO: return !kid_layer_ok(LY_MIX);
    case B_SAVE: return !kid_layer_ok(LY_SONG);
    }
    return 0;
}
void kid_refuse(void)
{
    char b[4];
    fmt_int(b, kid_level());
    ui_say("NOT YET · LEVEL ", b);
}
void kid_apply_layers(void)
{
    uint32_t l;
    for (l = LY_FX; l < LY_COUNT; l++)
        ly_bit[l] = kid_layer_ok(l) ? 1u << panel.btn[LAYER_BTN[l]] : 0u;
}
void kid_init(void)
{
    song.g[G_RMCH] = 0;                    /* REMOTE off: MIDI cannot press a panel button (finding 23) */
    if (!kid_track_ok(song.sel))
        song.sel = TRK_DRUM;               /* the drums first (level 1 is the drummer) */
    kid_apply_layers();
}

/* -------------------------------------------------------- the clubhouse --- */
/* S3 (docs/KID-MODE-STAGES.md §2.3): one page, three bands. The header (0..19)
 * shows ui.msg when there is one (finding 32), so the parent's messages and
 * the update countdown are never hidden; the body is the graph strip's slot
 * (240x124); the footer (202..239) is the button reminder. Drawn only when
 * something changed (finding 30). */
static const char *const KID_LVL_NAME[10] = {   /* docs/KID-MODE-PLAN.md §4 */
    "THE DRUMMER", "THE GROOVE", "THE BASS PLAYER", "THE CHORD PLAYER", "THE SOUND SHAPER",
    "THE SPACE MAKER", "THE MELODY", "THE STRINGS", "THE COMPOSER", "THE INVENTOR",
};
static const char *const KID_TILE[4] = {"DRUMS", "BASS", "KEYS", "LEAD"};   /* the band row (the map) */
static const uint8_t KID_TILE_TRK[4] = {TRK_DRUM, 0, 1, 2};                /* tile -> track index */

uint8_t kid_on_screen(void)
{
    return !ui.home && cur_page()->scope == SC_KID;
}

void kid_enter(void)
{
    ui.home = 0;
    ui.page = (uint8_t)page_first(FAM_KID);
    ui.fam_last[FAM_KID] = ui.page;
    ui.menu = 0;              /* finding 13: the parent menu must not swallow the kid handler */
    ui.hold_kind = 0;         /* ... nor a hold confirmation */
    ui.layer = LY_PLAY;
    ly_lock = LY_PLAY;        /* ... nor a locked layer (the first keypress would be eaten) */
    ui.home_t0 = 0;
    ui.entry_open = 0;
    ui.hot_t = 0;
    song.seq_mode = 0;        /* finding 19: the sequencer must not stay armed behind the clubhouse */
    ui.force = 1;
}

void kid_draw(void)
{
    uint8_t id = kid_mission_active ? kid_ms_id : kid_mission_current();   /* what PLAY would start */
    uint32_t lvl = kid_level(), i, dsum = 0;
    uint32_t sig = lvl * 131u + (ui.msg_t ? str_hash(7u, ui.msg) : 0u) +
                   (uint32_t)song.playing * 17u + (uint32_t)song.rec * 257u +
                   (uint32_t)(id & 0xFF) * 29u + (uint32_t)kid_rung * 53u +
                   (uint32_t)kid_mission_active * 97u;
    for (i = 0; i < 3u; i++) {                     /* the level's three, done or not */
        uint8_t m = kid_mission_of_level((uint8_t)lvl, (uint8_t)i);
        dsum = dsum * 3u + (m < KID_NMISSION ? kid_mission_done(m) : 0u);
    }
    sig += dsum * 11u;
    if (!ui.force && sig == ui.kid_sig)
        return;               /* finding 30: redraw on state change only */
    ui.kid_sig = sig;
    lcd_fill(0, H_HEAD, 240, Y_GRAPH - H_HEAD, C_BLACK);              /* the gaps between the bands */
    lcd_fill(0, Y_GRAPH + H_GRAPH, 240, Y_FOOT - Y_GRAPH - H_GRAPH, C_BLACK);
    cv_begin(240, H_HEAD, C_BLACK);                                   /* band 1: the header */
    if (ui.msg_t) {
        cv_text(4, 1, &FONT_S, ui.msg, C_HI);                         /* finding 32: never hide a message */
    } else {
        char b[16];
        str_cpy(b, "LEVEL ", sizeof b);                               /* (the card names the mission) */
        fmt_int(b + 6, (int32_t)lvl);
        cv_text(4, 1, &FONT_S, b, C_HI);
        cv_text(236 - text_w(&FONT_S, "SLOOP JR"), 1, &FONT_S, "SLOOP JR", C_GRAY);
    }
    cv_blit(0, Y_HEAD);
    cv_begin(240, H_GRAPH, C_BLACK);                                  /* band 2: the card */
    if (id >= KID_NMISSION) {                                         /* every mission done */
        cv_text(4, 10, &FONT_L, "ALL DONE", C_WHITE);
        cv_text(4, 50, &FONT_S, "YOU BUILT A BAND", C_HI);
    } else {
        char b[16];
        str_cpy(b, "MISSION ", sizeof b);                             /* (which one, small) */
        fmt_int(b + 8, (int32_t)id + 1);
        cv_text(4, 0, &FONT_S, b, C_GRAY);
        cv_text(236 - text_w(&FONT_S, KID_LVL_NAME[lvl - 1u]), 0, &FONT_S, KID_LVL_NAME[lvl - 1u], C_GRAY);
        cv_text(4, 17, &FONT_L, kid_mission_title(id), C_WHITE);      /* what she will make */
        cv_text(4, 50, &FONT_S, kid_mission_line(id, 0), C_HI);       /* what to do */
        /* the third line is hers to ask for (the ladder's first rung); until then, the second cue */
        if (kid_rung && kid_mission_hint(id)[0])
            cv_text(4, 66, &FONT_S, kid_mission_hint(id), C_WHITE);
        else if (kid_mission_line(id, 1)[0] && kid_mission_line(id, 1)[0] != '-')
            cv_text(4, 66, &FONT_S, kid_mission_line(id, 1), C_GRAY);
    }
    for (i = 0; i < 4u; i++) {                                        /* the band row: lit = hers */
        int32_t x = 4 + (int32_t)i * 59;
        uint16_t col = TE_COL[KID_TILE_TRK[i] & 3u];
        int on = kid_track_ok(KID_TILE_TRK[i]);
        cv_rect(x, 84, 55, 22, on ? col : C_LINE);
        cv_text(x + 4, 88, &FONT_S, KID_TILE[i], on ? C_WHITE : C_DIM);
    }
    if (id < KID_NMISSION) {                                          /* the level's three dots */
        uint8_t lv = KID_MISSIONS[id].level;
        for (i = 0; i < 3u; i++) {
            uint8_t m = kid_mission_of_level(lv, (uint8_t)i);
            int32_t x = 4 + (int32_t)i * 20;
            uint8_t d = m < KID_NMISSION ? kid_mission_done(m) : 0u;
            if (d == 2u) {                                            /* done: filled */
                cv_rect(x, 108, 12, 12, C_WHITE);
            } else {                                                  /* open (or up): a ring */
                uint16_t c = m == id ? C_HI : C_LINE;
                cv_rect(x, 108, 12, 12, c);
                cv_rect(x + 2, 110, 8, 8, C_BLACK);
            }
        }
        for (i = 0; i < 10u; i++) {                                   /* the levels, small */
            int32_t x = 76 + (int32_t)i * 15;
            cv_rect(x, 110, 8, 8, i + 1u == lvl ? C_HI : i + 1u < lvl ? C_WHITE : C_LINE);
        }
    }
    cv_blit(0, Y_GRAPH);
    lcd_fill(0, Y_FOOT, 240, 240 - Y_FOOT, C_BLACK);                  /* band 3: the footer */
    lcd_fill(0, Y_FOOT, 240, 1, C_LINE);
    cv_begin(240, 240 - Y_FOOT, C_BLACK);
    if (kid_mission_active) {
        cv_text(4, 6, &FONT_S, "HOME: DONE", C_GRAY);
        cv_text(236 - text_w(&FONT_S, "PLAY: STOP"), 6, &FONT_S, "PLAY: STOP", C_GRAY);
    } else {
        cv_text(4, 6, &FONT_S, "SEQ: HELP", C_GRAY);
        cv_text(236 - text_w(&FONT_S, "PLAY: GO"), 6, &FONT_S, "PLAY: GO", C_GRAY);
        cv_text(4, 18, &FONT_S, "HOME: FREE PLAY", C_DIM);
    }
    cv_blit(0, Y_FOOT);
}

/* the input map (docs/KID-MODE-IMPLEMENTATION.md §4, decision 11). On the
 * clubhouse: SEQ = hint, EDIT = undo, HOME tap = rooms, PLAY/REC as stock,
 * everything else = NOT YET. On instrument screens only HOME is intercepted
 * (tap = back to the clubhouse). HOME held 3 s = the parent menu (own timer:
 * btn_hold is fixed at 700 ms). OCT-/OCT+ are never taken. Returns 1 when
 * the input was consumed (ui_input must not run the stock handling). */
int kid_input(uint32_t pressed, uint32_t notes, uint32_t home)
{
    static uint32_t home_t0;   /* the 3 s parent-menu hold (ms, 0 = not holding) */
    uint32_t now = fm1_ms;
    uint32_t home_down = (fm1_in.buttons >> panel.btn[B_HOME]) & 1u;
    uint32_t play_rec = (1u << panel.btn[B_PLAY]) | (1u << panel.btn[B_REC]);
    kid_mission_tick(notes, now);   /* S5: the witnesses (keys, bars); nothing unless a mission is up */

    if (ui.menu)
        return 0;   /* the parent menu owns the input while it is open */

    if (home_down) {   /* HOME held 3 s: the parent menu (any kid screen) */
        if (!home_t0)
            home_t0 = now;
        else if (now - home_t0 > 3000u) {
            home_t0 = 0;
            layer_unlock();
            ui.menu = 1;
            ui.menu_sel = 0;
            ui.confirm = 0;
            ui.force = 1;
            song.seq_mode = 0;
            return 1;
        }
    } else {
        home_t0 = 0;
    }

    if (kid_on_screen()) {
        uint32_t home_bit = 1u << panel.btn[B_HOME];
        if (pressed & (1u << panel.btn[B_SEQ])) {          /* SEQ: the ladder's next rung */
            kid_mission_hint_next();                       /* hint -> listen -> do it for her -> park */
            enc_drop();
            return 1;
        }
        if (pressed & (1u << panel.btn[B_EDIT])) {         /* EDIT: undo (one button) */
            ui_message(undo_swap(0) ? "UNDO" : "NOTHING TO UNDO");
            enc_drop();
            return 1;
        }
        if (home == BT_TAP) {                              /* HOME tap: her song back, or the sandbox */
            if (kid_mission_active) {
                kid_req = 2;                               /* (the clock waits for the transport) */
            } else {
                ui.page = (uint8_t)page_first(FAM_TRK);
                ui.fam_last[FAM_TRK] = ui.page;
                ui.home = 0;
                page_entered();
            }
            enc_drop();
            return 1;
        }
        if (pressed & ~(play_rec | home_bit)) {            /* any other button: NOT YET (HOME is the tap above) */
            kid_refuse();
            enc_drop();
            return 1;
        }
        if (pressed & play_rec) {
            if ((pressed & (1u << panel.btn[B_PLAY])) && !kid_mission_active) {
                kid_req = 1;                               /* PLAY: the card's mission (the clock starts it) */
                enc_drop();
                return 1;
            }
            return 0;                                      /* a mission is up: PLAY / REC, as stock */
        }
        enc_drop();   /* knobs and keys do nothing here (and must not jump on exit) */
        return 1;
    }

    if (home == BT_TAP) {   /* an instrument screen: HOME is the only exit */
        kid_enter();
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------ the two rooms --- */
/* S4 (docs/KID-MODE-STAGES.md §2.1): the snapshot is the autosave object
 * (OBJ_AUTOSAVE), never section D (finding 3), no new flash object (finding 4),
 * no fifth project_t in RAM (finding 5). Her song is already in OBJ_AUTOSAVE
 * (autosave_tick saved it at the last quiet moment); entering a mission
 * flushes it if not, suspends autosave, and loads the seed. Leaving restores
 * it. A power cut during a mission brings back her song, not the seed. */
uint8_t kid_mission_active;   /* a mission is up (project.c: autosave_tick / persist_flush_now) */

int kid_mission_enter(const void *seed, uint32_t n)
{
    if (song.playing || transport_req) {   /* an erase under a running transport glitches */
        transport_req = 2;
        return 0;                          /* stopping: call again (nothing written yet) */
    }
    if (!flash_ok)
        return -1;                         /* no flash, no snapshot: a mission would have nowhere to come back from */
    proj_capture(&autosave_buf);           /* her song, as it stands */
    if (autosave_buf.sum != autosave_hash) {
        if (st_save(OBJ_AUTOSAVE, &autosave_buf, sizeof autosave_buf) != 0)
            return -1;                     /* her song cannot be saved: the mission is not entered */
        autosave_hash = autosave_buf.sum;
    }
    kid_mission_active = 1;                /* autosave_tick() returns early from now on */
    if (project_restore(4, seed, n) != 0) {   /* the seed is not a project */
        kid_mission_active = 0;
        return -1;
    }
    return 1;
}

int kid_mission_leave(void)
{
    int n;
    if (!kid_mission_active)
        return -1;
    if (song.playing || transport_req) {
        transport_req = 2;
        return 0;                          /* stopping: call again (nothing written yet) */
    }
    n = st_load(OBJ_AUTOSAVE, &proj_tmp, sizeof proj_tmp);   /* her song from flash */
    if (n <= 0 || project_restore(4, &proj_tmp, (uint32_t)n) != 0)
        return -1;                         /* still in the mission: a reset brings her song back */
    kid_mission_active = 0;
    autosave_hash = autosave_buf.sum;      /* project_restore() left her song in autosave_buf */
    return 1;
}

/* ------------------------------------------- the mission engine (S5) --- */
/* docs/kid_missions.tsv is the source of truth; build/gen/kid_missions.h is it as
 * C (generated by tools/gen_kid_missions.py, which also writes the printed cards).
 * A seed is a recipe expanded into the working project, never a stored project
 * (decision 3). A witness is a mask over state that already exists, and a check is a
 * comparison over those masks: no audio analysis, no recording buffer, no timers
 * (the curriculum's §4). */

typedef struct {                /* the witnesses: what she did since the mission started */
    uint32_t keys;              /* the note keys pressed (as the panel reports them: one bit a key) */
    uint32_t pmask[3];          /* P_* moved while the transport ran (P_COUNT <= 96) */
    uint64_t gmask;             /* G_* moved while the transport ran (G_COUNT <= 64) */
    uint16_t lmask;             /* LY_* layers used */
    uint16_t bar_u;             /* 1/16 bar of play, for KCHK_BARS */
    uint8_t bars, presets, saves, undone, samples;
    uint32_t last_ms;
} kid_wit_t;
static kid_wit_t kid_wit;       /* (all zero: no witness until a mission starts) */

uint8_t kid_ms_id = 0xFF;       /* the mission up, 0..29 */

static uint8_t kid_bits(uint32_t m)     /* how many bits are set */
{
    uint8_t n = 0;
    while (m) {
        n = (uint8_t)(n + (m & 1u));
        m >>= 1;
    }
    return n;
}

static uint8_t kid_bits64(uint64_t m)   /* (the globals: G_COUNT can pass 32) */
{
    uint8_t n = 0;
    while (m) {
        n = (uint8_t)(n + (m & 1u));
        m >>= 1;
    }
    return n;
}

static uint8_t kid_wit_zero(void) { return kid_ms_id < KID_NMISSION; }   /* (a mission is up) */

void kid_wit_param(uint32_t id)         /* ui_input.c edit_param: a knob moved a value */
{
    if (!kid_wit_zero() || !song.playing)
        return;                         /* (a mission's check is about playing: the curriculum) */
    if (id & 0x80u) {
        if ((id & 0x7Fu) < G_COUNT && (id & 0x7Fu) < 64u)
            kid_wit.gmask |= 1ull << (id & 0x7Fu);
        return;
    }
    if (id < P_COUNT && id < 96u)
        kid_wit.pmask[id >> 5] |= 1u << (id & 31u);
    if (id == P_E0 && song.sel == TRK_DRUM)   /* the drum kit: "she changed the sound" */
        kid_wit.presets++;
}
void kid_wit_preset(void) { if (kid_wit_zero()) kid_wit.presets++; }
void kid_wit_layer(uint32_t l) { if (kid_wit_zero() && l < 16u) kid_wit.lmask |= (uint16_t)(1u << l); }
void kid_wit_saved(void) { if (kid_wit_zero()) kid_wit.saves++; }
void kid_wit_undone(void)
{
    if (kid_wit_zero())
        kid_wit.undone = 1;
    if (!kid_safety) {                     /* curriculum: the "you cannot break it" lesson, once */
        kid_safety = 1;
        kid_safety_m = 1;
    }
}
void kid_wit_sample(void) { if (kid_wit_zero()) kid_wit.samples++; }


static void kid_clock(uint32_t now);   /* (defined with the witnesses below) */

void kid_mission_tick(uint32_t notes, uint32_t ms)
{
    uint32_t d, i, live = 0;
    kid_clock(ms);
    if (!kid_wit_zero()) {
        kid_wit.last_ms = ms;
        return;
    }
    kid_wit.keys |= notes;
    d = ms - kid_wit.last_ms;
    kid_wit.last_ms = ms;
    if (!song.playing || d > 200u)      /* (a stall is not music: no bar for it) */
        return;
    for (i = 0; i < NPART; i++)
        live += !trk[i].p[P_MUTE];
    if (live < 2u)
        return;                         /* KCHK_BARS: two parts or more sounding */
    kid_wit.bar_u = (uint16_t)(kid_wit.bar_u + d * (uint32_t)(song.g[G_BPM] > 0 ? song.g[G_BPM] : 1));
    while (kid_wit.bar_u >= 15000u && kid_wit.bars < 250u) {   /* 1/16 bar = 15000/bpm ms */
        kid_wit.bar_u = (uint16_t)(kid_wit.bar_u - 15000u);
        kid_wit.bars++;
    }
}

/* a track's written steps: a synth step with a note or a velocity, a drum step with a lane */
static uint32_t kid_wrote(const track_t *t, uint32_t *pcs)
{
    uint32_t i, n = 0, pc = 0;
    for (i = 0; i < NSTEP; i++) {
        if (t == TDRUM) {
            if (t->dstep[i].on[0] || t->dstep[i].on[1])
                n++;
            continue;
        }
        if (!t->step[i].note[0] && !t->step[i].vel)
            continue;
        n++;
        for (uint32_t j = 0; j < 4u; j++)
            if (t->step[i].note[j])
                pc |= 1u << (t->step[i].note[j] % 12u);
    }
    if (pcs)
        *pcs = pc;
    return n;
}

static uint8_t kid_chk(uint8_t kind, uint32_t a, uint32_t b)
{
    const track_t *t;
    uint32_t i;
    switch (kind) {
    case KCHK_HONOUR: return 1;                         /* SEQ is the test */
    case KCHK_KEYS:   return kid_bits(kid_wit.keys) >= a;
    case KCHK_LANE:
        if (a >= DRUM_LANES || !b)
            return 0;
        {   uint32_t s = 0;
            for (i = 0; i < NSTEP; i++)
                if ((TDRUM->dstep[i].on[a >> 3] >> (a & 7u)) & 1u)
                    s |= 1u << i;
            return (s & b) == b;
        }
    case KCHK_WROTE:
        t = a < NTRK ? &trk[a] : TDRUM;
        return kid_wrote(t, 0) >= b;
    case KCHK_NOTEPC:
        if (a >= NTRK)
            return 0;
        {   uint32_t pc = 0;
            kid_wrote(&trk[a], &pc);
            return pc && (pc & b) == b;
        }
    case KCHK_BARS:   return kid_wit.bars >= a;
    case KCHK_PARAM:
        return a & 0x80u ? (((a & 0x7Fu) < G_COUNT && (a & 0x7Fu) < 64u) &&
                            ((kid_wit.gmask >> (a & 0x7Fu)) & 1ull)) != 0
                         : (a < 96u && ((kid_wit.pmask[a >> 5] >> (a & 31u)) & 1u)) != 0;
    case KCHK_PARAMS:
        return (uint32_t)kid_bits(kid_wit.pmask[0]) + kid_bits(kid_wit.pmask[1]) +
               kid_bits(kid_wit.pmask[2]) + kid_bits64(kid_wit.gmask) >= a;
    case KCHK_PRESET: return kid_wit.presets >= a;
    case KCHK_LAYER:  return (kid_wit.lmask >> a) & 1u;
    case KCHK_SAVED:  return kid_wit.saves >= a;
    case KCHK_UNDONE: return kid_wit.undone != 0;
    case KCHK_ENGINE:
        for (i = 0; i < NPART; i++)
            if (trk[i].eng_req == a)
                return 1;
        return 0;
    case KCHK_SAMPLE: return kid_wit.samples >= a;
    default:          return 0;
    }
}

uint8_t kid_mission_pass(void)
{
    const kid_mission_t *m;
    if (!kid_wit_zero())
        return 0;
    m = &KID_MISSIONS[kid_ms_id];
    if (!kid_chk(m->check, m->c0, m->c1))
        return 0;
    return !m->check2 || kid_chk(m->check2, m->d0, m->d1);
}

/* ---- the record: two bits per mission (0 untouched, 1 started, 2 done) --- */
uint8_t kid_mission_done(uint8_t id)
{
    if (id >= KID_NMISSION)
        return 0;
    return (uint8_t)((kid_ram.mission[id >> 2] >> (2u * (id & 3u))) & 3u);
}
static void kid_mission_mark(uint8_t id, uint8_t v)
{
    uint8_t *p = &kid_ram.mission[id >> 2];
    *p = (uint8_t)((*p & ~(3u << (2u * (id & 3u)))) | (v << (2u * (id & 3u))));
    kid_dirty = 1;
    settings_later = 1;
}
/* the level is done when its three missions are: the badge, and one level up */
static void kid_level_check(uint8_t level)
{
    uint32_t id, done = 0;
    for (id = 0; id < KID_NMISSION; id++)
        if (KID_MISSIONS[id].level == level && kid_mission_done((uint8_t)id) == 2u)
            done++;
    if (done < 3u)
        return;
    if (level < 10u && kid_level() <= level)
        kid_set_level((uint8_t)(level + 1u));
    kid_ram.badges |= (uint16_t)(1u << level);
    kid_dirty = 1;
    settings_later = 1;
}

uint8_t kid_mission_finish(void)
{
    uint8_t lvl;
    if (!kid_mission_pass())
        return 0;
    kid_mission_mark(kid_ms_id, 2);
    kid_parked &= ~(1u << kid_ms_id);      /* (done is done: a parked card moves on) */
    kid_ram.sessions++;
    lvl = KID_MISSIONS[kid_ms_id].level;
    kid_level_check(lvl);
    return 1;
}

/* ---- the seeds: a recipe -> the working project ---- */
#define KID_SEED_STEPS 16u                      /* a seed's lanes and notes are 16 steps (the pattern: NSTEP) */
_Static_assert(NSTEP >= KID_SEED_STEPS, "a seed fits the pattern");
#define KID_PART_TRK {TRK_DRUM, 0u, 1u, 2u}     /* D drums, B bass (part 0), K keys (1), L lead (2) */
static const uint8_t KID_PART_TRACK[4] = KID_PART_TRK;

static void kid_seed_build(const kid_seed_t *s, project_t *p)
{
    static const uint8_t trk_of[4] = KID_PART_TRK;
    uint32_t i, k;
    for (k = 0; k < PROJ_NG; k++)
        if (k == G_BPM)
            p->g[k] = (int16_t)s->bpm;
    p->sel = (uint8_t)((s->parts & 2u) ? 0u : (s->parts & 4u) ? 1u : 2u);   /* the part she plays */
    for (i = 0; i < NTRK; i++) {
        proj_trk_t *t = &p->t[i];
        uint32_t mutes = 1;
        uint32_t j;
        for (j = 0; j < 4u; j++)
            if (trk_of[j] == i)
                mutes = !((s->parts >> j) & 1u);
        memset(&t->step, 0, sizeof t->step);
        memset(t->lock, 0, sizeof t->lock);
        for (k = 0; k < NSTEP; k++) {
            t->mf[k] = 3u << 6;                 /* 3 = the normal fill condition, no nudge */
        }
        t->p[P_MUTE] = (int16_t)mutes;
        t->p[P_LEVEL] = 900;                    /* (a level the mix expects: the chapters' default) */
    }
    p->t[TRK_DRUM].e[0] = (int16_t)s->kit;      /* P_E0: the drum kit */
    for (i = 0; i < 4u; i++) {
        uint8_t lane = s->lane_id[i];
        if (lane >= DRUM_LANES)
            continue;
        for (k = 0; k < KID_SEED_STEPS; k++)         /* (a seed names 16 steps: the pattern may be longer) */
            if ((s->lane[i] >> k) & 1u)
                p->t[TRK_DRUM].dstep[k].on[lane >> 3] |= (uint8_t)(1u << (lane & 7u));
    }
    for (k = 0; k < KID_SEED_STEPS; k++) {
        if (s->bass[k] == 0xFF)
            continue;
        p->t[0].step[k].note[0] = s->bass[k];   /* the bass is part 0 */
        p->t[0].step[k].vel = 100;
        p->t[0].step[k].n = 0;
        p->t[0].step[k].time = 0;
        p->t[0].step[k].flags = 0;
    }
    p->sum = proj_sum(p);
}

int kid_mission_start(uint8_t id)
{
    int r;
    const kid_seed_t *s;
    if (id >= KID_NMISSION)
        return -1;
    if (kid_mission_active && id == kid_ms_id)
        return 1;                               /* already up */
    if (kid_mission_active) {
        r = kid_mission_leave();
        if (r != 1)
            return r;                           /* (still stopping, or her song is not back: try again) */
    }
    rec_wait = 0;                               /* (finding 18: no REC, no free take, while a mission is up) */
    ft_on = 0;
    rec_tempo = 1;                              /* (finding 24: level 1's "REC, then play" is the grid, not a take) */
    proj_capture(&proj_tmp.v6);                 /* her song, as the seed's starting point */
    s = &KID_SEEDS[id];
    kid_seed_build(s, &proj_tmp.v6);
    r = kid_mission_enter(&proj_tmp.v6, (uint32_t)sizeof proj_tmp.v6);
    if (r != 1)
        return r;                               /* (nothing changed yet: the caller retries) */
    if (kid_mission_done(id) != 1u) {           /* a mission she has not started: fresh witnesses */
        memset(&kid_wit, 0, sizeof kid_wit);
        kid_wit.last_ms = fm1_ms;
    }
    kid_ms_id = id;
    if (kid_mission_done(id) == 0)
        kid_mission_mark(id, 1);                /* started: coming back resumes it */
    return 1;
}

int kid_mission_end(void)
{
    int r;
    if (!kid_mission_active)
        return -1;
    r = kid_mission_leave();
    if (r == 1)
        kid_ms_id = 0xFF;                       /* (the witnesses stay: a check can complete later) */
    return r;
}

/* ---- what the clubhouse and the mission screen draw ---- */
const char *kid_mission_title(uint8_t id) { return id < KID_NMISSION ? KID_MISSIONS[id].title : ""; }
const char *kid_mission_line(uint8_t id, uint32_t n)
{
    const char *t;
    if (id >= KID_NMISSION)
        return "";
    t = n ? KID_MISSIONS[id].l2 : KID_MISSIONS[id].l1;   /* ("-" in the TSV is no line) */
    return t ? t : "";
}
const char *kid_mission_hint(uint8_t id) { return id < KID_NMISSION ? KID_MISSIONS[id].hint : ""; }
uint8_t kid_mission_of_level(uint8_t level, uint8_t nth)
{
    uint32_t id;
    for (id = 0; id < KID_NMISSION; id++)
        if (KID_MISSIONS[id].level == level && !nth--)
            return (uint8_t)id;
    return 0xFF;
}

const char *kid_mission_done_line(uint8_t id) { return id < KID_NMISSION ? KID_MISSIONS[id].done : ""; }

/* ---- S6: the clubhouse plays it ---------------------------------------- */
/* Her card, then the ladder (curriculum §7): SEQ shows the hint, SEQ again plays the step while
 * she listens, SEQ again offers to park it. Nothing is marked failed and nothing is lost (a
 * parked mission comes back at the next power-on). The reward is informational and brief
 * (plan §5): the header names what she made, the band row lights up, the level's dots fill in.
 * No score, no streak, no "well done" for pressing a button. */

uint8_t kid_mission_current(void)
{
    uint32_t id;
    for (id = 0; id < KID_NMISSION; id++)        /* a parked one is offered first: it stays open */
        if (KID_MISSIONS[id].level <= kid_level() && ((kid_parked >> id) & 1u) &&
            kid_mission_done((uint8_t)id) != 2u)
            return (uint8_t)id;
    for (id = 0; id < KID_NMISSION; id++)
        if (KID_MISSIONS[id].level <= kid_level() && kid_mission_done((uint8_t)id) != 2u)
            return (uint8_t)id;
    return 0xFF;
}
void kid_mission_skip(void) { kid_parked |= 1u << kid_ms_id; }
uint8_t kid_mission_hint_rung(void) { return kid_rung; }
uint8_t kid_mission_demo(void) { return kid_demo_t != 0; }

void kid_mission_hint_next(void)     /* SEQ: the next rung (curriculum §7) */
{
    uint8_t id = kid_ms_id < KID_NMISSION ? kid_ms_id : kid_mission_current();
    if (id >= KID_NMISSION)
        return;
    if (kid_rung < 4u)
        kid_rung++;
    ui.force = 1;
    if (kid_rung == 2u) {                  /* hear it again: and for a drum mission, the step itself */
        if (KID_MISSIONS[id].check == KCHK_LANE && kid_ms_id == id &&
            KID_MISSIONS[id].c0 < DRUM_LANES) {
            uint32_t i, lane = KID_MISSIONS[id].c0;
            for (i = 0; i < 16u; i++)          /* (the check's mask is 16 steps wide) */
                if (((KID_MISSIONS[id].c1 >> i) & 1u) &&
                    !((TDRUM->dstep[i].on[lane >> 3] >> (lane & 7u)) & 1u)) {
                    TDRUM->dstep[i].on[lane >> 3] |= (uint8_t)(1u << (lane & 7u));
                    kid_demo_mask |= 1ull << i;      /* (only the steps it added: hers stay) */
                }
            kid_demo_lane = (uint8_t)lane;
            kid_demo_t = fm1_ms ? fm1_ms : 1u;
        }
        transport_req = 1;                 /* LISTEN: a demonstration, not a skip */
        ui_message("LISTEN");
    } else if (kid_rung == 3u) {
        ui_message("TAP SEQ: SKIP");
    } else if (kid_rung >= 4u) {
        kid_mission_skip();                /* parked, not failed: it comes back at power-on */
        kid_rung = 0;
        if (kid_ms_id < KID_NMISSION)
            kid_req = 2;                   /* leave it (her song back) and offer the next */
        ui_message("SKIPPED");
    }
}

/* the demonstration takes its loan back after a bar or two */
static void kid_demo_tick(uint32_t now)
{
    uint32_t i;
    if (!kid_demo_t || now - kid_demo_t < 2000u)
        return;
    kid_demo_t = 0;
    for (i = 0; i < NSTEP; i++)
        if ((kid_demo_mask >> i) & 1u)
            TDRUM->dstep[i].on[kid_demo_lane >> 3] &= (uint8_t)~(1u << (kid_demo_lane & 7u));
    kid_demo_mask = 0;
    transport_req = 2;
    ui.force = 1;
}

/* once a frame: what PLAY and HOME asked for, the demonstration's loan, the completion, and the
 * messages that must wait for the header (finding 28/32: ui.msg is 24 B and kid screens show it) */
static void kid_clock(uint32_t now)
{
    uint8_t id, before;
    int r;
    kid_demo_tick(now);
    if (kid_safety_m && !ui.msg_t) {
        kid_safety_m = 0;
        ui_message("SEE? GONE.");
    }
    if (kid_say2[0] && !ui.msg_t) {
        ui_message(kid_say2);
        kid_say2[0] = 0;
    }
    if (kid_req == 1u) {                       /* PLAY: start (or resume) the mission on the card */
        id = kid_mission_current();
        if (id >= KID_NMISSION) {
            kid_req = 0;
        } else {
            r = kid_mission_start(id);
            if (r) {
                kid_req = 0;
                kid_rung = 0;
                if (r == 1)
                    transport_req = 1;         /* so she hears it straight away */
                else
                    ui_message("TRY AGAIN");
                ui.force = 1;
            }                                  /* (0: still stopping: next frame) */
        }
    } else if (kid_req == 2u) {                /* HOME: back to the clubhouse, her song back */
        r = kid_mission_end();
        if (r) {
            kid_req = 0;
            kid_rung = 0;
            ui.force = 1;
        }
    }
    if (kid_ms_id >= KID_NMISSION || kid_demo_t) {   /* (a loan is not her work: it cannot pass) */
        kid_pass_t = 0;
        return;
    }
    if (kid_mission_done(kid_ms_id) == 2u || !kid_mission_pass()) {
        kid_pass_t = 0;
        return;
    }
    if (!kid_pass_t) {
        kid_pass_t = now ? now : 1u;           /* (a moment to hear it land) */
        return;
    }
    if (now - kid_pass_t < 600u)
        return;
    kid_pass_t = 0;
    before = kid_level();
    if (!kid_mission_finish())
        return;
    ui_message(kid_mission_done_line(kid_ms_id));   /* what she made, not "well done" */
    if (kid_level() != before) {                    /* the door that opened, said next */
        uint32_t n = 6;
        str_cpy(kid_say2, "LEVEL ", sizeof kid_say2);
        fmt_int(kid_say2 + 6, (int32_t)kid_level());
        while (kid_say2[n])
            n++;
        str_cpy(kid_say2 + n, " IS OPEN", sizeof kid_say2 - n);
    }
    ui.force = 1;
}
#endif
