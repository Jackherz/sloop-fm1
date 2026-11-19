/* SPDX-License-Identifier: GPL-3.0-only */
/* SLOOP JR (docs/KID-MODE-STAGES.md S1): the gate matrix. The real UI compiled
 * with FELUCCA_KID=1 (kid.h before hostsim.c, kid.c after storage.c). For
 * every level 1..10: which pages are reachable, which engines / tracks /
 * layers / kits, and the button table (a locked layer gets no ly_bit, REC
 * held never clears, MIDI Program Change is ignored, the song page cannot be
 * opened from the drum grid). Modelled on tests/ui_pages_test.c. */
#define FELUCCA_KID 1
#include "../firmware/src/kid.h"
#define FELUCCA_ARRANGER 1
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>
static uint16_t screen[240*240];
static void lcd_sync(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{ uint32_t i,j; assert(x+w<=240 && y+h<=240); for(j=0;j<h;j++) for(i=0;i<w;i++) screen[(y+j)*240+x+i]=p[j*w+i]; }
#include "../firmware/src/gfx.c"
static void lcd_fill(uint32_t x,uint32_t y,uint32_t w,uint32_t h,uint16_t c)
{ uint32_t i,j; assert(x+w<=240&&y+h<=240); for(j=0;j<h;j++)for(i=0;i<w;i++)screen[(y+j)*240+x+i]=swap16(c); }
static int32_t encs[7];
static uint32_t fm1_ticks(void) { return fm1_ms * 1000u * 24u; }
#define FM1_TICKS_PER_US 24u
static int32_t enc_late[7];                         /* a detent the timer ISR counts just after a read (as on the FM-1, where the
                                                     * scan runs while a UI pass does): it lands in the same pass */
static int32_t fm1_enc_take(uint32_t e) { int32_t s = encs[e]; encs[e] = enc_late[e]; enc_late[e] = 0; return s; }
static uint8_t fm1_led[16], fm1_led_dim[16], fm1_led_bg[16];
static volatile uint16_t fm1_led_bg_ns;
#define FM1_NCOL 16u
static const int8_t FM1_KEYMAP[5][16];
static void fm1_led_key(uint32_t id, int on) { (void)id; (void)on; }
static uint32_t edges_btn, notes_seen;
static uint32_t fm1_input_edges(int x) { uint32_t e = edges_btn; (void)x; edges_btn = 0; return e; }
static uint32_t fm1_input_note_edges(void) { uint32_t e = fm1_in.notes & ~notes_seen; notes_seen = fm1_in.notes; return e; }
static uint32_t wdt_tick_ms;                       /* (stress_test: 1, the blocking PANEL setup's clock runs out) */
static void fm1_wdt_feed(void) { fm1_ms += wdt_tick_ms; }
static int32_t fm1_adc_read(int c) { (void)c; return -1; }
static struct { uint32_t magic, stage, page, home, ui_frames; } felucca_dbg;
#define FELUCCA_ICONS 1
#define SCOPE_N 512u
static int16_t scope_buf[SCOPE_N], scope_bufr[SCOPE_N];
static uint32_t scope_w;
#include "../firmware/src/panel.c"
#include "../firmware/src/ui.c"
static uint32_t saves, loads, erases;
/* project_used: project.c (S4) */
/* project_save / project_load / project_erase / arrangement_save / section_store /
 * section_load: project.c (S4: included, so the autosave snapshot is real).
 * arrangement_ready / arrangement_apply / section_bars / song_backup / song_restore:
 * arranger_scene.c (included by project.c). */
static int up_used(uint32_t k) { return k < 2; }
static int up_load(uint32_t k) { (void)k; return 0; }
static int up_load_to(track_t *t, uint32_t k) { (void)t; (void)k; return 0; }
static int up_arp_of(uint32_t k, int16_t *a) { (void)k; (void)a; return 1; }
static uint32_t up_count(void) { return 2; }
static uint32_t up_nth(uint32_t n) { return n; }
static uint32_t up_rank(uint32_t s) { return s; }
static void up_name(uint32_t k, char *b) { str_cpy(b, k ? "MY PAD" : "MY LEAD", 13); }
static void up_slot_label(char *b, uint32_t k) { fmt_int(b, (int32_t)k + 1); }
static void up_ui(uint32_t op, uint32_t k) { (void)op; (void)k; }
#include "../firmware/src/ui_song.c"
#include "../firmware/src/ui_studio.c"
#include "../firmware/src/icons.c"
#include "../firmware/src/ui_draw.c"
#include "../firmware/src/ui_vis.c"
#include "../firmware/src/ui_layers.c"
#include "../firmware/src/ui_menu.c"
#include "../firmware/src/ui_input.c"
#include "../firmware/src/splash.c"
/* (2.7) the theme's CRC: storage.c's st_crc32. S4: a RAM NOR flash, so the
 * autosave object (OBJ_AUTOSAVE) really saves and loads (the snapshot test) */
#define FELUCCA_FLASH 1
static uint8_t flash_ok = 1;   /* (felucca.c: the JEDEC id matched) */
static uint32_t fl_jedec_ram(void) { return 0x856014u; }   /* persist_boot (unused here: flash_ok is preset) */
#define FL_FAR(fn) (fn)                  /* (fm1_flash.h: a plain call on the host) */
#define FL_JEDEC_OK(id) ((id) == 0x856014u || (id) == 0xB36014u)
static uint8_t nor[0x100000];  /* 1 MiB of NOR */
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    for (i = 0; i < n; i++)
        nor[off + i] &= s[i];   /* NOR: programming only clears bits */
    return 0;
}
#include "../firmware/src/storage.c"
#include "../firmware/src/project.c"
#include "../firmware/src/kid.c"
/* (2.7) the editor's LIVE page (firmware/src/editor_live.c): its reply captured here */
static uint8_t lv_out[96];
static uint32_t lv_n;
static void ed_b(uint32_t v) { if (lv_n < sizeof lv_out) lv_out[lv_n++] = (uint8_t)(v & 0x7Fu); }
static void ed_v(int32_t v) { uint32_t u = (uint32_t)(clamp(v, -8192, 8191) + 8192); ed_b(u); ed_b(u >> 7); }
#include "../firmware/src/editor_live.c"
static void live(uint32_t op, uint32_t x, uint32_t y, uint32_t z)
{
    uint8_t a[4] = {(uint8_t)op, (uint8_t)x, (uint8_t)y, (uint8_t)z};
    lv_n = 0;
    ed_live(a, 4);
}
static void live5(uint32_t op, uint32_t x, uint32_t y, uint32_t z, uint32_t w)
{
    uint8_t a[5] = {(uint8_t)op, (uint8_t)x, (uint8_t)y, (uint8_t)z, (uint8_t)w};
    lv_n = 0;
    ed_live(a, 5);
}
static const char *outdir;
static void ppm(const char *name) {
    char path[512]; snprintf(path,sizeof path,"%s/%s.ppm",outdir,name);
    FILE *f=fopen(path,"wb"); assert(f); fprintf(f,"P6\n240 240\n255\n");
    for(unsigned i=0;i<240*240;i++) { uint16_t p=swap16(screen[i]); uint8_t rgb[3]={(p>>11)*255/31,((p>>5)&63)*255/63,(p&31)*255/31}; fwrite(rgb,1,3,f); }
    fclose(f);
}
/* one UI frame (~16 ms): the audio between (as the ISR does), then input, LEDs, draw */
static void frame(void)
{
    uint32_t q;
    static int32_t o[CTL * 2];
    for (q = 0; q < 22u; q++) {
        uint32_t i;
        mix_block(o, CTL);
        for (i = 1; i < CTL; i += 2u) {             /* the scope, as audio.c fills it (the visualiser) */
            scope_bufr[scope_w & (SCOPE_N - 1u)] = vis_tap[2u * i + 1u];
            scope_buf[scope_w++ & (SCOPE_N - 1u)] = vis_tap[2u * i];
        }
    }
    ui_input(); ui_leds(); ui_draw(); fm1_ms += 16;
}
static void frames(uint32_t n) { while (n--) frame(); }
/* the transport stopped and the stop serviced (project_apply() asks for one) */
static void settle(void) { uint32_t k; for (k = 0; k < 400u && (song.playing || transport_req); k++) frame(); }
static uint32_t BT(uint32_t b) { return 1u << panel.btn[b]; }
static void press(uint32_t b) { edges_btn |= BT(b); fm1_in.buttons |= BT(b); frame(); }
static void release(uint32_t b) { fm1_in.buttons &= ~BT(b); frame(); }
static void tap(uint32_t b) { press(b); release(b); }
static void key(uint32_t k) { fm1_in.notes |= 1u << k; frame(); fm1_in.notes &= ~(1u << k); frame(); }
static int fails;
static void check(int ok, const char *what) { printf("ui: %-74s %s\n", what, ok ? "ok" : "FAIL"); fails += !ok; }
static const char *sub_line(void) { return layer_sub_shown; }   /* the layer's sub line as last drawn (ui_layers.c) */


