#pragma once
/// Turns one GLFW window's mouse, wheel, keys and committed characters (IME
/// results included) into tela::Runtime input, and keeps the runtime's
/// viewport equal to the window (framebuffer pixels, content scale as DPI).
/// The window's user pointer belongs to Pictor's surface provider, so the
/// callbacks reach this object through one process-wide slot: one attached
/// window per process (the Studio has one).
#include <tela/runtime.hpp>

#include <cstdint>

struct GLFWwindow;

namespace ergo::studio::tela_shell {

class GlfwTelaInput {
public:
    GlfwTelaInput(GLFWwindow* window, ::tela::Runtime& runtime);
    ~GlfwTelaInput();
    GlfwTelaInput(const GlfwTelaInput&)            = delete;
    GlfwTelaInput& operator=(const GlfwTelaInput&) = delete;

    /// Read the window's size, scale, visibility and focus into the runtime.
    /// True when the viewport changed.
    bool synchronize();

private:
    static void on_cursor(GLFWwindow*, double x, double y);
    static void on_button(GLFWwindow*, int button, int action, int mods);
    static void on_scroll(GLFWwindow*, double x, double y);
    static void on_key(GLFWwindow*, int key, int scancode, int action, int mods);
    static void on_char(GLFWwindow*, unsigned int codepoint);
    void pointer(::tela::PointerPhase phase, ::tela::PointerButton button, float wheel_x = 0, float wheel_y = 0);

    GLFWwindow*      window_;
    ::tela::Runtime& runtime_;
    double           x_ = 0, y_ = 0;
    float            ratio_x_ = 1, ratio_y_ = 1;
    std::uint32_t    modifiers_ = 0;
    std::uint64_t    pointer_sequence_ = 0, gesture_ = 0, key_sequence_ = 0;
};

} // namespace ergo::studio::tela_shell
