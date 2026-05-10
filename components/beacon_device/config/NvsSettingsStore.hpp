#pragma once

#include "config/ISettingsStore.hpp"

class NvsSettingsStore : public ISettingsStore {
public:
    bool load(Settings& out) override;
    bool save(const Settings& in) override;

private:
    static constexpr char NS[] = "satelite_cfg";
};
