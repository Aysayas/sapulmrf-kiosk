// The kiosk's screens: Welcome -> Purok -> Scan -> Weigh -> Receipt for residents, PIN -> Operator for staff.
// English / Tagalog, switchable on every screen; the language goes back to the default after each drop-off.
#include "ui.h"

#include "config.h"
#include "theme.h"

#ifndef DEFAULT_LANG
#define DEFAULT_LANG 1   // 0 = English, 1 = Tagalog
#endif

uint16_t *uiFrameBuf = nullptr;

// ------------------------------------------------------------ words (English, Tagalog)
enum Str {
  S_HELLO, S_HOW, S_START, S_START_SUB, S_STAFF, S_TODAY, S_TODAY_WAIT,
  S_PUROK_Q, S_PUROK, S_BACK, S_STEP1, S_STEP2, S_STEP3,
  S_SHOW, S_LOOKING, S_STEADY, S_NOTHING, S_GOT_IT, S_CHOOSE, S_LIVE, S_WAIT_CAM, S_PC_LOST,
  S_KIND_Q, S_WEIGHT_Q, S_WEIGHT_KG, S_FROM_CAMERA, S_YOU_CHOSE, S_FROM_SCALE, S_CONFIRM, S_ENTER_WEIGHT, S_PC_OFFLINE,
  S_THANKS, S_SAVED, S_SENDING, S_OFFLINE_T, S_OFFLINE, S_NOT_SAVED_T, S_NOT_SAVED, S_CODE, S_WEIGHT, S_KIND,
  S_BIN, S_COUNTER, S_DONE, S_RETURN,
  S_OP_MODE, S_OP_PIN, S_CLEAR, S_ENTER, S_PIN_4, S_CHECKING, S_WRONG_PIN, S_NO_ANSWER, S_PC_OFF_PIN, S_BACK_DROP,
  S_RECORDS, S_RECORDS_SUB, S_REFRESH, S_LOCK, S_PC_OFF_TABLE,
  S_PICK_KIND, S_CHANGE, S_NO_ROWS, S_FLOW1, S_FLOW2, S_FLOW3, S_RECORDING_T,
  S_COUNT
};
static const char *const WORDS[S_COUNT][2] = {
  {"Good day!", "Magandang araw!"},
  {"Open your bag and spread the items on the tray.", "Buksan ang supot at ikalat ang basura sa tray."},
  {"Start", "Simulan"},
  {"Tap to begin", "Pindutin para magsimula"},
  {LV_SYMBOL_SETTINGS "  Staff", LV_SYMBOL_SETTINGS "  Kawani"},
  {"Today: %s kg  ·  %d drop-offs", "Ngayon: %s kg  ·  %d nagdala"},
  {"Waiting for the system...", "Hinihintay ang sistema..."},
  {"Which purok are you from?", "Taga-saang purok ka?"},
  {"Purok", "Purok"},
  {"Back", "Bumalik"},
  {"Step 1 of 3", "Hakbang 1 sa 3"},
  {"Step 2 of 3", "Hakbang 2 sa 3"},
  {"Step 3 of 3", "Hakbang 3 sa 3"},
  {"Show your waste", "Ipakita ang basura"},
  {"Looking...", "Tinitingnan..."},
  {"Hold steady...", "Huwag munang galawin..."},
  {"Nothing seen yet. Spread the items apart or move them closer.", "Wala pang nakikita. Ikalat ang mga gamit o ilapit pa."},
  {"Got it: %s", "Nakuha: %s"},
  {"Choose", "Pumili"},
  {"LIVE", "LIVE"},
  {"Waiting for the camera...", "Hinihintay ang camera..."},
  {"Camera PC disconnected. Waiting...", "Naputol ang camera PC. Naghihintay..."},
  {"What kind of waste?", "Anong uri ng basura?"},
  {"How heavy is it?", "Gaano kabigat?"},
  {"Weight", "Timbang"},
  {"from the camera", "mula sa camera"},
  {"you chose", "pinili mo"},
  {"from the scale", "mula sa timbangan"},
  {"Confirm", "Kumpirmahin"},
  {"Enter the weight", "Ilagay ang timbang"},
  {"Camera PC offline", "Offline ang camera PC"},
  {"Thank you!", "Salamat!"},
  {"Your drop-off is recorded.", "Naitala na ang iyong basura."},
  {"Recording...", "Itinatala..."},
  {"Saved on the kiosk", "Naitala sa kiosk"},
  {"It will be sent when the system is back.", "Ipapadala ito pagbalik ng sistema."},
  {"Not saved", "Hindi naitala"},
  {"Please ask the MRF staff.", "Magtanong sa kawani ng MRF."},
  {"Receipt code", "Code ng resibo"},
  {"Weight", "Timbang"},
  {"Kind", "Uri"},
  {"Put it in the %s bin", "Ilagay sa %s na basurahan"},
  {"Leave it at the intake counter", "Iwan sa intake counter"},
  {"Done", "Tapos na"},
  {"Back to start: %d s", "Pabalik sa simula: %d s"},
  {"STAFF MODE", "PARA SA KAWANI"},
  {"Enter your\n4-digit PIN", "Ilagay ang\n4-digit na PIN"},
  {"Clear", "Burahin"},
  {"Enter", "Pasok"},
  {"Enter all 4 digits", "Ilagay ang 4 na numero"},
  {"Checking...", "Sinusuri..."},
  {"Wrong PIN, try again", "Mali ang PIN, subukan muli"},
  {"No answer from the camera PC. Try again.", "Walang sagot ang camera PC. Subukan muli."},
  {"Wrong PIN (camera PC offline)", "Mali ang PIN (offline ang camera PC)"},
  {"Back to drop-off", "Bumalik sa drop-off"},
  {"Recent drop-offs", "Mga huling nagdala"},
  {"Newest first, from SapulMRF", "Pinakabago muna, mula sa SapulMRF"},
  {LV_SYMBOL_REFRESH "  Refresh", LV_SYMBOL_REFRESH "  I-refresh"},
  {LV_SYMBOL_CLOSE "  Lock", LV_SYMBOL_CLOSE "  I-lock"},
  {"Camera PC offline", "Offline ang camera PC"},
  {"Choose the kind  " LV_SYMBOL_RIGHT, "Pumili ng uri  " LV_SYMBOL_RIGHT},
  {"Change", "Palitan"},
  {"No drop-offs yet", "Wala pang nagdala"},
  {"Purok", "Purok"},
  {"Camera", "Camera"},
  {"Weight", "Timbang"},
  {"Recording...", "Itinatala..."},
};
static int lang = DEFAULT_LANG;
static const char *T(Str s) { return WORDS[s][lang]; }

// Categories: the English name is what SapulMRF stores; the rest is how the kiosk shows it.
struct Cat { const char *key; const char *name[2]; uint32_t color; const char *bin[2]; };
static const Cat CATS[4] = {
  {"Mixed household", {"Mixed", "Halo-halo"}, T_CAT_MIXED, {nullptr, nullptr}},
  {"Recyclable", {"Recyclable", "Nare-recycle"}, T_CAT_RECYC, {"BLUE", "ASUL"}},
  {"Biodegradable", {"Biodegradable", "Nabubulok"}, T_CAT_BIO, {"GREEN", "BERDE"}},
  {"Residual", {"Residual", "Di-nabubulok"}, T_CAT_RESID, {"BLACK", "ITIM"}},
};

