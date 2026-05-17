

enum class TextSourceType {
    NONE=0,
    STATIC,
    SHORT_NAME,
    LONG_NAME,
    VARIABLE_FIELD,
    TIME,
    // COUNTDOWN, // TODO
};

struct TextSource {
    TextSourceType type;
    char staticText[32]; // For STATIC source
    uint8_t fieldIndex; // For VARIABLE_FIELD source
};

struct AlertSourceConfig {
    uint8_t alertIndex; // For alert sources, which alert to respond to.
    DeviceAlertTarget alertTarget;
};

struct TextSlotConfig {
    TextSource sources[4]; // Source + fallbacks if empty

    std::optional<AlertSourceConfig> alert; // nullptr if disabled.
};
