#include "ui.h"
#include <lvgl.h>
#include <deque>
#include <vector>
#include "config.h"
#include "grbl.h"
#include "jobstore.h"
#include "wifimgr.h"

// Fallback jog defaults (keep the UI compiling regardless of config.h revision).
#ifndef JOG_STEP_DEFAULT
#define JOG_STEP_DEFAULT 1.0f
#endif
#ifndef JOG_FEED_DEFAULT
#define JOG_FEED_DEFAULT 1000.0f
#endif
#ifndef JOG_CONTINUOUS_DIST
#define JOG_CONTINUOUS_DIST 1000.0f
#endif

// ===========================================================================
//  THEME / PALETTE
// ===========================================================================
#define COL_BG        lv_color_hex(0x15181C)
#define COL_BG2       lv_color_hex(0x1B1F24)
#define COL_PANEL     lv_color_hex(0x242A31)
#define COL_PANEL_HI  lv_color_hex(0x2E353D)
#define COL_LINE      lv_color_hex(0x333B44)
#define COL_ACCENT    lv_color_hex(0x17C3B2)
#define COL_TEXT      lv_color_hex(0xE6E9ED)
#define COL_MUTED     lv_color_hex(0x8A929B)

#define COL_IDLE      lv_color_hex(0x2EA043)
#define COL_RUN       lv_color_hex(0x2F81F7)
#define COL_HOLD      lv_color_hex(0xFFB300)
#define COL_ALARM     lv_color_hex(0xE5484D)
#define COL_OFF       lv_color_hex(0x3A424B)

// ===========================================================================
//  FONT SET  -- UNIFIED on Montserrat (ZERO mixed-font patchwork)
// ===========================================================================
//  The earlier build mixed a pixel-mono (UNSCII) font with Montserrat, which
//  looked like a patchwork. lv_font_conv (node/npm) is not available in this
//  environment, so a proper "techy" TTF (Share Tech Mono / DSEG7) cannot be
//  generated here. We therefore unify the ENTIRE UI on one family - Montserrat
//  - at a range of sizes. Every text element routes through a FONT_* define.
//
//  TODO (techy pass): generate an LVGL font from Share Tech Mono (labels) and
//  DSEG7 (DRO digits) with lv_font_conv, add them to lv_conf.h, and repoint
//  FONT_DRO / FONT_H1 / FONT_BTN below. Nothing else needs to change.
#define FONT_DRO   &lv_font_montserrat_44   // big DRO digits
#define FONT_LG    &lv_font_montserrat_28   // axis letters, jog arrows
#define FONT_H1    &lv_font_montserrat_22   // state chip, section headers, tabs
#define FONT_BTN   &lv_font_montserrat_16   // button labels
#define FONT_MD    &lv_font_montserrat_16   // dropdowns, inputs
#define FONT_SM    &lv_font_montserrat_14   // secondary text, net indicator
#define FONT_HBAR  &lv_font_montserrat_14   // ALL top-bar text (uniform size)
#define FONT_MONO  &lv_font_montserrat_14   // console (unified; techy TODO above)
#define FONT_TINY  &lv_font_montserrat_12   // segmented toggle, small chips

// ===========================================================================
//  Right-column layout grid (uniform margins + gutters)
// ===========================================================================
static const int RX0 = 484, RW = 308, GAP = 8;
static inline int cellW(int n)        { return (RW - (n - 1) * GAP) / n; }
static inline int cellX(int i, int n) { return RX0 + i * (cellW(n) + GAP); }

// ===========================================================================
//  State + widgets
// ===========================================================================
static float g_step     = JOG_STEP_DEFAULT;
static float g_feed     = JOG_FEED_DEFAULT;
static bool  g_showWork = true;

static lv_obj_t *g_views[3];              // 0 = Control, 1 = Jobs, 2 = WiFi
static lv_obj_t *g_tabBtns[2];            // CONTROL / JOBS
static lv_obj_t *g_netBtn, *g_netLbl;

static lv_obj_t *g_stateChip, *g_stateLbl;
static lv_obj_t *g_axisBig[3], *g_axisSmall[3];
static lv_obj_t *g_posToggle;
static lv_obj_t *g_jobStripLbl;
static lv_obj_t *g_console;

// Jobs view
static lv_obj_t *g_jobList, *g_jobCurLbl, *g_jobBar, *g_jobPctLbl;
static std::vector<String> g_jobPaths;

// WiFi view
static lv_obj_t *g_wifiStatus, *g_ssidDrop, *g_pass, *g_kb;
static std::vector<WifiScanResult> g_ssids;
static bool g_scanPending = false;

static std::deque<String> g_log;

