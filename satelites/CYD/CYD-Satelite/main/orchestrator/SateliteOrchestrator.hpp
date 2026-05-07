#pragma once

#include "orchestrator/IOrchestrator.hpp"

class SateliteOrchestrator : public IOrchestrator {
public:
    SateliteOrchestrator(ISettingsStore&      store,
                         const DeviceProfile& profile,
                         INetworkConnection&  network,
                         IBeaconConnection&   beacon,
                         IConsumer**          consumers,
                         uint8_t              consumerCount,
                         EspHttpServer&       http)
        : IOrchestrator(store, profile, network, beacon, consumers, consumerCount, http)
    {}

    void start() override;
    void stop()  override;

private:
    static constexpr char TAG[] = "SateliteOrch";

    void onNetworkChanged(const Settings::Network& s);
    void onBeaconChanged (const Settings::Beacon&  s);
    void onDisplayChanged(const Settings::Display& s);

    void onNetworkStatus(NetworkStatus status, esp_ip4_addr_t ip);

    void applyTally(TallyState state);
    void applyAlert(DeviceAlertAction action, DeviceAlertTarget target, uint32_t timeout);
    void applyDisplay(const Settings::Display& s);

    void registerHttpHandlers();
};
