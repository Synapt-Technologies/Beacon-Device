#include "sdkconfig.h"
#include "hub75.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_log.h>


#include "platform/Platform.hpp"
#include "config/NvsSettingsStore.hpp"
#include "config/DeviceProfile.hpp"
#include "networkConnection/StaWifiConnection.hpp"
#include "beaconConnection/TcpMqttBeaconConnection.hpp"
#include "consumer/display/Hub75LvglDisplayConsumer.hpp"
#include "consumer/IDisplayConsumer.hpp"
#include "httpServer/EspHttpServer.hpp"

#include "orchestrator/SateliteOrchestrator.hpp"

//? START testimports

// #include "hub75.h"
// #include <lvgl.h>
// #include <freertos/FreeRTOS.h>  // NOLINT(misc-header-include-cycle)
// #include <freertos/task.h>
// #include <freertos/semphr.h>

// ? add below?
// #include "hub75.h"
// #include "sdkconfig.h"
// #include <esp_log.h>
// #include <cstdio>

//? End testimports

static const char *const TAG = "HD-WF2 Satelite";

static inline Hub75Config getHub75Config() {
  Hub75Config config = {};

  // Panel dimensions
  config.panel_width = 64;
  config.panel_height = 32;

  // Scan wiring (scan rate is determined automatically from panel_height)
  config.scan_wiring = Hub75ScanWiring::STANDARD_TWO_SCAN;
  // Shift driver
  config.shift_driver = Hub75ShiftDriver::GENERIC;

  // Huidu HD-WF2
  config.pins.r1 = 2;
  config.pins.g1 = 6;
  config.pins.b1 = 10;
  config.pins.r2 = 3;
  config.pins.g2 = 7;
  config.pins.b2 = 11;
  config.pins.a = 39;
  config.pins.b = 38;
  config.pins.c = 37;
  config.pins.d = 36;
  config.pins.e = 21;
  config.pins.lat = 33;
  config.pins.oe = 35;
  config.pins.clk = 34;


  // Multi-panel layout
  config.layout_rows = 1;
  config.layout_cols = 1;
  config.layout = Hub75PanelLayout::HORIZONTAL;

  config.rotation = Hub75Rotation::ROTATE_0;

  config.output_clock_speed = Hub75ClockSpeed::HZ_10M;

  // Performance settings
  config.min_refresh_rate = 60;
  config.brightness = 255;

  // Timing settings
  config.latch_blanking = 1;

  config.double_buffer = false;

  config.clk_phase_inverted = false;

  // Note: Gamma correction is handled at compile-time via Kconfig (HUB75_GAMMA_MODE)
  // and applied in color_lut.cpp during LUT generation. No runtime config needed.

  return config;
}

//? START testcode

// // Global HUB75 driver instance (needed in flush callback)
// static Hub75Driver *g_driver = nullptr;

// // LVGL mutex for thread safety
// static SemaphoreHandle_t lvgl_mutex = nullptr;

// // Forward declaration
// extern "C" void example_lvgl_demo_ui(lv_display_t *disp);

// // LVGL mutex lock/unlock helpers
// static bool lvgl_lock(int timeout_ms) {
//   const TickType_t timeout_ticks = (timeout_ms < 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
//   return xSemaphoreTakeRecursive(lvgl_mutex, timeout_ticks) == pdTRUE;
// }

// static void lvgl_unlock() { xSemaphoreGiveRecursive(lvgl_mutex); }

// // LVGL display flush callback
// // Called by LVGL when a screen region needs updating
// static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
//   if (g_driver == nullptr) {
//     ESP_LOGE(TAG, "FLUSH: HUB75 driver is NULL!");
//     lv_display_flush_ready(disp);
//     return;
//   }

//   // Get area bounds and draw pixels
//   const uint16_t x = area->x1;
//   const uint16_t y = area->y1;
//   const uint16_t w = area->x2 - area->x1 + 1;
//   const uint16_t h = area->y2 - area->y1 + 1;

