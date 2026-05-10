
#pragma once


enum class TallyState {
    NONE = 0,
    DANGER = 4, // Light redish
    INFO = 8, // BLUE
    WARNING = 12, // Yellow
    PREVIEW = 16,
    PROGRAM = 20
};

enum class DeviceAlertAction {
    CLEAR = 0,
    IDENT = 2,
    INFO = 4,
    NORMAL = 6,
    PRIO = 8,
};

enum class DeviceAlertTarget {
    ALL = 0,
    OPERATOR = 1,
    TALENT = 2,
};

