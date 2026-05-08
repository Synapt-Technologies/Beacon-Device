idf_component_register(
    SRCS
        "main.cpp"
        "beaconConnection/IBeaconConnection.hpp"
        "beaconConnection/TcpMqttBeaconConnection.hpp"
        "config/Config.hpp"
        "config/Config.cpp"
        "config/DeviceProfile.hpp"
        "config/ISettingsStore.hpp"
        "config/NvsSettingsStore.hpp"
        "config/NvsSettingsStore.cpp"
        "config/Settings.hpp"
        "consumer/IConsumer.hpp"
        "consumer/SimpleRGBConsumer.hpp"
        "consumer/WS2812Consumer.hpp"
        "consumer/CYDDisplayConsumer.hpp"
        "consumer/CYDDisplayConsumer.cpp"
        "httpServer/EspHttpServer.hpp"
        "httpServer/EspHttpServer.cpp"
        "httpServer/HttpHandlers.hpp"
        "httpServer/HttpHandlers.cpp"
        "networkConnection/INetworkConnection.hpp"
        "networkConnection/IEthernetConnection.hpp"
        "networkConnection/IWifiConnection.hpp"
        "networkConnection/StaWifiConnection.hpp"
        "networkConnection/StaWifiConnection.cpp"
        "orchestrator/IOrchestrator.hpp"
        "orchestrator/NodeOrchestrator.hpp"
        "orchestrator/SateliteOrchestrator.hpp"
        "orchestrator/SateliteOrchestrator.cpp"
        "types/TallyTypes.hpp"
        "config.cpp"
        "led_controller.cpp"
        "led_layout.cpp"
        "wifi_manager.cpp"
        "mqtt_manager.cpp"
        "web_server.cpp"
        "beacon_app.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        esp_driver_gpio esp_driver_spi esp_lcd led_strip
        mqtt esp_netif esp_wifi nvs_flash
        esp_http_server cjson
        lvgl__lvgl
        espressif__esp_lcd_ili9341
        espressif__esp_lvgl_port
    EMBED_TXTFILES
        "ui/index.html"
        "ui/ui.css"
        "ui/ui.js"
)

