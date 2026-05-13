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
  config.pins.r1  = 2;
  config.pins.g1  = 10;
  config.pins.b1  = 6;
  config.pins.r2  = 3;
  config.pins.g2  = 11;
  config.pins.b2  = 7;
  config.pins.a   = 39;
  config.pins.b   = 38;
  config.pins.c   = 37;
  config.pins.d   = 36;
  config.pins.e   = 21;
  config.pins.lat = 33;
  config.pins.oe  = 35;
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


extern "C" void app_main() {
  ESP_LOGI(TAG, "HUB75 LVGL Simple Demo Starting...");

  vTaskDelay(pdMS_TO_TICKS(100));
  
  Hub75Config config = getHub75Config();

  Platform::init();

  ISettingsStore* settingsStore = new NvsSettingsStore();

  INetworkConnection* network = new StaWifiConnection("HD-WF2_Satellite");

  IBeaconConnection* beacon = new TcpMqttBeaconConnection();

  static const IDisplayConsumer::Zone hub75Zones[] = {
    {    0,  0,  10,  3,  1, DeviceAlertTarget::TALENT,    TallyState::NONE,    true },  // Top Left
    {    10, 0,  44,  3,  0, DeviceAlertTarget::TALENT,    TallyState::NONE,    true },  // Top Bar
    {    54, 0,  10,  3,  2, DeviceAlertTarget::TALENT,    TallyState::NONE,    true },  // Top Right
    {    0, 29,  10,  3,  1, DeviceAlertTarget::TALENT,    TallyState::NONE,    true },  // Bottom Left
    {   10, 29,  44,  3,  0, DeviceAlertTarget::TALENT,    TallyState::NONE,    true },  // Bottom Bar
    {   54, 29,  10,  3,  2, DeviceAlertTarget::TALENT,    TallyState::NONE,    true },  // Bottom Right
    {    0,  3,  10, 26,  1, DeviceAlertTarget::TALENT,    TallyState::PROGRAM, true },  // Left Bar
    {   54,  3,  10, 26,  2, DeviceAlertTarget::TALENT,    TallyState::PROGRAM, true },  // Right Bar
    {   10,  3,  44, 26,  0, DeviceAlertTarget::TALENT,    TallyState::PROGRAM, true },  // Center 
  };
  // static const IDisplayConsumer::Zone hub75Zones[] = {
  //   {    0,  0,  10,  3,  1, DeviceAlertTarget::TALENT,    TallyState::NONE,    true },  // Top Left
  //   {    10, 0,  44,  3,  0, DeviceAlertTarget::TALENT,    TallyState::PROGRAM, true },  // Top Bar
  //   {    54, 0,  10,  3,  2, DeviceAlertTarget::TALENT,    TallyState::NONE,    true },  // Top Right
  //   {    0, 29,  10,  3,  1, DeviceAlertTarget::TALENT,    TallyState::NONE,    true },  // Bottom Left
  //   {   10, 29,  44,  3,  0, DeviceAlertTarget::TALENT,    TallyState::PROGRAM, true },  // Bottom Bar
  //   {   54, 29,  10,  3,  2, DeviceAlertTarget::TALENT,    TallyState::NONE,    true },  // Bottom Right
  //   {    0,  3,  10, 26,  1, DeviceAlertTarget::TALENT,    TallyState::NONE,    true },  // Left Bar
  //   {   54,  3,  10, 26,  2, DeviceAlertTarget::TALENT,    TallyState::NONE,    true },  // Right Bar
  //   {   10,  3,  44, 26,  0, DeviceAlertTarget::TALENT,    TallyState::PROGRAM, true },  // Center 
  // };
  static const ILvglDisplayConsumer::FixedTextConfig text0 { &lv_font_montserrat_32, 255, LV_ALIGN_CENTER, 0, 0, 1 };
  // static const ILvglDisplayConsumer::AutoTextConfig text0 { 255, LV_ALIGN_CENTER, 0, 0, 1, 12, 0, 60, 30, false };
  static const ILvglDisplayConsumer::TextConfig* const textConf[] = { &text0 };
  IConsumer* consumer1 = new Hub75LvglDisplayConsumer(config, hub75Zones, 9, textConf, 1);

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

}