enum Cmd {
    CMD_ZERO_X, CMD_ZERO_Y, CMD_ZERO_Z, CMD_ZERO_ALL,
    CMD_HOME_XY, CMD_HOME_Z, CMD_HOME_ALL, CMD_UNLOCK,
    CMD_HOLD, CMD_RESUME, CMD_STOP,
    CMD_JOB_PAUSE, CMD_JOB_RESUME, CMD_JOB_STOP,
    CMD_WIFI_SCAN, CMD_WIFI_CONNECT, CMD_WIFI_FORGET, CMD_JOB_REFRESH
};

static inline intptr_t jogCode(int axis, int sign) { return (axis << 1) | (sign > 0 ? 1 : 0); }

static void show_view(int i);
static void rebuild_job_list();

// ===========================================================================
//  Event handlers  (UI runs on the GRBL task, so it calls Grbl directly)
// ===========================================================================
static void cmd_cb(lv_event_t *e)
{
    switch ((intptr_t)lv_event_get_user_data(e)) {
        case CMD_ZERO_X:     Grbl.zeroAxis('X'); break;
        case CMD_ZERO_Y:     Grbl.zeroAxis('Y'); break;
        case CMD_ZERO_Z:     Grbl.zeroAxis('Z'); break;
        case CMD_ZERO_ALL:   Grbl.zeroAll();     break;
        case CMD_HOME_ALL:   Grbl.home();        break;
        case CMD_HOME_XY:    Grbl.sendLine("$HX"); Grbl.sendLine("$HY"); break;
        case CMD_HOME_Z:     Grbl.sendLine("$HZ"); break;
        case CMD_UNLOCK:     Grbl.unlock();      break;
        case CMD_HOLD:       Grbl.feedHold();    break;
        case CMD_RESUME:     Grbl.resume();      break;
        case CMD_STOP:       Grbl.softReset();   break;
        case CMD_JOB_PAUSE:  Grbl.pauseJob();    break;
        case CMD_JOB_RESUME: Grbl.resumeJob();   break;
        case CMD_JOB_STOP:   Grbl.stopJob();     break;
        case CMD_JOB_REFRESH: rebuild_job_list(); break;
        case CMD_WIFI_SCAN:  wifi_scan_start(); g_scanPending = true; break;
        case CMD_WIFI_FORGET: wifi_forget(); break;
        case CMD_WIFI_CONNECT: {
            if (g_ssidDrop && g_pass) {
                char ssid[40];
                lv_dropdown_get_selected_str(g_ssidDrop, ssid, sizeof(ssid));
                wifi_connect(String(ssid), String(lv_textarea_get_text(g_pass)));
            }
            break;
        }
    }
}

static void jog_cb(lv_event_t *e)
{
    intptr_t code = (intptr_t)lv_event_get_user_data(e);
    int axis = code >> 1, sign = (code & 1) ? 1 : -1;
    char ax = "XYZ"[axis];
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) Grbl.jog(ax, sign * g_step, g_feed);
}

static void dist_cb(lv_event_t *e)
{
    char buf[16];
    lv_dropdown_get_selected_str(lv_event_get_target(e), buf, sizeof(buf));
    g_step = atof(buf);
}

static void feed_cb(lv_event_t *e)
{
    char buf[16];
    lv_dropdown_get_selected_str(lv_event_get_target(e), buf, sizeof(buf));
    g_feed = atof(buf);
}

static void posmode_cb(lv_event_t *e)
{
    g_showWork = (lv_btnmatrix_get_selected_btn(lv_event_get_target(e)) == 0);
}

static void tab_cb(lv_event_t *e) { show_view((intptr_t)lv_event_get_user_data(e)); }
static void net_cb(lv_event_t *e) { (void)e; show_view(2); }

static void run_job_cb(lv_event_t *e)
{
    intptr_t idx = (intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= (intptr_t)g_jobPaths.size()) return;
    fs::FS *fs = nullptr;
    String real;
    if (store_resolve(g_jobPaths[idx], fs, real) && fs) Grbl.startJob(*fs, real);
    show_view(0);                                   // jump to control to watch
}

static void del_job_cb(lv_event_t *e)
{
    intptr_t idx = (intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= (intptr_t)g_jobPaths.size()) return;
    store_delete(g_jobPaths[idx]);
    rebuild_job_list();
}

static void kb_event_cb(lv_event_t *e)
{
    lv_event_code_t c = lv_event_get_code(e);
    if (c == LV_EVENT_READY || c == LV_EVENT_CANCEL)
        lv_obj_add_flag(g_kb, LV_OBJ_FLAG_HIDDEN);
}

static void pass_focus_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        lv_keyboard_set_textarea(g_kb, g_pass);
        lv_obj_clear_flag(g_kb, LV_OBJ_FLAG_HIDDEN);
    }
}

