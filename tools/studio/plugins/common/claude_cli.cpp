#include "claude_cli.h"

#include "json_util.h"

namespace ergo::studio::plugins {

bool is_safe_model_name(const std::string& m) {
    if (m.empty() || m.size() > 100) return false;
    for (char c : m) {
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' ||
                        c == '_' || c == '-' || c == ':';
        if (!ok) return false;
    }
    return true;
}

ClaudeCli::ClaudeCli(std::shared_ptr<platform::IProcessRunner> runner, std::string bin, std::string model, int timeout_ms)
    : runner_(std::move(runner)), bin_(std::move(bin)), model_(std::move(model)), timeout_ms_(timeout_ms) {}

std::string ClaudeCli::not_ready_reason() const {
    if (!is_safe_model_name(model_)) return "invalid model name: " + model_;
    if (bin_.empty()) return "claude CLI program is empty";
    return {};
}

ClaudeCli::Reply ClaudeCli::ask(const std::string& prompt) const {
    Reply r;
    if (const std::string why = not_ready_reason(); !why.empty()) {
        r.error = why;
        return r;
    }
    platform::ProcessSpec spec;
    spec.program    = bin_;
    spec.args       = {"-p", "--model", model_, "--output-format", "json"};
    spec.stdin_text = prompt;
    spec.timeout_ms = timeout_ms_;
    const auto res = runner_->run(spec);
    if (!res.started) {
        r.error = "claude spawn failed: " + res.error;
        return r;
    }
    if (res.timed_out) {
        r.error = "claude " + res.error;
        return r;
    }
    if (res.exit_code != 0) {
        r.error = "claude exited " + std::to_string(res.exit_code) + ": " + res.err.substr(0, 300);
        return r;
    }
    JsonValue env;
    const JsonValue* result = nullptr;
    if (ergo::common::jsonm::parse(res.out, env)) result = field(env, "result");
    r.ok   = true;
    r.text = result && result->is_string() ? result->s : res.out;
    return r;
}

} // namespace ergo::studio::plugins
