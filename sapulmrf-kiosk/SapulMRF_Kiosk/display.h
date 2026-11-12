#pragma once
// Tear-free RGB panel driver for LVGL (replaces Arduino_GFX).
// Two full frame buffers in PSRAM: LVGL draws into the hidden one, and the panel switches to it
// on VSYNC, so a frame is never shown half-drawn. A bounce buffer in internal RAM feeds the panel's
// DMA, so busy PSRAM (Wi-Fi, JPEG decode, LVGL) can no longer make the picture shift or shimmer.
#include <lvgl.h>

bool displayBegin();                 // panel + LVGL display driver; false if the panel can't start
void displayFill(uint16_t rgb565);   // whole screen one colour, for the boot colour test (before LVGL draws)
uint32_t displayFlushMs();           // average time of the last swaps, for the stats line
