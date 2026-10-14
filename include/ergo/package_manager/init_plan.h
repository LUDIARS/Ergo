#pragma once

// `ergo_pkg init` が何を書くかを決める (純粋関数。 ファイルの読み書きは CLI が行う)。
//
//   通常:   ergo-packages.json (ホストの宣言) / cmake/ergo-bootstrap.cmake / .gitignore に ".ergo/"
//   --upm:  .ergo/host.json だけ (Unity プロジェクト。 追跡するファイル)
//
// 既にあるものは書き換えない (2 回目は無変更)。 例外は生成元の印つきで古くなったブートストラップで、
// これは作り直す。 既存の ergo-packages.json がホストの宣言でなければ誤り (Ergo のマニフェストを
// ホストと取り違えている)。

#include <optional>
#include <string>
#include <vector>

namespace ergo::package_manager {

extern const char* const kHostDeclarationPath;  ///< "ergo-packages.json"
extern const char* const kGitignorePath;        ///< ".gitignore"
extern const char* const kUpmHostPath;          ///< ".ergo/host.json"
extern const char* const kErgoRepository;       ///< Ergo の公開 URL

struct InitOptions {
    std::string code;                ///< --code (空なら書かない)
    bool        upm = false;         ///< --upm
    std::string ergo_rev;            ///< 生成元 Ergo の 40 桁 SHA
    std::string bootstrap_template;  ///< cmake/packages/ergo-bootstrap.cmake.in の中身
};

/// ホストに今あるファイル (無ければ nullopt)。
struct ExistingFiles {
    std::optional<std::string> declaration;
    std::optional<std::string> bootstrap;
    std::optional<std::string> gitignore;
    std::optional<std::string> upm_host;
};

struct FileWrite {
    std::string path;     ///< ホストのリポからの相対 ('/' 区切り)
    std::string content;
};

struct InitPlan {
    bool                     ok = true;
    std::string              error;   ///< ok == false の理由 (引数・入力の誤り)
    std::vector<FileWrite>   writes;  ///< 空なら無変更
    std::vector<std::string> notes;   ///< 人向けの補足
};

/// プロジェクトコードとして書ける形か (英字で始まり英数字・'-'・'_')。
bool is_valid_code(const std::string& code);

std::string render_host_declaration(const std::string& code, const std::string& ergo_rev);
std::string render_upm_host(const std::string& code);

/// .gitignore に ".ergo/" (または同じ意味の行) が無ければ足した中身を out に入れて true。
bool add_gitignore_entry(const std::string& in, std::string& out);

InitPlan plan_init(const InitOptions& options, const ExistingFiles& existing);

} // namespace ergo::package_manager
