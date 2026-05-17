#pragma once

#include "config/Config.hpp"
#include "config/ISettingsStore.hpp"
#include "config/DeviceProfile.hpp"
#include "networkConnection/INetworkConnection.hpp"
#include "beaconConnection/IBeaconConnection.hpp"
#include "consumer/ConsumerGroup.hpp"
#include "httpServer/EspHttpServer.hpp"
#include "httpServer/HttpHandlers.hpp"
#include <cstring>

class IOrchestrator {
public:
    static constexpr uint8_t MAX_GROUPS = 8;

    IOrchestrator(  ISettingsStore&      store,
                    const DeviceProfile& profile,
                    INetworkConnection&  network,
                    IBeaconConnection&   beacon,
                    ConsumerGroup**      groups,
                    uint8_t              groupCount,
                    EspHttpServer&       http
                ) :
          _config(store)
        , _profile(profile)
        , _network(network)
        , _beacon(beacon)
        , _groupCount(groupCount < MAX_GROUPS ? groupCount : MAX_GROUPS)
        , _http(http)
        , _httpCtx{_config, _profile, _network, _beacon}
    {
        memset(_groups, 0, sizeof(_groups));
        memcpy(_groups, groups, _groupCount * sizeof(ConsumerGroup*));
    }

    virtual ~IOrchestrator() = default;

    void start();
    virtual void stop() = 0;

protected:
    static constexpr uint32_t STARTUP_STACK_SIZE = 8192;
    virtual void doStart() = 0;

    Config               _config;
    const DeviceProfile& _profile;
    INetworkConnection&  _network;
    IBeaconConnection&   _beacon;
    ConsumerGroup*       _groups[MAX_GROUPS];
    uint8_t              _groupCount;
    EspHttpServer&       _http;

    // TODO Move to UI class.
    HttpCtx              _httpCtx;

    // TODO Check if needed, or should be stored in the INetworkConnection implementation.
    NetworkStatus  _networkStatus = NetworkStatus::DISCONNECTED;
    esp_ip4_addr_t _networkIp;
};
