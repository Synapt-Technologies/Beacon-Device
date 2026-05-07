#pragma once

#include "config/Settings.hpp"

class ISettingsStore {
public:
    virtual ~ISettingsStore() = default;

    virtual bool load(Settings& out) = 0;
    virtual bool save(const Settings& in) = 0;
};
