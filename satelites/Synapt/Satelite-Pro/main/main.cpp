#include <stdio.h>
#include "hub75.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

#include "platform/Platform.hpp"
#include "config/NvsSettingsStore.hpp"
#include "config/DeviceProfile.hpp"
#include "networkConnection/StaWifiConnection.hpp"
#include "beaconConnection/TcpMqttBeaconConnection.hpp"
#include "httpServer/EspHttpServer.hpp"
#include "consumer/display/Hub75LvglDisplayConsumer.hpp"

#include "orchestrator/SateliteOrchestrator.hpp"

namespace {
constexpr const char* TAG = "SateliteMain";

// Set true to isolate pure HUB75 hardware behavior (no LVGL/orchestrator/network).
constexpr bool HUB75_HARDWARE_DIAG_MODE = false;
constexpr bool HUB75_BITBANG_DIAG_MODE = false;

// Topology selector for bit-bang diagnostic:
// 1: clocks_per_latch=64, row_count_divisor=2 (standard two-scan)
// 2: clocks_per_latch=128, row_count_divisor=2
// 3: clocks_per_latch=64, row_count_divisor=4 (four-scan low)
// 4: clocks_per_latch=128, row_count_divisor=4 (four-scan full)
constexpr int HUB75_BITBANG_TOPOLOGY_TEST = 1;  // Change this to 2, 3, or 4 to test

const char* shiftDriverName(Hub75ShiftDriver driver)
{
    switch (driver) {
        case Hub75ShiftDriver::GENERIC:  return "GENERIC";
        case Hub75ShiftDriver::FM6126A:  return "FM6126A";
        case Hub75ShiftDriver::ICN2038S: return "ICN2038S";
        case Hub75ShiftDriver::FM6124:   return "FM6124";
        case Hub75ShiftDriver::MBI5124:  return "MBI5124";
        case Hub75ShiftDriver::DP3246:   return "DP3246";
        default:                         return "UNKNOWN";
    }
}

const char* scanWiringName(Hub75ScanWiring wiring)
{
    switch (wiring) {
        case Hub75ScanWiring::STANDARD_TWO_SCAN:  return "STANDARD_TWO_SCAN";
        case Hub75ScanWiring::SCAN_1_4_16PX_HIGH: return "SCAN_1_4_16PX_HIGH";
        case Hub75ScanWiring::SCAN_1_8_32PX_HIGH: return "SCAN_1_8_32PX_HIGH";
        case Hub75ScanWiring::SCAN_1_8_32PX_FULL: return "SCAN_1_8_32PX_FULL";
        case Hub75ScanWiring::SCAN_1_8_40PX_HIGH: return "SCAN_1_8_40PX_HIGH";
        case Hub75ScanWiring::SCAN_1_8_64PX_HIGH: return "SCAN_1_8_64PX_HIGH";
        default:                                  return "UNKNOWN";
    }
}

void logPinCapability(const char* name, int8_t pin)
{
    if (pin < 0) {
        ESP_LOGI(TAG, "Pin %-4s = %d (unused)", name, pin);
        return;
    }

    ESP_LOGI(TAG, "Pin %-4s = %d | valid=%d output=%d",
             name, pin, GPIO_IS_VALID_GPIO(pin), GPIO_IS_VALID_OUTPUT_GPIO(pin));
}

void logHub75Config(const Hub75Config& config)
{
    ESP_LOGI(TAG,
             "HUB75 config: %ux%u scan=%s driver=%s clk_phase_inv=%s clock=%u latch_blanking=%u brightness=%u double_buffer=%s",
             config.panel_width,
             config.panel_height,
             scanWiringName(config.scan_wiring),
             shiftDriverName(config.shift_driver),
             config.clk_phase_inverted ? "true" : "false",
             static_cast<unsigned int>(config.output_clock_speed),
             config.latch_blanking,
             config.brightness,
             config.double_buffer ? "true" : "false");

    logPinCapability("R1", config.pins.r1);
    logPinCapability("G1", config.pins.g1);
    logPinCapability("B1", config.pins.b1);
    logPinCapability("R2", config.pins.r2);
    logPinCapability("G2", config.pins.g2);
    logPinCapability("B2", config.pins.b2);
    logPinCapability("A", config.pins.a);
    logPinCapability("B", config.pins.b);
    logPinCapability("C", config.pins.c);
    logPinCapability("D", config.pins.d);
    logPinCapability("E", config.pins.e);
    logPinCapability("LAT", config.pins.lat);
    logPinCapability("OE", config.pins.oe);
    logPinCapability("CLK", config.pins.clk);
}

inline void pulseClock(const Hub75Pins& pins)
{
    gpio_set_level((gpio_num_t)pins.clk, 1);
    gpio_set_level((gpio_num_t)pins.clk, 0);
}

void setRowAddress(const Hub75Pins& pins, uint8_t row)
{
    gpio_set_level((gpio_num_t)pins.a, row & 0x01);
    gpio_set_level((gpio_num_t)pins.b, (row >> 1) & 0x01);
    gpio_set_level((gpio_num_t)pins.c, (row >> 2) & 0x01);
    gpio_set_level((gpio_num_t)pins.d, (row >> 3) & 0x01);
    if (pins.e >= 0) {
        gpio_set_level((gpio_num_t)pins.e, (row >> 4) & 0x01);
    }
}

void configureHub75PinsForBitbang(const Hub75Pins& pins)
{
    const int8_t pin_list[] = {
        pins.r1, pins.g1, pins.b1, pins.r2, pins.g2, pins.b2,
        pins.a,  pins.b,  pins.c,  pins.d,  pins.e,
        pins.lat, pins.oe, pins.clk
    };

    for (int8_t pin : pin_list) {
        if (pin < 0) {
            continue;
        }
        gpio_reset_pin((gpio_num_t)pin);
        gpio_set_direction((gpio_num_t)pin, GPIO_MODE_OUTPUT);
        gpio_set_level((gpio_num_t)pin, 0);
    }

    gpio_set_level((gpio_num_t)pins.oe, 1); // start blanked
    gpio_set_level((gpio_num_t)pins.lat, 0);
    gpio_set_level((gpio_num_t)pins.clk, 0);
}

[[noreturn]] void runHub75BitbangDiag(const Hub75Config& config)
{
    ESP_LOGW(TAG, "HUB75 bit-bang diagnostic mode enabled");
    ESP_LOGW(TAG, "Bypassing I2S/DMA and writing panel directly");
    ESP_LOGW(TAG, "If this still stays black, issue is likely power/cable/logic-level/pin routing");

    const Hub75Pins& pins = config.pins;
    configureHub75PinsForBitbang(pins);

    // Topology selector: determines clocks_per_latch and row_count divisor
    uint16_t clocks_per_latch = 64;
    uint8_t row_divisor = 2;
    const char* topology_desc = "";

    switch (HUB75_BITBANG_TOPOLOGY_TEST) {
        case 1:
            clocks_per_latch = 64;
            row_divisor = 2;
            topology_desc = "64 clocks, row_divisor=2 (standard two-scan)";
            break;
        case 2:
            clocks_per_latch = 128;
            row_divisor = 2;
            topology_desc = "128 clocks, row_divisor=2 (full-width two-scan)";
            break;
        case 3:
            clocks_per_latch = 64;
            row_divisor = 4;
            topology_desc = "64 clocks, row_divisor=4 (four-scan low)";
            break;
        case 4:
            clocks_per_latch = 128;
            row_divisor = 4;
            topology_desc = "128 clocks, row_divisor=4 (four-scan full)";
            break;
        default:
            ESP_LOGE(TAG, "Invalid HUB75_BITBANG_TOPOLOGY_TEST=%d; using default", HUB75_BITBANG_TOPOLOGY_TEST);
            clocks_per_latch = 64;
            row_divisor = 2;
            topology_desc = "fallback: 64 clocks, row_divisor=2";
    }

    const uint8_t row_count = static_cast<uint8_t>(config.panel_height / row_divisor);
    ESP_LOGW(TAG, "Bit-bang topology test: %s", topology_desc);
    ESP_LOGW(TAG, "Bit-bang row scan: %u rows (clocks_per_latch=%u)", row_count, clocks_per_latch);

    uint8_t color_step = 0;

    while (true) {
        uint8_t r = 0;
        uint8_t g = 0;
        uint8_t b = 0;
        switch (color_step) {
            case 0: r = 1; break;               // red
            case 1: g = 1; break;               // green
            case 2: b = 1; break;               // blue
            default: r = g = b = 1; break;      // white
        }

        TickType_t end = xTaskGetTickCount() + pdMS_TO_TICKS(1500);
        while (xTaskGetTickCount() < end) {
            for (uint8_t row = 0; row < row_count; row++) {
                gpio_set_level((gpio_num_t)pins.oe, 1); // blank during shifting/latching
                setRowAddress(pins, row);

                gpio_set_level((gpio_num_t)pins.r1, r);
                gpio_set_level((gpio_num_t)pins.g1, g);
                gpio_set_level((gpio_num_t)pins.b1, b);
                gpio_set_level((gpio_num_t)pins.r2, r);
                gpio_set_level((gpio_num_t)pins.g2, g);
                gpio_set_level((gpio_num_t)pins.b2, b);

                for (uint16_t x = 0; x < clocks_per_latch; x++) {
                    pulseClock(pins);
                }

                gpio_set_level((gpio_num_t)pins.lat, 1);
                esp_rom_delay_us(1);
                gpio_set_level((gpio_num_t)pins.lat, 0);

                gpio_set_level((gpio_num_t)pins.oe, 0); // show row
                esp_rom_delay_us(120);
            }
        }

        color_step = static_cast<uint8_t>((color_step + 1) & 0x03);
    }
}

[[noreturn]] void runHub75HardwareDiag(const Hub75Config& config)
{
    ESP_LOGW(TAG, "HUB75 hardware-only diagnostic mode enabled");
    ESP_LOGW(TAG, "Cycling full-screen RED/GREEN/BLUE/WHITE every 2 seconds");

    Hub75Driver driver(config);
    if (!driver.begin()) {
        ESP_LOGE(TAG, "HUB75 driver begin() failed in diagnostic mode");
        while (true) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    const uint16_t width = driver.get_width();
    const uint16_t height = driver.get_height();

    while (true) {
        driver.fill(0, 0, width, height, 255, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(2000));
        driver.fill(0, 0, width, height, 0, 255, 0);
        vTaskDelay(pdMS_TO_TICKS(2000));
        driver.fill(0, 0, width, height, 0, 0, 255);
        vTaskDelay(pdMS_TO_TICKS(2000));
        driver.fill(0, 0, width, height, 255, 255, 255);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
}

extern "C" void app_main()
{
    vTaskDelay(pdMS_TO_TICKS(1000));

    Platform::init();

    Hub75Config config = {};
    config.panel_width = 64;
    config.panel_height = 32;
    config.scan_wiring = Hub75ScanWiring::STANDARD_TWO_SCAN;
    config.shift_driver = Hub75ShiftDriver::MBI5124;
    config.clk_phase_inverted = true;
    config.output_clock_speed = Hub75ClockSpeed::HZ_10M;
    config.latch_blanking = 2;
    config.double_buffer  = false;
    config.brightness     = 255; // TODO: Override setbrightness

    config.pins.r1 = 39;
    config.pins.g1 = 40;
    config.pins.b1 = 4;
    config.pins.r2 = 38;
    config.pins.g2 = 36;
    config.pins.b2 = 2;
    config.pins.a = 14;
    config.pins.b = 13;
    config.pins.c = 10;
    config.pins.d = 8;
    // config.pins.e = 6;
    config.pins.e = -1; // Not used for 32-row panels
    config.pins.lat = 17;
    config.pins.oe = 21;
    config.pins.clk = 34;

    logHub75Config(config);

    if (HUB75_BITBANG_DIAG_MODE) {
        runHub75BitbangDiag(config);
    }

    if (HUB75_HARDWARE_DIAG_MODE) {
        runHub75HardwareDiag(config);
    }

    ISettingsStore* settingsStore = new NvsSettingsStore();
    INetworkConnection* network = new StaWifiConnection("Satelite_PRO");
    IBeaconConnection* beacon = new TcpMqttBeaconConnection();
    EspHttpServer httpServer = EspHttpServer();

    static const IDisplayConsumer::Zone hub75Zones[] = {
        {   0,   0,     64,  32,  1, DeviceAlertTarget::TALENT,    TallyState::NONE, true }, // background (always visible)
    };
    static const ILvglDisplayConsumer::TextConfig hub75Text[] = {
        { &lv_font_montserrat_28, 255, LV_ALIGN_CENTER, 0, 0 },
    };

    IConsumer* consumer1 = new Hub75LvglDisplayConsumer(config, hub75Zones, 1, hub75Text, 1);
    IConsumer* consumers[] = { consumer1 };

    DeviceProfile profile = DeviceProfile{
        .deviceType = DeviceType::SINGLE_TOPIC,
        .model = "Satellite PRO",
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