//   g_driver->draw_pixels(x, y, w, h, px_map, Hub75PixelFormat::RGB565);

// #ifdef CONFIG_HUB75_DOUBLE_BUFFER
//   g_driver->flip_buffer();
// #endif

//   lv_display_flush_ready(disp);
// }

// // LVGL timer task - calls lv_timer_handler() periodically
// static void lvgl_timer_task(void *arg) {
//   ESP_LOGI(TAG, "LVGL timer task started");

//   TickType_t last_wake_time = xTaskGetTickCount();

//   while (1) {
//     // Lock LVGL mutex
//     if (lvgl_lock(10)) {
//       // Calculate elapsed time and update LVGL tick (required for animations)
//       TickType_t current_time = xTaskGetTickCount();
//       uint32_t elapsed_ms = pdTICKS_TO_MS(current_time - last_wake_time);
//       last_wake_time = current_time;
//       lv_tick_inc(elapsed_ms);

//       // Handle LVGL timers and tasks (triggers redraws and animations)
//       uint32_t sleep_ms = lv_timer_handler();
//       lvgl_unlock();

//       // Ensure reasonable sleep bounds for smooth animations
//       if (sleep_ms > 100) {
//         sleep_ms = 100;  // Max 100ms for responsive animations
//       } else if (sleep_ms < 1) {
//         sleep_ms = 1;  // Min 1ms to prevent busy-wait
//       }

//       vTaskDelay(pdMS_TO_TICKS(sleep_ms));
//     } else {
//       ESP_LOGW(TAG, "Could not get LVGL lock");
//       vTaskDelay(pdMS_TO_TICKS(10));
//     }
//   }
// }


//? End testcode

