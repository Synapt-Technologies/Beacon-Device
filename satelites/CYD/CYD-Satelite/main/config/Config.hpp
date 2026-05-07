#pragma once

#include "config/ISettingsStore.hpp"
#include <functional>

class Config {
public:
    using NetworkCb = std::function<void(const Settings::Network&)>;
    using BeaconCb  = std::function<void(const Settings::Beacon&)>;
    using DisplayCb = std::function<void(const Settings::Display&)>;

    explicit Config(ISettingsStore& store);

    bool            load(bool applyCb = true);
    const Settings& get() const;

    // Apply a full or partial settings update.
    // Compares each category against the current settings, fires callbacks only
    // for categories that changed, then persists.
    // Returns false if saving to the store failed.
    bool apply(const Settings& updated);

    void onNetworkChanged(NetworkCb cb);
    void onBeaconChanged (BeaconCb  cb);
    void onDisplayChanged(DisplayCb cb);

private:
    ISettingsStore& _store;
    Settings        _settings;

    NetworkCb _networkCb;
    BeaconCb  _beaconCb;
    DisplayCb _displayCb;
};
