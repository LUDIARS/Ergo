#pragma once

// ergo_pkg の子プロセス起動の土台。 argv を 1 個ずつ分けたまま OS に渡し、 シェルを挟まない。
// Windows は CreateProcessW、 POSIX は posix_spawnp。 標準入出力のつなぎ方だけを選ぶ。

#include <string>
#include <vector>

namespace ergo::package_cli {

/// 子の標準出力・標準エラーのつなぎ先。
enum class StreamMode {
    inherit,   ///< 親の端末へそのまま流す
    discard,   ///< null デバイスへ捨てる
    capture,   ///< パイプで受け取る (標準出力だけに使える)
};

/// argv[0] を PATH から探して起動し、 終わるまで待って終了コードを返す。
/// 起動できない・終了コードが取れない (シグナルで終わった等) なら -1。
/// out_mode が capture のとき、 標準出力を captured に入れる (Windows では CRLF を LF にそろえる)。
/// err_mode に capture は使えない (-1 を返す)。標準入力は親から継承する。
int launch(const std::vector<std::string>& argv, StreamMode out_mode, StreamMode err_mode, std::string* captured);

} // namespace ergo::package_cli
