#include "ui.h"
#include "grbl.h"
#include <lvgl.h>

// ---------------------------------------------------------------------------
//  Action codes carried in each widget's user_data
// ---------------------------------------------------------------------------
enum Action {
    ACT_HOLD = 1, ACT_RESUME, ACT_RESET, ACT_UNLOCK, ACT_HOME,
    ACT_ZERO_X, ACT_ZERO_Y, ACT_ZERO_Z, ACT_ZERO_ALL, ACT_GOXY0,
    ACT_JOG_XP, ACT_JOG_XN, ACT_JOG_YP, ACT_JOG_YN, ACT_JOG_ZP, ACT_JOG_ZN,
    ACT_JOB_RUN, ACT_JOB_STOP, ACT_JOB_DELETE, ACT_JOB_REFRESH,
    ACT_CONSOLE_CLEAR,
};

// ---------------------------------------------------------------------------
//  Widget handles + UI state
// ---------------------------------------------------------------------------
namespace {

lv_obj_t *g_stateChip  = nullptr;
lv_obj_t *g_stateLabel = nullptr;
lv_obj_t *g_wifiLabel  = nullptr;

lv_obj_t *g_droVal[3]  = {nullptr, nullptr, nullptr};
lv_obj_t *g_mposLabel  = nullptr;

lv_obj_t *g_stepMtx    = nullptr;
lv_obj_t *g_feedMtx    = nullptr;

lv_obj_t *g_jobList    = nullptr;
lv_obj_t *g_selLabel   = nullptr;
lv_obj_t *g_runBtn     = nullptr;
lv_obj_t *g_progBar    = nullptr;
lv_obj_t *g_progLabel  = nullptr;

lv_obj_t *g_console    = nullptr;

float  g_step = JOG_STEP_DEFAULT;
float  g_feed = JOG_FEED_DEFAULT;
String g_selName;

const char *g_stepMap[] = {"0.1", "1", "10", ""};
const char *g_feedMap[] = {"100", "500", "1000", "2000", ""};

// colour palette
inline lv_color_t C(uint32_t hex) { return lv_color_hex(hex); }

// ---- small styling helpers ----------------------------------------------
void styleButton(lv_obj_t *btn, uint32_t bg)
{
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_bg_color(btn, C(bg), 0);
    lv_obj_set_style_bg_color(btn, C(0xffffff), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn, 0, 0);
}

lv_obj_t *makeButton(lv_obj_t *parent, const char *text, int x, int y, int w, int h,
                     Action act, const lv_font_t *font, uint32_t bg, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    styleButton(btn, bg);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, (void *)(intptr_t)act);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    if (font) lv_obj_set_style_text_font(lbl, font, 0);
    lv_obj_center(lbl);
    return btn;
}

// ---- forward decls --------------------------------------------------------
void refreshJobList();

// ---- event handlers -------------------------------------------------------
void actionCb(lv_event_t *e)
{
    Action act = (Action)(intptr_t)lv_event_get_user_data(e);
    switch (act) {
        case ACT_HOLD:      Grbl.feedHold();               break;
        case ACT_RESUME:    Grbl.resume();                 break;
        case ACT_RESET:     Grbl.softReset();              break;
        case ACT_UNLOCK:    Grbl.unlock();                 break;
        case ACT_HOME:      Grbl.home();                   break;
        case ACT_ZERO_X:    Grbl.zeroAxis('X');            break;
        case ACT_ZERO_Y:    Grbl.zeroAxis('Y');            break;
        case ACT_ZERO_Z:    Grbl.zeroAxis('Z');            break;
        case ACT_ZERO_ALL:  Grbl.zeroAll();                break;
        case ACT_GOXY0:     Grbl.gotoWorkXY0();            break;
        case ACT_JOG_XP:    Grbl.jog('X',  g_step, g_feed); break;
        case ACT_JOG_XN:    Grbl.jog('X', -g_step, g_feed); break;
        case ACT_JOG_YP:    Grbl.jog('Y',  g_step, g_feed); break;
        case ACT_JOG_YN:    Grbl.jog('Y', -g_step, g_feed); break;
        case ACT_JOG_ZP:    Grbl.jog('Z',  g_step, g_feed); break;
        case ACT_JOG_ZN:    Grbl.jog('Z', -g_step, g_feed); break;
        case ACT_JOB_STOP:  Grbl.stopJob();                break;
        case ACT_JOB_RUN:
            if (g_selName.length() && !Grbl.jobActive() && !WebUI::isUploading()) {
                Grbl.startJob(JobStore::fullPath(g_selName));
            }
            break;
        case ACT_JOB_DELETE:
            if (g_selName.length() && !Grbl.jobActive()) {
                JobStore::remove(g_selName);
                g_selName = "";
                if (g_selLabel) lv_label_set_text(g_selLabel, "No file selected");
                refreshJobList();
            }
            break;
        default: break;
    }
}

