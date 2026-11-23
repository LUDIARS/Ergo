#include "play_target.h"

#include <cmath>
#include <set>

namespace ergo::studio::plugins::play {
namespace {

const std::string* text_field(const JsonValue& v, const char* key) {
    const JsonValue* f = field(v, key);
    return f != nullptr && f->is_string() ? &f->s : nullptr;
}

bool parse_command(const JsonValue& v, const std::string& where, int default_timeout, int max_timeout, PlayCommand& out,
                   std::string& err) {
    if (!v.is_object()) {
        err = where + " must be an object {cmd, args}";
        return false;
    }
    const std::string* cmd = text_field(v, "cmd");
    if (cmd == nullptr || cmd->empty()) {
        err = where + ".cmd must be a non-empty string";
        return false;
    }
    out.cmd = *cmd;
    if (const JsonValue* args = field(v, "args"); args != nullptr) {
        if (!args->is_array()) {
            err = where + ".args must be an array of strings";
            return false;
        }
        for (const auto& a : *args->a) {
            if (!a.is_string()) {
                err = where + ".args must be an array of strings";
                return false;
            }
            out.args.push_back(a.s);
        }
    }
    out.timeout_sec = default_timeout;
    if (const JsonValue* t = field(v, "timeout_sec"); t != nullptr) {
        if (!t->is_number() || t->n < 1 || t->n > max_timeout || std::floor(t->n) != t->n) {
            err = where + ".timeout_sec must be a whole number 1.." + std::to_string(max_timeout);
            return false;
        }
        out.timeout_sec = static_cast<int>(t->n);
    }
    return true;
}

bool parse_buttons(const JsonValue& v, const std::string& where, std::vector<PlayButton>& out, std::string& err) {
    if (!v.is_array() || v.a->size() > kMaxButtons) {
        err = where + ".buttons must be an array of at most " + std::to_string(kMaxButtons);
        return false;
    }
    for (const auto& b : *v.a) {
        const std::string* label = b.is_object() ? text_field(b, "label") : nullptr;
        const std::string* key   = b.is_object() ? text_field(b, "key") : nullptr;
        if (label == nullptr || key == nullptr || label->empty()) {
            err = where + ".buttons entries need \"label\" and \"key\" strings";
            return false;
        }
        if (!is_key_name(*key)) {
            err = where + ".buttons: unknown key \"" + *key + "\"";
            return false;
        }
        out.push_back({*label, *key});
    }
    return true;
}

bool parse_number(const JsonValue& v, const std::string& what, double lo, double hi, double& out, std::string& err) {
    if (!v.is_number() || v.n < lo || v.n > hi || std::floor(v.n) != v.n) {
        err = what + " must be a whole number " + std::to_string(static_cast<long long>(lo)) + ".." +
              std::to_string(static_cast<long long>(hi));
        return false;
    }
    out = v.n;
    return true;
}

bool parse_one(const JsonValue& v, std::size_t index, const std::string& profile, const std::filesystem::path& root,
               PlayTarget& t, std::string& err) {
    const std::string where = "play[" + std::to_string(index) + "]";
    if (!v.is_object()) {
        err = where + " must be an object";
        return false;
    }
    const std::string* name = text_field(v, "name");
    if (name == nullptr || !is_valid_target_name(*name)) {
        err = where + ".name must match [a-z0-9-]{1,40}";
        return false;
    }
    t.name    = *name;
    t.profile = profile;
    t.root    = root;
    const std::string* label = text_field(v, "label");
    t.label   = label != nullptr && !label->empty() ? *label : *name;

    const JsonValue* run = field(v, "run");
    if (run == nullptr) {
        err = where + ".run is required";
        return false;
    }
    if (!parse_command(*run, where + ".run", 0, kMaxBuildSec, t.run, err)) return false;
    if (const JsonValue* build = field(v, "build"); build != nullptr) {
        PlayCommand c;
        if (!parse_command(*build, where + ".build", 3600, kMaxBuildSec, c, err)) return false;
        t.build = c;
    }
    const std::string* window = text_field(v, "window");
    if (window == nullptr || window->empty() || window->size() > 256) {
        err = where + ".window must be the window title (1..256 characters)";
        return false;
    }
    t.window = *window;
    double n = 0;
    if (const JsonValue* fps = field(v, "fps"); fps != nullptr) {
        if (!parse_number(*fps, where + ".fps", kMinFps, kMaxFps, n, err)) return false;
        t.fps = static_cast<int>(n);
    }
    if (const JsonValue* bitrate = field(v, "bitrate"); bitrate != nullptr) {
        if (!parse_number(*bitrate, where + ".bitrate", static_cast<double>(kMinBitrate), static_cast<double>(kMaxBitrate), n, err))
            return false;
        t.bitrate = static_cast<long long>(n);
    }
    if (const JsonValue* buttons = field(v, "buttons"); buttons != nullptr) {
        if (!parse_buttons(*buttons, where, t.buttons, err)) return false;
    }
    return true;
}

} // namespace

bool is_valid_target_name(const std::string& name) {
    if (name.empty() || name.size() > 40) return false;
    for (const char c : name) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
    }
    return true;
}

bool is_key_name(const std::string& name) {
    if (name.empty() || name.size() > 16) return false;
    for (const char c : name) {
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) return false;
    }
    return true;
}

bool parse_play_targets(const JsonValue& list, const std::string& profile, const std::filesystem::path& root,
                        std::vector<PlayTarget>& out, std::string& err) {
    if (!list.is_array()) {
        err = "play must be an array";
        return false;
    }
    std::vector<PlayTarget> parsed;
    std::set<std::string>   names;
    for (std::size_t i = 0; i < list.a->size(); ++i) {
        PlayTarget t;
        if (!parse_one((*list.a)[i], i, profile, root, t, err)) return false;
        if (!names.insert(t.name).second) {
            err = "play: the name \"" + t.name + "\" appears twice";
            return false;
        }
        parsed.push_back(std::move(t));
    }
    out.insert(out.end(), std::make_move_iterator(parsed.begin()), std::make_move_iterator(parsed.end()));
    return true;
}

} // namespace ergo::studio::plugins::play
