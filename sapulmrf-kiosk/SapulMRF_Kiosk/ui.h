#pragma once
// The kiosk's screens. The sketch (SapulMRF_Kiosk.ino) owns the network, the camera frames, the battery
// and the PC protocol; it calls these when something happens, and the UI calls back through wsSend/setMode.
#include <Arduino.h>
#include <ArduinoJson.h>
#include <lvgl.h>

void uiBegin();                                   // build every screen and the status strip, show Welcome
void uiLoop();                                    // timers: idle return, PIN timeout, "nothing seen" hint

// ---- from the PC
void uiOnDet(JsonDocument &d);                    // the camera's running result
void uiOnFrameShown(int fps);                     // a video frame was decoded into uiFrameBuf
void uiOnReceipt(JsonDocument &d);
void uiOnAlerts(JsonDocument &d);
void uiOnStats(float kg, int drops);
void uiOnAuth(JsonDocument &d);                   // an operator's PIN, checked by SapulMRF
void uiOnOperators(JsonDocument &d);              // who can sign in
void uiOnOp(JsonDocument &d);                     // SapulMRF's answer to an operator task
void uiOnPhoto(int deliveryId, bool ok);          // a camera-review photo was decoded into uiPhotoBuf

// ---- from the sketch
void uiSetLight(const char *color);               // "green" | "amber" | "red", decided by SapulMRF
void uiSetPcUp(bool up);
void uiSetNet(const char *line, const char *ip);
void uiSetBattery(const char *text, uint32_t color, bool visible);
void uiSetWeightFromScale(float kg);              // HX711, when fitted
bool uiWantsVideo();                              // the scan screen is showing and not frozen on a result
const char *uiScreenName();                       // for screenshots
bool uiGoto(const char *name);                    // dev tools: show a screen by name

extern uint16_t *uiFrameBuf;                      // FRAME_W x FRAME_H RGB565; the sketch decodes JPEG into it
extern uint16_t *uiPhotoBuf;                      // PHOTO_W x PHOTO_H RGB565, camera-review photo

// ---- provided by the sketch
void wsSend(JsonDocument &doc);
void setMode(const char *mode);
extern bool pcUp;