void consoleClearCb(lv_event_t *e)
{
    if (g_console) lv_textarea_set_text(g_console, "");
}

void stepMtxCb(lv_event_t *e)
{
    lv_obj_t *m = lv_event_get_target(e);
    uint16_t id = lv_btnmatrix_get_selected_btn(m);
    const char *t = lv_btnmatrix_get_btn_text(m, id);
    if (t) g_step = atof(t);
}

void feedMtxCb(lv_event_t *e)
{
    lv_obj_t *m = lv_event_get_target(e);
    uint16_t id = lv_btnmatrix_get_selected_btn(m);
    const char *t = lv_btnmatrix_get_btn_text(m, id);
    if (t) g_feed = atof(t);
}

void jobItemCb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    const char *txt = lv_list_get_btn_text(g_jobList, btn);
    if (txt) {
        g_selName = txt;
        if (g_selLabel) lv_label_set_text_fmt(g_selLabel, "Selected: %s", txt);
    }
}

void jobRefreshCb(lv_event_t *e) { refreshJobList(); }

void refreshJobList()
{
    if (!g_jobList) return;
    lv_obj_clean(g_jobList);
    auto files = JobStore::list();
    for (auto &f : files) {
        lv_obj_t *b = lv_list_add_btn(g_jobList, LV_SYMBOL_FILE, f.name.c_str());
        lv_obj_add_event_cb(b, jobItemCb, LV_EVENT_CLICKED, nullptr);
    }
    if (files.empty()) {
        lv_list_add_text(g_jobList, "No jobs. Upload over WiFi.");
    }
}

// ---- screen construction --------------------------------------------------
void buildTopBar(lv_obj_t *scr)
{
    lv_obj_t *bar = lv_obj_create(scr);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_size(bar, 800, 60);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, C(0x0f1218), 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);

    g_stateChip = lv_obj_create(bar);
    lv_obj_set_pos(g_stateChip, 8, 8);
    lv_obj_set_size(g_stateChip, 180, 44);
    lv_obj_clear_flag(g_stateChip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(g_stateChip, 8, 0);
    lv_obj_set_style_border_width(g_stateChip, 0, 0);
    lv_obj_set_style_bg_color(g_stateChip, C(0x7f8c8d), 0);
    g_stateLabel = lv_label_create(g_stateChip);
    lv_obj_set_style_text_font(g_stateLabel, &lv_font_montserrat_24, 0);
    lv_label_set_text(g_stateLabel, "---");
    lv_obj_center(g_stateLabel);

    g_wifiLabel = lv_label_create(bar);
    lv_obj_set_pos(g_wifiLabel, 200, 12);
    lv_obj_set_style_text_font(g_wifiLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_wifiLabel, C(0x9aa4b6), 0);
    lv_label_set_text(g_wifiLabel, "WiFi ...");

    makeButton(bar, "HOLD",  506, 8, 90, 44, ACT_HOLD,   &lv_font_montserrat_16, 0xe67e22, actionCb);
    makeButton(bar, "RESUME",604, 8, 90, 44, ACT_RESUME, &lv_font_montserrat_16, 0x27ae60, actionCb);
    makeButton(bar, "RESET", 702, 8, 90, 44, ACT_RESET,  &lv_font_montserrat_16, 0xc0392b, actionCb);
}

