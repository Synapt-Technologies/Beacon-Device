#pragma once
#include "interfaces.h"

class NvsConfig : public IConfig {
public:
    bool               load()                    override;
    bool               save()                    override;
    const DeviceConfig& get()              const  override;
    void               set(const DeviceConfig&)   override;

private:
    DeviceConfig             m_cfg;
    static constexpr const char* NS = "cyd_cfg";
};
