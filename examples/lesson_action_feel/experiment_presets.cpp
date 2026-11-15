#include "experiment_presets.h"
#include <utility>

namespace lesson_action_feel {
ExperimentSetup experiment_setup(Experiment selected, const Options& options, RuleFunction rule) {
    ExperimentSetup setup;
    setup.feel = initial_tuning(options);
    switch (selected) {
    case Experiment::Baseline: break;
    case Experiment::LongStop:
        setup.feel.hit_stop_per_power = 1.0f / 6.0f;
        setup.label = L"TRAP: long hit stop / input feels stalled"; break;
    case Experiment::ExcessiveKnockback:
        setup.feel.knockback_per_power = 24.0f;
        setup.label = L"TRAP: excessive knockback / chase after every hit"; break;
    case Experiment::ExcessiveShake:
        setup.presentation.shake_per_power = .60f;
        setup.presentation.shake_max = 4.0f;
        setup.presentation.shake_decay = 1.5f;
        setup.label = L"TRAP: excessive shake / targets hard to follow"; break;
    case Experiment::Custom:
        setup.feel.rule = std::move(rule);
        setup.label = L"LIVE C++ RULE / F6 rollback"; break;
    }
    return setup;
}
} // namespace lesson_action_feel
