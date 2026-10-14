#pragma once

// ergo_package_manager の検査結果 1 件。 構成時の規則 (cmake/packages/ErgoPackageRules.cmake) と
// 同じ形 "<id>|<severity>|<subject>|<message>" で書き出せる (spec/data/ergo-packages.md の findings)。
//
// id は PKG-xx (ハブ設計 §4)、 宣言の形の不備は MANIFEST、 漏れ検査は LEAK、 ブートストラップは BOOTSTRAP。

#include <string>
#include <vector>

namespace ergo::package_manager {

enum class Severity { Error, Warning, Record };

struct Finding {
    std::string id;
    Severity    severity = Severity::Error;
    std::string subject;
    std::string message;
};

const char* severity_name(Severity s);

/// "<id>|<severity>|<subject>|<message>"
std::string describe(const Finding& f);

bool has_severity(const std::vector<Finding>& findings, Severity s);

/// 構成時の STRICT と同じ: warning を error にする。
void promote_warnings(std::vector<Finding>& findings);

/// describe() の文字列順に並べ、 同じものを 1 つにする (出力を決定的にする)。
void sort_findings(std::vector<Finding>& findings);

} // namespace ergo::package_manager
