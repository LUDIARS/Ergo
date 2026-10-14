#pragma once

// `ergo_pkg add / remove / update` の書き換えを決める (純粋関数。 ファイルの読み書きと git は CLI)。
// どれも json_edit で該当の値だけを差し替え、 キー順・改行・字下げは変えない (spec/feature/package-manager.md §2.2)。
//
//   add     host.use (と --optional / --features) に足す。 Ergo のマニフェストに無い名前は非公開 packages に
//           足し、 repository と rev が要る。 link の結線ファイルが無ければ誤り (結線は生成しない)
//   remove  host.use / optional / features と非公開 packages から外す。 Ergo のモジュールが needs に
//           持つ名前なら拒む (modules.*.needs を壊す)
//   pin     持ち主のファイルの rev と expect_tag だけを書く (expect_tag が空なら外す)
//
// 書き換えの後に宣言を検査し直し、 新しく error が出る書き換え (機能指定の書式・PKG-02 など) は拒む。

#include "ergo/package_manager/ergo_manifest.h"
#include "ergo/package_manager/json_edit.h"

#include <string>
#include <vector>

namespace ergo::package_manager {

struct EditResult {
    bool                     ok = true;
    std::string              error;    ///< ok == false の理由 (引数・入力の誤り、 終了 2)
    std::string              text;     ///< 書き換え後 (ok のとき)
    bool                     changed = false;
    std::vector<std::string> notes;    ///< 人向けの補足
};

struct AddRequest {
    std::string name;
    bool        optional = false;
    std::string features;    ///< --features (空なら書かない)
    // 非公開パッケージだけ (Ergo のマニフェストにある名前に渡すと誤り)
    std::string kind;        ///< "link" (既定) / "tool"
    std::string code;
    std::string repository;
    std::string rev;         ///< 解決済みの 40 桁 SHA
    std::string expect_tag;
    std::string wiring;      ///< cmake.wiring (ホストのリポからの相対)。 空なら cmake/packages/<name>.cmake
    bool        wiring_exists = false;  ///< 結線ファイルがホストにあるか (CLI が確かめる)
};

/// 非公開パッケージの既定の結線ファイル。
std::string default_wiring_path(const std::string& name);

/// ergo は無ければ nullptr (そのとき共有かどうかを決められない名前は誤り)。
EditResult plan_add(const std::string& host_text, const ErgoManifest* ergo, const AddRequest& request);

EditResult plan_remove(const std::string& host_text, const ErgoManifest* ergo, const std::string& name);

/// pin_object (持ち主のファイルの pin のオブジェクト) の rev と expect_tag を書く。
EditResult plan_pin(const std::string& owner_text, const JsonPath& pin_object, const std::string& rev,
                    const std::string& expect_tag);

} // namespace ergo::package_manager
