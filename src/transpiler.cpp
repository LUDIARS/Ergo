#include "ergo/compose/transpiler.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

namespace ergo::compose {

Transpiler::Transpiler() = default;
Transpiler::~Transpiler() = default;

void Transpiler::setTypeDefPath(const std::string& path) {
    typeDefPath_ = path;
}

bool Transpiler::loadTypeDefinitions(const std::string& defsPath) {
    namespace fs = std::filesystem;

    if (!fs::exists(defsPath)) {
        return false;
    }

    loadedDefs_.clear();

    if (fs::is_directory(defsPath)) {
        for (const auto& entry : fs::directory_iterator(defsPath)) {
            if (entry.path().extension() == ".ts" ||
                entry.path().extension() == ".d.ts") {
                loadedDefs_.push_back(entry.path().string());
            }
        }
    } else {
        loadedDefs_.push_back(defsPath);
    }

    return !loadedDefs_.empty();
}

BuildResult Transpiler::transpile(TypeScriptSource& source) {
    BuildResult result;
    auto startTime = std::chrono::steady_clock::now();

    // ソースファイルの存在チェック
    if (!std::filesystem::exists(source.filePath)) {
        result.success = false;
        result.errorMessage = "Source file not found: " + source.filePath;
        source.status = TranspileStatus::Failed;
        return result;
    }

    source.status = TranspileStatus::Compiling;

    // CRC差分チェック — 変更がなければスキップ
    if (!source.diffInfo.compiledPath.empty() &&
        std::filesystem::exists(source.diffInfo.compiledPath)) {
        if (!checkDiff(source.diffInfo)) {
            source.status = TranspileStatus::Compiled;
            result.success = true;
            result.outputPath = source.diffInfo.compiledPath;
            return result;
        }
    }

    // TSソースを読み込み
    std::ifstream ifs(source.filePath);
    if (!ifs.is_open()) {
        result.success = false;
        result.errorMessage = "Cannot open source file: " + source.filePath;
        source.status = TranspileStatus::Failed;
        return result;
    }

    std::stringstream buffer;
    buffer << ifs.rdbuf();
    std::string tsCode = buffer.str();

    // --- TS→C++ 変換（スタブ実装） ---
    // 実際のトランスパイルロジックはArsのTS解析エンジンが担当
    // ここでは変換パイプラインのインタフェースを提供する
    std::string cppCode = "// Auto-generated from: " + source.filePath + "\n";
    cppCode += "// CRC32: " + std::to_string(calculateCrc32(source.filePath)) + "\n";
    cppCode += "// BustCount: " + std::to_string(source.diffInfo.bustCount + 1) + "\n";
    cppCode += "#pragma once\n\n";
    cppCode += "namespace ars::generated {\n\n";
    cppCode += "// TODO: Ars transpiler output\n";
    cppCode += "// Source: " + source.moduleName + "::" + source.actorName + "\n\n";
    cppCode += "} // namespace ars::generated\n";

    // 出力パスを決定
    namespace fs = std::filesystem;
    std::string outputDir = fs::path(source.filePath).parent_path().string();
    std::string baseName = fs::path(source.filePath).stem().string();
    std::string outputPath = outputDir + "/" + baseName + ".generated.h";

    // 出力ファイルに書き込み
    std::ofstream ofs(outputPath);
    if (!ofs.is_open()) {
        result.success = false;
        result.errorMessage = "Cannot write output file: " + outputPath;
        source.status = TranspileStatus::Failed;
        return result;
    }
    ofs << cppCode;
    ofs.close();

    // 差分情報を更新
    source.diffInfo.crc32 = calculateCrc32(source.filePath);
    source.diffInfo.bustCount++;
    source.diffInfo.compiledPath = outputPath;
    source.diffInfo.lastCompiled = std::chrono::steady_clock::now();

    // CRC/バスト数をヘッダに書き込み
    writeDiffHeader(outputPath, source.diffInfo);

    source.status = TranspileStatus::Compiled;

    auto endTime = std::chrono::steady_clock::now();
    result.success = true;
    result.outputPath = outputPath;
    result.buildTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    return result;
}

std::vector<BuildResult> Transpiler::transpileBatch(std::vector<TypeScriptSource>& sources) {
    std::vector<BuildResult> results;
    results.reserve(sources.size());
    for (auto& source : sources) {
        results.push_back(transpile(source));
    }
    return results;
}

bool Transpiler::checkDiff(FileDiffInfo& diffInfo) const {
    if (diffInfo.sourcePath.empty()) {
        return true; // 情報なし → 差分ありとして扱う
    }

    if (!std::filesystem::exists(diffInfo.sourcePath)) {
        return true;
    }

    uint32_t currentCrc = calculateCrc32(diffInfo.sourcePath);
    if (currentCrc != diffInfo.crc32) {
        diffInfo.crc32 = currentCrc;
        return true; // 差分あり
    }

    return false; // 差分なし
}

void Transpiler::writeDiffHeader(const std::string& cppPath, const FileDiffInfo& info) {
    // 既存ファイルを読み込み
    std::ifstream ifs(cppPath);
    if (!ifs.is_open()) return;

    std::stringstream buffer;
    buffer << ifs.rdbuf();
    std::string content = buffer.str();
    ifs.close();

    // 先頭にCRC/バスト数コメントを更新
    std::string header = "// ERGO_COMPOSE_CRC: " + std::to_string(info.crc32) + "\n";
    header += "// ERGO_COMPOSE_BUST: " + std::to_string(info.bustCount) + "\n";

    // 既存のヘッダを除去
    size_t headerEnd = 0;
    if (content.find("// ERGO_COMPOSE_CRC:") == 0) {
        auto pos = content.find('\n', 0);
        if (pos != std::string::npos) {
            pos = content.find('\n', pos + 1);
            if (pos != std::string::npos) {
                headerEnd = pos + 1;
            }
        }
    }

    std::ofstream ofs(cppPath);
    if (ofs.is_open()) {
        ofs << header << content.substr(headerEnd);
    }
}

std::optional<FileDiffInfo> Transpiler::readDiffHeader(const std::string& cppPath) {
    std::ifstream ifs(cppPath);
    if (!ifs.is_open()) return std::nullopt;

    std::string line1, line2;
    if (!std::getline(ifs, line1) || !std::getline(ifs, line2)) {
        return std::nullopt;
    }

    const std::string crcPrefix = "// ERGO_COMPOSE_CRC: ";
    const std::string bustPrefix = "// ERGO_COMPOSE_BUST: ";

    if (line1.find(crcPrefix) != 0 || line2.find(bustPrefix) != 0) {
        return std::nullopt;
    }

    FileDiffInfo info;
    info.crc32 = static_cast<uint32_t>(std::stoul(line1.substr(crcPrefix.size())));
    info.bustCount = static_cast<uint32_t>(std::stoul(line2.substr(bustPrefix.size())));
    info.compiledPath = cppPath;

    return info;
}

uint32_t Transpiler::calculateCrc32(const std::string& filePath) {
    std::ifstream ifs(filePath, std::ios::binary);
    if (!ifs.is_open()) return 0;

    // CRC-32 (ISO 3309 / ITU-T V.42)
    uint32_t crc = 0xFFFFFFFF;
    char buf[4096];
    while (ifs.read(buf, sizeof(buf)) || ifs.gcount() > 0) {
        auto count = ifs.gcount();
        for (std::streamsize i = 0; i < count; ++i) {
            uint8_t byte = static_cast<uint8_t>(buf[i]);
            crc ^= byte;
            for (int bit = 0; bit < 8; ++bit) {
                if (crc & 1) {
                    crc = (crc >> 1) ^ 0xEDB88320;
                } else {
                    crc >>= 1;
                }
            }
        }
    }
    return crc ^ 0xFFFFFFFF;
}

} // namespace ergo::compose