// ===========================================================================
//  Styling helpers
// ===========================================================================
static void style_btn(lv_obj_t *b, lv_color_t base)
{
    lv_obj_set_style_radius(b, 8, 0);
    lv_obj_set_style_bg_color(b, base, 0);
    lv_obj_set_style_bg_color(b, lv_color_lighten(base, 35), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_border_color(b, COL_LINE, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_text_color(b, COL_TEXT, 0);
}

static lv_obj_t *make_btn(lv_obj_t *parent, const char *text, int x, int y, int w, int h,
                          lv_color_t base, lv_event_cb_t cb, intptr_t code,
                          const lv_font_t *font, lv_color_t txt = COL_TEXT)
{
    lv_obj_t *b = lv_btn_create(parent);
    lv_obj_set_size(b, w, h);
    lv_obj_set_pos(b, x, y);
    style_btn(b, base);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, (void *)code);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, txt, 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(l);
    return b;
}

static lv_obj_t *make_panel(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *p = lv_obj_create(parent);
    lv_obj_set_size(p, w, h);
    lv_obj_set_pos(p, x, y);
    lv_obj_set_style_bg_color(p, COL_PANEL, 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(p, 1, 0);
    lv_obj_set_style_border_color(p, COL_LINE, 0);
    lv_obj_set_style_radius(p, 8, 0);
    lv_obj_set_style_pad_all(p, 0, 0);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    return p;
}

static lv_obj_t *make_hdr(lv_obj_t *parent, const char *text, int x, int y)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_color(l, COL_MUTED, 0);
    lv_obj_set_style_text_font(l, FONT_SM, 0);
    lv_obj_set_pos(l, x, y);
    return l;
}

// ===========================================================================
//  Header
// ===========================================================================
static void build_header(lv_obj_t *scr)
{
    lv_obj_t *bar = lv_obj_create(scr);
    lv_obj_set_size(bar, 800, 56);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, COL_BG2, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    // Whole top bar routes through one size (FONT_HBAR) so every header element
    // matches the state chip's "DISCONNECTED" text - no mixed sizes up here.
    make_btn(bar, "HOLD",   8,   8, 70,  40, COL_HOLD,  cmd_cb, CMD_HOLD,   FONT_HBAR, lv_color_hex(0x1b1200));
    make_btn(bar, "RESUME", 84,  8, 92,  40, COL_IDLE,  cmd_cb, CMD_RESUME, FONT_HBAR);
    make_btn(bar, "RESET",  182, 8, 78,  40, COL_ALARM, cmd_cb, CMD_STOP,   FONT_HBAR);

    // Wide state chip.
    g_stateChip = make_panel(bar, 266, 8, 150, 40);
    g_stateLbl = lv_label_create(g_stateChip);
    lv_label_set_text(g_stateLbl, "DISCONNECTED");
    lv_obj_set_style_text_font(g_stateLbl, FONT_HBAR, 0);
    lv_obj_set_style_text_color(g_stateLbl, COL_TEXT, 0);
    lv_obj_center(g_stateLbl);

    // Network / IP area (tap to open the WiFi screen).
    g_netBtn = lv_btn_create(bar);
    lv_obj_set_size(g_netBtn, 150, 40);
    lv_obj_set_pos(g_netBtn, 422, 8);
    style_btn(g_netBtn, COL_PANEL);
    lv_obj_add_event_cb(g_netBtn, net_cb, LV_EVENT_CLICKED, nullptr);
    g_netLbl = lv_label_create(g_netBtn);
    lv_label_set_text(g_netLbl, LV_SYMBOL_WIFI "  --");
    lv_obj_set_style_text_font(g_netLbl, FONT_HBAR, 0);
    lv_obj_set_style_text_color(g_netLbl, COL_MUTED, 0);
    lv_obj_center(g_netLbl);

    const char *names[2] = {"CONTROL", "JOBS"};
    for (int i = 0; i < 2; i++)
        g_tabBtns[i] = make_btn(bar, names[i], 580 + i * 108, 8, 104, 40,
                                COL_PANEL, tab_cb, i, FONT_HBAR);
}

// ===========================================================================
//  Control view: DRO
// ===========================================================================
static void build_dro(lv_obj_t *view)
{
    lv_obj_t *p = make_panel(view, 8, 6, 468, 180);

    lv_obj_t *title = lv_label_create(p);
    lv_label_set_text(title, "POSITION");
    lv_obj_set_style_text_color(title, COL_ACCENT, 0);   // readable, not near-invisible grey
    lv_obj_set_style_text_font(title, FONT_SM, 0);
    lv_obj_set_pos(title, 14, 9);

    static const char *posMap[] = {"WORK", "MACH", ""};
    g_posToggle = lv_btnmatrix_create(p);
    lv_obj_set_size(g_posToggle, 150, 28);
    lv_obj_set_pos(g_posToggle, 310, 4);
    lv_btnmatrix_set_map(g_posToggle, posMap);
    lv_btnmatrix_set_btn_ctrl_all(g_posToggle, LV_BTNMATRIX_CTRL_CHECKABLE);
    lv_btnmatrix_set_one_checked(g_posToggle, true);
    lv_btnmatrix_set_btn_ctrl(g_posToggle, 0, LV_BTNMATRIX_CTRL_CHECKED);
    lv_obj_set_style_text_font(g_posToggle, FONT_TINY, 0);
    lv_obj_set_style_pad_all(g_posToggle, 2, 0);
    lv_obj_set_style_bg_opa(g_posToggle, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_posToggle, 0, 0);
    lv_obj_add_event_cb(g_posToggle, posmode_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    // Three evenly-spaced rows. The axis letter stays pinned at the left, but
    // the big numeric readout is CENTERED across the full panel width so the
    // block reads balanced instead of crammed against the left. The alternate
    // (M/W) coordinate sits small at the right edge of each row.
    static const char *names[3] = {"X", "Y", "Z"};
    for (int i = 0; i < 3; i++) {
        int y = 40 + i * 46;

        lv_obj_t *ax = lv_label_create(p);
        lv_label_set_text(ax, names[i]);
        lv_obj_set_style_text_color(ax, COL_ACCENT, 0);
        lv_obj_set_style_text_font(ax, FONT_LG, 0);
        lv_obj_set_pos(ax, 18, y + 8);

        g_axisBig[i] = lv_label_create(p);
        lv_label_set_text(g_axisBig[i], "0.000");
        lv_obj_set_style_text_color(g_axisBig[i], COL_TEXT, 0);
        lv_obj_set_style_text_font(g_axisBig[i], FONT_DRO, 0);
        lv_obj_set_width(g_axisBig[i], lv_pct(100));
        lv_obj_set_style_text_align(g_axisBig[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(g_axisBig[i], 0, y);

        g_axisSmall[i] = lv_label_create(p);
        lv_label_set_text(g_axisSmall[i], "M 0.000");
        lv_obj_set_style_text_color(g_axisSmall[i], COL_MUTED, 0);
        lv_obj_set_style_text_font(g_axisSmall[i], FONT_SM, 0);
        lv_obj_set_width(g_axisSmall[i], 118);
        lv_obj_set_style_text_align(g_axisSmall[i], LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_pos(g_axisSmall[i], 342, y + 16);
    }
}

static void build_job_strip(lv_obj_t *view)
{
    lv_obj_t *p = make_panel(view, 8, 190, 468, 30);
    g_jobStripLbl = lv_label_create(p);
    lv_label_set_text(g_jobStripLbl, LV_SYMBOL_FILE "  No job loaded");
    lv_obj_set_style_text_color(g_jobStripLbl, COL_MUTED, 0);
    lv_obj_set_style_text_font(g_jobStripLbl, FONT_SM, 0);
    lv_obj_align(g_jobStripLbl, LV_ALIGN_LEFT_MID, 10, 0);
}

static void build_console(lv_obj_t *view)
{
    g_console = lv_textarea_create(view);
    lv_obj_set_size(g_console, 468, 194);
    lv_obj_set_pos(g_console, 8, 224);
    lv_textarea_set_cursor_click_pos(g_console, false);
    lv_obj_clear_flag(g_console, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_set_scroll_dir(g_console, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(g_console, LV_SCROLLBAR_MODE_ON);
    lv_obj_set_style_bg_color(g_console, lv_color_hex(0x0C0E11), 0);
    lv_obj_set_style_border_width(g_console, 1, 0);
    lv_obj_set_style_border_color(g_console, COL_LINE, 0);
    lv_obj_set_style_radius(g_console, 8, 0);
    lv_obj_set_style_text_color(g_console, COL_ACCENT, 0);
    lv_obj_set_style_text_font(g_console, FONT_MONO, 0);
    lv_textarea_set_text(g_console, "");
}

// ===========================================================================
//  Control view: jog cross + cluster (right column)
// ===========================================================================
static lv_obj_t *make_jog_btn(lv_obj_t *p, const char *text, int x, int y, int w, int h,
                              int axis, int sign)
{
    lv_obj_t *b = lv_btn_create(p);
    lv_obj_set_size(b, w, h);
    lv_obj_set_pos(b, x, y);
    style_btn(b, COL_PANEL);
    lv_obj_add_event_cb(b, jog_cb, LV_EVENT_CLICKED, (void *)jogCode(axis, sign));
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, FONT_LG, 0);
    lv_obj_set_style_text_color(l, COL_ACCENT, 0);
    lv_obj_center(l);
    return b;
}

static void build_jog_cluster(lv_obj_t *view)
{
    // Distance + feed selectors on one row. Each has its units in a small
    // header ABOVE the dropdown; the feed dropdown fills the remaining width to
    // the right-column grid edge (RX0 + RW).
    const int distW = 130;
    const int feedX = RX0 + distW + GAP;
    const int feedW = RW - distW - GAP;          // fills to RX0 + RW
    make_hdr(view, "DISTANCE (mm)", RX0, 6);
    make_hdr(view, "FEED (mm/min)", feedX, 6);

    lv_obj_t *dist = lv_dropdown_create(view);
    lv_dropdown_set_options(dist, "0.1\n1\n10\n50\n100");
    lv_dropdown_set_selected(dist, 1);
    lv_obj_set_size(dist, distW, 38);
    lv_obj_set_pos(dist, RX0, 26);
    lv_obj_set_style_text_font(dist, FONT_MD, 0);
    lv_obj_add_event_cb(dist, dist_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t *feed = lv_dropdown_create(view);
    lv_dropdown_set_options(feed, "100\n500\n1000\n2000");
    lv_dropdown_set_selected(feed, 2);
    lv_obj_set_size(feed, feedW, 38);
    lv_obj_set_pos(feed, feedX, 26);
    lv_obj_set_style_text_font(feed, FONT_MD, 0);
    lv_obj_add_event_cb(feed, feed_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    // Jog cross on a 4-col grid: cols 0-2 = XY, col 3 = Z.
    const int jw = cellW(4), jh = 58;
    const int r0 = 80, r1 = r0 + jh + GAP, r2 = r1 + jh + GAP;
    make_jog_btn(view, "Y+", cellX(1, 4), r0, jw, jh, 1, +1);
    make_jog_btn(view, "X-", cellX(0, 4), r1, jw, jh, 0, -1);
    make_jog_btn(view, "X+", cellX(2, 4), r1, jw, jh, 0, +1);
    make_jog_btn(view, "Y-", cellX(1, 4), r2, jw, jh, 1, -1);
    make_jog_btn(view, "Z+", cellX(3, 4), r0, jw, jh, 2, +1);
    make_jog_btn(view, "Z-", cellX(3, 4), r2, jw, jh, 2, -1);
    make_btn(view, "HOME\nXY", cellX(1, 4), r1, jw, jh, COL_PANEL_HI, cmd_cb, CMD_HOME_XY, FONT_BTN, COL_ACCENT);
    make_btn(view, "HOME\nZ",  cellX(3, 4), r1, jw, jh, COL_PANEL_HI, cmd_cb, CMD_HOME_Z,  FONT_BTN, COL_ACCENT);

    const int cy0 = r2 + jh + 14;
    make_btn(view, "ZERO X", cellX(0, 3), cy0, cellW(3), 40, COL_PANEL, cmd_cb, CMD_ZERO_X, FONT_BTN);
    make_btn(view, "ZERO Y", cellX(1, 3), cy0, cellW(3), 40, COL_PANEL, cmd_cb, CMD_ZERO_Y, FONT_BTN);
    make_btn(view, "ZERO Z", cellX(2, 3), cy0, cellW(3), 40, COL_PANEL, cmd_cb, CMD_ZERO_Z, FONT_BTN);

    const int cy1 = cy0 + 48;
    make_btn(view, "ZERO ALL", cellX(0, 2), cy1, cellW(2), 40, COL_ACCENT, cmd_cb, CMD_ZERO_ALL, FONT_BTN, lv_color_hex(0x03201E));
    make_btn(view, "HOME ALL", cellX(1, 2), cy1, cellW(2), 40, COL_PANEL, cmd_cb, CMD_HOME_ALL, FONT_BTN);

    const int cy2 = cy1 + 48;
    make_btn(view, "UNLOCK $X", cellX(0, 1), cy2, cellW(1), 40, COL_PANEL, cmd_cb, CMD_UNLOCK, FONT_BTN);
}

static void build_control_view(lv_obj_t *view)
{
    build_dro(view);
    build_job_strip(view);
    build_console(view);
    build_jog_cluster(view);
}

// ===========================================================================
//  Jobs view
// ===========================================================================
static void rebuild_job_list()
{
    if (!g_jobList) return;
    lv_obj_clean(g_jobList);
    g_jobPaths.clear();

    std::vector<JobEntry> files;
    store_list(files);

    if (files.empty()) {
        lv_obj_t *l = lv_label_create(g_jobList);
        lv_label_set_text(l, store_sd_ready()
                          ? "No jobs. Upload .nc/.gcode files over the web UI."
                          : "SD unavailable - using flash. Upload jobs over the web UI.");
        lv_obj_set_style_text_color(l, COL_MUTED, 0);
        lv_obj_set_style_text_font(l, FONT_SM, 0);
        return;
    }

    for (size_t i = 0; i < files.size(); i++) {
        JobEntry &f = files[i];
        g_jobPaths.push_back(f.vpath);

        lv_obj_t *row = lv_obj_create(g_jobList);
        lv_obj_set_size(row, lv_pct(100), 48);
        lv_obj_set_style_bg_color(row, COL_PANEL, 0);
        lv_obj_set_style_border_color(row, COL_LINE, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_radius(row, 6, 0);
        lv_obj_set_style_pad_all(row, 6, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *nm = lv_label_create(row);
        char buf[80];
        snprintf(buf, sizeof(buf), "%s   [%s]  %u B", f.name.c_str(), f.onSd ? "SD" : "FLASH", (unsigned)f.size);
        lv_label_set_text(nm, buf);
        lv_obj_set_style_text_color(nm, COL_TEXT, 0);
        lv_obj_set_style_text_font(nm, FONT_SM, 0);
        lv_obj_align(nm, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t *del = make_btn(row, LV_SYMBOL_TRASH, 0, 0, 56, 34, COL_PANEL_HI, del_job_cb, (intptr_t)i, FONT_BTN, COL_ALARM);
        lv_obj_align(del, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_t *run = make_btn(row, "RUN", 0, 0, 72, 34, COL_ACCENT, run_job_cb, (intptr_t)i, FONT_BTN, lv_color_hex(0x03201E));
        lv_obj_align(run, LV_ALIGN_RIGHT_MID, -62, 0);
    }
}

static void build_jobs_view(lv_obj_t *view)
{
    g_jobCurLbl = lv_label_create(view);
    lv_label_set_text(g_jobCurLbl, "Current job: none");
    lv_obj_set_style_text_color(g_jobCurLbl, COL_TEXT, 0);
    lv_obj_set_style_text_font(g_jobCurLbl, FONT_H1, 0);
    lv_obj_set_pos(g_jobCurLbl, 8, 8);

    make_btn(view, "REFRESH", 700, 6, 92, 36, COL_PANEL, cmd_cb, CMD_JOB_REFRESH, FONT_BTN);

    g_jobBar = lv_bar_create(view);
    lv_obj_set_size(g_jobBar, 600, 16);
    lv_obj_set_pos(g_jobBar, 8, 44);
    lv_bar_set_range(g_jobBar, 0, 100);
    lv_bar_set_value(g_jobBar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(g_jobBar, COL_PANEL_HI, 0);
    lv_obj_set_style_bg_color(g_jobBar, COL_ACCENT, LV_PART_INDICATOR);

    g_jobPctLbl = lv_label_create(view);
    lv_label_set_text(g_jobPctLbl, "0%");
    lv_obj_set_style_text_color(g_jobPctLbl, COL_MUTED, 0);
    lv_obj_set_style_text_font(g_jobPctLbl, FONT_SM, 0);
    lv_obj_set_pos(g_jobPctLbl, 616, 42);

    make_btn(view, "PAUSE",  8,   70, 120, 38, COL_HOLD,  cmd_cb, CMD_JOB_PAUSE,  FONT_BTN, lv_color_hex(0x1b1200));
    make_btn(view, "RESUME", 136, 70, 120, 38, COL_IDLE,  cmd_cb, CMD_JOB_RESUME, FONT_BTN);
    make_btn(view, "STOP",   264, 70, 120, 38, COL_ALARM, cmd_cb, CMD_JOB_STOP,   FONT_BTN);

    lv_obj_t *hdr = lv_label_create(view);
    lv_label_set_text(hdr, "STORED JOBS  (microSD + flash)");
    lv_obj_set_style_text_color(hdr, COL_MUTED, 0);
    lv_obj_set_style_text_font(hdr, FONT_SM, 0);
    lv_obj_set_pos(hdr, 8, 118);

    g_jobList = lv_obj_create(view);
    lv_obj_set_size(g_jobList, 784, 268);
    lv_obj_set_pos(g_jobList, 8, 140);
    lv_obj_set_style_bg_color(g_jobList, COL_BG2, 0);
    lv_obj_set_style_border_color(g_jobList, COL_LINE, 0);
    lv_obj_set_style_border_width(g_jobList, 1, 0);
    lv_obj_set_style_radius(g_jobList, 8, 0);
    lv_obj_set_style_pad_all(g_jobList, 8, 0);
    lv_obj_set_style_pad_row(g_jobList, 6, 0);
    lv_obj_set_flex_flow(g_jobList, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(g_jobList, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(g_jobList, LV_SCROLLBAR_MODE_AUTO);
}

// ===========================================================================
//  WiFi view
// ===========================================================================
static void build_wifi_view(lv_obj_t *view)
{
    lv_obj_t *t = lv_label_create(view);
    lv_label_set_text(t, "WIFI");
    lv_obj_set_style_text_color(t, COL_TEXT, 0);
    lv_obj_set_style_text_font(t, FONT_H1, 0);
    lv_obj_set_pos(t, 8, 8);

    g_wifiStatus = lv_label_create(view);
    lv_label_set_text(g_wifiStatus, "Mode: --   IP: --");
    lv_obj_set_style_text_color(g_wifiStatus, COL_MUTED, 0);
    lv_obj_set_style_text_font(g_wifiStatus, FONT_SM, 0);
    lv_obj_set_pos(g_wifiStatus, 120, 14);

    make_btn(view, "SCAN",   8,   44, 104, 40, COL_PANEL, cmd_cb, CMD_WIFI_SCAN,   FONT_BTN);
    make_btn(view, "FORGET", 560, 44, 120, 40, COL_PANEL, cmd_cb, CMD_WIFI_FORGET, FONT_BTN, COL_ALARM);

    g_ssidDrop = lv_dropdown_create(view);
    lv_dropdown_set_options(g_ssidDrop, "(scan for networks)");
    lv_obj_set_size(g_ssidDrop, 420, 40);
    lv_obj_set_pos(g_ssidDrop, 120, 44);
    lv_obj_set_style_text_font(g_ssidDrop, FONT_MD, 0);

    g_pass = lv_textarea_create(view);
    lv_textarea_set_one_line(g_pass, true);
    lv_textarea_set_password_mode(g_pass, true);
    lv_textarea_set_placeholder_text(g_pass, "WiFi password");
    lv_obj_set_size(g_pass, 420, 44);
    lv_obj_set_pos(g_pass, 8, 96);
    lv_obj_set_style_text_font(g_pass, FONT_MD, 0);
    lv_obj_add_event_cb(g_pass, pass_focus_cb, LV_EVENT_CLICKED, nullptr);

    make_btn(view, "CONNECT", 440, 96, 120, 44, COL_ACCENT, cmd_cb, CMD_WIFI_CONNECT, FONT_BTN, lv_color_hex(0x03201E));

    g_kb = lv_keyboard_create(view);
    lv_obj_set_size(g_kb, 800, 210);
    lv_obj_align(g_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(g_kb, g_pass);
    lv_obj_add_flag(g_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(g_kb, kb_event_cb, LV_EVENT_ALL, nullptr);
}

// ===========================================================================
//  View switching
// ===========================================================================
static void show_view(int i)
{
    for (int k = 0; k < 3; k++) {
        if (k == i) lv_obj_clear_flag(g_views[k], LV_OBJ_FLAG_HIDDEN);
        else        lv_obj_add_flag(g_views[k], LV_OBJ_FLAG_HIDDEN);
    }
    for (int k = 0; k < 2; k++) {
        bool active = (k == i);
        lv_obj_set_style_bg_color(g_tabBtns[k], active ? COL_PANEL_HI : COL_PANEL, 0);
        lv_obj_set_style_border_color(g_tabBtns[k], active ? COL_ACCENT : COL_LINE, 0);
        lv_obj_t *lbl = lv_obj_get_child(g_tabBtns[k], 0);
        lv_obj_set_style_text_color(lbl, active ? COL_ACCENT : COL_MUTED, 0);
    }
    lv_obj_set_style_border_color(g_netBtn, (i == 2) ? COL_ACCENT : COL_LINE, 0);

    if (i == 1) rebuild_job_list();
}

// ===========================================================================
//  Init
// ===========================================================================
void ui_init()
{
    lv_disp_t *disp = lv_disp_get_default();
    lv_theme_t *th = lv_theme_default_init(disp, COL_ACCENT, COL_MUTED, true, FONT_MD);
    lv_disp_set_theme(disp, th);

    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    build_header(scr);

    for (int i = 0; i < 3; i++) {
        g_views[i] = lv_obj_create(scr);
        lv_obj_set_size(g_views[i], 800, 420);
        lv_obj_set_pos(g_views[i], 0, 60);
        lv_obj_set_style_bg_color(g_views[i], COL_BG, 0);
        lv_obj_set_style_bg_opa(g_views[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(g_views[i], 0, 0);
        lv_obj_set_style_radius(g_views[i], 0, 0);
        lv_obj_set_style_pad_all(g_views[i], 0, 0);
        lv_obj_clear_flag(g_views[i], LV_OBJ_FLAG_SCROLLABLE);
    }

    build_control_view(g_views[0]);
    build_jobs_view(g_views[1]);
    build_wifi_view(g_views[2]);

    show_view(0);
    ui_console_log("pendant ready - waiting for GRBL...", false);
}

// ===========================================================================
//  Refresh from GRBL snapshot (+ WiFi + job state)
// ===========================================================================
void ui_refresh(const GrblSnapshot &s)
{
    static bool prevConn = false;
    if (s.connected && !prevConn)      ui_console_log(String("UART link up - GRBL ") + s.stateName, false);
    else if (!s.connected && prevConn) ui_console_log("UART link lost", false);
    prevConn = s.connected;

    // ---- State chip ----
    lv_color_t col = COL_OFF;
    const char *txt = "DISCONNECTED";
    if (s.connected) {
        txt = s.stateName;
        switch (s.state) {
            case GrblState::Idle:  col = COL_IDLE;  break;
            case GrblState::Run:
            case GrblState::Jog:
            case GrblState::Home:  col = COL_RUN;   break;
            case GrblState::Hold:  col = COL_HOLD;  break;
            case GrblState::Alarm:
            case GrblState::Door:  col = COL_ALARM; break;
            default:               col = COL_OFF;   break;
        }
    }
    lv_obj_set_style_bg_color(g_stateChip, col, 0);
    lv_label_set_text(g_stateLbl, txt);

    // ---- DRO ----
    const float *big   = g_showWork ? s.wpos : s.mpos;
    const float *small = g_showWork ? s.mpos : s.wpos;
    const char  *tag   = g_showWork ? "M" : "W";
    for (int i = 0; i < 3; i++) {
        char b[16];
        snprintf(b, sizeof(b), "%.3f", big[i]);
        lv_label_set_text(g_axisBig[i], b);
        char sm[24];
        snprintf(sm, sizeof(sm), "%s %.3f", tag, small[i]);
        lv_label_set_text(g_axisSmall[i], sm);
    }

    // ---- Job progress (strip + jobs view) ----
    if (Grbl.jobActive()) {
        int pct = (int)(Grbl.jobProgress() * 100.0f);
        char b[80];
        snprintf(b, sizeof(b), LV_SYMBOL_FILE "  %s  %u/%u  %d%%%s",
                 Grbl.jobName().c_str(), (unsigned)Grbl.jobLinesDone(),
                 (unsigned)Grbl.jobTotalLines(), pct, Grbl.jobPaused() ? "  (paused)" : "");
        lv_label_set_text(g_jobStripLbl, b);
        lv_label_set_text_fmt(g_jobCurLbl, "Current job: %s%s", Grbl.jobName().c_str(),
                              Grbl.jobPaused() ? "  (paused)" : "");
        lv_bar_set_value(g_jobBar, pct, LV_ANIM_OFF);
        lv_label_set_text_fmt(g_jobPctLbl, "%d%%", pct);
    } else {
        lv_label_set_text(g_jobStripLbl, LV_SYMBOL_FILE "  No job loaded");
        lv_label_set_text(g_jobCurLbl, "Current job: none");
        lv_bar_set_value(g_jobBar, 0, LV_ANIM_OFF);
        lv_label_set_text(g_jobPctLbl, "0%");
    }

    // ---- Network indicator + WiFi status ----
    const char *wmode = wifi_mode_str();
    bool wup = wifi_up();
    String ip = wifi_ip();
    lv_label_set_text_fmt(g_netLbl, LV_SYMBOL_WIFI "  %s %s", wmode,
                          ip.length() ? ip.c_str() : "--");
    lv_obj_set_style_text_color(g_netLbl, wup ? COL_ACCENT : COL_MUTED, 0);
    if (g_wifiStatus)
        lv_label_set_text_fmt(g_wifiStatus, "Mode: %s   IP: %s   SSID: %s",
                              wmode, ip.length() ? ip.c_str() : "--",
                              wifi_ssid().length() ? wifi_ssid().c_str() : "--");

    // ---- WiFi scan poll ----
    if (g_scanPending) {
        int n = wifi_scan_poll(g_ssids);
        if (n >= 0) {
            g_scanPending = false;
            String opts;
            if (g_ssids.empty()) {
                opts = "(no networks found)";
            } else {
                for (size_t i = 0; i < g_ssids.size(); i++) {
                    if (i) opts += "\n";
                    opts += g_ssids[i].ssid;
                }
            }
            if (g_ssidDrop) lv_dropdown_set_options(g_ssidDrop, opts.c_str());
        }
    }
}

// ===========================================================================
//  Console
// ===========================================================================
static void render_console()
{
    if (!g_console) return;
    String text;
    text.reserve(g_log.size() * 24);
    for (auto &l : g_log) { text += l; text += '\n'; }
    lv_textarea_set_text(g_console, text.c_str());
    lv_textarea_set_cursor_pos(g_console, LV_TEXTAREA_CURSOR_LAST);
}

void ui_console_log(const String &line, bool outgoing)
{
    g_log.push_back(String(outgoing ? "> " : "< ") + line);
    while (g_log.size() > 200) g_log.pop_front();
    render_console();
}

void ui_set_job(const String &name)
{
    if (!g_jobStripLbl) return;
    if (name.length())
        lv_label_set_text(g_jobStripLbl, (String(LV_SYMBOL_FILE "  ") + name).c_str());
    else
        lv_label_set_text(g_jobStripLbl, LV_SYMBOL_FILE "  No job loaded");
}

// ===========================================================================
//  Shared display mode (with the web UI)
// ===========================================================================
void ui_set_pos_mode(bool showWork)
{
    g_showWork = showWork;
    if (g_posToggle) {
        lv_btnmatrix_set_btn_ctrl(g_posToggle, showWork ? 0 : 1, LV_BTNMATRIX_CTRL_CHECKED);
        lv_btnmatrix_clear_btn_ctrl(g_posToggle, showWork ? 1 : 0, LV_BTNMATRIX_CTRL_CHECKED);
    }
}

bool ui_get_pos_mode() { return g_showWork; }
