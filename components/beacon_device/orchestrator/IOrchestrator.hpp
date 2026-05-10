#pragma once

#include "config/Config.hpp"
#include "config/ISettingsStore.hpp"
#include "config/DeviceProfile.hpp"
#include "networkConnection/INetworkConnection.hpp"
#include "beaconConnection/IBeaconConnection.hpp"
#include "consumer/IConsumer.hpp"
#include "httpServer/EspHttpServer.hpp"
#include "httpServer/HttpHandlers.hpp"
#include <cstring>

class IOrchestrator {
public:
    static constexpr uint8_t MAX_CONSUMERS = 8;

    IOrchestrator(  ISettingsStore&      store,
                    const DeviceProfile& profile,
                    INetworkConnection&  network,
                    IBeaconConnection&   beacon,
                    IConsumer**          consumers,
                    uint8_t              consumerCount,
                    EspHttpServer&       http
                ) :
          _config(store)
        , _profile(profile)
        , _network(network)
        , _beacon(beacon)
        , _consumerCount(consumerCount < MAX_CONSUMERS ? consumerCount : MAX_CONSUMERS)
        , _http(http)
        , _httpCtx{_config, _profile, _network, _beacon}
    {
        memset(_consumers, 0, sizeof(_consumers));
        memcpy(_consumers, consumers, _consumerCount * sizeof(IConsumer*));
    }

    virtual ~IOrchestrator() = default;

    virtual void start() = 0;
    virtual void stop()  = 0;

protected:
    Config               _config;
    const DeviceProfile& _profile;
    INetworkConnection&  _network;
    IBeaconConnection&   _beacon;
    IConsumer*           _consumers[MAX_CONSUMERS];
    uint8_t              _consumerCount;
    EspHttpServer&       _http;

    // TODO Move to UI class.
    HttpCtx              _httpCtx;

    // TODO Check if needed, or should be stored in the INetworkConnection implementation. Some devices may not have an IP.
    NetworkStatus  _networkStatus = NetworkStatus::DISCONNECTED;
    esp_ip4_addr_t _networkIp;
};
