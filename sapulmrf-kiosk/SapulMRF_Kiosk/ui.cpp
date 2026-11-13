// The kiosk's screens: Welcome -> Purok -> Scan -> Weigh -> Receipt for residents; for MRF operators, their name ->
// their own PIN -> operator home (today's numbers, Segregation, Sort into storage with the camera's guess).
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
  S_OP_MODE, S_OP_PIN, S_ENTER, S_PIN_4, S_CHECKING, S_WRONG_PIN, S_NO_ANSWER, S_BACK_DROP, S_LOCK,
  S_PICK_KIND, S_CHANGE, S_FLOW1, S_FLOW2, S_FLOW3, S_RECORDING_T,
  S_WHO, S_NO_OPS, S_NEED_PC, S_LOADING, S_HI, S_T_SEG, S_T_SORT,
  S_STORAGE, S_NO_PHOTO,
  S_PICK_MAT, S_PICK_AREA, S_WAIT_SEG, S_WAIT_SORT, S_NO_WAITING, S_SEGREGATED, S_MIXED_C, S_WET, S_DRY, S_KG_F, S_BAGS,
  S_SAVE, S_SAVING, S_SAVED_OK, S_SIGNED_OUT, S_RESIDUAL_M, S_N_WAITING, S_CAM_GUESS, S_AREA_IS, S_TOTAL,
  S_T_COMPOST, S_WET_WAITING, S_START_BATCH, S_NOTHING_WAITING, S_NO_BATCHES, S_TURNING, S_WATERING,
  S_INSPECT, S_FINISH, S_COMPOST_MADE, S_START_KG, S_BATCH_LINE, S_SAVE_FINISH,
  S_COUNT
};
static const char *const WORDS[][2] = {
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
  {"Enter your PIN", "Ilagay ang iyong PIN"},
  {"Enter", "Pasok"},
  {"Enter 4 to 6 digits", "Ilagay ang 4 hanggang 6 na numero"},
  {"Checking...", "Sinusuri..."},
  {"Wrong PIN, try again", "Mali ang PIN, subukan muli"},
  {"No answer from the camera PC. Try again.", "Walang sagot ang camera PC. Subukan muli."},
  {"Back to drop-off", "Bumalik sa drop-off"},
  {LV_SYMBOL_CLOSE "  Lock", LV_SYMBOL_CLOSE "  I-lock"},
  {"Choose the kind  " LV_SYMBOL_RIGHT, "Pumili ng uri  " LV_SYMBOL_RIGHT},
  {"Change", "Palitan"},
  {"Purok", "Purok"},
  {"Camera", "Camera"},
  {"Weight", "Timbang"},
  {"Recording...", "Itinatala..."},
  {"Who is signing in?", "Sino ang papasok?"},
  {"No operator has a kiosk PIN yet. Set one on the SapulMRF website: Profile, then Kiosk PIN.",
   "Wala pang operator na may kiosk PIN. Gumawa sa SapulMRF website: Profile, tapos Kiosk PIN."},
  {"Staff sign-in needs the camera PC and SapulMRF.", "Kailangan ng camera PC at SapulMRF para makapasok ang kawani."},
  {"Loading...", "Kinukuha..."},
  {"Hi, %s", "Kumusta, %s"},
  {"Segregation", "Segregasyon"},
  {"Sort into storage", "Ilagay sa imbakan"},
  {"Storage", "Imbakan"},
  {"No photo", "Walang litrato"},
  {"Which material is it?", "Anong materyal ito?"},
  {"Which storage area?", "Saang imbakan?"},
  {"Pick a drop-off to check", "Pumili ng susuriing drop-off"},
  {"Pick a drop-off to sort", "Pumili ng isosort na drop-off"},
  {"Nothing waiting.", "Walang naghihintay."},
  {"Segregated", "Nakahiwalay"},
  {"Mixed", "Halo-halo"},
  {"Wet kg", "Basa kg"},
  {"Dry kg", "Tuyo kg"},
  {"kg", "kg"},
  {"Bags", "Sako"},
  {"Save", "I-save"},
  {"Saving...", "Sine-save..."},
  {"Saved", "Na-save"},
  {"You were signed out.", "Na-sign out ka."},
  {"Residual", "Di-nabubulok"},
  {"%d waiting", "%d naghihintay"},
  {"%s  ·  camera %d%%", "%s  ·  camera %d%%"},
  {"Area: %s  " LV_SYMBOL_RIGHT, "Imbakan: %s  " LV_SYMBOL_RIGHT},
  {"Total %s kg", "Kabuuan %s kg"},
  {"Compost", "Kompost"},
  {"%s kg wet waste waiting", "%s kg basang basura ang naghihintay"},
  {"Start batch", "Simulan ang batch"},
  {"No wet waste waiting. Do the wet/dry checks first.", "Walang naghihintay na basang basura. Mag-segregate muna."},
  {"No open batches", "Walang bukas na batch"},
  {"Turn", "Haluin"},
  {"Water", "Diligan"},
  {"Check", "Suriin"},
  {"Finish", "Tapusin"},
  {"Compost made", "Nagawang kompost"},
  {"Wet waste", "Basang basura"},
  {"%s  ·  day %d  ·  %s kg", "%s  ·  araw %d  ·  %s kg"},
  {"Save finished", "I-save na tapos"},
};
static_assert(sizeof WORDS / sizeof WORDS[0] == S_COUNT, "every word needs English and Tagalog");
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
static lv_obj_t *scrList, *scrSortMat, *scrSeg, *scrSort, *scrCompost;   // staff: chooser, material with photo, the two forms
static lv_obj_t *lblToday, *alertBox, *lblAlert;
static lv_obj_t *hintBar, *lblHint;
static lv_obj_t *imgVideo, *lblVideoWait, *lblLive, *frameRing, *gotBar, *lblGot, *lblScanStatus, *barSteady;
static lv_obj_t *lblCount[3];
static lv_obj_t *chipPurok, *chipCat, *lblWeight, *lblKg, *btnConfirm, *kpWeight;
static lv_obj_t *rcBand, *rcIcon, *rcTitle, *rcSub, *lblCode, *rcWeight, *rcKind, *rcPurok, *rcBin, *lblBin, *barCountdown, *lblCountdown;
static lv_obj_t *pinDots[6], *lblPinMsg, *lblPinName, *opAlert, *lblOpAlert, *scanTile[3];
// status strip (lv_layer_top: survives rebuilding the screens)
static lv_obj_t *statusDot, *batPill, *lblBat, *langBtn[2], *toastBox, *lblToast;
static lv_timer_t *receiptTimer = nullptr, *freezeTimer = nullptr, *toastTimer = nullptr;
static String lastAlert = "", lastNet = "", lastIp = "";
static int lastAlertMore = -1;
static char todayText[96] = "";