/* the plan's §4, written out per page (PAGES[] index; 0 = never: TOOLS) */
static const uint8_t WANT_PAGE_LVL[40] = {
    5, 5, 10, 10, 10, 6, 6, 6, 6, 6, 6, 6, 4, 4, 5, 5, 5, 5, 5,
    1, 10, 10, 1, 10, 10, 10, 10, 10, 3, 9, 9, 0, 7, 7, 9, 9, 9, 1, 1,
    1,   /* SLOOP JR (the clubhouse: always reachable, it is home) */
};
static const uint8_t WANT_ENG_LVL[13] = {3, 3, 3, 3, 10, 7, 7, 7, 8, 10, 8, 8, 10};
static int page_idx(const char *title, uint32_t fam)
{
    uint32_t i;
    for (i = 0; i < NPAGES; i++)
        if (!strcmp(PAGES[i].title, title) && PAGES[i].fam == fam)
            return (int)i;
    return -1;
}
static int msg_is_refusal(void) { return !strncmp(ui.msg, "NOT YET", 7); }


/* ---- the mission engine's harness (kid tests 4 and 5) ---- */
/* (KID_PART_TRACK: kid.c's D/B/K/L -> track table, one definition) */
static uint32_t r5;                                                  /* test 5's start result */
static int end_mission(void)                                          /* (leave defers while it stops) */
{
    uint32_t k;
    for (k = 0; k < 400u; k++) {
        int r = kid_mission_end();
        if (r != 0)
            return r;
        frame();
    }
    return -1;
}

/* the first n keys of the panel's key bits (14..40) */
static void keys_n(uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        kid_mission_tick(1u << (14u + i), fm1_ms);
}
/* one step of a check, short of its boundary */
static int just_before(uint8_t kind, uint32_t a, uint32_t b)
{
    switch (kind) {
    case KCHK_KEYS:   keys_n(a - 1u); return kid_chk(kind, a, b);
    case KCHK_BARS:   kid_wit.bars = (uint8_t)(a - 1u); return kid_chk(kind, a, b);
    case KCHK_PARAMS: for (uint32_t i = 0; i + 1u < a; i++) kid_wit_param((uint32_t)P_ATK + i); return kid_chk(kind, a, b);
    case KCHK_PRESET: kid_wit.presets = (uint8_t)(a - 1u); return kid_chk(kind, a, b);
    case KCHK_SAVED:  kid_wit.saves = (uint8_t)(a - 1u); return kid_chk(kind, a, b);
    case KCHK_WROTE:  { track_t *t = &trk[a]; for (uint32_t i = 0; i + 1u < b; i++)
                            t->step[i].note[0] = (uint8_t)(48u + i), t->step[i].vel = 100; }
                      return kid_chk(kind, a, b);
    case KCHK_LANE:   return kid_chk(kind, a, b);   /* (the seed's lanes are the "before" state) */
    case KCHK_NOTEPC: { track_t *t = &trk[a]; uint32_t pc = 0;   /* one class short */
                        for (uint32_t i = 0; i < 12u; i++) if ((b >> i & 1u) && !pc) pc = i + 1u;
                        t->step[0].note[0] = (uint8_t)(48u + pc - 1u); t->step[0].vel = 100; }
                      return kid_chk(kind, a, b);
    default:          return 0;                    /* (a binary check has no boundary below it) */
    }
}
/* install exactly what the check asks for */
static void satisfy(uint8_t kind, uint32_t a, uint32_t b)
{
    switch (kind) {
    case KCHK_HONOUR: return;
    case KCHK_KEYS:   keys_n(a); return;
    case KCHK_LANE:   for (uint32_t i = 0; i < 16u; i++)   /* (the check's mask is 16 bits) */
                          if (b >> i & 1u) TDRUM->dstep[i].on[a >> 3] |= (uint8_t)(1u << (a & 7u));
                      return;
    case KCHK_WROTE:  { track_t *t = &trk[a]; for (uint32_t i = 0; i < b; i++)
                            t->step[i].note[0] = (uint8_t)(48u + i), t->step[i].vel = 100; }
                      return;
    case KCHK_NOTEPC: { track_t *t = &trk[a]; uint32_t j = 0;
                        for (uint32_t i = 0; i < 12u; i++)
                            if (b >> i & 1u) t->step[j].note[0] = (uint8_t)(48u + i), t->step[j++].vel = 100; }
                      return;
    case KCHK_BARS:   kid_wit.bars = (uint8_t)a; return;
    case KCHK_PARAM:  kid_wit_param(a); return;
    case KCHK_PARAMS: for (uint32_t i = 0; i < a; i++) kid_wit_param(a ? (uint32_t)P_ATK + i : 0u); return;
    case KCHK_PRESET: kid_wit.presets = (uint8_t)a; return;
    case KCHK_LAYER:  kid_wit_layer(a); return;
    case KCHK_SAVED:  kid_wit.saves = (uint8_t)a; return;
    case KCHK_UNDONE: kid_wit_undone(); return;
    case KCHK_ENGINE: trk[0].eng_req = (uint8_t)a; return;
    case KCHK_SAMPLE: kid_wit_sample(); return;
    default: return;
    }
}
/* clear a check's witness again (the second check's boundary test) */
static void undo_kind(uint8_t kind, uint32_t a, uint32_t b)
{
    switch (kind) {
    case KCHK_HONOUR: return;                        /* (cannot be taken away) */
    case KCHK_KEYS:   kid_wit.keys = 0; return;
    case KCHK_LANE:   for (uint32_t i = 0; i < 16u; i++)
                          TDRUM->dstep[i].on[a >> 3] &= (uint8_t)~(1u << (a & 7u));
                      return;
    case KCHK_WROTE:  memset(trk[a].step, 0, sizeof trk[a].step); return;
    case KCHK_NOTEPC: memset(trk[a].step, 0, sizeof trk[a].step); return;
    case KCHK_BARS:   kid_wit.bars = 0; return;
    case KCHK_PARAM:  kid_wit.pmask[0] = kid_wit.pmask[1] = kid_wit.pmask[2] = kid_wit.gmask = 0; return;
    case KCHK_PARAMS: kid_wit.pmask[0] = kid_wit.pmask[1] = kid_wit.pmask[2] = kid_wit.gmask = 0; return;
    case KCHK_PRESET: kid_wit.presets = 0; return;
    case KCHK_LAYER:  kid_wit.lmask = 0; return;
    case KCHK_SAVED:  kid_wit.saves = 0; return;
    case KCHK_UNDONE: kid_wit.undone = 0; return;
    case KCHK_ENGINE: for (uint32_t i = 0; i < NPART; i++) trk[i].eng_req = (uint8_t)i; return;
    case KCHK_SAMPLE: kid_wit.samples = 0; return;
    default: return;
    }
}

