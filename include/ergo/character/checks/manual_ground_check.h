#pragma once

/// ergo::character — ManualGroundCheck: ホストが接地状態を毎フレーム
/// 供給する最小の Check 実装。
///
/// レイキャスト等の実判定はホスト固有 (Pictor / 物理エンジン / タイル
/// マップ…) なので、本モジュールは「外から set する」実装のみ持つ。
/// ホスト側で IGroundContact を直接実装してもよい。
///
/// Spec: spec/module/character.md

#include "ergo/character/ground_contact.h"

namespace ergo::character {

class ManualGroundCheck : public IGroundContact {
public:
    void set_on_ground(bool grounded) { grounded_ = grounded; }
    bool is_on_ground() const override { return grounded_; }

private:
    bool grounded_ = true;
};

} // namespace ergo::character
