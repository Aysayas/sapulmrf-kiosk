/*
  SapulMRF Kiosk – ESP32-S3 7" (800x480) touch board
  Resident drop-off:  Welcome -> Camera scan -> Weigh & confirm -> Receipt
  Operator mode:      Lock icon -> PIN -> Operator station (recent intake)

  The PC runs YOLO on the USB camera (pc_bridge/kiosk_bridge.py). Like the
  SapulMRF stream, the KIOSK is the TCP server (port 5000) and the PC connects
  to the IP shown on the welcome screen. The kiosk joins your Wi-Fi or a phone
  hotspot; if neither is found it makes its own hotspot (192.168.4.1).

  Packets, both directions:  [1 byte type][4 byte little-endian length][payload]
    'J' PC->kiosk  JPEG frame          'A' kiosk->PC  ACK, frame shown (send next)
    'M' either way JSON message (det, receipt, stats, auth, recent / mode, intake, pin...)

  Battery: INA219 on the shared I2C bus (GPIO19/20), same wiring as the
  Alangan translator. Icon + % top-right on every screen.

  Arduino IDE settings (Tools menu):
    Board: ESP32S3 Dev Module · PSRAM: OPI PSRAM · Flash: 16MB (QIO)
    Partition: Huge APP (3MB No OTA) · USB CDC On Boot: Enabled

  Libraries (Library Manager):
    (display: display.cpp drives the RGB panel itself through ESP-IDF, ESP32 core 3.x)
    lvgl 8.3.x            (copy lv_conf.h next to the lvgl folder; see README)
    TAMC_GT911
    JPEGDEC (bitbank2)
    ArduinoJson 7.x
    (Wi-Fi, TCP, UDP and the INA219 use only built-in ESP32 libraries)
    HX711 (bogde)         only if USE_HX711 = 1
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <JPEGDEC.h>
#include <TAMC_GT911.h>
#include <lvgl.h>
#include "config.h"
#include "display.h"
#include "theme.h"
#include "ui.h"
#if USE_HX711
#include <HX711.h>
HX711 scale;
#endif

// ------------------------------------------------------------ display + touch
TAMC_GT911 tp(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST, 800, 480);
static lv_indev_drv_t indev_drv;

void my_touch(lv_indev_drv_t *, lv_indev_data_t *data) {
  tp.read();
  if (tp.isTouched) {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = tp.points[0].x;
    data->point.y = tp.points[0].y;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

// ------------------------------------------------------------ app state
WiFiServer server(STREAM_PORT);
WiFiClient pc;
WiFiUDP beacon;
JPEGDEC jpeg;
bool pcUp = false;
bool apMode = false;
String netName = "", netIp = "";

// incoming packet state machine (never blocks the UI)
uint8_t rxHdr[5]; int rxHdrGot = 0;
uint8_t *rxBuf = nullptr; uint32_t rxLen = 0, rxGot = 0; char rxType = 0;

// battery
bool batFound = false;
float batPct = -1, batMv = 0, batMa = 0;

int detFps = 0;                          // the PC's send rate, shown on the video

void sendPacket(char type, const uint8_t *data, uint32_t len) {
  if (!pc || !pc.connected()) return;
  uint8_t h[5] = {(uint8_t)type, (uint8_t)len, (uint8_t)(len >> 8), (uint8_t)(len >> 16), (uint8_t)(len >> 24)};
  pc.write(h, 5);
  if (len) pc.write(data, len);
}
void wsSend(JsonDocument &doc) { String s; serializeJson(doc, s); sendPacket('M', (const uint8_t *)s.c_str(), s.length()); }
void setMode(const char *m) { JsonDocument d; d["type"] = "mode"; d["mode"] = m; wsSend(d); }

// ------------------------------------------------------------ screenshots
// The frame on screen right now, as it really looks: a 'shot' message naming the screen, then the raw
// 800x480 RGB565 (little-endian) frame as an 'S' packet. The PC saves it as a PNG.
void sendSnapshot() {
  JsonDocument d; d["type"] = "shot"; d["screen"] = uiScreenName(); wsSend(d);
  sendPacket('S', (const uint8_t *)displayShown(), 800 * 480 * 2);
}

// ------------------------------------------------------------ incoming data
int videoScale = 1;   // 2 = the PC sent a half-size frame (240x180): each pixel becomes a 2x2 block

int jpegDraw(JPEGDRAW *p) {                 // copy decoded block into the frame buffer
  for (int y = 0; y < p->iHeight; y++) {
    const uint16_t *src = &p->pPixels[y * p->iWidth];
    if (videoScale == 1) {
      int dy = p->y + y;
      if (dy >= FRAME_H) break;
      int w = min(p->iWidth, FRAME_W - p->x);
      memcpy(&uiFrameBuf[dy * FRAME_W + p->x], src, w * 2);
    } else {
      int dy = (p->y + y) * 2, dx = p->x * 2;
      if (dy + 1 >= FRAME_H) break;
      int w = min(p->iWidth, (FRAME_W - dx) / 2);
      uint16_t *row = &uiFrameBuf[dy * FRAME_W + dx];
      for (int x = 0; x < w; x++) row[2 * x] = row[2 * x + 1] = src[x];
      memcpy(row + FRAME_W, row, w * 4);    // the second line is a copy of the first
    }
  }
  return 1;
}

uint32_t statFrames = 0, statDecodeMs = 0;   // video frames shown and their decode time, per stats period

void onFrame(uint8_t *data, size_t len) {
  if (!uiWantsVideo()) return;            // other screens, or frozen on a result
  if (jpeg.openRAM(data, len, jpegDraw)) {
    uint32_t t0 = millis();
    videoScale = jpeg.getWidth() * 2 <= FRAME_W ? 2 : 1;
    jpeg.setPixelType(RGB565_LITTLE_ENDIAN);
    jpeg.decode(0, 0, 0);
    jpeg.close();
    statDecodeMs += millis() - t0;
    statFrames++;
    uiOnFrameShown(detFps);
  }
}

void onText(uint8_t *data, size_t len) {
  JsonDocument d;
  if (deserializeJson(d, data, len)) return;
  const char *t = d["type"] | "";
  if (!strcmp(t, "det")) { detFps = d["fps"] | 0; uiOnDet(d); }
  else if (!strcmp(t, "receipt")) uiOnReceipt(d);
  else if (!strcmp(t, "alerts")) uiOnAlerts(d);
  else if (!strcmp(t, "stats")) uiOnStats((float)(d["kg"] | 0.0), (int)(d["drops"] | 0));
  else if (!strcmp(t, "auth")) uiOnAuth(d);
  else if (!strcmp(t, "recent")) uiOnRecent(d);
  else if (!strcmp(t, "actuate")) {
    if (d["light"].is<const char *>()) uiSetLight(d["light"]);
    beep(d["buzz"] | 0);
  }
  else if (!strcmp(t, "snapshot")) sendSnapshot();   // the PC asks for a screenshot (design review, thesis)
#if DEV_TOOLS
  else if (!strcmp(t, "goto")) uiGoto(d["screen"] | "");   // dev only: show a screen, or "en"/"tl"
#endif
}

// ------------------------------------------------------------ PC link (TCP server, like sapul stream)
void setPcStatus(bool up) {
  pcUp = up;
  uiSetPcUp(up);
}

void handlePacket() {
  if (rxType == 'J') {
    onFrame(rxBuf, rxLen);
    sendPacket('A', nullptr, 0);          // ACK: shown, PC may send the next frame
  } else if (rxType == 'M') {
    onText(rxBuf, rxLen);
  }
}

void pcLoop() {
  if (server.hasClient()) {               // new PC: replace the old one
    WiFiClient n = server.available();
    if (pc && pc.connected()) pc.stop();
    pc = n;
    pc.setNoDelay(true);
    rxHdrGot = 0; rxType = 0;
    Serial.printf("PC connected from %s\n", pc.remoteIP().toString().c_str());
    setPcStatus(true);
    setMode(uiWantsVideo() ? "scan" : "idle");
    JsonDocument d; d["type"] = "stats"; wsSend(d);
  }
  if (pcUp && !pc.connected()) {
    Serial.println("PC disconnected");
    pc.stop();
    setPcStatus(false);
    rxHdrGot = 0; rxType = 0;
  }
  uint32_t t0 = millis();
  while (pc && pc.available() && millis() - t0 < 30) {   // read a slice, keep the UI smooth
    if (rxType == 0) {
      while (rxHdrGot < 5 && pc.available()) rxHdr[rxHdrGot++] = pc.read();
      if (rxHdrGot < 5) break;
      rxHdrGot = 0;
      rxType = (char)rxHdr[0];
      rxLen = rxHdr[1] | (rxHdr[2] << 8) | (rxHdr[3] << 16) | ((uint32_t)rxHdr[4] << 24);
      rxGot = 0;
      if (rxLen > MAX_PACKET || (rxType != 'J' && rxType != 'M')) {
        Serial.printf("Bad packet %c len %u - dropping PC\n", rxType, rxLen);
        pc.stop(); rxType = 0; break;
      }
      if (rxLen == 0) { handlePacket(); rxType = 0; continue; }
    }
    int n = pc.read(rxBuf + rxGot, rxLen - rxGot);
    if (n <= 0) break;
    rxGot += n;
    if (rxGot == rxLen) { handlePacket(); rxType = 0; }
  }
}

// ------------------------------------------------------------ Wi-Fi / hotspot
void showNet(const char *line) {
  uiSetNet(line, "");
  lv_timer_handler();
}

bool tryJoin(const char *ssid, const char *pass) {
  if (!ssid || !*ssid) return false;
  char buf[64]; snprintf(buf, sizeof buf, LV_SYMBOL_WIFI " Joining %s...", ssid);
  showNet(buf);
  WiFi.begin(ssid, pass);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < WIFI_TRY_MS) { lv_timer_handler(); delay(20); }
  if (WiFi.status() == WL_CONNECTED) return true;
  WiFi.disconnect();
  delay(100);
  return false;
}

void netStart() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);                   // keeps frame latency low
#if !FORCE_OWN_HOTSPOT
  if (tryJoin(WIFI1_SSID, WIFI1_PASS)) netName = WIFI1_SSID;
  else if (tryJoin(WIFI2_SSID, WIFI2_PASS)) netName = WIFI2_SSID;
#endif

  if (netName.length()) {
    apMode = false;
    WiFi.setAutoReconnect(true);
    netIp = WiFi.localIP().toString();
  } else {                                // no network found: be the hotspot
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    apMode = true;
    netName = AP_SSID;
    netIp = WiFi.softAPIP().toString();
  }
  char line[64];
  snprintf(line, sizeof line, LV_SYMBOL_WIFI " %s: %s", apMode ? "Hotspot" : "Wi-Fi", netName.c_str());
  uiSetNet(line, ("Kiosk IP " + netIp).c_str());
  Serial.printf("%s %s  IP %s  port %d\n", apMode ? "Hotspot" : "Wi-Fi", netName.c_str(), netIp.c_str(), STREAM_PORT);
  server.begin();
  server.setNoDelay(true);
}

void beaconLoop() {                       // lets the PC script find the kiosk without typing the IP
  static uint32_t last = 0;
  if (millis() - last < 2000) return;
  last = millis();
  IPAddress ip = apMode ? WiFi.softAPIP() : WiFi.localIP();
  IPAddress mask = apMode ? IPAddress(255, 255, 255, 0) : WiFi.subnetMask();
  IPAddress bc(ip[0] | ~mask[0], ip[1] | ~mask[1], ip[2] | ~mask[2], ip[3] | ~mask[3]);
  char msg[48];
  snprintf(msg, sizeof msg, "SAPULMRF %s %d", ip.toString().c_str(), STREAM_PORT);
  beacon.beginPacket(bc, BEACON_PORT);
  beacon.write((const uint8_t *)msg, strlen(msg));
  beacon.endPacket();

  if (!apMode) {                          // Wi-Fi dropped / came back
    bool up = WiFi.status() == WL_CONNECTED;
    String now = up ? WiFi.localIP().toString() : String("-");
    if (now != netIp) {
      netIp = now;
      char line[64];
      snprintf(line, sizeof line, up ? LV_SYMBOL_WIFI " Wi-Fi: %s" : LV_SYMBOL_WARNING " Lost %s", netName.c_str());
      uiSetNet(line, ("Kiosk IP " + netIp).c_str());
    }
  }
}

// ------------------------------------------------------------ battery (INA219, as in the Alangan translator)
bool inaWrite(uint8_t reg, uint16_t v) {
  Wire.beginTransmission(INA219_ADDR);
  Wire.write(reg); Wire.write(v >> 8); Wire.write(v & 0xFF);
  return Wire.endTransmission() == 0;
}

bool inaRead(uint8_t reg, uint16_t &v) {
  Wire.beginTransmission(INA219_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)INA219_ADDR, (uint8_t)2) != 2) return false;
  v = (Wire.read() << 8) | Wire.read();
  return true;
}

float lipoPercent(float mv) {             // resting Li-ion curve, single cell
  static const float T[][2] = {{3300, 0}, {3500, 5}, {3600, 10}, {3700, 30}, {3750, 45}, {3800, 55},
                               {3850, 63}, {3900, 70}, {4000, 82}, {4100, 93}, {BATTERY_FULL_MV, 100}};
  const int N = sizeof T / sizeof T[0];
  if (mv <= T[0][0]) return 0;
  if (mv >= T[N - 1][0]) return 100;
  for (int i = 1; i < N; i++)
    if (mv < T[i][0]) return T[i - 1][1] + (mv - T[i - 1][0]) * (T[i][1] - T[i - 1][1]) / (T[i][0] - T[i - 1][0]);
  return 100;
}

void beep(int n) {
  // shortcut: no buzzer fitted yet, so only logged; drive it here once the actuator board is wired (plan step 4)
  if (n > 0) Serial.printf("Beep x%d\n", n);
}

void batteryInit() {
#if BATTERY_USE_INA219
  Wire.beginTransmission(INA219_ADDR);
  batFound = Wire.endTransmission() == 0 && inaWrite(0x00, 0x399F);   // 32V, ±320mV, continuous
  Serial.printf("Battery: INA219 %s (0x%02X)\n", batFound ? "found" : "not found", INA219_ADDR);
#endif
}

void batteryLoop() {
  static uint32_t last = 0, lastLog = 0;
  if (!batFound || millis() - last < 2000) return;
  last = millis();
  uint16_t rb, rs;
  if (!inaRead(0x02, rb) || !inaRead(0x01, rs)) return;
  batMv = (rb >> 3) * 4.0f;                              // bus voltage, 4 mV/bit
  batMa = ((int16_t)rs * 0.01f) / INA219_SHUNT_OHM;      // shunt 10 uV/bit -> mA (+ = discharging)
  if (batMv < 2500) { uiSetBattery("", 0, false); return; }  // no cell on VIN+

  bool charging = batMa < BATTERY_CHARGE_MA;
  float restMv = batMv + (batMa > 0 ? batMa * BATTERY_INTERNAL_MOHM / 1000.0f : 0);  // undo load sag
  float p = lipoPercent(restMv);
  batPct = batPct < 0 ? p : batPct * 0.8f + p * 0.2f;   // smooth
  int pct = (int)(batPct + 0.5f);

  const char *sym = charging ? LV_SYMBOL_CHARGE
                  : pct > 80 ? LV_SYMBOL_BATTERY_FULL : pct > 55 ? LV_SYMBOL_BATTERY_3
                  : pct > 30 ? LV_SYMBOL_BATTERY_2 : pct > 10 ? LV_SYMBOL_BATTERY_1 : LV_SYMBOL_BATTERY_EMPTY;
  char txt[24]; snprintf(txt, sizeof txt, "%s %d%%", sym, pct);
  bool low = pct < BATTERY_LOW_PCT && !charging;
  uiSetBattery(txt, low ? T_ERR : charging ? T_OK : T_INK, true);

  if (millis() - lastLog > 10000) {
    lastLog = millis();
    Serial.printf("Battery: %.0f mV, %.0f mA%s -> %d%%\n", batMv, batMa, charging ? ", charging" : "", pct);
  }
}

// ------------------------------------------------------------ backlight (PWM on TFT_BL)
void setBacklight(int pct) {
  pct = constrain(pct, 5, 100);
  uint32_t duty = (255 * pct) / 100;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  static bool attached = false;
  if (!attached) { ledcAttach(TFT_BL, 5000, 8); attached = true; }
  ledcWrite(TFT_BL, duty);
#else
  static bool attached = false;
  if (!attached) { ledcSetup(0, 5000, 8); ledcAttachPin(TFT_BL, 0); attached = true; }
  ledcWrite(0, duty);
#endif
}

// ------------------------------------------------------------ setup / loop
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("PSRAM: %u KB total, %u KB free\n", ESP.getPsramSize() / 1024, ESP.getFreePsram() / 1024);
  if (ESP.getPsramSize() == 0) Serial.println("!! PSRAM is OFF - set Tools > PSRAM > OPI PSRAM, the screen needs it");
#if BACKLIGHT_PWM
  setBacklight(BACKLIGHT_PCT);
#else
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
#endif
  lv_init();
  bool ok = displayBegin();
  Serial.printf("Display begin: %s\n", ok ? "OK (2 frame buffers, bounce buffer, VSYNC swap)" : "FAILED (check PSRAM / pins)");
#if DISPLAY_TEST
  // Colour test: red, green, blue, white for half a second each
  const uint16_t cols[4] = {0xF800, 0x07E0, 0x001F, 0xFFFF};
  const char *names[4] = {"red", "green", "blue", "white"};
  for (int i = 0; i < 4; i++) { Serial.printf("Display test: %s\n", names[i]); displayFill(cols[i]); delay(500); }
  displayFill(0x0000);
#endif
  Wire.begin(TOUCH_SDA, TOUCH_SCL);      // shared by GT911 touch and INA219
  tp.begin();
  tp.setRotation(ROTATION_INVERTED);   // if taps land mirrored, try ROTATION_NORMAL

  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touch;
  lv_indev_drv_register(&indev_drv);

  rxBuf = (uint8_t *)ps_malloc(MAX_PACKET);
  uiBegin();
  lv_timer_handler();

  batteryInit();
  batteryLoop();

#if USE_HX711
  scale.begin(HX711_DOUT, HX711_SCK);
  scale.set_scale(HX711_SCALE);
  scale.tare();
#endif

  netStart();                            // Wi-Fi -> phone hotspot -> own hotspot
}

// Every 5 s on Serial: what the video and display actually do (evidence for tuning and for the thesis).
void statsLoop() {
  static uint32_t last = 0;
  if (millis() - last < 5000) return;
  float secs = (millis() - last) / 1000.0f;
  last = millis();
  Serial.printf("[stats] video %.1f fps, decode %lu ms avg | swap %lu ms | RAM %u KB, PSRAM %u KB free | RSSI %d dBm\n",
                statFrames / secs, statFrames ? statDecodeMs / statFrames : 0, displayFlushMs(),
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024, ESP.getFreePsram() / 1024, WiFi.RSSI());
  statFrames = statDecodeMs = 0;
}

void loop() {
  pcLoop();
  beaconLoop();
  batteryLoop();
  lv_timer_handler();
  statsLoop();

#if USE_HX711
  static uint32_t lastW = 0;
  if (millis() - lastW > 300 && scale.is_ready()) {
    lastW = millis();
    uiSetWeightFromScale(max(0.0f, scale.get_units(3)));
  }
#endif

  uiLoop();
  delay(2);
}