/* S6: one mission played from the clubhouse, as she would: PLAY, do what the card
 * says, and the header that follows (the clubhouse input path, not the engine's) */
static int play_one(uint8_t id, int *line_ok)
{
    const kid_mission_t *m = &KID_MISSIONS[id];
    uint32_t t;
    tap(B_PLAY);
    for (t = 0; t < 300u && !(kid_mission_active && kid_ms_id == id); t++)
        frame();
    if (!(kid_mission_active && kid_ms_id == id))
        return 0;
    song.playing = 1;                       /* (a knob counts only while it plays) */
    satisfy(m->check, m->c0, m->c1);
    if (m->check2)
        satisfy(m->check2, m->d0, m->d1);
    song.playing = 0;
    for (t = 0; t < 600u && kid_mission_done(id) != 2u; t++)
        frame();
    if (line_ok)
        *line_ok = kid_mission_done(id) == 2u &&
                   !strncmp(ui.msg, kid_mission_done_line(id), strlen(kid_mission_done_line(id)));
    return kid_mission_done(id) == 2u;
}
/* HOME on the clubhouse: a running mission ends, her song comes back */
static int leave_mission(void)
{
    uint32_t t;
    tap(B_HOME);
    for (t = 0; t < 300u && kid_mission_active; t++)
        frame();
    return !kid_mission_active && kid_on_screen();
}

