#include "ergo/studio/tela/glfw_tela_input.h"

#include "ergo/studio/tela/utf8_append.h"

#include <GLFW/glfw3.h>

#include <cmath>
#include <string>
#include <utility>

namespace ergo::studio::tela_shell {
namespace {

GlfwTelaInput* g_attached = nullptr;

std::uint32_t modifier_bits(int mods) {
    std::uint32_t bits = 0;
    if (mods & GLFW_MOD_SHIFT) bits |= ::tela::modifier_shift;
    if (mods & GLFW_MOD_CONTROL) bits |= ::tela::modifier_control;
    if (mods & GLFW_MOD_ALT) bits |= ::tela::modifier_alt;
    if (mods & GLFW_MOD_SUPER) bits |= ::tela::modifier_meta;
    return bits;
}

::tela::Key translate(int key) {
    switch (key) {
        case GLFW_KEY_TAB: return ::tela::Key::tab;
        case GLFW_KEY_ENTER:
        case GLFW_KEY_KP_ENTER: return ::tela::Key::enter;
        case GLFW_KEY_ESCAPE: return ::tela::Key::escape;
        case GLFW_KEY_BACKSPACE: return ::tela::Key::backspace;
        case GLFW_KEY_DELETE: return ::tela::Key::delete_forward;
        case GLFW_KEY_LEFT: return ::tela::Key::left;
        case GLFW_KEY_RIGHT: return ::tela::Key::right;
        case GLFW_KEY_UP: return ::tela::Key::up;
        case GLFW_KEY_DOWN: return ::tela::Key::down;
        case GLFW_KEY_HOME: return ::tela::Key::home;
        case GLFW_KEY_END: return ::tela::Key::end;
        case GLFW_KEY_PAGE_UP: return ::tela::Key::page_up;
        case GLFW_KEY_PAGE_DOWN: return ::tela::Key::page_down;
        case GLFW_KEY_A: return ::tela::Key::a;
        default: return ::tela::Key::none;
    }
}

} // namespace

GlfwTelaInput::GlfwTelaInput(GLFWwindow* window, ::tela::Runtime& runtime) : window_(window), runtime_(runtime) {
    g_attached = this;
    glfwSetCursorPosCallback(window, on_cursor);
    glfwSetMouseButtonCallback(window, on_button);
    glfwSetScrollCallback(window, on_scroll);
    glfwSetKeyCallback(window, on_key);
    glfwSetCharCallback(window, on_char);
}

GlfwTelaInput::~GlfwTelaInput() {
    glfwSetCursorPosCallback(window_, nullptr);
    glfwSetMouseButtonCallback(window_, nullptr);
    glfwSetScrollCallback(window_, nullptr);
    glfwSetKeyCallback(window_, nullptr);
    glfwSetCharCallback(window_, nullptr);
    if (g_attached == this) g_attached = nullptr;
}

bool GlfwTelaInput::synchronize() {
    int fb_w = 0, fb_h = 0, win_w = 0, win_h = 0;
    float scale_x = 1, scale_y = 1;
    glfwGetFramebufferSize(window_, &fb_w, &fb_h);
    glfwGetWindowSize(window_, &win_w, &win_h);
    glfwGetWindowContentScale(window_, &scale_x, &scale_y);
    ratio_x_ = win_w ? static_cast<float>(fb_w) / static_cast<float>(win_w) : 1.0f;
    ratio_y_ = win_h ? static_cast<float>(fb_h) / static_cast<float>(win_h) : 1.0f;
    ::tela::Viewport next = runtime_.viewport();
    next.host_id   = "ergo-studio";
    next.view_id   = "window";
    next.width     = fb_w;
    next.height    = fb_h;
    next.dpi_scale = scale_x > 0 ? scale_x : 1.0f;
    next.visible   = fb_w > 0 && fb_h > 0 && !glfwGetWindowAttrib(window_, GLFW_ICONIFIED);
    next.focused   = glfwGetWindowAttrib(window_, GLFW_FOCUSED) != 0;
    const auto& now = runtime_.viewport();
    if (next.host_id == now.host_id && next.view_id == now.view_id && next.width == now.width &&
        next.height == now.height && next.dpi_scale == now.dpi_scale && next.visible == now.visible &&
        next.focused == now.focused) {
        return false;
    }
    ++next.revision;
    runtime_.viewport(std::move(next));
    return true;
}

void GlfwTelaInput::pointer(::tela::PointerPhase phase, ::tela::PointerButton button, float wheel_x, float wheel_y) {
    ::tela::HostPointerEvent e;
    e.sequence          = ++pointer_sequence_;
    e.viewport_revision = runtime_.viewport().revision;
    e.gesture_id        = gesture_;
    e.phase             = phase;
    e.button            = button;
    // The viewport starts at desktop (0, 0): framebuffer pixels are the desktop pixels.
    e.desktop_x = static_cast<int>(std::lround(x_ * ratio_x_));
    e.desktop_y = static_cast<int>(std::lround(y_ * ratio_y_));
    e.wheel_x   = wheel_x;
    e.wheel_y   = wheel_y;
    e.modifiers = modifiers_;
    runtime_.pointer(e, ::tela::InputSource::native);
}

void GlfwTelaInput::on_cursor(GLFWwindow*, double x, double y) {
    if (!g_attached) return;
    g_attached->x_ = x;
    g_attached->y_ = y;
    g_attached->pointer(::tela::PointerPhase::move, ::tela::PointerButton::none);
}

void GlfwTelaInput::on_button(GLFWwindow*, int button, int action, int mods) {
    if (!g_attached || button != GLFW_MOUSE_BUTTON_LEFT) return;
    g_attached->modifiers_ = modifier_bits(mods);
    if (action == GLFW_PRESS) ++g_attached->gesture_;
    g_attached->pointer(action == GLFW_PRESS ? ::tela::PointerPhase::down : ::tela::PointerPhase::up,
                        ::tela::PointerButton::primary);
}

void GlfwTelaInput::on_scroll(GLFWwindow*, double x, double y) {
    // GLFW's positive y scrolls away from the user, as the contract's wheel_y does.
    if (g_attached) {
        g_attached->pointer(::tela::PointerPhase::wheel, ::tela::PointerButton::none, static_cast<float>(x),
                            static_cast<float>(y));
    }
}

void GlfwTelaInput::on_key(GLFWwindow* w, int key, int, int action, int mods) {
    if (!g_attached) return;
    g_attached->modifiers_ = modifier_bits(mods);
    if (action == GLFW_RELEASE) return;
    // Paste: the clipboard arrives as committed text (the runtime has no clipboard of its own).
    if (key == GLFW_KEY_V && (mods & GLFW_MOD_CONTROL) && action == GLFW_PRESS) {
        if (const char* clip = glfwGetClipboardString(w)) {
            g_attached->runtime_.text({++g_attached->key_sequence_, clip});
        }
        return;
    }
    const auto k = translate(key);
    if (k == ::tela::Key::none) return;
    g_attached->runtime_.key({++g_attached->key_sequence_, k, true, g_attached->modifiers_});
}

void GlfwTelaInput::on_char(GLFWwindow*, unsigned int cp) {
    // Committed characters only (GLFW delivers an IME's result here, not its composition).
    if (!g_attached || cp < 0x20 || cp == 0x7F || (cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10FFFF) return;
    std::string s;
    append_utf8(s, cp);
    g_attached->runtime_.text({++g_attached->key_sequence_, std::move(s)});
}

} // namespace ergo::studio::tela_shell
