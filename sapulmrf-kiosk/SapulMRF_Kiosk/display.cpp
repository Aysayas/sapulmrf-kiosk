#include "display.h"

#include <Arduino.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_rgb.h>
#include <freertos/semphr.h>

#include "config.h"

static const int W = 800, H = 480;
static esp_lcd_panel_handle_t panel = nullptr;
static SemaphoreHandle_t vsyncSem = nullptr;
static uint16_t *fb[2] = {nullptr, nullptr};
static uint16_t *shown = nullptr;          // the buffer the panel is scanning out
static lv_disp_draw_buf_t drawBuf;
static lv_disp_drv_t drv;
static uint32_t flushMsAvg = 0;

static bool IRAM_ATTR onVsync(esp_lcd_panel_handle_t, const esp_lcd_rgb_panel_event_data_t *, void *) {
  BaseType_t woken = pdFALSE;
  xSemaphoreGiveFromISR(vsyncSem, &woken);
  return woken == pdTRUE;
}

// LVGL direct mode: it has drawn straight into one of our frame buffers. On the last area of a
// refresh, show that buffer from the next VSYNC on, then copy what changed into the other buffer,
// which LVGL draws into next.
static void flush(lv_disp_drv_t *d, const lv_area_t *, lv_color_t *color) {
  if (lv_disp_flush_is_last(d)) {
    uint32_t t0 = millis();
    uint16_t *front = (uint16_t *)color;
    uint16_t *back = front == fb[0] ? fb[1] : fb[0];
    xSemaphoreTake(vsyncSem, 0);                                 // forget a VSYNC from before this frame
    esp_lcd_panel_draw_bitmap(panel, 0, 0, W, H, front);         // one of our buffers: the driver just switches
    xSemaphoreTake(vsyncSem, pdMS_TO_TICKS(100));                // the panel now scans `front`
    shown = front;
    lv_disp_t *disp = _lv_refr_get_disp_refreshing();
    for (int i = 0; i < disp->inv_p; i++) {
      if (disp->inv_area_joined[i]) continue;
      const lv_area_t &a = disp->inv_areas[i];
      size_t bytes = (a.x2 - a.x1 + 1) * 2;
      for (int y = a.y1; y <= a.y2; y++) memcpy(&back[y * W + a.x1], &front[y * W + a.x1], bytes);
    }
    flushMsAvg = (flushMsAvg * 7 + (millis() - t0)) / 8;
  }
  lv_disp_flush_ready(d);
}

bool displayBegin() {
  esp_lcd_rgb_panel_config_t cfg = {};
  cfg.clk_src = LCD_CLK_SRC_DEFAULT;
  cfg.timings.pclk_hz = RGB_PCLK_HZ;
  cfg.timings.h_res = W;
  cfg.timings.v_res = H;
  // Same timing and polarities the Arduino_GFX setup used for this panel
  cfg.timings.hsync_front_porch = 180;
  cfg.timings.hsync_pulse_width = 30;
  cfg.timings.hsync_back_porch = 16;
  cfg.timings.vsync_front_porch = 12;
  cfg.timings.vsync_pulse_width = 13;
  cfg.timings.vsync_back_porch = 10;
  cfg.timings.flags.hsync_idle_low = 1;
  cfg.timings.flags.vsync_idle_low = 1;
  cfg.timings.flags.pclk_active_neg = 1;
  cfg.data_width = 16;
  cfg.bits_per_pixel = 16;
  cfg.num_fbs = 2;
  cfg.bounce_buffer_size_px = W * 10;      // 10 lines in internal RAM, refilled from PSRAM
  cfg.dma_burst_size = 64;
  cfg.hsync_gpio_num = TFT_HSYNC;
  cfg.vsync_gpio_num = TFT_VSYNC;
  cfg.de_gpio_num = TFT_DE;
  cfg.pclk_gpio_num = TFT_PCLK;
  cfg.disp_gpio_num = -1;
  const int pins[16] = {TFT_B0, TFT_B1, TFT_B2, TFT_B3, TFT_B4, TFT_G0, TFT_G1, TFT_G2,
                        TFT_G3, TFT_G4, TFT_G5, TFT_R0, TFT_R1, TFT_R2, TFT_R3, TFT_R4};
  for (int i = 0; i < 16; i++) cfg.data_gpio_nums[i] = pins[i];
  cfg.flags.fb_in_psram = 1;

  if (esp_lcd_new_rgb_panel(&cfg, &panel) != ESP_OK) return false;
  vsyncSem = xSemaphoreCreateBinary();
  esp_lcd_rgb_panel_event_callbacks_t cbs = {};
  cbs.on_vsync = onVsync;
  esp_lcd_rgb_panel_register_event_callbacks(panel, &cbs, nullptr);
  if (esp_lcd_panel_reset(panel) != ESP_OK || esp_lcd_panel_init(panel) != ESP_OK) return false;
  esp_lcd_rgb_panel_get_frame_buffer(panel, 2, (void **)&fb[0], (void **)&fb[1]);
  memset(fb[0], 0, W * H * 2);
  memset(fb[1], 0, W * H * 2);
  shown = fb[0];

  lv_disp_draw_buf_init(&drawBuf, fb[0], fb[1], W * H);
  lv_disp_drv_init(&drv);
  drv.hor_res = W;
  drv.ver_res = H;
  drv.flush_cb = flush;
  drv.draw_buf = &drawBuf;
  drv.direct_mode = 1;                     // LVGL draws into the frame buffers themselves
  lv_disp_drv_register(&drv);
  return true;
}

void displayFill(uint16_t rgb565) {
  for (int i = 0; i < W * H; i++) fb[0][i] = rgb565;   // the panel shows buffer 0 until LVGL's first swap
}

uint32_t displayFlushMs() { return flushMsAvg; }

const uint16_t *displayShown() { return shown; }
