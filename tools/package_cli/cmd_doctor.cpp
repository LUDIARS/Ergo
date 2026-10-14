// ergo_pkg doctor — 宣言の検査 (PKG 規則)、 ブートストラップの古さ、 漏れ検査 (§8)。 何も書き換えない。
// error が 1 件でもあれば終了 1。 --strict は構成時の STRICT と同じく warning も error に数える。

#include "commands.h"
#include "file_io.h"
#include "host_context.h"

#include "ergo/package_manager/bootstrap.h"
#include "ergo/package_manager/declaration_check.h"
#include "ergo/package_manager/init_plan.h"
#include "ergo/package_manager/leak_check.h"

#include <cstdio>

namespace ergo::package_cli {

namespace {

pm::BootstrapStatus check_bootstrap_file(const HostContext& ctx, std::vector<pm::Finding>& out) {
    const auto text = read_file(join(ctx.host_dir, pm::kBootstrapPath));
    if (text) {
        auto leaks = pm::leak_findings(pm::find_text_leaks(*text), pm::kBootstrapPath);
        out.insert(out.end(), leaks.begin(), leaks.end());
    }
    if (!ctx.bootstrap_template) {
        out.push_back({"BOOTSTRAP", pm::Severity::Record, pm::kBootstrapPath,
                       "freshness not checked: Ergo checkout not found (pass --ergo <dir>)"});
        return {};
    }
    const auto status = pm::check_bootstrap(text ? &*text : nullptr, *ctx.bootstrap_template);
    switch (status.state) {
    case pm::BootstrapState::Missing:
        out.push_back({"BOOTSTRAP", pm::Severity::Warning, pm::kBootstrapPath,
                       "missing: run `ergo_pkg init` to write it"});
        break;
    case pm::BootstrapState::Foreign:
        out.push_back({"BOOTSTRAP", pm::Severity::Warning, pm::kBootstrapPath,
                       "no ergo-bootstrap-source line: not written by ergo_pkg init"});
        break;
    case pm::BootstrapState::Stale:
        out.push_back({"BOOTSTRAP", pm::Severity::Error, pm::kBootstrapPath,
                       "stale (written from Ergo " + status.source +
                           "): run `ergo_pkg init` to refresh it"});
        break;
    case pm::BootstrapState::Current:
        break;
    }
    return status;
}

void check_json_leaks(const std::string& host, const char* rel, std::vector<pm::Finding>& out) {
    jsonm::JsonValue doc;
    if (!read_json(join(host, rel), doc, nullptr)) return;
    auto leaks = pm::leak_findings(pm::find_leaks(doc), rel);
    out.insert(out.end(), leaks.begin(), leaks.end());
}

void print_json(const std::vector<pm::Finding>& findings, const pm::BootstrapStatus& bootstrap) {
    auto doc = jsonm::JsonValue::make_object();
    auto b   = jsonm::JsonValue::make_object();
    b.set("state", jsonm::JsonValue::make_string(pm::bootstrap_state_name(bootstrap.state)));
    b.set("source", jsonm::JsonValue::make_string(bootstrap.source));
    doc.set("bootstrap", std::move(b));
    auto arr = jsonm::JsonValue::make_array();
    for (const auto& f : findings) {
        auto o = jsonm::JsonValue::make_object();
        o.set("id", jsonm::JsonValue::make_string(f.id));
        o.set("severity", jsonm::JsonValue::make_string(pm::severity_name(f.severity)));
        o.set("subject", jsonm::JsonValue::make_string(f.subject));
        o.set("message", jsonm::JsonValue::make_string(f.message));
        arr.push(std::move(o));
    }
    doc.set("findings", std::move(arr));
    std::printf("%s\n", jsonm::serialize(doc).c_str());
}

} // namespace

int run_doctor(const Args& args) {
    HostContext ctx;
    std::string error;
    if (!load_host_context(args, ctx, &error)) {
        std::fprintf(stderr, "ergo_pkg doctor: %s\n", error.c_str());
        return kExitInput;
    }
    const bool upm_host = read_file(join(args.host, pm::kUpmHostPath)).has_value() && !ctx.declaration_text;
    if (!ctx.has_declaration && !upm_host) {
        std::fprintf(stderr, "ergo_pkg doctor: no host declaration in %s (run `ergo_pkg init`)\n",
                     args.host.c_str());
        return kExitInput;
    }

    std::vector<pm::Finding> findings = ctx.read_findings;
    pm::BootstrapStatus bootstrap;
    if (ctx.has_declaration) {
        auto checked = pm::check_declaration(ctx.declaration,
                                             ctx.has_ergo_manifest ? &ctx.ergo_manifest : nullptr);
        findings.insert(findings.end(), checked.begin(), checked.end());
        check_json_leaks(args.host, pm::kHostDeclarationPath, findings);
        bootstrap = check_bootstrap_file(ctx, findings);
    }
    check_json_leaks(args.host, pm::kUpmHostPath, findings);
    if (args.strict) pm::promote_warnings(findings);
    pm::sort_findings(findings);

    if (args.json) {
        print_json(findings, bootstrap);
    } else {
        for (const auto& n : ctx.notes) std::fprintf(stderr, "ergo_pkg doctor: %s\n", n.c_str());
        for (const auto& f : findings) std::printf("%s\n", pm::describe(f).c_str());
        std::printf("ergo_pkg doctor: %zu finding(s), bootstrap %s\n", findings.size(),
                    pm::bootstrap_state_name(bootstrap.state));
    }
    return pm::has_severity(findings, pm::Severity::Error) ? kExitFindings : kExitOk;
}

} // namespace ergo::package_cli
