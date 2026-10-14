#include "labels.h"

#include "ergo/package_model/version_label.h"

#include <vector>

namespace ergo::package_viz::detail {

namespace {

using package_model::Package;
using package_model::PackageKind;

std::string join(const std::vector<std::string>& parts, const char* sep) {
    std::string out;
    for (const auto& p : parts) {
        if (p.empty()) continue;
        if (!out.empty()) out += sep;
        out += p;
    }
    return out;
}

std::string title_line(const Package& p) {
    switch (p.kind) {
    case PackageKind::Ergo: return "Ergo";
    case PackageKind::Host: return "host";
    default: break;
    }
    return p.code.empty() ? p.name : p.name + " (" + p.code + ")";
}

/// 型を短く (STATIC_LIBRARY → STATIC)。
std::string short_type(const std::string& type) {
    const std::string suffix = "_LIBRARY";
    if (type.size() > suffix.size() &&
        type.compare(type.size() - suffix.size(), suffix.size(), suffix) == 0) {
        return type.substr(0, type.size() - suffix.size());
    }
    return type;
}

} // namespace

std::string marks_line(const Package& p, bool undeclared) {
    std::vector<std::string> marks;
    if (!p.drift.empty() && p.drift != "none") marks.push_back("★drift:" + p.drift);
    if (p.dirty) marks.push_back("dirty");
    if (!p.rev_override.empty()) marks.push_back("上書き");
    if (undeclared) marks.push_back("宣言漏れ");
    if (p.kind == PackageKind::Observe) marks.push_back("未観測");
    if (p.kind == PackageKind::Tool && !p.initialized) marks.push_back("未初期化");
    return join(marks, " ");
}

std::string package_label(const Package& p, bool undeclared, std::size_t target_count,
                          const package_model::PackageModel& model) {
    std::vector<std::string> lines;
    lines.push_back(title_line(p));
    switch (p.kind) {
    case PackageKind::Link:
        lines.push_back(package_model::version_line(p.version));
        lines.push_back("SHA " + package_model::short_sha(p.head.empty() ? p.pin : p.head) +
                        (p.head.empty() ? " (pin)" : ""));
        lines.push_back("状態 " + p.state + (p.route.empty() ? "" : "/" + p.route));
        if (!p.features.empty()) lines.push_back("機能 " + join(p.features, ", "));
        break;
    case PackageKind::Tool:
        lines.push_back(package_model::version_line(p.version));
        lines.push_back("SHA " + package_model::short_sha(p.gitlink) + " (gitlink)");
        lines.push_back("ツール · " + p.path);
        break;
    case PackageKind::Ergo: {
        std::size_t enabled = 0;
        for (const auto& m : model.modules) enabled += m.enabled ? 1 : 0;
        lines.push_back("モード " + (model.mode.empty() ? std::string("?") : model.mode) +
                        (model.strict ? " · STRICT" : ""));
        lines.push_back("宣言モジュール " + std::to_string(enabled) + "/" +
                        std::to_string(model.modules.size()) + " 有効");
        if (model.has_codemodel) lines.push_back(std::to_string(target_count) + " ターゲット");
        break;
    }
    case PackageKind::Host:
        lines.push_back(std::to_string(target_count) + " ターゲット");
        break;
    case PackageKind::ThirdParty:
        lines.push_back("第三者 · " + std::to_string(target_count) + " ターゲット");
        break;
    case PackageKind::Observe:
        lines.push_back("観測 · " + p.group);
        break;
    }
    const std::string marks = marks_line(p, undeclared);
    if (!marks.empty()) lines.push_back(marks);
    return join(lines, " | ");
}

std::string target_label(const package_model::Target& t, bool undeclared) {
    std::vector<std::string> lines{t.name, short_type(t.type),
                                   t.layer.empty() ? t.package : t.package + " · " + t.layer};
    if (undeclared) lines.push_back("★宣言漏れ");
    return join(lines, " | ");
}

std::string collapsed_label(const Package& p, std::size_t target_count) {
    std::vector<std::string> lines{title_line(p),
                                   std::to_string(target_count) + " ターゲット (畳み込み)"};
    if (p.kind == PackageKind::Link) {
        lines.push_back(package_model::version_line(p.version));
        lines.push_back("SHA " + package_model::short_sha(p.head.empty() ? p.pin : p.head));
    }
    const std::string marks = marks_line(p, false);
    if (!marks.empty()) lines.push_back(marks);
    return join(lines, " | ");
}

} // namespace ergo::package_viz::detail