void makeAxisRow(lv_obj_t *parent, const char *axis, int y, int idx)
{
    lv_obj_t *a = lv_label_create(parent);
    lv_obj_set_pos(a, 6, y + 10);
    lv_obj_set_style_text_font(a, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(a, C(0x6fa8ff), 0);
    lv_label_set_text(a, axis);

    lv_obj_t *v = lv_label_create(parent);
    lv_obj_set_pos(v, 60, y);
    lv_obj_set_size(v, 290, 50);
    lv_obj_set_style_text_font(v, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(v, "0.000");
    g_droVal[idx] = v;
}

void buildControlTab(lv_obj_t *tab)
{
    lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(tab, 6, 0);

    // ---- DRO panel ----
    lv_obj_t *dro = lv_obj_create(tab);
    lv_obj_set_pos(dro, 0, 0);
    lv_obj_set_size(dro, 360, 200);
    lv_obj_clear_flag(dro, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(dro, C(0x1b1f2a), 0);
    lv_obj_set_style_border_width(dro, 0, 0);
    lv_obj_set_style_pad_all(dro, 4, 0);
    makeAxisRow(dro, "X", 6, 0);
    makeAxisRow(dro, "Y", 62, 1);
    makeAxisRow(dro, "Z", 118, 2);
    g_mposLabel = lv_label_create(dro);
    lv_obj_set_pos(g_mposLabel, 6, 172);
    lv_obj_set_style_text_font(g_mposLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_mposLabel, C(0x8a93a6), 0);
    lv_label_set_text(g_mposLabel, "M: 0.000  0.000  0.000");

    // ---- jog pad ----
    lv_obj_t *jog = lv_obj_create(tab);
    lv_obj_set_pos(jog, 365, 0);
    lv_obj_set_size(jog, 415, 200);
    lv_obj_clear_flag(jog, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(jog, C(0x1b1f2a), 0);
    lv_obj_set_style_border_width(jog, 0, 0);
    lv_obj_set_style_pad_all(jog, 4, 0);
    const lv_font_t *jf = &lv_font_montserrat_20;
    makeButton(jog, "Y+", 95,   4, 76, 52, ACT_JOG_YP, jf, 0x2f6bff, actionCb);
    makeButton(jog, "X-", 10,  64, 76, 52, ACT_JOG_XN, jf, 0x2f6bff, actionCb);
    makeButton(jog, "X+", 180, 64, 76, 52, ACT_JOG_XP, jf, 0x2f6bff, actionCb);
    makeButton(jog, "Y-", 95, 124, 76, 52, ACT_JOG_YN, jf, 0x2f6bff, actionCb);
    makeButton(jog, "Z+", 320,  4, 76, 52, ACT_JOG_ZP, jf, 0x8e44ad, actionCb);
    makeButton(jog, "Z-", 320,124, 76, 52, ACT_JOG_ZN, jf, 0x8e44ad, actionCb);

    // ---- step / feed selectors ----
    lv_obj_t *stepCap = lv_label_create(tab);
    lv_obj_set_pos(stepCap, 4, 202);
    lv_obj_set_style_text_font(stepCap, &lv_font_montserrat_14, 0);
    lv_label_set_text(stepCap, "STEP (mm)");
    lv_obj_t *feedCap = lv_label_create(tab);
    lv_obj_set_pos(feedCap, 400, 202);
    lv_obj_set_style_text_font(feedCap, &lv_font_montserrat_14, 0);
    lv_label_set_text(feedCap, "FEED (mm/min)");

    g_stepMtx = lv_btnmatrix_create(tab);
    lv_btnmatrix_set_map(g_stepMtx, g_stepMap);
    lv_obj_set_pos(g_stepMtx, 0, 220);
    lv_obj_set_size(g_stepMtx, 388, 44);
    lv_btnmatrix_set_one_checked(g_stepMtx, true);
    lv_btnmatrix_set_btn_ctrl_all(g_stepMtx, LV_BTNMATRIX_CTRL_CHECKABLE);
    lv_btnmatrix_set_btn_ctrl(g_stepMtx, 1, LV_BTNMATRIX_CTRL_CHECKED);  // "1"
    lv_obj_add_event_cb(g_stepMtx, stepMtxCb, LV_EVENT_VALUE_CHANGED, nullptr);

    g_feedMtx = lv_btnmatrix_create(tab);
    lv_btnmatrix_set_map(g_feedMtx, g_feedMap);
    lv_obj_set_pos(g_feedMtx, 396, 220);
    lv_obj_set_size(g_feedMtx, 388, 44);
    lv_btnmatrix_set_one_checked(g_feedMtx, true);
    lv_btnmatrix_set_btn_ctrl_all(g_feedMtx, LV_BTNMATRIX_CTRL_CHECKABLE);
    lv_btnmatrix_set_btn_ctrl(g_feedMtx, 2, LV_BTNMATRIX_CTRL_CHECKED);  // "1000"
    lv_obj_add_event_cb(g_feedMtx, feedMtxCb, LV_EVENT_VALUE_CHANGED, nullptr);

    // ---- action buttons ----
    const lv_font_t *af = &lv_font_montserrat_14;
    int y = 300, w = 104, h = 52, gap = 8, x = 4;
    makeButton(tab, "Zero X",  x + 0 * (w + gap), y, w, h, ACT_ZERO_X,   af, 0x34495e, actionCb);
    makeButton(tab, "Zero Y",  x + 1 * (w + gap), y, w, h, ACT_ZERO_Y,   af, 0x34495e, actionCb);
    makeButton(tab, "Zero Z",  x + 2 * (w + gap), y, w, h, ACT_ZERO_Z,   af, 0x34495e, actionCb);
    makeButton(tab, "Zero All",x + 3 * (w + gap), y, w, h, ACT_ZERO_ALL, af, 0x16a085, actionCb);
    makeButton(tab, "Home",    x + 4 * (w + gap), y, w, h, ACT_HOME,     af, 0x2f6bff, actionCb);
    makeButton(tab, "Unlock",  x + 5 * (w + gap), y, w, h, ACT_UNLOCK,   af, 0xd35400, actionCb);
    makeButton(tab, "Go XY0",  x + 6 * (w + gap), y, w, h, ACT_GOXY0,    af, 0x34495e, actionCb);
}

void buildJobsTab(lv_obj_t *tab)
{
    lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(tab, 6, 0);

    g_jobList = lv_list_create(tab);
    lv_obj_set_pos(g_jobList, 0, 0);
    lv_obj_set_size(g_jobList, 470, 360);
    lv_obj_set_style_bg_color(g_jobList, C(0x1b1f2a), 0);

    g_selLabel = lv_label_create(tab);
    lv_obj_set_pos(g_selLabel, 480, 4);
    lv_obj_set_width(g_selLabel, 300);
    lv_label_set_long_mode(g_selLabel, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(g_selLabel, &lv_font_montserrat_20, 0);
    lv_label_set_text(g_selLabel, "No file selected");

    const lv_font_t *bf = &lv_font_montserrat_16;
    g_runBtn = makeButton(tab, "RUN",     480, 90,  140, 56, ACT_JOB_RUN,   bf, 0x27ae60, actionCb);
    makeButton(tab, "STOP",    640, 90,  140, 56, ACT_JOB_STOP,   bf, 0xc0392b, actionCb);
    makeButton(tab, "Delete",  480, 156, 140, 44, ACT_JOB_DELETE, bf, 0x34495e, actionCb);
    makeButton(tab, "Refresh", 640, 156, 140, 44, ACT_JOB_REFRESH,bf, 0x34495e, jobRefreshCb);

    g_progBar = lv_bar_create(tab);
    lv_obj_set_pos(g_progBar, 480, 224);
    lv_obj_set_size(g_progBar, 300, 16);
    lv_bar_set_range(g_progBar, 0, 100);
    lv_bar_set_value(g_progBar, 0, LV_ANIM_OFF);

    g_progLabel = lv_label_create(tab);
    lv_obj_set_pos(g_progLabel, 480, 250);
    lv_obj_set_style_text_font(g_progLabel, &lv_font_montserrat_16, 0);
    lv_label_set_text(g_progLabel, "idle");

    lv_obj_t *hint = lv_label_create(tab);
    lv_obj_set_pos(hint, 480, 300);
    lv_obj_set_width(hint, 300);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint, C(0x8a93a6), 0);
    lv_label_set_text_fmt(hint, "Upload jobs over WiFi:\nJoin '%s' then open http://%s",
                          WebUI::ssid().c_str(), WebUI::ipAddress().c_str());

    refreshJobList();
}

void buildConsoleTab(lv_obj_t *tab)
{
    lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(tab, 6, 0);

    g_console = lv_textarea_create(tab);
    lv_obj_set_pos(g_console, 0, 0);
    lv_obj_set_size(g_console, 780, 310);
    lv_textarea_set_max_length(g_console, 6000);
    lv_textarea_set_cursor_click_pos(g_console, false);
    lv_obj_set_style_text_font(g_console, &lv_font_montserrat_14, 0);

    makeButton(tab, "Clear", 0, 320, 120, 40, ACT_CONSOLE_CLEAR, &lv_font_montserrat_16,
               0x34495e, consoleClearCb);
}

}  // namespace

// ---------------------------------------------------------------------------
//  Public API
// ---------------------------------------------------------------------------
void Ui::create()
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, C(0x12151c), 0);

    buildTopBar(scr);

    lv_obj_t *tv = lv_tabview_create(scr, LV_DIR_TOP, 44);
    lv_obj_set_pos(tv, 0, 60);
    lv_obj_set_size(tv, 800, 420);
    lv_obj_set_style_bg_color(tv, C(0x12151c), 0);

    buildControlTab(lv_tabview_add_tab(tv, "Control"));
    buildJobsTab(lv_tabview_add_tab(tv, "Jobs"));
    buildConsoleTab(lv_tabview_add_tab(tv, "Console"));
}

void Ui::tick()
{
    const GrblSnapshot &s = Grbl.status();

    // state chip
    uint32_t col = 0x7f8c8d;
    const char *name = s.stateName;
    if (!s.connected) { col = 0x7f8c8d; name = "NO LINK"; }
    else switch (s.state) {
        case GrblState::Idle:  col = 0x27ae60; break;
        case GrblState::Run:
        case GrblState::Jog:
        case GrblState::Home:  col = 0x2f6bff; break;
        case GrblState::Hold:
        case GrblState::Door:  col = 0xe67e22; break;
        case GrblState::Alarm: col = 0xc0392b; break;
        default:               col = 0x7f8c8d; break;
    }
    if (g_stateChip)  lv_obj_set_style_bg_color(g_stateChip, C(col), 0);
    if (g_stateLabel) lv_label_set_text(g_stateLabel, name);

    // DRO
    char buf[24];
    for (int i = 0; i < 3; i++) {
        if (!g_droVal[i]) continue;
        snprintf(buf, sizeof(buf), "%.3f", s.wpos[i]);
        lv_label_set_text(g_droVal[i], buf);
    }
    if (g_mposLabel) {
        lv_label_set_text_fmt(g_mposLabel, "M: %.3f  %.3f  %.3f",
                              s.mpos[0], s.mpos[1], s.mpos[2]);
    }

    // wifi label (static content, cheap to refresh)
    if (g_wifiLabel) {
        lv_label_set_text_fmt(g_wifiLabel, "WiFi: %s\n%s",
                              WebUI::ssid().c_str(), WebUI::ipAddress().c_str());
    }

    // job progress
    if (g_progBar && g_progLabel) {
        if (Grbl.jobActive()) {
            int pct = (int)(Grbl.jobProgress() * 100);
            lv_bar_set_value(g_progBar, pct, LV_ANIM_OFF);
            lv_label_set_text_fmt(g_progLabel, "%s  line %u/%u  %d%%",
                                  Grbl.jobName().c_str(),
                                  (unsigned)Grbl.jobLinesDone(),
                                  (unsigned)Grbl.jobTotalLines(), pct);
        } else if (WebUI::isUploading()) {
            lv_label_set_text(g_progLabel, "uploading...");
        }
    }
}

void Ui::logLine(const String &line, bool outgoing)
{
    if (!g_console) return;
    // Keep the textarea from growing without bound.
    const char *cur = lv_textarea_get_text(g_console);
    if (cur && strlen(cur) > 5000) lv_textarea_set_text(g_console, "");
    lv_textarea_add_text(g_console, outgoing ? "> " : "  ");
    lv_textarea_add_text(g_console, line.c_str());
    lv_textarea_add_text(g_console, "\n");
}