void goTo(lv_obj_t *scr);
static void buildAll(int show = -1);
static void enterStaff(lv_obj_t *scr, lv_obj_t *cur);
static bool isStaff(lv_obj_t *s);

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
  toastBox = box(top, T_MARGIN, 60, 800 - 2 * T_MARGIN, 52, T_OK, 12);   // SapulMRF's answer to a save
  lblToast = text(toastBox, "", &atk_b24, 0xFFFFFF, 16, 0, 800 - 2 * T_MARGIN - 32);
  lv_label_set_long_mode(lblToast, LV_LABEL_LONG_DOT);
  lv_obj_set_height(lblToast, 30);
  lv_obj_align(lblToast, LV_ALIGN_LEFT_MID, 16, 0);
  lv_obj_add_flag(toastBox, LV_OBJ_FLAG_HIDDEN);
}

static void toastHide(lv_timer_t *) { lv_obj_add_flag(toastBox, LV_OBJ_FLAG_HIDDEN); toastTimer = nullptr; }

static void toast(const char *s, bool ok) {
  lv_label_set_text(lblToast, s);
  lv_obj_set_style_bg_color(toastBox, lv_color_hex(ok ? T_OK : T_ERR), 0);
  lv_obj_clear_flag(toastBox, LV_OBJ_FLAG_HIDDEN);
  if (toastTimer) lv_timer_del(toastTimer);
  toastTimer = lv_timer_create(toastHide, ok ? 2500 : 6000, nullptr);   // errors stay long enough to read
  lv_timer_set_repeat_count(toastTimer, 1);
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
static void openStaff();
static void onStaff(lv_event_t *) { openStaff(); }

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
  lv_obj_update_layout(lblWeight);
  lv_obj_align_to(lblKg, lblWeight, LV_ALIGN_OUT_RIGHT_BOTTOM, 10, -12);   // follows the number's width
  // Without the PC the drop-off would be lost. With a weight but no kind, the button opens the picker.
  bool ok = pcUp && purok > 0 && weightKg > 0.05f;
  if (ok) lv_obj_clear_state(btnConfirm, LV_STATE_DISABLED); else lv_obj_add_state(btnConfirm, LV_STATE_DISABLED);
  lv_obj_set_style_text_color(lv_obj_get_child(btnConfirm, 0), lv_color_hex(ok ? 0xFFFFFF : T_INK2), 0);
  setButtonText(btnConfirm, !pcUp ? T(S_PC_OFFLINE) : weightKg <= 0.05f ? T(S_ENTER_WEIGHT) : catSet ? T(S_CONFIRM) : T(S_PICK_KIND));
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
  lblKg = text(scrWeigh, "kg", &atk_b32, T_INK2);
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
  if (!catSet) { goTo(scrPick); return; }      // weight typed, kind not chosen yet
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

// ------------------------------------------------------------ Staff: each operator signs in with their own PIN
// The PC checks the PIN with SapulMRF and keeps that session. Every save below is a normal SapulMRF call as this
// operator, so the website's rules, approvals and Activity log apply; the kiosk shows what SapulMRF answers.
#define MAX_ITEMS 8
struct Item { int id; String label; };
struct Wait { int id; String label; float weight, dry; int sugId, pct, guessMat; String guess; };   // a drop-off
static Item ops[MAX_ITEMS], mats[MAX_ITEMS], areas[MAX_ITEMS];
static Wait waits[MAX_ITEMS];
static int nOps = 0, nMats = 0, nAreas = 0, nWaits = 0;
static bool signedIn = false, busy = false;   // busy: a save is on its way, buttons wait for SapulMRF
static uint32_t busySince = 0;
static String opName = "";
enum ListKind { L_NAMES, L_SEG, L_SORT, L_SORT_AREA };
static ListKind listKind = L_NAMES;
static int selOp = -1, selWait = -1, selMat = -1, selArea = -1;   // selMat == nMats: residual
static bool listLoading = false;
static int segCond = -1;                       // 0 segregated, 1 mixed
uint16_t *uiPhotoBuf = nullptr;
static lv_img_dsc_t photoDsc;

static bool isOpScreen(lv_obj_t *s) {
  return s == scrOp || s == scrSortMat || s == scrSeg || s == scrSort || s == scrCompost ||
         (s == scrList && listKind != L_NAMES);
}

static void opAsk(const char *what) { JsonDocument d; d["type"] = "op"; d["do"] = what; wsSend(d); }

static void setBusy(bool b) { busy = b; busySince = b ? millis() : 0; }

static void lockNow() {
  if (signedIn) { JsonDocument d; d["type"] = "lock"; wsSend(d); }
  signedIn = false; opName = ""; setBusy(false);
}

static void choose(ListKind k) { listKind = k; goTo(scrList); }
static void openStaff() { choose(L_NAMES); }

// The storage area for a material: the first whose name mentions it ("Area A · Plastics" for Plastic), else none.
static int areaFor(int mat) {
  String want = mat >= 0 && mat < nMats ? mats[mat].label : String("Residual");
  want.toLowerCase();
  for (int i = 0; i < nAreas; i++) {
    String a = areas[i].label;
    a.toLowerCase();
    if (a.indexOf(want) >= 0) return i;
  }
  return -1;
}

// ---- the chooser: operator names, waiting drop-offs, storage areas
static lv_obj_t *listBox, *lblListTitle, *lblListMsg;

static void onListItem(lv_event_t *e) {
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  switch (listKind) {
    case L_NAMES: selOp = i; goTo(scrPin); break;
    case L_SEG: selWait = i; segCond = -1; goTo(scrSeg); break;
    case L_SORT: selWait = i; goTo(scrSortMat); break;
    case L_SORT_AREA: selArea = i; goTo(scrSort); break;
  }
}

static void onListBack(lv_event_t *) {
  switch (listKind) {
    case L_NAMES: goTo(scrHome); break;
    case L_SORT_AREA: goTo(scrSort); break;
    default: goTo(scrOp);
  }
}

static void paintList() {
  lv_obj_clean(listBox);
  bool drops = listKind == L_SEG || listKind == L_SORT;
  int n = listKind == L_NAMES ? nOps : drops ? nWaits : nAreas;
  for (int i = 0; i < n; i++) {
    const char *s = listKind == L_NAMES ? ops[i].label.c_str() : drops ? waits[i].label.c_str() : areas[i].label.c_str();
    button(listBox, s, (i % 2) * 376, (i / 2) * 70, 360, 60, T_CARD, T_INK, drops ? &atk_r18 : &atk_b24, onListItem,
           (void *)(intptr_t)i, T_LINE);
  }
  if (n) lv_obj_add_flag(lblListMsg, LV_OBJ_FLAG_HIDDEN);
  else {
    lv_label_set_text(lblListMsg, listLoading ? T(S_LOADING) : listKind == L_NAMES ? T(S_NO_OPS) : T(S_NO_WAITING));
    lv_obj_clear_flag(lblListMsg, LV_OBJ_FLAG_HIDDEN);
  }
}

static void enterList() {
  static const Str titles[] = {S_WHO, S_WAIT_SEG, S_WAIT_SORT, S_PICK_AREA};
  lv_label_set_text(lblListTitle, T(titles[listKind]));
  listLoading = true;
  if (listKind == L_SORT_AREA) {
    if (!nAreas) opAsk("areas"); else listLoading = false;
  } else {
    if (listKind == L_NAMES) nOps = 0; else nWaits = 0;
    if (!pcUp) { listLoading = false; paintList(); lv_label_set_text(lblListMsg, T(S_NEED_PC)); return; }
    if (listKind == L_NAMES) { JsonDocument d; d["type"] = "operators"; wsSend(d); }
    else { JsonDocument d; d["type"] = "op"; d["do"] = "waiting"; d["for"] = listKind == L_SORT ? "sorting" : "segregation"; wsSend(d); }
  }
  paintList();
}

static void buildList() {
  scrList = screen();
  lblListTitle = text(scrList, "", &atk_b32, T_INK, T_MARGIN, 64);
  listBox = box(scrList, T_MARGIN, 116, 800 - 2 * T_MARGIN, 4 * 70, T_BG, 0);
  lblListMsg = text(scrList, "", &atk_r24, T_INK2, T_MARGIN, 140, 800 - 2 * T_MARGIN);
  button(scrList, T(S_BACK), T_MARGIN, 404, 180, T_TAP, T_CARD, T_INK, &atk_b24, onListBack, nullptr, T_LINE);
}

void uiOnOperators(JsonDocument &d) {
  nOps = 0;
  for (JsonObject o : d["items"].as<JsonArray>()) {
    if (nOps == MAX_ITEMS) break;
    ops[nOps++] = {o["id"] | 0, String((const char *)(o["name"] | ""))};
  }
  if (lv_scr_act() != scrList || listKind != L_NAMES) return;
  listLoading = false;
  paintList();
  const char *err = d["error"] | "";
  if (!nOps && *err) lv_label_set_text(lblListMsg, err);
}

// ---- PIN: 4 to 6 digits, checked by SapulMRF (5 wrong tries lock that operator for 5 minutes)
static void refreshPin() {
  for (int i = 0; i < 6; i++)
    lv_obj_set_style_bg_opa(pinDots[i], i < (int)pinEntry.length() ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
}

static void onPinKey(lv_event_t *e) {
  lv_obj_t *m = lv_event_get_target(e);
  const char *k = lv_btnmatrix_get_btn_text(m, lv_btnmatrix_get_selected_btn(m));
  if (!k || pinAskedAt) return;              // one check at a time
  lv_label_set_text(lblPinMsg, "");
  if (!strcmp(k, LV_SYMBOL_BACKSPACE)) { if (pinEntry.length()) pinEntry.remove(pinEntry.length() - 1); }
  else if (!strcmp(k, T(S_ENTER))) {
    if (pinEntry.length() < 4) lv_label_set_text(lblPinMsg, T(S_PIN_4));
    else if (!pcUp || selOp < 0) lv_label_set_text(lblPinMsg, T(S_NEED_PC));
    else {
      JsonDocument d; d["type"] = "pin"; d["user_id"] = ops[selOp].id; d["pin"] = pinEntry; wsSend(d);
      lv_label_set_text(lblPinMsg, T(S_CHECKING));
      pinAskedAt = millis();
    }
  } else if (pinEntry.length() < 6) pinEntry += k;
  refreshPin();
}

static void onPinBack(lv_event_t *) { choose(L_NAMES); }

static void buildPin() {
  static const char *pmap[2][16] = {
    {"1", "2", "3", "\n", "4", "5", "6", "\n", "7", "8", "9", "\n", LV_SYMBOL_BACKSPACE, "0", "Enter", ""},
    {"1", "2", "3", "\n", "4", "5", "6", "\n", "7", "8", "9", "\n", LV_SYMBOL_BACKSPACE, "0", "Pasok", ""}};
  scrPin = screen();
  lv_obj_t *side = box(scrPin, 24, 64, 336, 392, T_BRAND, 24);
  text(side, T(S_OP_MODE), &atk_r18, 0xC9DDD0, 28, 28);
  lblPinName = text(side, "", &atk_b24, 0xFFFFFF, 28, 60, 290);   // b24: a full name like "Simulator (operator)" fits
  lv_label_set_long_mode(lblPinName, LV_LABEL_LONG_DOT);
  lv_obj_set_height(lblPinName, 32);
  text(side, T(S_OP_PIN), &atk_r24, 0xDCEFE2, 28, 108, 290);
  for (int i = 0; i < 6; i++) {
    pinDots[i] = box(side, 28 + i * 46, 160, 30, 30, 0xFFFFFF, LV_RADIUS_CIRCLE);
    lv_obj_set_style_border_width(pinDots[i], 3, 0);
    lv_obj_set_style_border_color(pinDots[i], lv_color_hex(0x9FBBA9), 0);
  }
  lblPinMsg = text(side, "", &atk_r18, 0xFFC9C0, 28, 210, 290);
  button(side, T(S_BACK), 28, 300, 280, T_TAP, T_BRAND, 0xFFFFFF, &atk_b24, onPinBack, nullptr, 0x5E8C70);
  lv_obj_t *kp = keypad(scrPin, pmap[lang], 384, 64, 392, 392, onPinKey);
  lv_obj_set_style_text_font(kp, &atk_b32, LV_PART_ITEMS);
}

void uiOnAuth(JsonDocument &d) {
  pinAskedAt = 0;
  if (d["ok"] | false) {
    signedIn = true;
    opName = (const char *)(d["name"] | "");
    nMats = nAreas = 0;
    opAsk("materials"); opAsk("areas");       // for sorting
    goTo(scrOp);
    return;
  }
  pinEntry = ""; refreshPin();
  const char *err = d["error"] | "";
  lv_label_set_text(lblPinMsg, !*err || !strcmp(err, "Wrong PIN.") ? T(S_WRONG_PIN) : err);   // lockout: SapulMRF says how long
}

// ---- operator home: today's numbers and storage on the right, the two tasks with what's waiting on the left
static lv_obj_t *lblHi, *lblNetLine, *lblIpLine, *lblOpToday, *tileCount[3];
static lv_obj_t *areaRow[4], *areaName[4], *areaBar[4], *areaPct[4];
static void onOpLock(lv_event_t *) { goTo(scrHome); }   // goTo signs out when leaving the staff screens
static void onTile(lv_event_t *e) {
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  if (i == 2) goTo(scrCompost); else choose(i ? L_SORT : L_SEG);
}

static void buildOperator() {
  scrOp = screen();
  lblHi = text(scrOp, "", &atk_b32, T_INK, T_MARGIN, 64, 520);
  lv_label_set_long_mode(lblHi, LV_LABEL_LONG_DOT);
  lv_obj_set_height(lblHi, 44);
  opAlert = box(scrOp, T_MARGIN, 112, 800 - 2 * T_MARGIN, 40, T_WARN_BG, 10);
  lblOpAlert = text(opAlert, "", &atk_r18, T_WARN, 14, 0, 800 - 2 * T_MARGIN - 28);
  lv_label_set_long_mode(lblOpAlert, LV_LABEL_LONG_DOT);
  lv_obj_set_height(lblOpAlert, 24);
  lv_obj_align(lblOpAlert, LV_ALIGN_LEFT_MID, 14, 0);
  lv_obj_add_flag(opAlert, LV_OBJ_FLAG_HIDDEN);
  const Str tiles[3] = {S_T_SEG, S_T_SORT, S_T_COMPOST};
  const char *icons[3] = {LV_SYMBOL_SHUFFLE, LV_SYMBOL_DRIVE, LV_SYMBOL_LOOP};
  for (int i = 0; i < 3; i++) {                  // three 72 px tiles between the greeting and the Lock button
    lv_obj_t *b = button(scrOp, "", T_MARGIN, 164 + i * 80, 360, 72, i ? T_CARD : T_PRIMARY, i ? T_INK : 0xFFFFFF,
                         &atk_b24, onTile, (void *)(intptr_t)i, i ? T_LINE : 0);
    lv_obj_t *l = lv_obj_get_child(b, 0);
    lv_label_set_text_fmt(l, "%s  %s", icons[i], T(tiles[i]));
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_LEFT, 0);   // the button centres its label; title on top, count under
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_size(l, 328, 30);
    lv_obj_align(l, LV_ALIGN_TOP_LEFT, 0, 4);
    tileCount[i] = text(b, "", &atk_r18, i ? T_INK2 : 0xDCEFE2);
    lv_obj_align(tileCount[i], LV_ALIGN_BOTTOM_LEFT, 36, -4);   // under the title, past its icon
  }
  const int px = T_MARGIN + 376, pw = 800 - T_MARGIN - px;
  lblOpToday = text(scrOp, todayText[0] ? todayText : T(S_LOADING), &atk_r18, T_INK, px, 170, pw);
  text(scrOp, T(S_STORAGE), &atk_b24, T_INK, px, 206);
  for (int i = 0; i < 4; i++) {
    areaRow[i] = box(scrOp, px, 240 + i * 40, pw, 38, T_BG, 0);   // name on top, a thin bar under it: the full name fits
    areaName[i] = text(areaRow[i], "", &atk_r18, T_INK, 0, 0, pw - 76);
    lv_label_set_long_mode(areaName[i], LV_LABEL_LONG_DOT);
    lv_obj_set_height(areaName[i], 24);
    areaBar[i] = progress(areaRow[i], 0, 28, pw - 76, 6, T_PRIMARY);
    areaPct[i] = text(areaRow[i], "", &atk_b24, T_INK);
    lv_obj_align(areaPct[i], LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_flag(areaRow[i], LV_OBJ_FLAG_HIDDEN);
  }
  lblNetLine = text(scrOp, lastNet.c_str(), &atk_r18, T_INK2, 272, 408, 496);
  lblIpLine = text(scrOp, lastIp.c_str(), &atk_r18, T_INK2, 272, 432, 496);
  button(scrOp, T(S_LOCK), T_MARGIN, 404, 220, T_TAP, T_CARD, T_INK, &atk_b24, onOpLock, nullptr, T_LINE);
}

static void paintHome(JsonObject o) {
  char k[16]; fmtKg(k, sizeof k, o["kg"] | 0.0f);
  lv_label_set_text_fmt(lblOpToday, T(S_TODAY), k, (int)(o["drops"] | 0));
  lv_label_set_text_fmt(tileCount[0], T(S_N_WAITING), (int)(o["segregate"] | 0));
  lv_label_set_text_fmt(tileCount[1], T(S_N_WAITING), (int)(o["sort"] | 0));
  char w[16]; fmtKg(w, sizeof w, o["waiting"] | 0.0f);
  lv_label_set_text_fmt(tileCount[2], "%s kg", w);   // wet waste waiting for compost
  JsonArray a = o["areas"];
  for (int i = 0; i < 4; i++) {
    if (i >= (int)a.size()) { lv_obj_add_flag(areaRow[i], LV_OBJ_FLAG_HIDDEN); continue; }
    int pct = a[i]["pct"] | 0;
    lv_label_set_text(areaName[i], a[i]["name"] | "");
    lv_bar_set_value(areaBar[i], pct > 100 ? 100 : pct, LV_ANIM_OFF);
    uint32_t col = pct > 100 ? T_ERR : pct >= 85 ? T_WARN : T_PRIMARY;   // over capacity is red, like the website
    lv_obj_set_style_bg_color(areaBar[i], lv_color_hex(col), LV_PART_INDICATOR);
    lv_label_set_text_fmt(areaPct[i], "%d%%", pct);
    lv_obj_set_style_text_color(areaPct[i], lv_color_hex(pct > 100 ? T_ERR : T_INK), 0);
    lv_obj_clear_flag(areaRow[i], LV_OBJ_FLAG_HIDDEN);
  }
}

// ---- Sort, step 2: the drop-off's photo and the material; the camera's guess is the highlighted button
static lv_obj_t *imgPhoto, *lblNoPhoto, *lblSortMatTitle, *matBox;

static void onMat(lv_event_t *e) {
  selMat = (int)(intptr_t)lv_event_get_user_data(e);
  selArea = areaFor(selMat);                     // usually right; the form lets the operator change it
  goTo(scrSort);
}
static void onSortMatBack(lv_event_t *) { choose(L_SORT); }

static void paintSortMat() {
  lv_obj_clean(matBox);
  if (selWait < 0 || selWait >= nWaits) return;
  const Wait &w = waits[selWait];
  lv_label_set_text(lblSortMatTitle, w.label.c_str());
  // shortcut: one column fits 4 materials + Residual (SapulMRF has 4); more need two columns
  for (int i = 0; i <= nMats && i < 5; i++) {   // the materials, then Residual
    int id = i < nMats ? mats[i].id : 0;
    bool guess = w.sugId && i < nMats && id == w.guessMat;   // a suggestion is always a material
    lv_obj_t *b = button(matBox, "", 0, i * 64, 248, 56, guess ? T_PRIMARY : T_CARD,
                         guess ? 0xFFFFFF : T_INK, &atk_b24, onMat, (void *)(intptr_t)i, guess ? 0 : T_LINE);
    const char *name = i < nMats ? mats[i].label.c_str() : T(S_RESIDUAL_M);
    if (guess) lv_label_set_text_fmt(lv_obj_get_child(b, 0), T(S_CAM_GUESS), name, w.pct);
    else lv_label_set_text(lv_obj_get_child(b, 0), name);
  }
}

static void buildSortMat() {
  scrSortMat = screen();
  lblSortMatTitle = text(scrSortMat, "", &atk_r18, T_INK, T_MARGIN, 64, 480);
  lv_obj_t *ph = box(scrSortMat, 24, 116, PHOTO_W * 3 / 2, PHOTO_H * 3 / 2, T_VIDEO, 12);
  imgPhoto = lv_img_create(ph);
  lv_img_set_src(imgPhoto, &photoDsc);
  lv_img_set_zoom(imgPhoto, 384);              // 320x180 shown at 1.5x
  lv_obj_center(imgPhoto);
  lv_obj_add_flag(imgPhoto, LV_OBJ_FLAG_HIDDEN);
  lblNoPhoto = text(ph, T(S_LOADING), &atk_r24, 0xC9D3CC);
  lv_obj_center(lblNoPhoto);
  const int px = 24 + PHOTO_W * 3 / 2 + 24;
  text(scrSortMat, T(S_PICK_MAT), &atk_b24, T_INK, px, 64, 800 - px - 24);
  matBox = box(scrSortMat, px, 100, 800 - px - 24, 5 * 64, T_BG, 0);
  button(scrSortMat, T(S_BACK), T_MARGIN, 404, 180, T_TAP, T_CARD, T_INK, &atk_b24, onSortMatBack, nullptr, T_LINE);
}

static void enterSortMat() {
  if (!nMats) opAsk("materials");
  paintSortMat();
  lv_obj_add_flag(imgPhoto, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(lblNoPhoto, T(S_LOADING));
  lv_obj_clear_flag(lblNoPhoto, LV_OBJ_FLAG_HIDDEN);
  if (selWait < 0 || selWait >= nWaits) return;
  JsonDocument d; d["type"] = "photo"; d["delivery_id"] = waits[selWait].id; wsSend(d);
}

void uiOnPhoto(int deliveryId, bool ok) {
  if (lv_scr_act() != scrSortMat || selWait < 0 || waits[selWait].id != deliveryId) return;   // an older photo
  if (ok) { lv_obj_clear_flag(imgPhoto, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(lblNoPhoto, LV_OBJ_FLAG_HIDDEN); lv_obj_invalidate(imgPhoto); }
  else { lv_label_set_text(lblNoPhoto, T(S_NO_PHOTO)); lv_obj_clear_flag(lblNoPhoto, LV_OBJ_FLAG_HIDDEN); }
}

// ---- Segregation and sorting forms: number fields typed on one keypad, prefilled from what SapulMRF knows
struct NumField { lv_obj_t *btn; String v; Str label; bool dec; };
static NumField fWet = {nullptr, "", S_WET, true}, fDry = {nullptr, "", S_DRY, true};
static NumField fKg = {nullptr, "", S_KG_F, true}, fBags = {nullptr, "", S_BAGS, false};
static NumField *activeField = nullptr;
static bool dryTyped = false;                  // until the operator types Dry, it follows total - wet
static lv_obj_t *lblSegTitle, *lblSegTotal, *btnCond[2], *btnSegSave, *lblSortTitle, *btnArea, *btnSortSave;

static void paintField(NumField &f) {
  if (!f.btn) return;
  lv_label_set_text_fmt(lv_obj_get_child(f.btn, 0), "%s    %s", T(f.label), f.v.length() ? f.v.c_str() : "-");
  bool on = activeField == &f;
  lv_obj_set_style_border_width(f.btn, on ? 4 : 2, 0);
  lv_obj_set_style_border_color(f.btn, lv_color_hex(on ? T_PRIMARY : T_LINE), 0);
}

static void paintSave(lv_obj_t *b, bool ready) {
  bool ok = ready && !busy && pcUp;
  if (ok) lv_obj_clear_state(b, LV_STATE_DISABLED); else lv_obj_add_state(b, LV_STATE_DISABLED);
  lv_obj_set_style_text_color(lv_obj_get_child(b, 0), lv_color_hex(ok ? 0xFFFFFF : T_INK2), 0);
  setButtonText(b, busy ? T(S_SAVING) : T(S_SAVE));
}

static void paintSeg() {
  for (int i = 0; i < 2; i++) {
    bool on = segCond == i;
    lv_obj_set_style_bg_color(btnCond[i], lv_color_hex(on ? T_SOFT : T_CARD), 0);
    lv_obj_set_style_border_color(btnCond[i], lv_color_hex(on ? T_PRIMARY : T_LINE), 0);
    lv_obj_set_style_border_width(btnCond[i], on ? 4 : 2, 0);
  }
  paintField(fWet); paintField(fDry);
  paintSave(btnSegSave, segCond >= 0 && fWet.v.toFloat() + fDry.v.toFloat() > 0);
}

static void paintSort() {
  paintField(fKg); paintField(fBags);
  lv_obj_t *l = lv_obj_get_child(btnArea, 0);
  bool has = selArea >= 0 && selArea < nAreas;
  if (has) lv_label_set_text_fmt(l, T(S_AREA_IS), areas[selArea].label.c_str());
  else lv_label_set_text(l, T(S_PICK_AREA));
  lv_obj_set_style_border_width(btnArea, has ? 2 : 3, 0);
  lv_obj_set_style_border_color(btnArea, lv_color_hex(has ? T_LINE : T_WARN), 0);
  paintSave(btnSortSave, has && fKg.v.toFloat() > 0);
}

static void paintCompostForm();
static void repaintForm() {
  lv_obj_t *s = lv_scr_act();
  if (s == scrSeg) paintSeg(); else if (s == scrSort) paintSort(); else if (s == scrCompost) paintCompostForm();
}

static void onField(lv_event_t *e) {
  activeField = (NumField *)lv_event_get_user_data(e);
  repaintForm();
}

static void onFormKey(lv_event_t *e) {
  lv_obj_t *m = lv_event_get_target(e);
  const char *k = lv_btnmatrix_get_btn_text(m, lv_btnmatrix_get_selected_btn(m));
  if (!k || !activeField) return;
  String &v = activeField->v;
  if (!strcmp(k, LV_SYMBOL_BACKSPACE)) { if (v.length()) v.remove(v.length() - 1); }
  else if (!strcmp(k, ".")) { if (activeField->dec && v.indexOf('.') < 0) v += v.length() ? "." : "0."; }
  else if (v.length() < 6) v += k;
  if (activeField == &fDry) dryTyped = true;
  if (activeField == &fWet && !dryTyped && selWait >= 0) {   // dry = what's left of the drop-off
    float rest = waits[selWait].weight - fWet.v.toFloat();
    char s[16]; fmtKg(s, sizeof s, rest > 0 ? rest : 0);
    fDry.v = fWet.v.length() ? s : "";
  }
  repaintForm();
}

static void onCond(lv_event_t *e) { segCond = (int)(intptr_t)lv_event_get_user_data(e); paintSeg(); }
static void onSegBack(lv_event_t *) { choose(L_SEG); }
static void onSortBack(lv_event_t *) { goTo(scrSortMat); }
static void onAreaChip(lv_event_t *) { choose(L_SORT_AREA); }

static void onSegSave(lv_event_t *) {
  if (busy || selWait < 0) return;
  JsonDocument d; d["type"] = "op"; d["do"] = "segregate"; d["delivery_id"] = waits[selWait].id;
  d["condition"] = segCond == 1 ? "mixed" : "segregated";
  d["wet_kg"] = fWet.v.toFloat(); d["dry_kg"] = fDry.v.toFloat();
  wsSend(d); setBusy(true); paintSeg();
}

static void onSortSave(lv_event_t *) {
  if (busy || selWait < 0 || selArea < 0) return;
  const Wait &w = waits[selWait];
  JsonDocument d; d["type"] = "op"; d["do"] = "sort"; d["delivery_id"] = w.id;
  if (selMat >= 0 && selMat < nMats) d["material_id"] = mats[selMat].id;   // none = residual
  d["area_id"] = areas[selArea].id; d["kg"] = fKg.v.toFloat(); d["bags"] = fBags.v.toInt();
  if (w.sugId) { d["suggestion_id"] = w.sugId; d["guess_material_id"] = w.guessMat; }   // the PC answers the camera too
  wsSend(d); setBusy(true); paintSort();
}

static const char *formKeys[] = {"1", "2", "3", "\n", "4", "5", "6", "\n", "7", "8", "9", "\n", ".", "0", LV_SYMBOL_BACKSPACE, ""};

static lv_obj_t *fieldButton(lv_obj_t *parent, NumField &f, int y) {
  f.btn = button(parent, "", T_MARGIN, y, 356, T_TAP, T_CARD, T_INK, &atk_b24, onField, &f, T_LINE);
  return f.btn;
}

static void buildSeg() {
  scrSeg = screen();
  lblSegTitle = text(scrSeg, "", &atk_r18, T_INK, T_MARGIN, 62, 356);
  btnCond[0] = button(scrSeg, T(S_SEGREGATED), T_MARGIN, 116, 172, T_TAP, T_CARD, T_INK, &atk_b24, onCond, (void *)0, T_LINE);
  btnCond[1] = button(scrSeg, T(S_MIXED_C), T_MARGIN + 184, 116, 172, T_TAP, T_CARD, T_INK, &atk_b24, onCond, (void *)1, T_LINE);
  lblSegTotal = text(scrSeg, "", &atk_b24, T_INK, T_MARGIN, 190);
  fieldButton(scrSeg, fWet, 228);
  fieldButton(scrSeg, fDry, 302);
  lv_obj_t *kp = keypad(scrSeg, formKeys, 420, 64, 356, 328, onFormKey);
  lv_obj_set_style_text_font(kp, &atk_b48, LV_PART_ITEMS);
  button(scrSeg, T(S_BACK), T_MARGIN, 404, 172, T_TAP, T_CARD, T_INK, &atk_b24, onSegBack, nullptr, T_LINE);
  btnSegSave = button(scrSeg, T(S_SAVE), T_MARGIN + 184, 404, 564, T_TAP, T_PRIMARY, 0xFFFFFF, &atk_b32, onSegSave);
}

static void buildSort() {
  scrSort = screen();
  lblSortTitle = text(scrSort, "", &atk_r18, T_INK, T_MARGIN, 62, 356);
  // below the 3-line title (drop-off, purok, material); r18 so "Area: Demo Area A · Plastics ›" stays on one line
  btnArea = button(scrSort, "", T_MARGIN, 140, 356, T_TAP, T_CARD, T_INK, &atk_r18, onAreaChip, nullptr, T_LINE);
  fieldButton(scrSort, fKg, 228);
  fieldButton(scrSort, fBags, 302);
  lv_obj_t *kp = keypad(scrSort, formKeys, 420, 64, 356, 328, onFormKey);
  lv_obj_set_style_text_font(kp, &atk_b48, LV_PART_ITEMS);
  button(scrSort, T(S_BACK), T_MARGIN, 404, 172, T_TAP, T_CARD, T_INK, &atk_b24, onSortBack, nullptr, T_LINE);
  btnSortSave = button(scrSort, T(S_SAVE), T_MARGIN + 184, 404, 564, T_TAP, T_PRIMARY, 0xFFFFFF, &atk_b32, onSortSave);
}

static void enterForm(lv_obj_t *scr, lv_obj_t *cur) {
  if (selWait < 0 || selWait >= nWaits) return;
  const Wait &w = waits[selWait];
  char k[16];
  if (scr == scrSeg) {
    if (cur == scrList) { fWet.v = fDry.v = ""; activeField = &fWet; dryTyped = false; }   // a new drop-off
    lv_label_set_text(lblSegTitle, w.label.c_str());
    fmtKg(k, sizeof k, w.weight);
    lv_label_set_text_fmt(lblSegTotal, T(S_TOTAL), k);
    paintSeg();
  } else {
    if (cur == scrSortMat) {                     // a new material: kg starts at what's left after segregation
      fmtKg(k, sizeof k, w.dry > 0 ? w.dry : w.weight);
      fKg.v = k; fBags.v = ""; activeField = &fBags;
    }
    const char *mat = selMat >= 0 && selMat < nMats ? mats[selMat].label.c_str() : T(S_RESIDUAL_M);
    lv_label_set_text_fmt(lblSortTitle, "%s\n" LV_SYMBOL_RIGHT "  %s", w.label.c_str(), mat);
    paintSort();
  }
}

// ---- SapulMRF's answers, relayed by the PC
static void fillItems(Item *items, int &n, JsonArray a) {
  n = 0;
  for (JsonObject o : a) {
    if (n == MAX_ITEMS) break;
    items[n++] = {o["id"] | 0, String((const char *)(o["name"] | ""))};
  }
}

static void fillWaits(JsonArray a) {
  nWaits = 0;
  for (JsonObject o : a) {
    if (nWaits == MAX_ITEMS) break;
    Wait &w = waits[nWaits++];
    w.id = o["id"] | 0;
    w.weight = o["weight_kg"] | 0.0f;
    w.dry = o["dry_kg"] | 0.0f;
    w.sugId = o["suggestion_id"] | 0;
    w.pct = o["pct"] | 0;
    w.guessMat = o["guess_material_id"] | 0;
    w.guess = (const char *)(o["guess"] | "");
    char k[16]; fmtKg(k, sizeof k, w.weight);   // "D-0123 · 3.5 kg" and "Purok 1 · Recyclable"
    w.label = String((const char *)(o["code"] | "")) + "  ·  " + k + " kg\n" + (const char *)(o["source_area"] | "")
            + "  ·  " + (const char *)(o["category"] | "");
  }
}

// ---- Compost: wet waste waiting, start a batch from it, log turning / watering / checks, finish
static const int MAX_BATCHES = 4;
struct Batch { int id; String label; };
static Batch batches[MAX_BATCHES];
static int nBatches = 0, finishing = -1;        // finishing = the batch whose compost kg is being typed
static float wetWaiting = 0;
static NumField fCompost = {nullptr, "", S_START_KG, true};
static lv_obj_t *lblCompWait, *batchBox, *btnCompSave;

static void paintCompostForm() {
  char k[16]; fmtKg(k, sizeof k, wetWaiting);
  if (finishing >= 0) lv_label_set_text(lblCompWait, batches[finishing].label.c_str());
  else if (wetWaiting <= 0) lv_label_set_text(lblCompWait, T(S_NOTHING_WAITING));
  else lv_label_set_text_fmt(lblCompWait, T(S_WET_WAITING), k);
  fCompost.label = finishing >= 0 ? S_COMPOST_MADE : S_START_KG;
  paintField(fCompost);
  float v = fCompost.v.toFloat();
  paintSave(btnCompSave, v > 0 && (finishing >= 0 || (wetWaiting > 0 && v <= wetWaiting + 0.5f)));   // SapulMRF allows 0.5 kg over
  if (!busy) setButtonText(btnCompSave, T(finishing >= 0 ? S_SAVE_FINISH : S_START_BATCH));
}

static void paintBatches();

static void onBatchDo(lv_event_t *e) {           // user data: batch index * 4 + action (0-2 log it, 3 finish)
  if (busy) return;
  int v = (int)(intptr_t)lv_event_get_user_data(e), i = v / 4, a = v % 4;
  if (i >= nBatches) return;
  if (a == 3) { finishing = i; fCompost.v = ""; activeField = &fCompost; paintBatches(); paintCompostForm(); return; }
  static const char *const kinds[3] = {"turning", "watering", "inspection"};
  JsonDocument d; d["type"] = "op"; d["do"] = "compost_log"; d["batch_id"] = batches[i].id; d["activity"] = kinds[a];
  wsSend(d); setBusy(true); paintCompostForm();
}

static void paintBatches() {
  lv_obj_clean(batchBox);
  if (!nBatches) { text(batchBox, T(S_NO_BATCHES), &atk_r18, T_INK2, 0, 0); return; }
  static const Str acts[4] = {S_TURNING, S_WATERING, S_INSPECT, S_FINISH};
  for (int i = 0; i < nBatches; i++) {
    lv_obj_t *c = box(batchBox, 0, i * 84, 376, 76, T_CARD, 12);
    lv_obj_set_style_border_width(c, i == finishing ? 4 : 2, 0);
    lv_obj_set_style_border_color(c, lv_color_hex(i == finishing ? T_PRIMARY : T_LINE), 0);
    text(c, batches[i].label.c_str(), &atk_b24, T_INK, 12, 4);
    for (int a = 0; a < 4; a++)
      button(c, T(acts[a]), 8 + a * 91, 36, 85, 34, a == 3 ? T_PRIMARY : T_BG, a == 3 ? 0xFFFFFF : T_INK, &atk_r18,
             onBatchDo, (void *)(intptr_t)(i * 4 + a), a == 3 ? 0 : T_LINE);
  }
}

static void fillCompost(JsonObject o) {
  wetWaiting = o["waiting"] | 0.0f;
  nBatches = 0;
  for (JsonObject b : o["batches"].as<JsonArray>()) {
    if (nBatches == MAX_BATCHES) break;
    char k[16]; fmtKg(k, sizeof k, b["start_kg"] | 0.0f);
    char line[64]; snprintf(line, sizeof line, T(S_BATCH_LINE), (const char *)(b["code"] | ""), (int)(b["day"] | 0), k);
    batches[nBatches++] = {b["id"] | 0, String(line)};
  }
  if (finishing >= nBatches) finishing = -1;
  if (finishing < 0) {                           // a new batch starts with everything that's waiting
    char k[16]; fmtKg(k, sizeof k, wetWaiting);
    fCompost.v = wetWaiting > 0 ? k : "";
  }
  activeField = &fCompost;
  paintBatches();
  paintCompostForm();
}

static void enterCompost() {
  finishing = -1; nBatches = 0; wetWaiting = 0; fCompost.v = ""; activeField = &fCompost;
  lv_label_set_text(lblCompWait, T(S_LOADING));
  paintBatches();
  opAsk("compost");
}

static void onCompostBack(lv_event_t *) {
  if (finishing >= 0) { enterCompost(); return; }   // leave Finish, back to starting a batch
  goTo(scrOp);
}

static void onCompostSave(lv_event_t *) {
  if (busy) return;
  JsonDocument d; d["type"] = "op";
  if (finishing >= 0) { d["do"] = "compost_finish"; d["batch_id"] = batches[finishing].id; }
  else d["do"] = "compost_start";
  d["kg"] = fCompost.v.toFloat();
  wsSend(d); setBusy(true); paintCompostForm();
}

static void buildCompost() {
  scrCompost = screen();
  text(scrCompost, T(S_T_COMPOST), &atk_b32, T_INK, T_MARGIN, 58);
  lblCompWait = text(scrCompost, "", &atk_r18, T_INK, T_MARGIN, 104, 356);
  fieldButton(scrCompost, fCompost, 156);
  lv_obj_t *kp = keypad(scrCompost, formKeys, T_MARGIN, 228, 356, 164, onFormKey);
  lv_obj_set_style_text_font(kp, &atk_b32, LV_PART_ITEMS);
  batchBox = box(scrCompost, 400, 64, 376, 330, T_BG);
  button(scrCompost, T(S_BACK), T_MARGIN, 404, 172, T_TAP, T_CARD, T_INK, &atk_b24, onCompostBack, nullptr, T_LINE);
  btnCompSave = button(scrCompost, T(S_START_BATCH), T_MARGIN + 184, 404, 564, T_TAP, T_PRIMARY, 0xFFFFFF, &atk_b32,
                       onCompostSave);
}

void uiOnOp(JsonDocument &d) {
  const char *what = d["do"] | "";
  bool ok = d["ok"] | false;
  lv_obj_t *cur = lv_scr_act();
  bool compostSave = !strncmp(what, "compost_", 8);
  bool save = !strcmp(what, "segregate") || !strcmp(what, "sort") || compostSave;
  if (save) setBusy(false);
  if (d["signed_out"] | false) {                 // the session ended (30 minutes, or locked elsewhere)
    signedIn = false;
    if (isOpScreen(cur)) { toast(T(S_SIGNED_OUT), false); goTo(scrHome); }
    return;
  }
  if (!ok) {
    toast(d["error"] | "SapulMRF error", false);   // SapulMRF's own message, e.g. a full storage area
    listLoading = false;
    if (cur == scrList) paintList();
    if (cur == scrSeg) paintSeg();
    if (cur == scrSort) paintSort();
    if (cur == scrCompost) paintCompostForm();
    return;
  }
  if (compostSave) {                             // saved: show it, then the fresh balance and batches
    toast(T(S_SAVED_OK), true);
    finishing = -1; fCompost.v = "";
    if (cur == scrCompost) opAsk("compost");
    return;
  }
  if (!strcmp(what, "compost")) { if (cur == scrCompost) fillCompost(d["data"].as<JsonObject>()); return; }
  if (save) {
    const char *note = d["note"] | "";           // the sort saved, but the camera's suggestion wasn't updated
    if (*note) toast((String(T(S_SAVED_OK)) + ". " + note).c_str(), false); else toast(T(S_SAVED_OK), true);
    choose(!strcmp(what, "segregate") ? L_SEG : L_SORT);
    return;
  }
  if (!strcmp(what, "today")) { if (cur == scrOp) paintHome(d["data"].as<JsonObject>()); return; }
  if (!strcmp(what, "waiting")) fillWaits(d["data"].as<JsonArray>());
  else if (!strcmp(what, "materials")) fillItems(mats, nMats, d["data"].as<JsonArray>());
  else if (!strcmp(what, "areas")) fillItems(areas, nAreas, d["data"].as<JsonArray>());
  else return;
  if (cur == scrList) { listLoading = false; paintList(); }
  if (cur == scrSortMat) paintSortMat();
  if (cur == scrSort && !strcmp(what, "areas")) { if (selArea < 0) selArea = areaFor(selMat); paintSort(); }
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
  if (!up) { signedIn = false; setBusy(false); }   // the PC signs the operator out when the link drops
  if (!up && isOpScreen(lv_scr_act())) goTo(scrHome);
}

// ------------------------------------------------------------ navigation
static bool isStaff(lv_obj_t *s) { return s == scrPin || s == scrList || isOpScreen(s); }

// What a staff screen fetches or repaints when it shows (also after a language switch rebuilds it).
static void enterStaff(lv_obj_t *scr, lv_obj_t *cur) {
  if (scr == scrPin) {
    pinEntry = ""; pinAskedAt = 0; lv_label_set_text(lblPinMsg, ""); refreshPin();
    lv_label_set_text(lblPinName, selOp >= 0 ? ops[selOp].label.c_str() : "");
  }
  if (scr == scrList) enterList();
  if (scr == scrOp) { lv_label_set_text_fmt(lblHi, T(S_HI), opName.c_str()); paintAlert(); opAsk("today"); }
  if (scr == scrSortMat) enterSortMat();
  if (scr == scrSeg || scr == scrSort) enterForm(scr, cur);
  if (scr == scrCompost) { if (cur != scrCompost) enterCompost(); else { paintBatches(); paintCompostForm(); } }   // a language switch rebuilt it
}

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
  if (signedIn && !isOpScreen(scr)) lockNow();   // leaving the staff screens signs out (Lock, idle, Back)
  enterStaff(scr, cur);
  if (isStaff(scr)) lv_obj_clear_flag(statusDot, LV_OBJ_FLAG_HIDDEN);   // residents never see an unexplained red dot
  else lv_obj_add_flag(statusDot, LV_OBJ_FLAG_HIDDEN);
  lv_disp_trig_activity(nullptr);
  if (scr == cur) return;                          // the chooser shows its next list in place
  lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_FADE_ON, 150, 0, false);
}

// Builds (or, after a language switch, rebuilds) every screen and shows the same step again.
static void buildAll(int show) {
  lv_obj_t *cur = lv_scr_act();
  lv_obj_t *old[] = {scrHome, scrPurok, scrScan, scrPick, scrWeigh, scrReceipt, scrPin, scrOp, scrList, scrSortMat,
                     scrSeg, scrSort, scrCompost};
  const int N = sizeof old / sizeof old[0];
  int at = 0;
  for (int i = 0; i < N; i++) if (old[i] && old[i] == cur) at = i;
  if (show >= 0) at = show;
  for (NumField *f : {&fWet, &fDry, &fKg, &fBags, &fCompost}) f->btn = nullptr;
  buildHome(); buildPurok(); buildScan(); buildPick(); buildWeigh(); buildReceipt(); buildPin(); buildOperator();
  buildList(); buildSortMat(); buildSeg(); buildSort(); buildCompost();
  paintAlert();
  paintLang();
  lv_obj_t *now[] = {scrHome, scrPurok, scrScan, scrPick, scrWeigh, scrReceipt, scrPin, scrOp, scrList, scrSortMat,
                     scrSeg, scrSort, scrCompost};
  lv_obj_t *scr = now[at];
  if (scr == scrWeigh) refreshWeigh();
  if (scr == scrReceipt) paintReceipt();
  if (scr == scrScan) frozen = false;
  enterStaff(scr, scr);                          // same screen: keep the form's values
  if (isStaff(scr)) lv_obj_clear_flag(statusDot, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(statusDot, LV_OBJ_FLAG_HIDDEN);
  lv_scr_load(scr);
  for (int i = 0; i < N; i++) if (old[i]) lv_obj_del(old[i]);
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
  uiPhotoBuf = (uint16_t *)ps_malloc(PHOTO_W * PHOTO_H * 2);
  memset(uiPhotoBuf, 0, PHOTO_W * PHOTO_H * 2);
  photoDsc.header.always_zero = 0;
  photoDsc.header.w = PHOTO_W;
  photoDsc.header.h = PHOTO_H;
  photoDsc.header.cf = LV_IMG_CF_TRUE_COLOR;
  photoDsc.data_size = PHOTO_W * PHOTO_H * 2;
  photoDsc.data = (const uint8_t *)uiPhotoBuf;
  buildStatusStrip();
  buildAll();
  lv_scr_load(scrHome);
}

void uiLoop() {
  if (busy && millis() - busySince > 10000) {         // a save SapulMRF never answered
    setBusy(false);
    toast(T(S_NO_ANSWER), false);
    if (lv_scr_act() == scrSeg) paintSeg();
    if (lv_scr_act() == scrSort) paintSort();
    if (lv_scr_act() == scrCompost) paintCompostForm();
  }
  if (pinAskedAt && millis() - pinAskedAt > 8000) {   // the PC never answered the PIN (SapulMRF hashes it slowly)
    pinAskedAt = 0;
    pinEntry = ""; refreshPin();
    lv_label_set_text(lblPinMsg, T(S_NO_ANSWER));
  }
  if (lv_scr_act() == scrScan && !frozen && millis() - lastSeen > 5000) lv_obj_clear_flag(hintBar, LV_OBJ_FLAG_HIDDEN);
  uint32_t idle = isOpScreen(lv_scr_act()) ? OP_IDLE_MS : IDLE_RETURN_MS;   // an operator's screen: 5 min, then Lock
  if (lv_scr_act() != scrHome && lv_disp_get_inactive_time(nullptr) > idle) goTo(scrHome);
}

// ------------------------------------------------------------ screenshots / dev tools
const char *uiScreenName() {
  lv_obj_t *s = lv_scr_act();
  return s == scrHome ? "welcome" : s == scrPurok ? "purok" : s == scrScan ? "scan" : s == scrPick ? "pick"
       : s == scrWeigh ? "weigh" : s == scrReceipt ? "receipt" : s == scrPin ? "pin" : s == scrOp ? "operator"
       : s == scrList ? "list" : s == scrSortMat ? "material" : s == scrSeg ? "segregate"
       : s == scrSort ? "sort" : "other";
}

bool uiGoto(const char *n) {
  // Staff screens need a real sign-in: without one the PC refuses their data and the kiosk goes home.
  lv_obj_t *all[] = {scrHome, scrPurok, scrScan, scrPick, scrWeigh, scrReceipt, scrPin, scrOp};
  const char *names[] = {"welcome", "purok", "scan", "pick", "weigh", "receipt", "pin", "operator"};
  if (!strcmp(n, "en") || !strcmp(n, "tl")) { lang = !strcmp(n, "tl"); buildAll(); return true; }
  if (!strcmp(n, "who")) { choose(L_NAMES); return true; }
  if (!strcmp(n, "seglist")) { choose(L_SEG); return true; }
  if (!strcmp(n, "sortlist")) { choose(L_SORT); return true; }
  for (int i = 0; i < 8; i++) if (!strcmp(n, names[i])) {
    if (all[i] == scrWeigh && !purok) purok = 3;    // a believable screen for review shots
    goTo(all[i]);
    return true;
  }
  return false;
}