extern "C" void app_main() {
  ESP_LOGI(TAG, "HUB75 LVGL Simple Demo Starting...");

  vTaskDelay(pdMS_TO_TICKS(100));
  
  Hub75Config config = getHub75Config();

  Platform::init();

  ISettingsStore* settingsStore = new NvsSettingsStore();

  INetworkConnection* network = new StaWifiConnection("HD-WF2_Satellite");

  IBeaconConnection* beacon = new TcpMqttBeaconConnection();

  static const IDisplayConsumer::Zone hub75Zones[] = {
    {   0,   0,     320,  240,  0, DeviceAlertTarget::TALENT,    TallyState::NONE, true }, // background (always visible)
  };
  IConsumer* consumer1 = new Hub75LvglDisplayConsumer(config, hub75Zones, 7);

  IConsumer* consumers[] = { consumer1 };

  EspHttpServer httpServer = EspHttpServer();

  DeviceProfile profile = DeviceProfile{
    .deviceType = DeviceType::SINGLE_TOPIC,
    .model = "HD-WF2 Satellite",
    .consumerCount = 1,
  };

  SateliteOrchestrator orchestrator = SateliteOrchestrator(*settingsStore, profile, *network, *beacon, consumers, profile.consumerCount, httpServer);
  orchestrator.start();

  // Keep app_main alive so stack-allocated runtime objects (orchestrator/http server)
  // remain valid for the lifetime of the firmware.
  while (true) {
      vTaskDelay(portMAX_DELAY);
  }

  // ESP_LOGI(TAG, "Configuration:");
  // ESP_LOGI(TAG, "  Panel: %dx%d pixels", config.panel_width, config.panel_height);
  // ESP_LOGI(TAG, "  Double buffering: %s", config.double_buffer ? "ENABLED" : "DISABLED");

  // // Create and initialize HUB75 driver
  // static Hub75Driver driver(config);  // Static to persist
  // g_driver = &driver;

  // if (!driver.begin()) {
  //   ESP_LOGE(TAG, "Failed to initialize HUB75 driver!");
  //   return;
  // }

  // ESP_LOGI(TAG, "HUB75 driver initialized");
  // ESP_LOGI(TAG, "  Display: %ux%u pixels", driver.get_width(), driver.get_height());
  // ESP_LOGI(TAG, "  Clock: %lu Hz, Bit depth: %u, Refresh: %u Hz", (unsigned long) config.output_clock_speed,
  //          HUB75_BIT_DEPTH, config.min_refresh_rate);
  // ESP_LOGI(TAG, "  Pins - R1=%d G1=%d B1=%d R2=%d G2=%d B2=%d", config.pins.r1, config.pins.g1, config.pins.b1,
  //          config.pins.r2, config.pins.g2, config.pins.b2);
  // ESP_LOGI(TAG, "  Pins - A=%d B=%d C=%d D=%d E=%d CLK=%d LAT=%d OE=%d", config.pins.a, config.pins.b, config.pins.c,
  //          config.pins.d, config.pins.e, config.pins.clk, config.pins.lat, config.pins.oe);

  // // Quick hardware test - RGB color bars
  // driver.clear();
  // ESP_LOGI(TAG, "Drawing RGB test pattern...");
  // const uint16_t bar_width = driver.get_width() / 3;
  // for (uint16_t y = 0; y < driver.get_height(); y++) {
  //   for (uint16_t x = 0; x < driver.get_width(); x++) {
  //     if (x < bar_width) {
  //       driver.set_pixel(x, y, 255, 0, 0);  // Red
  //     } else if (x < bar_width * 2) {
  //       driver.set_pixel(x, y, 0, 255, 0);  // Green
  //     } else {
  //       driver.set_pixel(x, y, 0, 0, 255);  // Blue
  //     }
  //   }
  // }
  // vTaskDelay(pdMS_TO_TICKS(1000));
  // driver.clear();

  // // Initialize LVGL
  // ESP_LOGI(TAG, "Initializing LVGL...");

  // lvgl_mutex = xSemaphoreCreateRecursiveMutex();
  // if (lvgl_mutex == nullptr) {
  //   ESP_LOGE(TAG, "Failed to create LVGL mutex!");
  //   return;
  // }

  // lv_init();

  // lv_display_t *disp = lv_display_create(driver.get_width(), driver.get_height());
  // if (disp == nullptr) {
  //   ESP_LOGE(TAG, "Failed to create LVGL display!");
  //   return;
  // }

  // // Set color format and create draw buffer (LVGL manages memory)
  // lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  // lv_draw_buf_t *draw_buf = lv_draw_buf_create(driver.get_width(), driver.get_height(), LV_COLOR_FORMAT_RGB565, 0);
  // if (draw_buf == nullptr) {
  //   ESP_LOGE(TAG, "Failed to create LVGL draw buffer!");
  //   return;
  // }
  // lv_display_set_draw_buffers(disp, draw_buf, nullptr);
  // lv_display_set_render_mode(disp, LV_DISPLAY_RENDER_MODE_PARTIAL);
  // lv_display_set_flush_cb(disp, lvgl_flush_cb);

  // ESP_LOGI(TAG, "LVGL initialized");

  // // Create demo UI
  // ESP_LOGI(TAG, "Creating demo UI...");
  // if (lvgl_lock(0)) {
  //   example_lvgl_demo_ui(disp);
  //   lv_obj_invalidate(lv_screen_active());
  //   lv_refr_now(disp);
  //   lvgl_unlock();
  // } else {
  //   ESP_LOGE(TAG, "Could not lock LVGL for UI creation!");
  // }

  // // Start LVGL timer task
  // BaseType_t ret = xTaskCreate(lvgl_timer_task, "lvgl_timer",
  //                              4096,     // Stack size
  //                              nullptr,  // No parameters
  //                              4,        // Priority
  //                              nullptr   // No task handle needed
  // );

  // if (ret != pdPASS) {
  //   ESP_LOGE(TAG, "Failed to create LVGL timer task!");
  //   return;
  // }

  // ESP_LOGI(TAG, "LVGL running, demo UI displayed");

  // // Main loop - nothing to do, LVGL task handles everything
  // while (true) {
  //   vTaskDelay(pdMS_TO_TICKS(1000));
  // }
}