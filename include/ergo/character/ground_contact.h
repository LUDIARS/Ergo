#pragma once

/// ergo::character — 接地情報の読み口。
///
/// TCC の `IGroundContact` (GroundCheck が実装) に対応する。実際の
/// 接地判定 (レイキャスト等) はホスト固有なので、本モジュールは
/// インターフェースと手動実装 (checks/manual_ground_check.h) のみ持つ。
/// Gravity 等の Effect が DI で参照する。
///
/// Spec: spec/module/character.md

namespace ergo::character {

class IGroundContact {
public:
    virtual ~IGroundContact() = default;

    /// 接地しているか (このフレームのセンサー結果)。
    virtual bool is_on_ground() const = 0;
};

} // namespace ergo::character
