#include "ergo/package_manager/finding.h"

#include <algorithm>

namespace ergo::package_manager {

const char* severity_name(Severity s) {
    switch (s) {
    case Severity::Error:   return "error";
    case Severity::Warning: return "warning";
    case Severity::Record:  return "record";
    }
    return "error";
}

std::string describe(const Finding& f) {
    return f.id + "|" + severity_name(f.severity) + "|" + f.subject + "|" + f.message;
}

bool has_severity(const std::vector<Finding>& findings, Severity s) {
    return std::any_of(findings.begin(), findings.end(),
                       [s](const Finding& f) { return f.severity == s; });
}

void promote_warnings(std::vector<Finding>& findings) {
    for (auto& f : findings) {
        if (f.severity == Severity::Warning) f.severity = Severity::Error;
    }
}

void sort_findings(std::vector<Finding>& findings) {
    std::sort(findings.begin(), findings.end(),
              [](const Finding& a, const Finding& b) { return describe(a) < describe(b); });
    findings.erase(std::unique(findings.begin(), findings.end(),
                               [](const Finding& a, const Finding& b) {
                                   return describe(a) == describe(b);
                               }),
                   findings.end());
}

} // namespace ergo::package_manager