// ------------------------------------------------------------ state
static int category = 0;            // index into CATS
static bool catFromCamera = false;
static bool catSet = false;        // false = nobody chose yet: Weigh asks for it
static int purok = 0;               // 0 = not chosen
static float weightKg = 0;
static bool weightFromScale = false;
static String weightEntry = "";
static String pinEntry = "";
static uint32_t pinAskedAt = 0;
static int receiptCountdown = 0;
static bool frozen = false;         // scan screen holds the frame of a stable result
static uint32_t scanSince = 0, lastSeen = 0;
static int detCounts[3] = {0, 0, 0};   // recyclable, residual, bio
static bool afterDropOff = false;      // going home from a receipt resets the language

// screens and the widgets that change
static lv_obj_t *scrHome, *scrPurok, *scrScan, *scrPick, *scrWeigh, *scrReceipt, *scrPin, *scrOp;
static lv_obj_t *lblToday, *alertBox, *lblAlert;
static lv_obj_t *hintBar, *lblHint;
static lv_obj_t *imgVideo, *lblVideoWait, *lblLive, *frameRing, *gotBar, *lblGot, *lblScanStatus, *barSteady;
static lv_obj_t *lblCount[3];
static lv_obj_t *chipPurok, *chipCat, *lblWeight, *btnConfirm, *kpWeight;
static lv_obj_t *rcBand, *rcIcon, *rcTitle, *rcSub, *lblCode, *rcWeight, *rcKind, *rcPurok, *rcBin, *lblBin, *barCountdown, *lblCountdown;
static lv_obj_t *pinDots[4], *lblPinMsg, *tblRecent, *lblNoRows, *opAlert, *lblOpAlert, *scanTile[3];
// status strip (lv_layer_top: survives rebuilding the screens)
static lv_obj_t *statusDot, *batPill, *lblBat, *langBtn[2];
static lv_timer_t *receiptTimer = nullptr, *freezeTimer = nullptr;
static String lastAlert = "", lastNet = "", lastIp = "";
static int lastAlertMore = -1;
static char todayText[96] = "";

void goTo(lv_obj_t *scr);
static void buildAll(int show = -1);

// ------------------------------------------------------------ components
static lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h, uint32_t bg, int radius = 0) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_bg_color(o, lv_color_hex(bg), 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(o, radius, 0);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  return o;
}

static lv_obj_t *text(lv_obj_t *parent, const char *s, const lv_font_t *f, uint32_t color, int x = 0, int y = 0, int wrapW = 0) {
  lv_obj_t *l = lv_label_create(parent);
  lv_label_set_text(l, s);
  lv_obj_set_style_text_font(l, f, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
  if (wrapW) { lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP); lv_obj_set_width(l, wrapW); }
  lv_obj_set_pos(l, x, y);
  return l;
}

// A big button: one label, a darker shade while pressed, faded with the reason shown when disabled.
static lv_obj_t *button(lv_obj_t *parent, const char *s, int x, int y, int w, int h, uint32_t bg, uint32_t fg,
                        const lv_font_t *f, lv_event_cb_t cb, void *user = nullptr, uint32_t border = 0) {
  lv_obj_t *b = lv_btn_create(parent);
  lv_obj_set_pos(b, x, y);
  lv_obj_set_size(b, w, h);
  lv_obj_set_style_radius(b, 16, 0);
  lv_obj_set_style_shadow_width(b, 0, 0);
  lv_obj_set_style_bg_color(b, lv_color_hex(bg), 0);
  lv_obj_set_style_bg_color(b, lv_color_darken(lv_color_hex(bg), LV_OPA_20), LV_STATE_PRESSED);
  lv_obj_set_style_bg_color(b, lv_color_hex(T_LINE), LV_STATE_DISABLED);
  lv_obj_set_style_text_color(b, lv_color_hex(T_INK2), LV_STATE_DISABLED);
  if (border) { lv_obj_set_style_border_width(b, 2, 0); lv_obj_set_style_border_color(b, lv_color_hex(border), 0); }
  lv_obj_t *l = lv_label_create(b);
  lv_label_set_text(l, s);
  lv_obj_set_style_text_font(l, f, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(fg), 0);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(l, w - 16);
  lv_obj_center(l);
  if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);
  return b;
}

static void setButtonText(lv_obj_t *b, const char *s) { lv_label_set_text(lv_obj_get_child(b, 0), s); }

