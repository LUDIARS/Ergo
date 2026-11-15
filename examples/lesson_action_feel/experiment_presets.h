#pragma once
#include "options.h"
#include "presentation_tuning.h"

namespace lesson_action_feel {
enum class Experiment { Baseline, LongStop, ExcessiveKnockback, ExcessiveShake, Custom };
struct ExperimentSetup {
    FeelTuning feel;
    PresentationTuning presentation;
    const wchar_t* label = L"BASELINE";
};
ExperimentSetup experiment_setup(Experiment selected, const Options& options, RuleFunction rule = {});
} // namespace lesson_action_feel
