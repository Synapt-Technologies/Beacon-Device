#include "config/Config.hpp"
#include <cstring>

Config::Config(ISettingsStore& store) : _store(store) {}

bool Config::load(bool applyCb)
{
    if (!_store.load(_settings)) {
        return false;
    }

    if (applyCb) {
        if (_networkCb) _networkCb(_settings.network);
        if (_beaconCb) _beaconCb(_settings.beacon);
        if (_displayCb) _displayCb(_settings.display);
    }

    return true;
}

const Settings& Config::get() const { return _settings; }

bool Config::apply(const Settings& updated)
{
    if (memcmp(&_settings.network, &updated.network, sizeof(Settings::Network)) != 0) {
        _settings.network = updated.network;
        if (_networkCb) _networkCb(_settings.network);
    }

    if (memcmp(&_settings.beacon, &updated.beacon, sizeof(Settings::Beacon)) != 0) {
        _settings.beacon = updated.beacon;
        if (_beaconCb) _beaconCb(_settings.beacon);
    }

    if (memcmp(&_settings.display, &updated.display, sizeof(Settings::Display)) != 0) {
        _settings.display = updated.display;
        if (_displayCb) _displayCb(_settings.display);
    }

    memcpy(_settings.deviceName, updated.deviceName, sizeof(_settings.deviceName));

    return _store.save(_settings);
}

void Config::onNetworkChanged(NetworkCb cb) { _networkCb = cb; }
void Config::onBeaconChanged (BeaconCb  cb) { _beaconCb  = cb; }
void Config::onDisplayChanged(DisplayCb cb) { _displayCb = cb; }