static lv_obj_t *keypad(lv_obj_t *parent, const char **map, int x, int y, int w, int h, lv_event_cb_t cb) {
  lv_obj_t *k = lv_btnmatrix_create(parent);
  lv_btnmatrix_set_map(k, map);
  lv_obj_set_pos(k, x, y);
  lv_obj_set_size(k, w, h);
  lv_obj_set_style_bg_opa(k, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(k, 0, 0);
  lv_obj_set_style_pad_all(k, 0, 0);
  lv_obj_set_style_pad_gap(k, 12, 0);
  lv_obj_set_style_bg_color(k, lv_color_hex(T_CARD), LV_PART_ITEMS);
  lv_obj_set_style_bg_color(k, lv_color_hex(T_SOFT), LV_PART_ITEMS | LV_STATE_PRESSED);
  lv_obj_set_style_border_color(k, lv_color_hex(T_LINE), LV_PART_ITEMS);
  lv_obj_set_style_border_width(k, 2, LV_PART_ITEMS);
  lv_obj_set_style_radius(k, 14, LV_PART_ITEMS);
  lv_obj_set_style_shadow_width(k, 0, LV_PART_ITEMS);
  lv_obj_set_style_text_font(k, &atk_b32, LV_PART_ITEMS);
  lv_obj_set_style_text_color(k, lv_color_hex(T_INK), LV_PART_ITEMS);
  lv_obj_add_event_cb(k, cb, LV_EVENT_VALUE_CHANGED, nullptr);
  return k;
}

static lv_obj_t *progress(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color) {
  lv_obj_t *b = lv_bar_create(parent);
  lv_obj_set_pos(b, x, y);
  lv_obj_set_size(b, w, h);
  lv_bar_set_range(b, 0, 100);
  lv_obj_set_style_bg_color(b, lv_color_hex(T_LINE), 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(b, lv_color_hex(color), LV_PART_INDICATOR);
  lv_obj_set_style_radius(b, h / 2, 0);
  lv_obj_set_style_radius(b, h / 2, LV_PART_INDICATOR);
  return b;
}

static lv_obj_t *screen() {
  lv_obj_t *s = lv_obj_create(nullptr);
  lv_obj_remove_style_all(s);
  lv_obj_set_size(s, 800, 480);
  lv_obj_set_style_bg_color(s, lv_color_hex(T_BG), 0);
  lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
  lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
  return s;
}

static void fmtKg(char *out, size_t n, float kg) { snprintf(out, n, "%.1f", kg); }   // LVGL's printf has no %f

// ------------------------------------------------------------ status strip (every screen)
static void onLang(lv_event_t *e) {
  int l = (int)(intptr_t)lv_event_get_user_data(e);
  if (l == lang) return;
  lang = l;
  buildAll();
}

static void paintLang() {
  for (int i = 0; i < 2; i++) {
    bool on = i == lang;
    lv_obj_set_style_bg_color(langBtn[i], lv_color_hex(on ? T_BRAND : T_CARD), 0);
    lv_obj_set_style_text_color(lv_obj_get_child(langBtn[i], 0), lv_color_hex(on ? 0xFFFFFF : T_INK), 0);
  }
}

static void buildStatusStrip() {
  lv_obj_t *top = lv_layer_top();
  lv_obj_t *brand = text(top, "SapulMRF", &atk_b24, T_BRAND, T_MARGIN, 14);
  lv_obj_clear_flag(brand, LV_OBJ_FLAG_CLICKABLE);
  const char *names[2] = {"EN", "TL"};
  for (int i = 0; i < 2; i++) {
    langBtn[i] = button(top, names[i], 800 - T_MARGIN - 140 + i * 70, 6, 70, 44, T_CARD, T_INK, &atk_b24, onLang,
                        (void *)(intptr_t)i, T_LINE);
    lv_obj_set_style_radius(langBtn[i], 0, 0);
  }
  lv_obj_set_style_radius(langBtn[0], 12, 0);   // a two-part pill
  lv_obj_set_style_radius(langBtn[1], 12, 0);
  paintLang();
  statusDot = box(top, 800 - T_MARGIN - 140 - 32, 20, 16, 16, T_LINE, LV_RADIUS_CIRCLE);   // grey until SapulMRF answers
  batPill = box(top, 800 - T_MARGIN - 140 - 32 - 112, 12, 100, 32, T_CARD, 16);
  lv_obj_set_style_border_width(batPill, 2, 0);
  lv_obj_set_style_border_color(batPill, lv_color_hex(T_LINE), 0);
  lblBat = text(batPill, "", &atk_r18, T_INK);
  lv_obj_center(lblBat);
  lv_obj_add_flag(batPill, LV_OBJ_FLAG_HIDDEN);
}

void uiSetLight(const char *c) {
  uint32_t col = !strcmp(c, "green") ? T_OK : !strcmp(c, "amber") ? T_WARN : !strcmp(c, "red") ? T_ERR : T_LINE;
  lv_obj_set_style_bg_color(statusDot, lv_color_hex(col), 0);
}

void uiSetBattery(const char *s, uint32_t color, bool visible) {
  if (!visible) { lv_obj_add_flag(batPill, LV_OBJ_FLAG_HIDDEN); return; }
  lv_label_set_text(lblBat, s);
  lv_obj_set_style_text_color(lblBat, lv_color_hex(color), 0);
  lv_obj_clear_flag(batPill, LV_OBJ_FLAG_HIDDEN);
}

// ------------------------------------------------------------ Welcome
static void onStart(lv_event_t *) { goTo(scrPurok); }
static void onStaff(lv_event_t *) { goTo(scrPin); }

static void buildHome() {
  scrHome = screen();
  text(scrHome, T(S_HELLO), &atk_b48, T_INK, T_MARGIN, 84);
  text(scrHome, T(S_HOW), &atk_r24, T_INK2, T_MARGIN, 156, 404);
  // How a drop-off goes, in three steps (staff alerts live on the staff screen, not here)
  const Str steps[3] = {S_FLOW1, S_FLOW2, S_FLOW3};
  for (int i = 0; i < 3; i++) {
    lv_obj_t *n = box(scrHome, T_MARGIN + i * 140, 262, 40, 40, T_BRAND, LV_RADIUS_CIRCLE);
    lv_obj_t *num = text(n, "", &atk_b24, 0xFFFFFF);
    lv_label_set_text_fmt(num, "%d", i + 1);
    lv_obj_center(num);
    text(scrHome, T(steps[i]), &atk_r18, T_INK, T_MARGIN + i * 140 + 48, 270);
  }
  alertBox = lblAlert = nullptr;
  lblToday = text(scrHome, todayText[0] ? todayText : T(S_TODAY_WAIT), &atk_r24, T_INK, T_MARGIN, 404, 420);

  lv_obj_t *start = button(scrHome, "", 476, 76, 292, 300, T_PRIMARY, 0xFFFFFF, &atk_b48, onStart);
  lv_obj_set_style_radius(start, 28, 0);
  lv_obj_t *l = lv_obj_get_child(start, 0);
  lv_label_set_text_fmt(l, "%s  " LV_SYMBOL_RIGHT, T(S_START));
  lv_obj_align(l, LV_ALIGN_CENTER, 0, -18);
  lv_obj_t *sub = text(start, T(S_START_SUB), &atk_r18, 0xDCEFE2);
  lv_obj_set_style_text_align(sub, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(sub, 260);
  lv_label_set_long_mode(sub, LV_LABEL_LONG_WRAP);
  lv_obj_align(sub, LV_ALIGN_CENTER, 0, 40);
  button(scrHome, T(S_STAFF), 576, 392, 192, 64, T_BG, T_INK2, &atk_r18, onStaff);
}

void uiOnStats(float kg, int drops) {
  char k[16]; fmtKg(k, sizeof k, kg);
  snprintf(todayText, sizeof todayText, T(S_TODAY), k, drops);
  lv_label_set_text(lblToday, todayText);
}

// Open alerts are for staff: they show on the staff screen only.
static void paintAlert() {
  if (!opAlert) return;
  if (lastAlert.length() == 0) { lv_obj_add_flag(opAlert, LV_OBJ_FLAG_HIDDEN); return; }
  if (lastAlertMore > 0) lv_label_set_text_fmt(lblOpAlert, LV_SYMBOL_WARNING "  %s  (+%d)", lastAlert.c_str(), lastAlertMore);
  else lv_label_set_text_fmt(lblOpAlert, LV_SYMBOL_WARNING "  %s", lastAlert.c_str());
  lv_obj_clear_flag(opAlert, LV_OBJ_FLAG_HIDDEN);
}

void uiOnAlerts(JsonDocument &d) {
  JsonArray items = d["items"];
  lastAlert = items.size() ? String((const char *)(items[0]["title"] | "")) : String("");
  lastAlertMore = (int)items.size() - 1;
  paintAlert();
}

// ------------------------------------------------------------ Purok
static void onPurok(lv_event_t *e) { purok = (int)(intptr_t)lv_event_get_user_data(e); goTo(scrScan); }
static void onBackHome(lv_event_t *) { goTo(scrHome); }

static void buildPurok() {
  scrPurok = screen();
  text(scrPurok, T(S_PUROK_Q), &atk_b32, T_INK, T_MARGIN, 72);
  lv_obj_t *step = text(scrPurok, T(S_STEP1), &atk_r18, T_INK2);
  lv_obj_align(step, LV_ALIGN_TOP_RIGHT, -T_MARGIN, 82);
  for (int i = 0; i < NUM_PUROKS; i++) {
    int row = i / 4, col = i % 4, inRow = row == 0 ? 4 : NUM_PUROKS - 4;
    int x0 = (800 - (inRow * 170 + (inRow - 1) * 16)) / 2;
    lv_obj_t *b = button(scrPurok, "", x0 + col * 186, 136 + row * 132, 170, 116, T_CARD, T_INK, &atk_b48, onPurok,
                         (void *)(intptr_t)(i + 1), T_LINE);
    lv_obj_t *n = lv_obj_get_child(b, 0);
    lv_label_set_text_fmt(n, "%d", i + 1);
    lv_obj_align(n, LV_ALIGN_CENTER, 0, 10);
    lv_obj_t *cap = text(b, T(S_PUROK), &atk_r18, T_INK2);
    lv_obj_align(cap, LV_ALIGN_TOP_MID, 0, 6);
  }
  button(scrPurok, T(S_BACK), T_MARGIN, 404, 180, T_TAP, T_CARD, T_INK, &atk_b24, onBackHome, nullptr, T_LINE);
}

// ------------------------------------------------------------ Scan
static void onScanBack(lv_event_t *) { goTo(scrPurok); }
static void onChoose(lv_event_t *) { goTo(scrPick); }   // also cancels the "Got it" auto-advance (goTo stops it)
static lv_img_dsc_t frameDsc;

static void buildScan() {
  scrScan = screen();
  lv_obj_t *vid = box(scrScan, 24, 64, FRAME_W, FRAME_H, T_VIDEO, 0);
  imgVideo = lv_img_create(vid);
  lv_img_set_src(imgVideo, &frameDsc);
  lv_obj_set_pos(imgVideo, 0, 0);
  lblVideoWait = text(vid, pcUp ? T(S_WAIT_CAM) : T(S_PC_LOST), &atk_r24, 0xC9D3CC);
  lv_obj_center(lblVideoWait);
  lv_obj_t *live = box(vid, 12, 12, 176, 36, T_ERR, 18);
  lblLive = text(live, T(S_LIVE), &atk_b24, 0xFFFFFF);
  lv_obj_center(lblLive);
  frameRing = box(vid, 0, 0, FRAME_W, FRAME_H, T_VIDEO, 0);   // coloured frame around a stable result
  lv_obj_set_style_bg_opa(frameRing, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(frameRing, 10, 0);
  gotBar = box(vid, 0, FRAME_H - 76, FRAME_W, 76, T_PRIMARY, 0);
  lblGot = text(gotBar, "", &atk_b32, 0xFFFFFF, 20, 0);
  lv_obj_align(lblGot, LV_ALIGN_LEFT_MID, 20, 0);
  lv_obj_t *fix = button(gotBar, T(S_CHANGE), FRAME_W - 150, 10, 136, 56, 0xFFFFFF, T_INK, &atk_b24, onChoose);
  lv_obj_add_flag(gotBar, LV_OBJ_FLAG_CLICKABLE);
  (void)fix;
  lv_obj_add_flag(frameRing, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(gotBar, LV_OBJ_FLAG_HIDDEN);
  hintBar = box(vid, 0, FRAME_H - 84, FRAME_W, 84, 0x000000, 0);   // "nothing seen" help, over the video
  lv_obj_set_style_bg_opa(hintBar, LV_OPA_70, 0);
  lblHint = text(hintBar, T(S_NOTHING), &atk_b24, 0xFFFFFF, 0, 0, FRAME_W - 40);
  lv_obj_set_style_text_align(lblHint, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(lblHint);
  lv_obj_add_flag(hintBar, LV_OBJ_FLAG_HIDDEN);

  const int px = 24 + FRAME_W + 24, pw = 800 - px - 24;
  text(scrScan, T(S_STEP2), &atk_r18, T_INK2, px, 64);
  text(scrScan, T(S_SHOW), &atk_b32, T_INK, px, 88, pw);
  lblScanStatus = text(scrScan, T(S_LOOKING), &atk_r24, T_INK2, px, 170, pw);
  barSteady = progress(scrScan, px, 238, pw, 14, T_PRIMARY);
  const int order[3] = {1, 3, 2};   // Recyclable, Residual, Biodegradable (det counts: recyclable, residual, bio)
  for (int i = 0; i < 3; i++) {
    const Cat &c = CATS[order[i]];
    lv_obj_t *t = scanTile[i] = box(scrScan, px, 262 + i * 46, pw, 40, T_CARD, 12);
    lv_obj_set_style_border_width(t, 2, 0);
    lv_obj_set_style_border_color(t, lv_color_hex(T_LINE), 0);
    lv_obj_set_style_opa(t, LV_OPA_50, 0);   // dim until the camera finds something
    box(t, 0, 0, 10, 40, c.color, 0);
    text(t, c.name[lang], &atk_r18, T_INK, 22, 8);
    lblCount[i] = text(t, "0", &atk_b24, T_INK);
    lv_obj_align(lblCount[i], LV_ALIGN_RIGHT_MID, -14, 0);
  }
  button(scrScan, T(S_BACK), px, 404, pw / 2 - 6, T_TAP, T_CARD, T_INK, &atk_b24, onScanBack, nullptr, T_LINE);
  button(scrScan, T(S_CHOOSE), px + pw / 2 + 6, 404, pw / 2 - 6, T_TAP, T_CARD, T_INK, &atk_b24, onChoose, nullptr, T_LINE);
}

static void afterFreeze(lv_timer_t *) { freezeTimer = nullptr; goTo(scrWeigh); }

void uiOnDet(JsonDocument &d) {
  detCounts[0] = d["recyclable"] | 0; detCounts[1] = d["residual"] | 0; detCounts[2] = d["bio"] | 0;
  if (lv_scr_act() != scrScan || frozen) return;
  for (int i = 0; i < 3; i++) {
    lv_label_set_text_fmt(lblCount[i], "%d", detCounts[i]);
    lv_obj_set_style_opa(scanTile[i], detCounts[i] ? LV_OPA_COVER : LV_OPA_50, 0);
  }
  const char *cat = d["category"] | "Nothing detected";
  int found = -1;
  for (int i = 0; i < 4; i++) if (!strcmp(cat, CATS[i].key)) found = i;
  float prog = d["progress"] | 0.0f;
  lv_bar_set_value(barSteady, (int)(prog * 100), LV_ANIM_ON);
  if (found < 0) {
    lv_label_set_text(lblScanStatus, T(S_LOOKING));
    return;
  }
  lastSeen = millis();
  lv_obj_add_flag(hintBar, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_bg_color(barSteady, lv_color_hex(CATS[found].color), LV_PART_INDICATOR);
  lv_label_set_text(lblScanStatus, T(S_STEADY));
  if (d["stable"] | false) {     // freeze on what was recorded, show it, then move on by itself
    frozen = true;
    category = found;
    catFromCamera = true;
    catSet = true;
    lv_obj_set_style_border_color(frameRing, lv_color_hex(CATS[found].color), 0);
    lv_obj_set_style_bg_color(gotBar, lv_color_hex(CATS[found].color), 0);
    lv_label_set_text_fmt(lblGot, T(S_GOT_IT), CATS[found].name[lang]);
    lv_obj_add_flag(hintBar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(frameRing, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(gotBar, LV_OBJ_FLAG_HIDDEN);
    freezeTimer = lv_timer_create(afterFreeze, 2500, nullptr);   // time to tap Change
    lv_timer_set_repeat_count(freezeTimer, 1);
  }
}

void uiOnFrameShown(int fps) {
  lv_obj_add_flag(lblVideoWait, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text_fmt(lblLive, "LIVE  ·  %d fps", fps);
  lv_obj_invalidate(imgVideo);
}

bool uiWantsVideo() { return lv_scr_act() == scrScan && !frozen; }

// ------------------------------------------------------------ Category picker
static void onPick(lv_event_t *e) {
  category = (int)(intptr_t)lv_event_get_user_data(e);
  catFromCamera = false;
  catSet = true;
  goTo(scrWeigh);
}
static void onPickBack(lv_event_t *) { goTo(scrWeigh); }

static void buildPick() {
  scrPick = screen();
  text(scrPick, T(S_KIND_Q), &atk_b32, T_INK, T_MARGIN, 72);
  for (int i = 0; i < 4; i++) {
    lv_obj_t *b = button(scrPick, CATS[i].name[lang], T_MARGIN + (i % 2) * 376, 132 + (i / 2) * 132, 360, 116,
                         CATS[i].color, 0xFFFFFF, &atk_b32, onPick, (void *)(intptr_t)i);
    (void)b;
  }
  button(scrPick, T(S_BACK), T_MARGIN, 404, 180, T_TAP, T_CARD, T_INK, &atk_b24, onPickBack, nullptr, T_LINE);
}

// ------------------------------------------------------------ Weigh & confirm
static void refreshWeigh() {
  lv_label_set_text_fmt(lv_obj_get_child(chipPurok, 0), "%s %d   ·   %s  " LV_SYMBOL_RIGHT, T(S_PUROK), purok, T(S_CHANGE));
  lv_obj_t *cl = lv_obj_get_child(chipCat, 0);
  if (catSet) {
    const Cat &c = CATS[category];
    lv_label_set_text_fmt(cl, "%s  ·  %s  " LV_SYMBOL_RIGHT, c.name[lang], catFromCamera ? T(S_FROM_CAMERA) : T(S_YOU_CHOSE));
    lv_obj_set_style_bg_color(chipCat, lv_color_hex(c.color), 0);
    lv_obj_set_style_text_color(cl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(chipCat, 0, 0);
  } else {                                 // the camera didn't decide and nobody chose: ask, never assume
    lv_label_set_text(cl, T(S_PICK_KIND));
    lv_obj_set_style_bg_color(chipCat, lv_color_hex(T_CARD), 0);
    lv_obj_set_style_text_color(cl, lv_color_hex(T_INK), 0);
    lv_obj_set_style_border_width(chipCat, 3, 0);
    lv_obj_set_style_border_color(chipCat, lv_color_hex(T_WARN), 0);
  }
  char k[16];
  bool empty = !weightFromScale && !weightEntry.length();
  if (weightFromScale) fmtKg(k, sizeof k, weightKg);
  else snprintf(k, sizeof k, "%s", empty ? "0.0" : weightEntry.c_str());
  lv_label_set_text(lblWeight, k);
  lv_obj_set_style_text_color(lblWeight, lv_color_hex(empty ? T_LINE : T_INK), 0);
  bool ok = pcUp && purok > 0 && catSet && weightKg > 0.05f;   // without the PC the drop-off would be lost
  if (ok) lv_obj_clear_state(btnConfirm, LV_STATE_DISABLED); else lv_obj_add_state(btnConfirm, LV_STATE_DISABLED);
  lv_obj_set_style_text_color(lv_obj_get_child(btnConfirm, 0), lv_color_hex(ok ? 0xFFFFFF : T_INK2), 0);
  setButtonText(btnConfirm, !pcUp ? T(S_PC_OFFLINE) : !catSet ? T(S_PICK_KIND) : weightKg > 0.05f ? T(S_CONFIRM) : T(S_ENTER_WEIGHT));
}

static void onWeightKey(lv_event_t *e) {
  lv_obj_t *m = lv_event_get_target(e);
  const char *k = lv_btnmatrix_get_btn_text(m, lv_btnmatrix_get_selected_btn(m));
  if (!k) return;
  if (!strcmp(k, LV_SYMBOL_BACKSPACE)) { if (weightEntry.length()) weightEntry.remove(weightEntry.length() - 1); }
  else if (!strcmp(k, ".")) { if (weightEntry.indexOf('.') < 0) weightEntry += weightEntry.length() ? "." : "0."; }
  else if (weightEntry.length() < 5) weightEntry += k;
  weightKg = weightEntry.toFloat();
  weightFromScale = false;
  refreshWeigh();
}

static void onConfirm(lv_event_t *);
static void onChangePurok(lv_event_t *) { goTo(scrPurok); }
static void onWeighBack(lv_event_t *) { goTo(scrScan); }

static void buildWeigh() {
  static const char *kmap[] = {"1", "2", "3", "\n", "4", "5", "6", "\n", "7", "8", "9", "\n", ".", "0", LV_SYMBOL_BACKSPACE, ""};
  scrWeigh = screen();
  text(scrWeigh, T(S_WEIGHT_Q), &atk_b32, T_INK, T_MARGIN, 68);
  lv_obj_t *step = text(scrWeigh, T(S_STEP3), &atk_r18, T_INK2);
  lv_obj_align(step, LV_ALIGN_TOP_LEFT, T_MARGIN, 112);
  chipPurok = button(scrWeigh, "", T_MARGIN, 146, 348, 56, T_CARD, T_INK, &atk_r18, onChangePurok, nullptr, T_LINE);
  chipCat = button(scrWeigh, "", T_MARGIN, 212, 348, 56, T_CAT_MIXED, 0xFFFFFF, &atk_b24, onChoose);
  lv_obj_t *wl = text(scrWeigh, USE_HX711 ? T(S_FROM_SCALE) : T(S_WEIGHT_KG), &atk_r18, T_INK2, T_MARGIN, 284);
  (void)wl;
  lblWeight = text(scrWeigh, "0", &atk_b72, T_INK, T_MARGIN, 304);
  lv_obj_t *kgl = text(scrWeigh, "kg", &atk_b32, T_INK2);
  lv_obj_align_to(kgl, lblWeight, LV_ALIGN_OUT_RIGHT_BOTTOM, 10, -12);
  kpWeight = keypad(scrWeigh, kmap, 404, 68, 372, 324, onWeightKey);
  lv_obj_set_style_text_font(kpWeight, &atk_b48, LV_PART_ITEMS);   // big digits and a visible decimal point
  button(scrWeigh, T(S_BACK), T_MARGIN, 404, 180, T_TAP, T_CARD, T_INK, &atk_b24, onWeighBack, nullptr, T_LINE);
  btnConfirm = button(scrWeigh, T(S_CONFIRM), 228, 404, 548, T_TAP, T_PRIMARY, 0xFFFFFF, &atk_b32, onConfirm);
#if USE_HX711
  lv_obj_add_flag(kpWeight, LV_OBJ_FLAG_HIDDEN);   // the scale fills the weight instead
#endif
}

void uiSetWeightFromScale(float kg) {
  weightKg = kg;
  weightFromScale = true;
  if (lv_scr_act() == scrWeigh) refreshWeigh();
}

// ------------------------------------------------------------ Receipt
enum RcState { RC_SENDING, RC_SAVED, RC_OFFLINE, RC_NOT_SAVED };
static RcState rcState = RC_SENDING;
static String rcCode = "";

static void paintReceipt() {
  static const uint32_t band[4] = {T_INK2, T_OK, T_WARN, T_ERR};
  static const char *icon[4] = {LV_SYMBOL_UPLOAD, LV_SYMBOL_OK, LV_SYMBOL_SAVE, LV_SYMBOL_CLOSE};
  lv_obj_set_style_bg_color(rcBand, lv_color_hex(band[rcState]), 0);
  lv_label_set_text(rcIcon, icon[rcState]);
  lv_label_set_text(rcTitle, rcState == RC_NOT_SAVED ? T(S_NOT_SAVED_T) : rcState == RC_OFFLINE ? T(S_OFFLINE_T)
                          : rcState == RC_SENDING ? T(S_RECORDING_T) : T(S_THANKS));
  lv_label_set_text(rcSub, rcState == RC_SENDING ? "" : rcState == RC_SAVED ? T(S_SAVED)
                         : rcState == RC_OFFLINE ? T(S_OFFLINE) : T(S_NOT_SAVED));
  bool waiting = rcState == RC_SENDING;     // the countdown starts once the system has answered
  if (waiting) lv_obj_add_flag(barCountdown, LV_OBJ_FLAG_HIDDEN); else lv_obj_clear_flag(barCountdown, LV_OBJ_FLAG_HIDDEN);
  if (waiting) lv_obj_add_flag(lblCountdown, LV_OBJ_FLAG_HIDDEN); else lv_obj_clear_flag(lblCountdown, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(lblCode, rcState == RC_NOT_SAVED ? "-" : rcCode.length() ? rcCode.c_str() : "...");
  char k[24]; fmtKg(k, sizeof k, weightKg); strcat(k, " kg");
  lv_label_set_text(rcWeight, k);
  lv_label_set_text(rcKind, CATS[category].name[lang]);
  lv_label_set_text_fmt(rcPurok, "%d", purok);
  const Cat &c = CATS[category];
  lv_obj_set_style_bg_color(rcBin, lv_color_hex(c.color), 0);
  if (c.bin[lang]) lv_label_set_text_fmt(lblBin, T(S_BIN), c.bin[lang]);
  else lv_label_set_text(lblBin, T(S_COUNTER));
}

static void onDone(lv_event_t *) { afterDropOff = true; goTo(scrHome); }

static void receiptTick(lv_timer_t *) {
  if (rcState == RC_SENDING) return;        // no countdown before the system answers
  if (--receiptCountdown <= 0) { afterDropOff = true; goTo(scrHome); return; }
  lv_label_set_text_fmt(lblCountdown, T(S_RETURN), receiptCountdown);
  lv_bar_set_value(barCountdown, receiptCountdown * 100 / RECEIPT_RETURN_S, LV_ANIM_ON);
}

static lv_obj_t *rcRow(lv_obj_t *parent, int y, const char *k) {
  text(parent, k, &atk_r24, T_INK2, 352, y);
  lv_obj_t *v = text(parent, "", &atk_b24, T_INK);
  lv_obj_align(v, LV_ALIGN_TOP_RIGHT, -T_MARGIN, y);
  return v;
}

static void buildReceipt() {
  scrReceipt = screen();
  rcBand = box(scrReceipt, 24, 64, 304, 392, T_OK, 24);
  rcIcon = text(rcBand, LV_SYMBOL_OK, &lv_font_montserrat_40, 0xFFFFFF, 28, 28);
  rcTitle = text(rcBand, "", &atk_b48, 0xFFFFFF, 28, 96, 256);
  rcSub = text(rcBand, "", &atk_r24, 0xFFFFFF, 28, 220, 256);

  text(scrReceipt, T(S_CODE), &atk_r18, T_INK2, 352, 68);
  lblCode = text(scrReceipt, "...", &atk_b72, T_INK, 352, 86);
  box(scrReceipt, 352, 178, 800 - 352 - T_MARGIN, 2, T_LINE, 0);
  rcWeight = rcRow(scrReceipt, 192, T(S_WEIGHT));
  rcKind = rcRow(scrReceipt, 228, T(S_KIND));
  rcPurok = rcRow(scrReceipt, 264, T(S_PUROK));
  rcBin = box(scrReceipt, 352, 306, 800 - 352 - T_MARGIN, 64, T_CAT_RECYC, 14);
  lblBin = text(rcBin, "", &atk_b24, 0xFFFFFF, 16, 0, 800 - 352 - T_MARGIN - 32);
  lv_obj_align(lblBin, LV_ALIGN_LEFT_MID, 16, 0);
  barCountdown = progress(scrReceipt, 352, 390, 200, 8, T_INK2);
  lblCountdown = text(scrReceipt, "", &atk_r18, T_INK2, 352, 410, 210);
  button(scrReceipt, T(S_DONE), 568, 392, 800 - 568 - T_MARGIN, T_TAP, T_PRIMARY, 0xFFFFFF, &atk_b32, onDone);
}

static void onConfirm(lv_event_t *) {
  JsonDocument d;
  d["type"] = "intake";
  d["weight"] = weightKg;
  d["category"] = CATS[category].key;
  d["purok"] = purok;
  d["manual"] = !weightFromScale;
  d["recyclables"] = detCounts[0];
  wsSend(d);
  rcState = RC_SENDING;
  rcCode = "";
  goTo(scrReceipt);
}

void uiOnReceipt(JsonDocument &d) {
  bool saved = d["ok"] | true;            // ok:false = the PC couldn't save it
  rcCode = (const char *)(d["code"] | "");
  rcState = !saved ? RC_NOT_SAVED : rcCode.startsWith("K-") ? RC_OFFLINE : RC_SAVED;
  receiptCountdown = RECEIPT_RETURN_S;      // counted from the answer, not from Confirm
  if (lv_scr_act() == scrReceipt) {
    paintReceipt();
    lv_label_set_text_fmt(lblCountdown, T(S_RETURN), receiptCountdown);
    lv_bar_set_value(barCountdown, 100, LV_ANIM_OFF);
  }
}

// ------------------------------------------------------------ Staff PIN
static void refreshPin() {
  for (int i = 0; i < 4; i++)
    lv_obj_set_style_bg_opa(pinDots[i], i < (int)pinEntry.length() ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
}

static void onPinKey(lv_event_t *e) {
  lv_obj_t *m = lv_event_get_target(e);
  const char *k = lv_btnmatrix_get_btn_text(m, lv_btnmatrix_get_selected_btn(m));
  if (!k) return;
  lv_label_set_text(lblPinMsg, "");
  if (!strcmp(k, LV_SYMBOL_BACKSPACE)) { if (pinEntry.length()) pinEntry.remove(pinEntry.length() - 1); }
  else if (!strcmp(k, T(S_ENTER))) {
    if (pinEntry.length() != 4) lv_label_set_text(lblPinMsg, T(S_PIN_4));
    else if (pcUp) {                       // the PC checks it
      JsonDocument d; d["type"] = "pin"; d["pin"] = pinEntry; wsSend(d);
      lv_label_set_text(lblPinMsg, T(S_CHECKING));
      pinAskedAt = millis();
    } else if (pinEntry == KIOSK_PIN) {    // PC offline: the kiosk's own PIN
      lv_table_set_row_cnt(tblRecent, 2);
      lv_table_set_cell_value(tblRecent, 1, 0, "-");
      lv_table_set_cell_value(tblRecent, 1, 3, T(S_PC_OFF_TABLE));
      goTo(scrOp);
    } else { pinEntry = ""; lv_label_set_text(lblPinMsg, T(S_PC_OFF_PIN)); }
  } else if (pinEntry.length() < 4) pinEntry += k;
  refreshPin();
}

static void buildPin() {
  static const char *pmap[2][16] = {
    {"1", "2", "3", "\n", "4", "5", "6", "\n", "7", "8", "9", "\n", LV_SYMBOL_BACKSPACE, "0", "Enter", ""},
    {"1", "2", "3", "\n", "4", "5", "6", "\n", "7", "8", "9", "\n", LV_SYMBOL_BACKSPACE, "0", "Pasok", ""}};
  scrPin = screen();
  lv_obj_t *side = box(scrPin, 24, 64, 336, 392, T_BRAND, 24);
  text(side, T(S_OP_MODE), &atk_r18, 0xC9DDD0, 28, 28);
  text(side, T(S_OP_PIN), &atk_b32, 0xFFFFFF, 28, 60, 290);
  for (int i = 0; i < 4; i++) {
    pinDots[i] = box(side, 28 + i * 48, 172, 30, 30, 0xFFFFFF, LV_RADIUS_CIRCLE);
    lv_obj_set_style_border_width(pinDots[i], 3, 0);
    lv_obj_set_style_border_color(pinDots[i], lv_color_hex(0x9FBBA9), 0);
  }
  lblPinMsg = text(side, "", &atk_r18, 0xFFC9C0, 28, 220, 290);
  button(side, T(S_BACK_DROP), 28, 300, 280, T_TAP, T_BRAND, 0xFFFFFF, &atk_b24, onBackHome, nullptr, 0x5E8C70);
  lv_obj_t *kp = keypad(scrPin, pmap[lang], 384, 64, 392, 392, onPinKey);
  lv_obj_set_style_text_font(kp, &atk_b32, LV_PART_ITEMS);
}

// ------------------------------------------------------------ Staff: recent drop-offs
static void onOpLock(lv_event_t *) { JsonDocument d; d["type"] = "lock"; wsSend(d); goTo(scrHome); }
static void onOpRefresh(lv_event_t *) { JsonDocument d; d["type"] = "recent"; wsSend(d); }
static lv_obj_t *lblNetLine, *lblIpLine;

static void buildOperator() {
  scrOp = screen();
  text(scrOp, T(S_RECORDS), &atk_b32, T_INK, T_MARGIN, 64);
  text(scrOp, T(S_RECORDS_SUB), &atk_r18, T_INK2, T_MARGIN, 106);
  lblNetLine = text(scrOp, lastNet.c_str(), &atk_r18, T_INK2, 420, 72, 348);
  lblIpLine = text(scrOp, lastIp.c_str(), &atk_r18, T_INK2, 420, 96, 348);
  opAlert = box(scrOp, T_MARGIN, 134, 800 - 2 * T_MARGIN, 40, T_WARN_BG, 10);
  lblOpAlert = text(opAlert, "", &atk_r18, T_WARN, 14, 0, 800 - 2 * T_MARGIN - 28);
  lv_label_set_long_mode(lblOpAlert, LV_LABEL_LONG_DOT);
  lv_obj_set_height(lblOpAlert, 24);
  lv_obj_align(lblOpAlert, LV_ALIGN_LEFT_MID, 14, 0);
  lv_obj_add_flag(opAlert, LV_OBJ_FLAG_HIDDEN);
  tblRecent = lv_table_create(scrOp);
  lv_obj_set_pos(tblRecent, T_MARGIN, 184);
  lv_obj_set_size(tblRecent, 800 - 2 * T_MARGIN, 208);
  lv_table_set_col_cnt(tblRecent, 5);
  const int cw[5] = {128, 120, 120, 220, 144};
  const char *hd[5][2] = {{"Code", "Code"}, {"Time", "Oras"}, {"Purok", "Purok"}, {"Kind", "Uri"}, {"Weight", "Timbang"}};
  for (int i = 0; i < 5; i++) { lv_table_set_col_width(tblRecent, i, cw[i]); lv_table_set_cell_value(tblRecent, 0, i, hd[i][lang]); }
  lv_obj_set_style_text_font(tblRecent, &atk_r18, LV_PART_ITEMS);
  lv_obj_set_style_pad_ver(tblRecent, 8, LV_PART_ITEMS);
  lv_obj_set_style_border_color(tblRecent, lv_color_hex(T_LINE), 0);
  lv_obj_set_style_radius(tblRecent, 14, 0);
  lblNoRows = text(scrOp, T(S_NO_ROWS), &atk_r24, T_INK2);
  lv_obj_align_to(lblNoRows, tblRecent, LV_ALIGN_CENTER, 0, 16);
  button(scrOp, T(S_LOCK), T_MARGIN, 404, 220, T_TAP, T_CARD, T_INK, &atk_b24, onOpLock, nullptr, T_LINE);
  button(scrOp, T(S_REFRESH), 800 - T_MARGIN - 220, 404, 220, T_TAP, T_PRIMARY, 0xFFFFFF, &atk_b24, onOpRefresh);
}

void uiOnAuth(JsonDocument &d) {
  pinAskedAt = 0;
  if (d["ok"] | false) { onOpRefresh(nullptr); goTo(scrOp); }
  else { pinEntry = ""; refreshPin(); lv_label_set_text(lblPinMsg, T(S_WRONG_PIN)); }
}

void uiOnRecent(JsonDocument &d) {
  JsonArray rows = d["rows"];
  lv_table_set_row_cnt(tblRecent, rows.size() + 1);
  if (rows.size()) lv_obj_add_flag(lblNoRows, LV_OBJ_FLAG_HIDDEN); else lv_obj_clear_flag(lblNoRows, LV_OBJ_FLAG_HIDDEN);
  int r = 1;
  for (JsonObject o : rows) {
    lv_table_set_cell_value(tblRecent, r, 0, o["code"] | "");
    lv_table_set_cell_value(tblRecent, r, 1, o["time"] | "");
    lv_table_set_cell_value(tblRecent, r, 2, o["purok"] | "");
    lv_table_set_cell_value(tblRecent, r, 3, o["cat"] | "");
    lv_table_set_cell_value(tblRecent, r, 4, o["kg"] | "");
    r++;
  }
}

void uiSetNet(const char *line, const char *ip) {
  lastNet = line;
  lastIp = ip;
  if (lblNetLine) { lv_label_set_text(lblNetLine, line); lv_label_set_text(lblIpLine, ip); }
}

// ------------------------------------------------------------ PC link
void uiSetPcUp(bool up) {
  if (lv_scr_act() == scrScan && !frozen) {
    lv_label_set_text(lblVideoWait, up ? T(S_WAIT_CAM) : T(S_PC_LOST));
    if (!up) lv_obj_clear_flag(lblVideoWait, LV_OBJ_FLAG_HIDDEN);   // hidden once video has played
  }
  if (lv_scr_act() == scrWeigh) refreshWeigh();
  if (!up && lv_scr_act() == scrReceipt && rcState == RC_SENDING) { rcState = RC_NOT_SAVED; paintReceipt(); }
  if (!up) uiSetLight("red");              // no PC = no SapulMRF: nothing can be recorded
}

// ------------------------------------------------------------ navigation
void goTo(lv_obj_t *scr) {
  lv_obj_t *cur = lv_scr_act();
  if (freezeTimer) { lv_timer_del(freezeTimer); freezeTimer = nullptr; }
  if (cur == scrScan && scr != scrScan) setMode("idle");
  if (scr == scrScan) {
    frozen = false;
    for (int i = 0; i < 3; i++) lv_obj_set_style_opa(scanTile[i], LV_OPA_50, 0);
    scanSince = lastSeen = millis();
    for (int i = 0; i < 3; i++) { detCounts[i] = 0; lv_label_set_text(lblCount[i], "0"); }
    lv_bar_set_value(barSteady, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(barSteady, lv_color_hex(T_PRIMARY), LV_PART_INDICATOR);
    lv_label_set_text(lblScanStatus, T(S_LOOKING));
    lv_obj_add_flag(frameRing, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(gotBar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(hintBar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(lblVideoWait, LV_OBJ_FLAG_HIDDEN);
    setMode("scan");
  }
  if (scr == scrWeigh) {
    if (!USE_HX711 && (cur == scrScan || cur == scrPick)) { weightEntry = ""; weightKg = 0; }
    refreshWeigh();
  }
  if (scr == scrReceipt) {
    paintReceipt();
    receiptCountdown = RECEIPT_RETURN_S;
    lv_label_set_text_fmt(lblCountdown, T(S_RETURN), receiptCountdown);
    lv_bar_set_value(barCountdown, 100, LV_ANIM_OFF);
    if (!receiptTimer) receiptTimer = lv_timer_create(receiptTick, 1000, nullptr);
    else lv_timer_resume(receiptTimer);
  } else if (receiptTimer) lv_timer_pause(receiptTimer);
  if (scr == scrHome) {
    purok = 0; category = 0; catFromCamera = false; catSet = false;
    JsonDocument d; d["type"] = "stats"; wsSend(d);
    if (afterDropOff && lang != DEFAULT_LANG) {   // the next resident starts in the default language
      afterDropOff = false;
      lang = DEFAULT_LANG;
      buildAll(0);
      return;
    }
    afterDropOff = false;
  }
  if (scr == scrPin) { pinEntry = ""; lv_label_set_text(lblPinMsg, ""); refreshPin(); }
  if (scr == scrOp) onOpRefresh(nullptr);
  // The SapulMRF light is for staff; residents never see an unexplained red dot
  if (scr == scrPin || scr == scrOp) lv_obj_clear_flag(statusDot, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(statusDot, LV_OBJ_FLAG_HIDDEN);
  lv_disp_trig_activity(nullptr);
  lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_FADE_ON, 150, 0, false);
}

// Builds (or, after a language switch, rebuilds) every screen and shows the same step again.
static void buildAll(int show) {
  lv_obj_t *cur = lv_scr_act();
  lv_obj_t *old[] = {scrHome, scrPurok, scrScan, scrPick, scrWeigh, scrReceipt, scrPin, scrOp};
  int at = 0;
  for (int i = 0; i < 8; i++) if (old[i] && old[i] == cur) at = i;
  if (show >= 0) at = show;
  buildHome(); buildPurok(); buildScan(); buildPick(); buildWeigh(); buildReceipt(); buildPin(); buildOperator();
  paintAlert();
  paintLang();
  lv_obj_t *now[] = {scrHome, scrPurok, scrScan, scrPick, scrWeigh, scrReceipt, scrPin, scrOp};
  lv_obj_t *scr = now[at];
  if (scr == scrWeigh) refreshWeigh();
  if (scr == scrReceipt) paintReceipt();
  if (scr == scrScan) frozen = false;
  if (scr == scrPin || scr == scrOp) lv_obj_clear_flag(statusDot, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(statusDot, LV_OBJ_FLAG_HIDDEN);
  lv_scr_load(scr);
  for (int i = 0; i < 8; i++) if (old[i]) lv_obj_del(old[i]);
  JsonDocument d; d["type"] = "stats"; wsSend(d);   // today's line again, in the new language
}

void uiBegin() {
  uiFrameBuf = (uint16_t *)ps_malloc(FRAME_W * FRAME_H * 2);
  memset(uiFrameBuf, 0, FRAME_W * FRAME_H * 2);
  frameDsc.header.always_zero = 0;
  frameDsc.header.w = FRAME_W;
  frameDsc.header.h = FRAME_H;
  frameDsc.header.cf = LV_IMG_CF_TRUE_COLOR;
  frameDsc.data_size = FRAME_W * FRAME_H * 2;
  frameDsc.data = (const uint8_t *)uiFrameBuf;
  buildStatusStrip();
  buildAll();
  lv_scr_load(scrHome);
}

void uiLoop() {
  if (pinAskedAt && millis() - pinAskedAt > 3000) {   // the PC never answered the PIN
    pinAskedAt = 0;
    pinEntry = ""; refreshPin();
    lv_label_set_text(lblPinMsg, T(S_NO_ANSWER));
  }
  if (lv_scr_act() == scrScan && !frozen && millis() - lastSeen > 5000) lv_obj_clear_flag(hintBar, LV_OBJ_FLAG_HIDDEN);
  if (lv_scr_act() != scrHome && lv_disp_get_inactive_time(nullptr) > IDLE_RETURN_MS) goTo(scrHome);
}

// ------------------------------------------------------------ screenshots / dev tools
const char *uiScreenName() {
  lv_obj_t *s = lv_scr_act();
  return s == scrHome ? "welcome" : s == scrPurok ? "purok" : s == scrScan ? "scan" : s == scrPick ? "pick"
       : s == scrWeigh ? "weigh" : s == scrReceipt ? "receipt" : s == scrPin ? "pin" : s == scrOp ? "operator" : "other";
}

bool uiGoto(const char *n) {
  lv_obj_t *all[] = {scrHome, scrPurok, scrScan, scrPick, scrWeigh, scrReceipt, scrPin, scrOp};
  const char *names[] = {"welcome", "purok", "scan", "pick", "weigh", "receipt", "pin", "operator"};
  if (!strcmp(n, "en") || !strcmp(n, "tl")) { lang = !strcmp(n, "tl"); buildAll(); return true; }
  for (int i = 0; i < 8; i++) if (!strcmp(n, names[i])) {
    if (all[i] == scrWeigh && !purok) purok = 3;    // a believable screen for review shots
    goTo(all[i]);
    return true;
  }
  return false;
}
