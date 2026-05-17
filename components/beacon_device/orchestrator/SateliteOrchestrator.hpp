#pragma once

#include "orchestrator/IOrchestrator.hpp"

class SateliteOrchestrator : public IOrchestrator {
public:
    SateliteOrchestrator(ISettingsStore&      store,
                         const DeviceProfile& profile,
                         INetworkConnection&  network,
                         IBeaconConnection&   beacon,
                         ConsumerGroup**      groups,
                         uint8_t              groupCount,
                         EspHttpServer&       http)
        : IOrchestrator(store, profile, network, beacon, groups, groupCount, http)
    {}

    void stop() override;

protected:
    void doStart() override;

private:
    static constexpr char TAG[] = "SateliteOrch";

    BeaconStatus _beaconStatus = BeaconStatus::DISCONNECTED;

    void onNetworkChanged(const Settings::Network& s);
    void onBeaconChanged (const Settings::Beacon&  s);
    void onDisplayChanged(const Settings::Display& s);
    void onRuntimeChanged(const Settings::Runtime& s);

    void onNetworkStatus(NetworkStatus status, esp_ip4_addr_t ip);
    void onBeaconStatus(BeaconStatus status);

    void applyTally(TallyState state);
    void applyAlert(DeviceAlertType type, DeviceAlertAction action,
                    DeviceAlertTarget target, uint32_t timeout, const char* text);
    void applyDisplay(const Settings::Display& s);

    void registerHttpHandlers();
};
