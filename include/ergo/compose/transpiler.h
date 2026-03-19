#pragma once

#include "ergo/compose/types.h"
#include <string>
#include <vector>
#include <optional>

namespace ergo::compose {

/// TypeScript→C++トランスパイラ
/// 型定義ファイル(.d.ts)に基づき、TSコードをC++に変換する
class Transpiler {
public:
    Transpiler();
    ~Transpiler();

    /// 型定義ディレクトリを設定
    void setTypeDefPath(const std::string& path);

    /// 型定義ファイルをロード
    bool loadTypeDefinitions(const std::string& defsPath);

    /// 単一TSファイルをC++に変換
    /// @param source TS ソース情報
    /// @return 変換結果（成功時はC++コードパス）
    BuildResult transpile(TypeScriptSource& source);

    /// 複数TSファイルを一括変換
    std::vector<BuildResult> transpileBatch(std::vector<TypeScriptSource>& sources);

    /// CRC/バスト数を計算しファイル差分をチェック
    /// @return true: 差分あり（再変換必要）
    bool checkDiff(FileDiffInfo& diffInfo) const;

    /// C++出力ファイルのヘッダにCRC/バスト数を書き込む
    static void writeDiffHeader(const std::string& cppPath, const FileDiffInfo& info);

    /// C++出力ファイルからCRC/バスト数を読み取る
    static std::optional<FileDiffInfo> readDiffHeader(const std::string& cppPath);

    /// CRC32を計算
    static uint32_t calculateCrc32(const std::string& filePath);

private:
    std::string typeDefPath_;
    std::vector<std::string> loadedDefs_;
};

} // namespace ergo::compose
