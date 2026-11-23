#pragma once
/// Keeps every screen of the Studio on the one declaration: native runtimes
/// (the Tela window) and the browser pages. pump() runs on the UI thread,
/// once per frame of the window or every few ms when the Studio has no window.
#include "ergo/studio/tela/browser_screens.h"
#include "ergo/studio/tela/studio_declaration.h"

#include <tela/runtime.hpp>

#include <vector>

namespace ergo::studio::tela_shell {

class ScreenHub {
public:
    ScreenHub(StudioDeclaration& decl, BrowserScreens& browsers) : decl_(decl), browsers_(browsers) {}

    /// A runtime the caller drives (input, viewport, drawing); it gets the
    /// declaration and theme from pump(). Must outlive its remove().
    void add(::tela::Runtime& runtime, ScreenAccess access);
    void remove(::tela::Runtime& runtime);

    /// Refresh the declaration, hand it to the screens whose copy is older,
    /// then run the browser pages.
    void pump(float dt);

    StudioDeclaration& declaration() { return decl_; }

private:
    struct Native {
        ::tela::Runtime* runtime;
        ScreenAccess     access;
        uint64_t         declared;
    };
    StudioDeclaration&  decl_;
    BrowserScreens&     browsers_;
    std::vector<Native> natives_;
};

} // namespace ergo::studio::tela_shell
