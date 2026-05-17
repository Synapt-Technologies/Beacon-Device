#pragma once

#include "config/Config.hpp"
#include "config/ISettingsStore.hpp"
#include "config/DeviceProfile.hpp"
#include "networkConnection/INetworkConnection.hpp"
#include "beaconConnection/IBeaconConnection.hpp"
#include "consumer/IConsumer.hpp"
#include "httpServer/EspHttpServer.hpp"
#include "httpServer/HttpHandlers.hpp"
#include "orchestrator/handlers/TallyHandler.hpp"
#include <cstring>

// TODO: Rewrite! Lot of satelite code that should not be here!
class IOrchestrator {
public:
    static constexpr uint8_t MAX_CONSUMERS = 8;

    IOrchestrator(  ISettingsStore&      store,
                    const DeviceProfile& profile,
                    INetworkConnection&  network,
                    IBeaconConnection&   beacon,
                    TallyHandler&        tallyHandler,
                    EspHttpServer&       http
                ) :
          _config(store)
        , _profile(profile)
        , _network(network)
        , _beacon(beacon)
        , _tallyHandler(tallyHandler)
        , _http(http)
        , _httpCtx{_config, _profile, _network, _beacon}
    {
    }

    virtual ~IOrchestrator() = default;

    void addConsumer(IConsumer* consumer) {
        if (_consumerCount >= MAX_CONSUMERS) {
            ESP_LOGW("Orchestrator", "Max consumers reached, cannot add more");
            return;    
        }
        _consumers[_consumerCount++] = consumer;
        _tallyHandler.addConsumer(consumer);
    }

    void start();
    virtual void stop() = 0;

protected:
    static constexpr char TAG[] = "SateliteOrch";

    static constexpr uint32_t STARTUP_STACK_SIZE = 8192;
    virtual void doStart() = 0;
    
    Config               _config;
    const DeviceProfile& _profile;
    INetworkConnection&  _network;
    IBeaconConnection&   _beacon;
    IConsumer*           _consumers[MAX_CONSUMERS] = {};
    uint8_t              _consumerCount            = 0;
    TallyHandler&        _tallyHandler;
    EspHttpServer&       _http;

    // TODO Move to UI class.
    HttpCtx              _httpCtx;

    // TODO Check if needed, or should be stored in the INetworkConnection implementation. Some devices may not have an IP.
    NetworkStatus  _networkStatus = NetworkStatus::DISCONNECTED;
    esp_ip4_addr_t _networkIp;
};
