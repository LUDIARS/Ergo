// Teacher-owned C++ source. Build only the lesson_rules target, then press F5 in the game.
// Never load code from students/uploads: this DLL executes with the game's OS permissions.
#include <algorithm>

extern "C" __declspec(dllexport) unsigned lesson_rule_version() { return 1; }
extern "C" __declspec(dllexport) float lesson_rule_evaluate(unsigned kind, float power, float baseline) {
    // Exercise: compare a linear stop with this bounded rule. Edit the actual expression,
    // rebuild the DLL, reload with F5; F6 restores the previous working snapshot.
    if (kind == 0) return std::min(power / 24.0f, 0.12f);
    return baseline;
}