int main(void)
{
    uint32_t i, l, lvl;
    panel = PANEL_DEFAULT;
    layers_init();
    settings.palette = 4;
    palette_set(4);
    host_tracks_init();
    for (i = 0; i < NPART; i++) { set_engine_of(&trk[i], TRK_DEF[i][0]); apply_preset_to(&trk[i], TRK_DEF[i][1]); trk[i].engine = trk[i].eng_req; }
    TDRUM->p[P_E0] = DRUM_DEFAULT_KIT;
    bank_resolve();
    check(NPAGES == 40, "40 pages (the gate table is one level per page)");

    /* the level clamps to 1..10 */
    kid_set_level(0); check(kid_level() == 1, "level 0 clamps to 1");
    kid_set_level(11); check(kid_level() == 10, "level 11 clamps to 10");

    /* 1: the gate matrix, levels 1..10 */
    song.sel = 0; TSEL->eng_req = 0;   /* ANALOG: EDIT > FILTER is shown, so page_shown == kid_page_ok */
    for (lvl = 1; lvl <= 10; lvl++) {
        int ok = 1;
        kid_set_level((uint8_t)lvl);
        for (i = 0; i < NPAGES; i++) {
            uint8_t want = WANT_PAGE_LVL[i] && lvl >= WANT_PAGE_LVL[i];
            ok &= kid_page_ok(i) == want;
            ok &= page_shown(&PAGES[i]) == want;
        }
        for (i = 0; i < NENGINES; i++)
            ok &= kid_engine_ok(i) == (lvl >= WANT_ENG_LVL[i]);
        ok &= kid_engine_ok(NENGINES) == (lvl >= 9);   /* user presets */
        ok &= kid_track_ok(TRK_DRUM) == 1;
        ok &= kid_track_ok(0) == (lvl >= 3);
        ok &= kid_track_ok(1) == (lvl >= 4);
        ok &= kid_track_ok(2) == (lvl >= 7);
        ok &= kid_kit_max() == (lvl <= 1 ? 2u : lvl == 2 ? 8u : DRUM_KITS - 1u);
        {   char b[40]; snprintf(b, sizeof b, "gate matrix at level %u", lvl); check(ok, b); }
    }
    check(page_idx("TOOLS", FAM_SAVE) >= 0 && !kid_page_ok((uint32_t)page_idx("TOOLS", FAM_SAVE)),
          "TOOLS (NEW LOOP) is never reachable in kid mode");

    /* 2: page_first always returns a shown page; page_fix settles */
    for (lvl = 1; lvl <= 10; lvl++) {
        int ok = 1;
        kid_set_level((uint8_t)lvl);
        for (l = 0; l < FAM_COUNT; l++) {
            uint32_t pf = page_first(l);
            ok &= pf < NPAGES && page_shown(&PAGES[pf]);
            ok &= PAGES[pf].fam == l || PAGES[pf].fam == FAM_TRK;   /* the family, or the fallback */
        }
        for (i = 0; i < NPAGES; i++) if (!kid_page_ok(i)) {
            ui.page = (uint8_t)i; ui.home = 0; page_fix();
            ok &= page_shown(&PAGES[ui.page]);
        }
        {   char b[40]; snprintf(b, sizeof b, "page_first / page_fix at level %u", lvl); check(ok, b); }
    }

    /* 3: open_family refuses a fully gated family, with the one-line message */
    kid_set_level(1);
    ui.page = (uint8_t)page_idx("TRACKS", FAM_TRK); ui.home = 0;
    open_family(FAM_ENV);
    check(ui.page == (uint8_t)page_idx("TRACKS", FAM_TRK) && msg_is_refusal(),
          "L1: ENV button refuses (NOT YET), the page does not move");
    kid_set_level(5);
    open_family(FAM_ENV);
    check(PAGES[ui.page].fam == FAM_ENV, "L5: ENV opens");
    kid_set_level(1);
    open_family(FAM_TRK);
    check(PAGES[ui.page].fam == FAM_TRK, "L1: TRACKS opens");

    /* 4: values: engines and presets */
    song.sel = 0; TSEL->eng_req = 0;
    kid_set_level(1); select_engine(5);
    check(TSEL->eng_req == 0 && msg_is_refusal(), "L1: select_engine(FORMANT) refuses");
    kid_set_level(7); select_engine(5);
    check(TSEL->eng_req == 5, "L7: select_engine(FORMANT) works");
    kid_set_level(3); select_engine(4);
    check(TSEL->eng_req == 5, "L3: select_engine(SAMPLE) refuses");
    kid_set_level(10); select_engine(9);
    check(TSEL->eng_req == 9, "L10: select_engine(FM6) works");
    {   /* a factory preset on a locked engine */
        uint32_t n, k, locked = 0xFFFFFFFFu;
        for (n = 0; n < NBANK; n++) if (preset_at(n, &k) == ENGI_FM6) { locked = n; break; }
        check(locked != 0xFFFFFFFFu, "found an FM6 preset in the list");
        song.sel = 0; TSEL->eng_req = 0;
        kid_set_level(7); preset_go(locked);
        check(TSEL->eng_req == 0 && msg_is_refusal(), "L7: an FM6 preset refuses");
        kid_set_level(10); preset_go(locked);
        check(TSEL->eng_req == ENGI_FM6, "L10: an FM6 preset loads");
    }

    /* 5: tracks: locked tracks are skipped in the direction of travel */
    kid_set_level(1); song.sel = TRK_DRUM; track_select(2);
    check(song.sel == TRK_DRUM, "L1: ALGORITHM cannot leave the drums");
    track_select(0);
    check(song.sel == TRK_DRUM, "L1: ALGORITHM the other way cannot leave the drums either");
    kid_set_level(3); song.sel = TRK_DRUM; track_select(2);
    check(song.sel == 0, "L3: ALGORITHM left from the drums lands on track 1 (skipping track 3)");
    kid_set_level(4); song.sel = 0; track_select(1);
    check(song.sel == 1, "L4: track 2 selects");
    kid_set_level(7); song.sel = 1; track_select(2);
    check(song.sel == 2, "L7: track 3 selects");

    /* 6: MIDI Program Change is ignored (and its requests dropped) */
    for (lvl = 1; lvl <= 10; lvl++) {
        uint32_t e0;
        kid_set_level((uint8_t)lvl);
        song.sel = 0; TSEL->eng_req = 0; e0 = TSEL->eng_req;
        pc_req[0] = 5; pc_poll();
        check(pc_req[0] == 0 && TSEL->eng_req == e0, "MIDI PC ignored");
        if (pc_req[0] || TSEL->eng_req != e0) break;
    }

    /* 7: the drum kit writers clamp to the level's kits */
    kid_set_level(1); song.sel = TRK_DRUM; TDRUM->p[P_E0] = 2;
    ui.page = (uint8_t)page_idx("DRUMS", FAM_TRK); ui.home = 0; ui.menu = 0;
    encs[panel.enc[EN_PRESET]] = 5; frame();
    check(TDRUM->p[P_E0] == 2, "L1: PRESETS on the DRUMS page clamps to kit 3");
    ui.page = (uint8_t)page_idx("TRACKS", FAM_TRK); ui.home = 0;
    encs[panel.enc[EN_PRESET]] = 5; frame();
    check(TDRUM->p[P_E0] == 2, "L1: PRESETS on TRACKS clamps to kit 3");
    kid_set_level(3);
    encs[panel.enc[EN_PRESET]] = 5; frame();
    check(TDRUM->p[P_E0] == 7, "L3: all kits: PRESETS turns past kit 3");

    /* 8: the button table: locked layers get no ly_bit; REC held is blocked */
    for (lvl = 1; lvl <= 10; lvl++) {
        int ok = 1;
        kid_set_level((uint8_t)lvl);
        kid_apply_layers();
        for (l = LY_FX; l < LY_COUNT; l++) {
            ok &= (ly_bit[l] == 0) == !kid_layer_ok(l);
            fm1_in.buttons = 1u << panel.btn[LAYER_BTN[l]];
            ly_lock = LY_PLAY;
            ok &= layer_now() == (kid_layer_ok(l) ? l : LY_PLAY);
        }
        ok &= kid_btn_blocked(B_REC) == 1;
        ok &= kid_btn_blocked(B_FX) == !kid_layer_ok(LY_FX);
        ok &= kid_btn_blocked(B_SAVE) == !kid_layer_ok(LY_SONG);
        ok &= kid_btn_blocked(B_PLAY) == 0 && kid_btn_blocked(B_HOME) == 0;
        {   char b[40]; snprintf(b, sizeof b, "button table at level %u", lvl); check(ok, b); }
    }
    fm1_in.buttons = 0;

    /* 9: REC held never clears the track */
    {
        uint8_t snap[sizeof TDRUM->dstep];
        uint32_t rb = 1u << panel.btn[B_REC];
        kid_set_level(1);
        song.sel = TRK_DRUM; song.rec = 0; rec_wait = 0; ui.hold_kind = 0;
        ui.page = (uint8_t)page_idx("TRACKS", FAM_TRK); ui.home = 0;
        memcpy(snap, TDRUM->dstep, sizeof snap);
        fm1_in.buttons = rb;
        holds_input(rb, 1000);                       /* the press: rec_toggle arms */
        holds_input(0, 1000 + 700 + 100);            /* held past 0.7 s: stock arms the clear */
        holds_input(0, 1000 + 700 + 1300 + 100);     /* held to the end: stock clears */
        check(ui.hold_kind == 0 && !memcmp(snap, TDRUM->dstep, sizeof snap),
              "REC held 2 s: no clear ring, the pattern is untouched");
        fm1_in.buttons = 0;
    }

    /* 10: the song page cannot be opened from the drum grid */
    kid_set_level(1);
    ui.page = (uint8_t)page_idx("DRUMS", FAM_TRK); ui.home = 0; song.sel = TRK_DRUM;
    drum_screen_input(1u << panel.btn[B_SAVE], 0, 0);
    check(PAGES[ui.page].scope != SC_SONG && msg_is_refusal(),
          "L1: SAVE on the drum grid does not open the song page");
    studio_open(SC_SONG);
    check(PAGES[ui.page].scope != SC_SONG, "L1: studio_open(SC_SONG) refuses");
    kid_set_level(9);
    studio_open(SC_SONG);
    check(PAGES[ui.page].scope == SC_SONG, "L9: studio_open(SC_SONG) opens");
    studio_open(SC_DRUM);
    check(PAGES[ui.page].scope == SC_DRUM, "L9: studio_open(SC_DRUM) opens");

    /* 11: kid_init: REMOTE off, the drums first */
    kid_set_level(1);
    song.g[G_RMCH] = 5; song.sel = 0;
    kid_init();
    check(song.g[G_RMCH] == 0 && song.sel == TRK_DRUM && ly_bit[LY_FX] == 0,
          "kid_init at L1: REMOTE off, the drums selected, FX layer locked");
    kid_set_level(10);
    song.sel = 0;
    kid_init();
    check(song.sel == 0 && ly_bit[LY_FX] != 0, "kid_init at L10: track 1 kept, FX layer open");

    /* 12: the progress record (S2): round trip, torn write, stock compat */
    check(sizeof(kid_rec_t) == 24, "the progress record is 24 B");
    kid_reset();
    check(kid_level() == 1, "first boot with no record: level 1");
    for (lvl = 1; lvl <= 10; lvl++) {
        kid_rec_t r;
        kid_set_level((uint8_t)lvl);
        check(kid_dirty == 1, "a level change marks the record dirty (saved when quiet)");
        kid_dirty = 0;
        kid_fill(&r);
        check(r.magic == KID_MAGIC && r.level == lvl, "the record carries the level");
        kid_reset();   /* a fresh boot */
        kid_take(&r);
        {   char b[40]; snprintf(b, sizeof b, "round trip at level %u", lvl); check(kid_level() == lvl, b); }
    }
    {   /* a torn write fails the CRC: the previous copy stands */
        kid_rec_t r;
        kid_set_level(4); kid_fill(&r);
        r.level = 9;   /* torn: the level changed but the CRC did not */
        kid_reset(); kid_take(&r);
        check(kid_level() == 1, "a torn write (bad CRC) is rejected");
        kid_set_level(4); kid_fill(&r);
        r.magic = 0;
        kid_reset(); kid_take(&r);
        check(kid_level() == 1, "a record with no magic is rejected");
        kid_set_level(4); kid_fill(&r);
        r.crc ^= 1u;
        kid_reset(); kid_take(&r);
        check(kid_level() == 1, "a record with a bad CRC is rejected");
    }
    {   /* stock compat: a kid-written record still validates stock's own 80 bytes */
        side_rec_t sr;
        theme_t th0 = theme; uint8_t tv0 = theme_valid;
        uint16_t fav0[FAV_N];
        memcpy(fav0, fav, sizeof fav);
        memset(&theme, 0, sizeof theme);   /* a valid theme (magic, CRC, apart colours) */
        theme.magic = THEME_MAGIC; theme.c[4] = 0xFFFF; theme.hi = 0xFFFF;
        theme.crc = st_crc32(&theme, __builtin_offsetof(theme_t, crc));
        theme_valid = 1; fav[0] = 7; fav[1] = 9;   /* her theme and favourites */
        kid_set_level(6);
        side_fill(&sr);
        check(sr.fcrc == st_crc32(&sr.fmagic, 4u + sizeof sr.fav),
              "the stock 80 bytes still check out (stock 2.8.5 reads them)");
        check(sizeof sr == 80u + sizeof(kid_rec_t), "the kid block sits after the stock 80 bytes");
        theme_valid = 0; fav[0] = FAV_NONE; fav[1] = FAV_NONE;   /* a fresh boot */
        kid_reset();
        side_take(&sr);
        check(theme_valid && fav[0] == 7 && fav[1] == 9 && kid_level() == 6,
              "theme, favourites and level all come back");
        theme = th0; theme_valid = tv0; memcpy(fav, fav0, sizeof fav);
    }
    {   /* parent reset: clear wipes the record */
        kid_rec_t r;
        kid_set_level(8);
        kid_reset();
        check(kid_level() == 1 && kid_dirty == 1, "reset: level 1, marked dirty");
        kid_fill(&r);
        check(r.magic == 0, "reset: no record is written");
        kid_set_level(10);
        check(kid_level() == 10, "unlock all: level 10");
    }

    /* 3: the clubhouse (S3): one page, always reachable, the four buttons */
    {
        int club = page_idx("SLOOP JR", FAM_KID);
        check(club >= 0 && PAGES[club].scope == SC_KID, "the clubhouse page exists (FAM_KID, SC_KID)");
        check(club == NPAGES - 1, "the clubhouse is appended (no page moved)");
        check(FAM_BTN[FAM_KID] == B_HOME, "FAM_BTN[FAM_KID] is HOME (finding 27: not B_FX)");
        for (lvl = 1; lvl <= 10; lvl++) {
            kid_set_level((uint8_t)lvl);
            check(kid_page_ok((uint32_t)club) && page_shown(&PAGES[club]),
                  "the clubhouse is reachable at every level");
        }
        check(page_for_drum(&PAGES[club]) == 0, "the clubhouse is no track's page");
        check(page_first(FAM_KID) == (uint32_t)club, "page_first(FAM_KID) is the clubhouse");
    }
    {   /* kid_enter: the clubhouse, with the menu/holds/locks cleared (finding 13, 19) */
        kid_set_level(1);
        ui.menu = 1; ui.hold_kind = 1; ui.layer = LY_FX; ly_lock = LY_FX; ui.home_t0 = 12345;
        song.seq_mode = 1; ui.page = 0; ui.home = 0;
        kid_enter();
        check(kid_on_screen(), "kid_enter puts the clubhouse up");
        check(ui.page == page_first(FAM_KID), "kid_enter lands on the clubhouse page");
        check(ui.menu == 0 && ui.hold_kind == 0 && ui.layer == LY_PLAY && ly_lock == LY_PLAY && ui.home_t0 == 0,
              "kid_enter clears the menu, holds and layer locks (finding 13)");
        check(song.seq_mode == 0, "kid_enter disarms the sequencer (finding 19)");
        /* page_fix must settle on the clubhouse (it is shown, so it stays) */
        {
            uint32_t p = ui.page;
            page_fix();
            check(ui.page == p, "page_fix settles on the clubhouse");
        }
        /* go_home opens the clubhouse too */
        ui.page = 0; ui.home = 0;
        go_home();
        check(kid_on_screen(), "go_home opens the clubhouse");
    }
    {   /* the clubhouse draws (three bands, header shows ui.msg) without crashing */
        kid_set_level(3);
        kid_enter();
        ui.force = 1;
        ui_draw();
        {
            uint32_t i, lit = 0;
            for (i = 0; i < 240u * 240u; i++) if (screen[i]) lit++;
            check(lit > 100, "the clubhouse draws something (header, tiles, footer)");
        }
        outdir = "build/host";
        ppm("kid-clubhouse");   /* the layout, for the eye (S3: legible at arm's length) */
        /* a message replaces the header (finding 32) */
        ui_message("HELLO");
        ui.force = 1;
        ui_draw();
        check(!strncmp(ui.msg, "HELLO", 5), "the header carries ui.msg on the clubhouse");
        ui.msg_t = 0;
    }
    {   /* the input map on the clubhouse (driven through ui_input; S6: SEQ is the ladder,
         * PLAY starts the card's mission, HOME gives her song back) */
        uint32_t r0;
        kid_reset();
        kid_set_level(1);
        settle();
        kid_enter();
        frames(2);
        /* SEQ: the ladder's first rung: the hint appears on the card, not as a message */
        r0 = kid_mission_hint_rung();
        tap(B_SEQ);
        check(kid_mission_hint_rung() == r0 + 1u || r0 >= 3u, "SEQ on the clubhouse asks for the hint");
        check(!ui.msg_t, "the hint is on the card, not in the header");
        ui.force = 1;
        ui_draw();                                   /* (the card draws with the hint) */
        /* EDIT: undo (one button) */
        tap(B_EDIT);
        check(ui.msg_t && (!strncmp(ui.msg, "UNDO", 4) || !strncmp(ui.msg, "NOTHING", 7)),
              "EDIT on the clubhouse is undo");
        ui.msg_t = 0;
        /* the safety lesson (curriculum §6): the first undo says the change is not lost,
         * once, and only when the header is free */
        ui.msg_t = 0;
        kid_wit_undone();
        frames(2);
        check(!strncmp(ui.msg, "SEE?", 4), "the first undo says the change is not lost");
        ui.msg_t = 0;
        ui.msg[0] = 0;
        kid_wit_undone();
        frames(2);
        check(ui.msg[0] == 0, "and the lesson is said once");
        /* another button: NOT YET (a mash opens nothing) */
        tap(B_ENV);
        check(msg_is_refusal(), "another button on the clubhouse is NOT YET");
        check(kid_on_screen(), "a mash cannot leave the clubhouse");
        ui.msg_t = 0;
        /* PLAY: starts the mission on the card (and the transport with it) */
        tap(B_PLAY);
        for (i = 0; i < 300u && !kid_mission_active; i++) frame();
        check(kid_mission_active && kid_ms_id == 0, "PLAY on the clubhouse starts the mission on the card");
        check(song.playing || transport_req, "and the transport plays it");
        /* HOME tap: a running mission gives her song back, and she stays on the clubhouse */
        tap(B_HOME);
        for (i = 0; i < 300u && kid_mission_active; i++) frame();
        check(!kid_mission_active && kid_on_screen(), "HOME on a running mission ends it, on the clubhouse");
        /* HOME tap with nothing up: the sandbox (free play) */
        tap(B_HOME);
        check(!kid_on_screen(), "HOME tap on the clubhouse goes to the sandbox");
        check(PAGES[ui.page].fam == FAM_TRK, "the sandbox is the TRACKS room");
        /* HOME tap on an instrument screen: back to the clubhouse */
        tap(B_HOME);
        check(kid_on_screen(), "HOME tap on an instrument screen returns to the clubhouse");
    }
    {   /* HOME held 3 s: the parent menu (own timer, not btn_hold's 700 ms) */
        kid_set_level(1);
        kid_enter();
        frames(2);
        fm1_in.buttons |= BT(B_HOME);   /* hold HOME */
        for (i = 0; i < 220u; i++) frame();   /* ~3.5 s */
        fm1_in.buttons &= ~BT(B_HOME);
        frame();
        check(ui.menu == 1, "HOME held 3 s opens the parent menu");
        /* while the menu is open, kid_input hands over (returns 0) */
        check(kid_input(0, 0, BT_NONE) == 0, "the parent menu owns the input while open");
        ui.menu = 0; ui.force = 1;
        kid_enter();
        check(kid_on_screen() && ui.menu == 0, "leaving the parent menu returns to the clubhouse");
    }


    /* 3: two rooms (STAGES S4, gate G4): a mission cannot damage her song.
     * Enter with the transport stopping -> the call defers; play; edit the seed;
     * leave -> her song is byte-identical. 30 cycles, then one power cut. */
    {   static project_t her, seed, got, rd;
        uint32_t cyc, bad = 0, entered, left, why[8] = {0};
        int r;

        ui.menu = 0; kid_set_level(3); kid_enter(); ui.force = 1; frames(2);
        /* her song: a sound, a kit, a pattern, a tempo */
        song.sel = 0;
        set_engine_of(&trk[0], 1); apply_preset_to(&trk[0], 1); trk[0].eng_req = 1;   /* DIGITAL */
        TDRUM->p[P_E0] = 5;
        for (i = 0; i < NSTEP; i++) {
            trk[0].step[i].note[0] = (uint8_t)(48u + i % 12u);
            trk[0].step[i].vel = 100;
            trk[0].step[i].n = (uint8_t)(i % 2u);
        }
        song.g[G_BPM] = 96;
        song.g[G_DRREV] = 40;
        proj_capture(&her);
        project_restore(4, &her, sizeof her);   /* canonical: what a load leaves */
        for (i = 0; i < 50u && (transport_req || song.playing); i++) frame();   /* (project_apply() stops) */
        proj_capture(&her);                     /* the reference */
        check(her.sum != 0, "test 3: her song is a project");

        /* entering while the transport runs defers until it has stopped */
        song.playing = 0; transport_req = 0;
        transport_req = 1;                      /* PLAY: the transport starts on the next block */
        frame();
        check(song.playing, "test 3: the transport runs");
        r = kid_mission_enter(&her, sizeof her);
        check(r == 0 && !kid_mission_active, "test 3: a running transport defers the entry");
        for (i = 0; i < 200u && (song.playing || transport_req); i++) frame();
        check(!song.playing && !transport_req, "test 3: the stop is serviced");
        project_restore(4, &her, sizeof her);
        proj_capture(&got);
        check(memcmp(&got, &her, sizeof her) == 0, "test 3: her song restores byte-identical");

        for (cyc = 0; cyc < 30u; cyc++) {
            settle();   /* (a restore asks for a stop: service it before the next one) */
            /* the seed for this cycle: the working project changed, then captured */
            TDRUM->p[P_E0] = (int16_t)(cyc % DRUM_KITS);
            song.g[G_BPM] = (int16_t)(60u + cyc);
            trk[1].step[0].note[0] = (uint8_t)(36u + cyc % 24u);
            trk[1].step[0].vel = (uint8_t)(40u + cyc);
            proj_capture(&seed);
            project_restore(4, &her, sizeof her);       /* her song back */
            proj_capture(&got);
            if (memcmp(&got, &her, sizeof her)) { bad++; why[0]++; }
            /* enter (retrying while the transport stops) */
            entered = 0;
            for (i = 0; i < 200u && !entered; i++) {
                r = kid_mission_enter(&seed, sizeof seed);
                if (r == 1) entered = 1;
                else if (r < 0) break;
                else frame();
            }
            if (!entered || !kid_mission_active) { bad++; why[1]++; }
            if (song.g[G_BPM] != (int16_t)(60u + cyc)) { bad++; why[2]++; }   /* the seed is up */
            /* play the mission and work in it */
            tap(B_PLAY);
            frames(6);
            if (!song.playing) { bad++; why[3]++; }
            for (i = 0; i < 8u; i++) {
                trk[0].step[i].note[0] = (uint8_t)(60u + i);
                trk[0].step[i].vel = 1;
                TDRUM->p[P_E0] = 0;
                song.g[G_BPM] = 200;
                frames(1);
            }
            /* mid-mission, neither autosave nor a flush may write the seed */
            ui.menu = 0; rec_wait = 0; ft_on = 0; ui_input_ms = 0;
            autosave_ms = 0; autosave_checked = 0; fm1_ms += 100000u;
            autosave_tick();
            persist_flush_now();
            memset(&rd, 0, sizeof rd);
            {   int n = st_load(OBJ_AUTOSAVE, &rd, sizeof rd);
                if (n != (int)sizeof(project_t) || memcmp(&rd, &her, sizeof her) != 0) { bad++; why[4]++; }
            }
            /* leave (the transport is running: the first call defers) */
            left = 0;
            for (i = 0; i < 200u && !left; i++) {
                r = kid_mission_leave();
                if (r == 1) left = 1;
                else if (r < 0) break;
                else frame();
            }
            if (!left || kid_mission_active) { bad++; why[5]++; }
            proj_capture(&got);
            if (memcmp(&got, &her, sizeof her)) { bad++; why[6]++; }      /* byte-identical */
            if (autosave_hash != her.sum) { bad++; why[7]++; }
            if (!left) { printf("cycle %u: could not leave the mission\n", cyc); break; }
        }
        if (bad)
            printf("  (her song back / entered / seed up / playing / flash / left / identical / hash:"
                   " %u %u %u %u %u %u %u %u)\n", why[0], why[1], why[2], why[3], why[4], why[5], why[6], why[7]);
        check(bad == 0, "test 3: 30 missions, her song byte-identical after each (proj_hash)");

        /* the power cut: in a mission, the seed edited, then the machine is reset */
        settle();
        TDRUM->p[P_E0] = 2;
        song.g[G_BPM] = 71;
        proj_capture(&seed);
        project_restore(4, &her, sizeof her);
        settle();
        proj_capture(&got);
        check(memcmp(&got, &her, sizeof her) == 0, "test 3: her song is up before the last mission");
        entered = 0;
        for (i = 0; i < 200u && !entered; i++) {
            r = kid_mission_enter(&seed, sizeof seed);
            if (r == 1) entered = 1;
            else if (r < 0) break;
            else frame();
        }
        check(entered && kid_mission_active, "test 3: in a mission when the power goes");
        trk[0].step[3].note[0] = 99; song.g[G_BPM] = 199;   /* the seed is worked in */
        kid_mission_active = 0;                             /* RAM is gone */
        autosave_resume();                                  /* boot: OBJ_AUTOSAVE -> the working project */
        proj_capture(&got);
        check(memcmp(&got, &her, sizeof her) == 0, "test 3: a power cut brings her song back, not the seed");

        /* no flash, no snapshot: the mission is refused rather than losing her song */
        settle();
        flash_ok = 0;
        r = kid_mission_enter(&seed, sizeof seed);
        check(r == -1 && !kid_mission_active, "test 3: no flash refuses the mission");
        flash_ok = 1;
        proj_capture(&got);
        check(memcmp(&got, &her, sizeof her) == 0, "test 3: a refused mission leaves her song alone");
    }


    /* 4 and 5: the mission engine (STAGES S5). Every seed expands and passses proj_ok();
     * every check passes at its boundary and not one step before; a mission left and
     * re-entered resumes at the same state; the null mission (SEQ) completes. */
    {   static project_t got;
        uint32_t id, ok = 0, seedok = 0, bnd = 0, n, t, want = 0;

        kid_reset();                     /* (a clean progress record: level 1, nothing done) */
        kid_set_level(10);               /* (a full gate: the engine is what is under test) */
        settle();

        /* every seed expands to a project, with its tempo, kit and parts */
        for (id = 0; id < KID_NMISSION; id++) {
            const kid_seed_t *sd = &KID_SEEDS[id];
            uint32_t i, parts_ok = 1;
            for (i = 0; i < 100u; i++) {         /* (start defers while the transport stops) */
                int r = kid_mission_start((uint8_t)id);
                if (r != 0) { ok = r == 1; break; }
                frame();
            }
            if (!ok)
                break;
            proj_capture(&got);
            n = (uint32_t)proj_ok(&got);
            n &= (uint32_t)(song.g[G_BPM] == (int16_t)sd->bpm);
            n &= (uint32_t)(TDRUM->p[P_E0] == (int16_t)sd->kit);
            for (i = 0; i < 4u; i++) {           /* the seeded drum lanes */
                uint32_t k, m = 0;
                if (sd->lane_id[i] >= DRUM_LANES)
                    continue;
                for (k = 0; k < NSTEP; k++)
                    if ((TDRUM->dstep[k].on[sd->lane_id[i] >> 3] >> (sd->lane_id[i] & 7u)) & 1u)
                        m |= 1u << k;
                n &= (uint32_t)(m == sd->lane[i]);
            }
            seedok += n;
            /* the parts the seed does not name are muted, the ones it does are not */
            for (i = 0; i < 4u; i++)
                parts_ok &= (trk[KID_PART_TRACK[i]].p[P_MUTE] != 0) == !((sd->parts >> i) & 1u);
            seedok += parts_ok;
            kid_mission_end();
            settle();
        }
        {
            char b[64];
            snprintf(b, sizeof b, "test 4: 30 seeds expand (project, tempo, kit, lanes, parts) (%u/%u)", seedok, 60u);
            check(seedok == 60u, b);
        }

        /* a synthetic witness sequence per check: not one step before the boundary */
        for (id = 0; id < KID_NMISSION; id++) {
            const kid_mission_t *m = &KID_MISSIONS[id];
            uint32_t k, i;
            int r;
            kid_reset();
            kid_set_level(10);
            settle();
            for (k = 0; k < 100u; k++) {
                r = kid_mission_start((uint8_t)id);
                if (r != 0) break;
                frame();
            }
            if (r != 1) { bnd++; continue; }
            song.playing = 1;                       /* (a knob counts only while it plays) */
            {   int jb = just_before(m->check, m->c0, m->c1);   /* not one step before the boundary */
                if (jb) printf("DBG id %u: check %u (%u,%u) passes before\n", id, m->check, m->c0, m->c1);
                bnd += jb == 0; }
            satisfy(m->check, m->c0, m->c1);
            if (!kid_chk(m->check, m->c0, m->c1)) printf("DBG id %u: check %u (%u,%u) not at boundary\n", id, m->check, m->c0, m->c1);
            bnd += kid_chk(m->check, m->c0, m->c1) != 0;        /* and it is true at the boundary */
            if (m->check2) {
                bnd += kid_chk(m->check2, m->d0, m->d1) == 0;   /* the second one is still open */
                satisfy(m->check2, m->d0, m->d1);
                if (!kid_mission_pass()) printf("DBG id %u: check2 %u (%u,%u) missing\n", id, m->check2, m->d0, m->d1);
                bnd += kid_mission_pass() != 0;                 /* both: the mission passes */
            } else {
                bnd += kid_mission_pass() != 0;                 /* the one check completes it */
            }
            song.playing = 0;
            kid_mission_end();
            settle();
        }
        for (id = 0; id < KID_NMISSION; id++)
            want += KID_MISSIONS[id].check2 ? 4u : 3u;
        {
            char b[72];
            snprintf(b, sizeof b, "test 4: every check at its boundary, not one step before (%u/%u)", bnd, want);
            check(bnd == want, b);
        }

        /* 5: left and re-entered, a started mission resumes at the same state */
        kid_reset();
        kid_set_level(10);
        settle();
        for (t = 0; t < 100u; t++) { if ((r5 = kid_mission_start(0)) != 0) break; frame(); }
        check(r5 == 1 && kid_mission_done(0) == 1, "test 5: a mission that starts is marked started");
        for (i = 0; i < 5u; i++)
            kid_mission_tick(1u << (14u + i), fm1_ms);       /* five keys */
        check(!kid_mission_pass(), "test 5: five keys are not eight");
        check(end_mission() == 1, "test 5: left");
        settle();
        for (t = 0; t < 100u; t++) { if ((r5 = kid_mission_start(0)) != 0) break; frame(); }
        check(r5 == 1, "test 5: re-entered");
        kid_mission_tick(1u << 20, fm1_ms);                  /* one more key: six, not eight */
        check(!kid_mission_pass(), "test 5: the witnesses came back with it");
        for (i = 0; i < 2u; i++)
            kid_mission_tick(1u << (21u + i), fm1_ms);
        check(kid_mission_pass(), "test 5: and the mission can be finished");

        /* the null mission: SEQ completes it (the plumbing without content) */
        id = 28u;                                            /* SHOW AND TELL: honour */
        end_mission();
        settle();
        for (t = 0; t < 100u; t++) { if ((r5 = kid_mission_start((uint8_t)id)) != 0) break; frame(); }
        check(r5 == 1 && kid_mission_pass(), "the null mission passes with no witnesses");
        check(kid_mission_finish() == 1 && kid_mission_done((uint8_t)id) == 2, "the null mission completes");
        check(kid_mission_finish() == 1, "completing twice is harmless");
        end_mission();
        settle();


        /* a level's three missions: the badge, and one level up */
        kid_reset();
        kid_set_level(1);
        settle();
        {   uint32_t m, lvl1_ok = 0;
            for (m = 0; m < 3u; m++) {                       /* missions 1, 2, 3: level 1 */
                const kid_mission_t *mm = &KID_MISSIONS[m];
                for (t = 0; t < 100u; t++) { if ((r5 = kid_mission_start((uint8_t)m)) != 0) break; frame(); }
                song.playing = 1;
                satisfy(mm->check, mm->c0, mm->c1);
                if (mm->check2)
                    satisfy(mm->check2, mm->d0, mm->d1);
                lvl1_ok += kid_mission_finish() == 1;
                song.playing = 0;
                end_mission();
                settle();
            }
            check(lvl1_ok == 3, "the three missions of level 1 complete");
            check(kid_level() == 2, "and the gate opens level 2");
            check((kid_ram.badges & 2u) != 0, "with the level's badge");
        }
        check(kid_mission_of_level(1, 0) == 0 && kid_mission_of_level(2, 0) == 3 &&
              kid_mission_of_level(1, 3) == 0xFF, "the first mission of a level (and none past the last)");
    }

    /* 6: levels 1-3 played from the clubhouse (S6). The card, the tiles and the dots are the
     * whole manual: PLAY starts the card's mission, the check finishes it, the header names
     * what she made, the third mission of a level opens the next one. Then the ladder
     * (hint -> listen, with the step played for her -> park) and the parked mission's return. */
    {   uint32_t id, t, n, ok_card = 0, ok_play = 0, ok_done = 0, ok_line = 0, ok_leave = 0;
        uint32_t started = 0, ladder = 0, loan = 0, returned = 0, parked = 0, open2 = 0;

        kid_reset();
        kid_set_level(1);
        settle();
        kid_enter();
        frames(2);

        /* level 1: missions 1, 2, 3 */
        for (id = 0; id < 3u; id++) {
            int line = 0;
            ok_card += kid_mission_current() == id;
            ok_play += play_one((uint8_t)id, &line);
            ok_line += line;
            ok_done += kid_mission_done((uint8_t)id) == 2u;
            if (id == 2u) {                        /* the third one opens the level... */
                open2 += kid_level() == 2u;
                ui.msg_t = 0;                      /* ...and says so next */
                frames(2);
                open2 += !strncmp(ui.msg, "LEVEL 2 IS OPEN", 15);
                ui.msg_t = 0;
            }
            ok_leave += leave_mission();
        }
        check(ok_card == 3 && ok_play == 3 && ok_done == 3 && ok_leave == 3,
              "test 6: level 1 played from the clubhouse (card, PLAY, done, HOME)");
        check(ok_line == 3, "test 6: each header names what she made");
        check(open2 == 2, "test 6: the third mission opens level 2, and it says so");

        /* the ladder, on mission 4 (BACKBEAT: the hat lane, steps 5 and 13) */
        tap(B_PLAY);
        for (t = 0; t < 300u && !(kid_mission_active && kid_ms_id == 3); t++)
            frame();
        started += kid_mission_active && kid_ms_id == 3;
        check(started == 1, "test 6: the next card's mission starts (mission 4)");
        n = KID_MISSIONS[3].c0;                    /* the lane the demo borrows */
        ladder += kid_mission_hint_rung() == 0;
        ui.force = 1;                              /* the card, for the eye (S6 layout) */
        ui_draw();
        ppm("kid-card");
        tap(B_SEQ);                                /* rung 1: the hint, on the card */
        ladder += kid_mission_hint_rung() == 1u && !ui.msg_t;
        ui.force = 1;                              /* ...and with the hint asked for */
        ui_draw();
        ppm("kid-card-hint");
        tap(B_SEQ);                                /* rung 2: listen, and the step played */
        ladder += kid_mission_hint_rung() == 2u && !strncmp(ui.msg, "LISTEN", 6);
        loan += ((TDRUM->dstep[4].on[n >> 3] >> (n & 7u)) & 1u) == 1u &&
                ((TDRUM->dstep[12].on[n >> 3] >> (n & 7u)) & 1u) == 1u;
        for (t = 0; t < 400u && kid_mission_demo(); t++)
            frame();                               /* (a bar or two, then the loan is taken back) */
        returned += !kid_mission_demo() &&
                    ((TDRUM->dstep[4].on[n >> 3] >> (n & 7u)) & 1u) == 0u &&
                    kid_mission_done(3) != 2u;     /* (a loan is not her work: no reward) */
        check(ladder == 3 && loan == 1 && returned == 1,
              "test 6: the ladder's first two rungs, a borrowed step, and it taken back");
        {
            int line = 0;
            const kid_mission_t *m = &KID_MISSIONS[3];
            song.playing = 1;
            satisfy(m->check, m->c0, m->c1);       /* now she does it herself */
            song.playing = 0;
            for (t = 0; t < 600u && kid_mission_done(3) != 2u; t++)
                frame();
            line = kid_mission_done(3) == 2u &&
                   !strncmp(ui.msg, kid_mission_done_line(3), strlen(kid_mission_done_line(3)));
            check(line, "test 6: and then it completes, with its line");
        }
        ok_leave = leave_mission();

        /* mission 5 (IN BETWEEN), played the same way */
        {   int line = 0;
            ok_card += kid_mission_current() == 4;
            ok_play += play_one(4u, &line);
            ok_line += line;
            ok_done += kid_mission_done(4) == 2u;
            ok_leave += leave_mission();
        }

        /* rung 4: park mission 6 and hear it offered again (nothing marks it failed) */
        {   uint32_t r;
            tap(B_PLAY);
            for (t = 0; t < 300u && !(kid_mission_active && kid_ms_id == 5); t++)
                frame();
            r = kid_mission_active && kid_ms_id == 5;
            for (n = 0; n < 4u; n++)               /* hint, listen, do-it-for-her, park */
                tap(B_SEQ);
            parked += r && kid_mission_hint_rung() == 0 &&
                      !strncmp(ui.msg, "SKIPPED", 7) && kid_mission_current() == 5;
            for (t = 0; t < 300u && kid_mission_active; t++)
                frame();
            parked += !kid_mission_active && kid_on_screen();   /* (her song back) */
            check(parked == 2, "test 6: SEQ four times parks the mission, and it is still the card");
        }

        /* levels 2 and 3: the parked mission first, then 6 to 9 */
        {   uint32_t c2 = 0, p2 = 0, d2 = 0, l2 = 0, v2 = 0;
            for (id = 5; id < 9u; id++) {
                int line = 0;
                c2 += kid_mission_current() == id;
                p2 += play_one((uint8_t)id, &line);
                l2 += line;
                d2 += kid_mission_done((uint8_t)id) == 2u;
                if (id == 8u) {
                    open2 += kid_level() == 4u;
                    ui.msg_t = 0;
                    frames(2);
                    open2 += !strncmp(ui.msg, "LEVEL 4 IS OPEN", 15);
                    ui.msg_t = 0;
                }
                v2 += leave_mission();
            }
            check(c2 == 4 && p2 == 4 && d2 == 4 && l2 == 4 && v2 == 4,
                  "test 6: levels 2 and 3 played the same way (missions 5-9)");
            check(open2 == 4, "test 6: the third mission of level 3 opens level 4");
        }
    }

    printf("kid: %d failure%s\n", fails, fails == 1 ? "" : "s");
    return fails != 0;
}
