/* SPDX-License-Identifier: GPL-3.0-only */
/* SLOOP JR: the gate (docs/KID-MODE-STAGES.md S1). The level -> unlock tables
 * are the plan's §4 (one new control per level); every predicate is a pure
 * function of the level, so the gate matrix test (tests/kid_test.c) can pin
 * them. Locked means unreachable, never deleted, never rewritten: a project
 * restore (proj_apply) is deliberately not gated. */
#if FELUCCA_KID
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
static const char *const KID_LVL_HINT[10] = {   /* SEQ on the clubhouse (the S5 ladder replaces these) */
    "WHITE KEYS = DRUMS", "SNARE 2 AND 4", "BASS: ROOT NOTES", "CHORDS: FOLLOW BASS", "KNOB 1: FILTER",
    "DELAY + REVERB", "LEAD OVER CHORDS", "PLUCK + BOW STRINGS", "SECTIONS A-D", "FM6 + YOUR SAMPLES",
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
    uint32_t sig = kid_level() * 131u + (ui.msg_t ? str_hash(7u, ui.msg) : 0u) +
                   (uint32_t)song.playing * 17u + (uint32_t)song.rec * 257u;
    uint32_t i, lvl = kid_level();
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
        str_cpy(b, "LEVEL ", sizeof b);
        fmt_int(b + 6, (int32_t)lvl);
        cv_text(4, 1, &FONT_S, b, C_HI);
        cv_text(236 - text_w(&FONT_S, "SLOOP JR"), 1, &FONT_S, "SLOOP JR", C_GRAY);
    }
    cv_blit(0, Y_HEAD);
    cv_begin(240, H_GRAPH, C_BLACK);                                  /* band 2: the body */
    cv_text(4, 4, &FONT_S, KID_LVL_NAME[lvl - 1u], C_WHITE);
    for (i = 0; i < 4u; i++) {                                        /* the tiles: lit = unlocked */
        int32_t x = 4 + (int32_t)i * 59;
        uint16_t col = TE_COL[KID_TILE_TRK[i] & 3u];
        int on = kid_track_ok(KID_TILE_TRK[i]);
        cv_rect(x, 30, 55, 26, on ? col : C_LINE);
        cv_text(x + 4, 36, &FONT_S, KID_TILE[i], on ? C_WHITE : C_DIM);
    }
    for (i = 0; i < 10u; i++) {                                       /* the level dots: the current one lit */
        int32_t x = 12 + (int32_t)i * 23;
        cv_rect(x, 68, 14, 14, i + 1u == lvl ? C_HI : i + 1u < lvl ? C_WHITE : C_LINE);
    }
    cv_text(4, 92, &FONT_S, KID_LVL_HINT[lvl - 1u], C_GRAY);
    cv_blit(0, Y_GRAPH);
    lcd_fill(0, Y_FOOT, 240, 240 - Y_FOOT, C_BLACK);                  /* band 3: the footer */
    lcd_fill(0, Y_FOOT, 240, 1, C_LINE);
    cv_begin(240, 240 - Y_FOOT, C_BLACK);
    cv_text(4, 6, &FONT_S, "SEQ: HELP", C_GRAY);
    cv_text(236 - text_w(&FONT_S, "HOME: FREE PLAY"), 6, &FONT_S, "HOME: FREE PLAY", C_GRAY);
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
    (void)notes;   /* keys are the instrument's; on the clubhouse they do nothing (S5: missions) */

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
        if (pressed & (1u << panel.btn[B_SEQ])) {          /* SEQ: the hint */
            ui_message(KID_LVL_HINT[kid_level() - 1u]);
            enc_drop();
            return 1;
        }
        if (pressed & (1u << panel.btn[B_EDIT])) {         /* EDIT: undo (one button) */
            ui_message(undo_swap(0) ? "UNDO" : "NOTHING TO UNDO");
            enc_drop();
            return 1;
        }
        if (home == BT_TAP) {                              /* HOME tap: the sandbox (free play) */
            ui.page = (uint8_t)page_first(FAM_TRK);
            ui.fam_last[FAM_TRK] = ui.page;
            ui.home = 0;
            page_entered();
            enc_drop();
            return 1;
        }
        if (pressed & ~(play_rec | home_bit)) {            /* any other button: NOT YET (HOME is the tap above) */
            kid_refuse();
            enc_drop();
            return 1;
        }
        if (pressed & play_rec)                            /* PLAY / REC: as stock */
            return 0;
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
#endif
