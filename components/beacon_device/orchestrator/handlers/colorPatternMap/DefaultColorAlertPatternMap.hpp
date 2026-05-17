#pragma once

// TODO: Add fixed version with conf. pattern and version that uses the config store.
class DefaultColorAlertPatternMap : public IColorAlertPatternMap {
public:
    virtual ~DefaultColorAlertPatternMap() = default;

    const ColorAlertPattern* getPattern(DeviceAlertAction action) override {
        switch (action) {
            case DeviceAlertAction::IDENT:  return &PATTERNS[0];
            case DeviceAlertAction::INFO:   return &PATTERNS[1];
            case DeviceAlertAction::NORMAL: return &PATTERNS[2];
            case DeviceAlertAction::PRIO:   return &PATTERNS[3];
            default:                        return nullptr;
        }
    }
private:

    static const TallyState IDENT[][8] = {
        { TallyState::NONE,    TallyState::NONE,    TallyState::NONE,    TallyState::NONE    },
        { TallyState::PREVIEW, TallyState::PROGRAM, TallyState::PREVIEW, TallyState::PROGRAM },
        { TallyState::PROGRAM, TallyState::PREVIEW, TallyState::PROGRAM, TallyState::PREVIEW },
        { TallyState::PROGRAM, TallyState::NONE,    TallyState::PREVIEW, TallyState::NONE    },
        { TallyState::NONE,    TallyState::PROGRAM, TallyState::NONE,    TallyState::PREVIEW },
    };
    static const TallyState INFO[][8] = {
        { TallyState::NONE, TallyState::NONE, TallyState::NONE, TallyState::NONE },
        { TallyState::INFO, TallyState::NONE, TallyState::INFO, TallyState::NONE },
        { TallyState::INFO, TallyState::NONE, TallyState::INFO, TallyState::NONE },
    };
    static const TallyState NORMAL[][8] = {
        { TallyState::NONE,    TallyState::NONE,    TallyState::NONE,    TallyState::NONE },
        { TallyState::WARNING, TallyState::NONE,    TallyState::WARNING, TallyState::NONE },
        { TallyState::WARNING, TallyState::NONE,    TallyState::WARNING, TallyState::NONE },
    };
    static const TallyState PRIO[][8] = {
        { TallyState::NONE,    TallyState::NONE,    TallyState::NONE,    TallyState::NONE    },
        { TallyState::PROGRAM, TallyState::WARNING, TallyState::PROGRAM, TallyState::WARNING },
        { TallyState::WARNING, TallyState::PROGRAM, TallyState::WARNING, TallyState::PROGRAM },
        { TallyState::PROGRAM, TallyState::NONE,    TallyState::WARNING, TallyState::NONE    },
        { TallyState::NONE,    TallyState::PROGRAM, TallyState::NONE,    TallyState::WARNING },
    };

    static const ColorAlertPattern PATTERNS[] = { // Steptime, patterns, variantCount, patternLen
        { 400, IDENT,  5, 4 },
        { 300, INFO,   3, 4 },
        { 400, NORMAL, 3, 4 },
        { 150, PRIO,   5, 4 },
    };

}