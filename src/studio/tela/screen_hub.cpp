#include "ergo/studio/tela/screen_hub.h"

#include <algorithm>

namespace ergo::studio::tela_shell {

void ScreenHub::add(::tela::Runtime& runtime, ScreenAccess access) {
    runtime.theme(decl_.theme());
    natives_.push_back({&runtime, access, 0});
}

void ScreenHub::remove(::tela::Runtime& runtime) {
    natives_.erase(std::remove_if(natives_.begin(), natives_.end(), [&](const Native& n) { return n.runtime == &runtime; }),
                   natives_.end());
}

void ScreenHub::pump(float dt) {
    decl_.refresh(dt);
    for (auto& n : natives_) {
        if (n.declared == decl_.revision()) continue;
        n.runtime->document(decl_.document(n.access));
        n.declared = decl_.revision();
    }
    browsers_.pump(decl_);
}

} // namespace ergo::studio::tela_shell
