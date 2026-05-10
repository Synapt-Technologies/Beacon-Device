#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "driver/gpio.h"

#include "platform/Platform.hpp"
#include "config/NvsSettingsStore.hpp"
#include "config/DeviceProfile.hpp"
#include "networkConnection/StaWifiConnection.hpp"
#include "beaconConnection/TcpMqttBeaconConnection.hpp"
#include "consumer/SimpleRGBConsumer.hpp"
#include "consumer/WS2812Consumer.hpp"
#include "consumer/display/Ili9341LvglDisplayConsumer.hpp"
#include "httpServer/EspHttpServer.hpp"

#include "orchestrator/SateliteOrchestrator.hpp"

// ── Pin / hardware constants ──────────────────────────────────────────────────

#define FIX_LED_R_GPIO          GPIO_NUM_4
#define FIX_LED_G_GPIO          GPIO_NUM_16
#define FIX_LED_B_GPIO          GPIO_NUM_17
#define ADD_LED_STRIP_GPIO      22
#define ADD_LED_STRIP_LED_NUMBER 64

static led_strip_handle_t createLedStrip()
{
    led_strip_config_t stripCfg = {
        ADD_LED_STRIP_GPIO,
        ADD_LED_STRIP_LED_NUMBER,
        LED_MODEL_WS2812,
        LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        { false },
    };
    led_strip_spi_config_t spiCfg = {
        SPI_CLK_SRC_DEFAULT,
        SPI2_HOST,
        { true },
    };
    led_strip_handle_t strip;
    ESP_ERROR_CHECK(led_strip_new_spi_device(&stripCfg, &spiCfg, &strip));
    return strip;
}

// ── Composition root ──────────────────────────────────────────────────────────

// ── LED layout (static to avoid stack overflow) ────────────────────────────────

// static LedLayout g_layout;

extern "C" void app_main()
{
    vTaskDelay(pdMS_TO_TICKS(100));

    Platform::init();

    ISettingsStore* settingsStore = new NvsSettingsStore();
    DeviceProfile profile = DeviceProfile{
        .deviceType = DeviceType::SINGLE_TOPIC,
        .model = "CYD Satellite",
        .consumerCount = 3,
    };
    INetworkConnection* network = new StaWifiConnection("CYD_Satellite");

    IBeaconConnection* beacon = new TcpMqttBeaconConnection();

    static StripSection ws2812Sections[] = {
        { 0, 1, DeviceAlertTarget::OPERATOR },
        { 24, 0, DeviceAlertTarget::OPERATOR },
        { 40, 2, DeviceAlertTarget::OPERATOR },
    };
    IConsumer* consumer1 = new WS2812Consumer(createLedStrip(), ADD_LED_STRIP_LED_NUMBER, ws2812Sections, 3);
    IConsumer* consumer2 = new SimpleRGBConsumer(FIX_LED_R_GPIO, FIX_LED_G_GPIO, FIX_LED_B_GPIO, DeviceAlertTarget::OPERATOR);

    static const IDisplayConsumer::Zone cydZones[] = {
        {   0,   0,     320,  240,  0, DeviceAlertTarget::TALENT,    TallyState::NONE, true }, // background (always visible)
        {   0,   0,      40,  240,  1, DeviceAlertTarget::TALENT,    TallyState::NONE, true }, // left alert bar
        {  40,   0,     120,   10,  1, DeviceAlertTarget::TALENT,    TallyState::NONE, true }, // left alert bar
        {  40,   230,   120,   10,  1, DeviceAlertTarget::TALENT,    TallyState::NONE, true }, // left alert bar
        { 280,   0,      40,  240,  2, DeviceAlertTarget::TALENT,    TallyState::NONE, true }, // right alert bar
        { 160,   0,     120,   10,  2, DeviceAlertTarget::TALENT,    TallyState::NONE, true }, // right alert bar
        { 160,   230,   120,   10,  2, DeviceAlertTarget::TALENT,    TallyState::NONE, true }, // left alert bar
    };
    IConsumer* consumer3 = new Ili9341LvglDisplayConsumer(cydZones, 7);

    IConsumer* consumers[] = { consumer1, consumer2, consumer3 };

    EspHttpServer httpServer = EspHttpServer();

    SateliteOrchestrator orchestrator = SateliteOrchestrator(*settingsStore, profile, *network, *beacon, consumers, profile.consumerCount, httpServer);
    orchestrator.start();

    // Keep app_main alive so stack-allocated runtime objects (orchestrator/http server)
    // remain valid for the lifetime of the firmware.
    while (true) {
        vTaskDelay(portMAX_DELAY);
    }
    
}
