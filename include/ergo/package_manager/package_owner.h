#pragma once

// pin の持ち主の判定 (spec/feature/package-manager.md §2.2 の update / add)。
//
//   ergo            → ホストの宣言の host.ergo
//   非公開パッケージ → ホストの宣言の packages.<name>
//   共有パッケージ   → Ergo のマニフェストの packages.<name> (ホストは Ergo の pin を上げて受け取る)
//
// 非公開と Ergo のマニフェストの同名 (PKG-15) は持ち主が決まらないので Conflict。

#include "ergo/package_manager/ergo_manifest.h"
#include "ergo/package_manager/host_declaration.h"
#include "ergo/package_manager/json_edit.h"

#include <string>

namespace ergo::package_manager {

enum class PackageOwner {
    HostErgo,     ///< host.ergo
    HostPrivate,  ///< ホストの packages.<name>
    ErgoShared,   ///< Ergo のマニフェストの packages.<name>
    Conflict,     ///< 非公開と Ergo のマニフェストの同名 (PKG-15)
    Unknown,      ///< どちらにも無い
};

/// ergo の名前 (host.ergo を指す)。
extern const char* const kErgoPackageName;

const char* owner_name(PackageOwner owner);

/// host / ergo は無ければ nullptr。
PackageOwner owner_of(const std::string& name, const HostDeclaration* host, const ErgoManifest* ergo);

/// 持ち主のファイルでの pin のあるオブジェクトの経路 ({"host","ergo"} / {"packages",<name>})。
JsonPath pin_path(PackageOwner owner, const std::string& name);

} // namespace ergo::package_manager
