

enum class TextSource {
    NONE=0,
    STATIC,
    SHORT_NAME,
    LONG_NAME,
    VARIABLE_FIELD,
    TIME,
    COUNTDOWN,
};

struct TextSlotConfig {
    TextSource source;
    struct AlertSourceConfig {
        uint8_t alertIndex; // For alert sources, which alert to respond to.
        DeviceAlertTarget alertTarget;

    } alert;
    char staticText[32]; // For STATIC source
    uint8_t fieldIndex; // For VARIABLE_FIELD source

}