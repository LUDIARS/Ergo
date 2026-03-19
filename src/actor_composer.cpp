#include "ergo/compose/actor_composer.h"

#include <algorithm>
#include <queue>
#include <unordered_set>
#include <sstream>

namespace ergo::compose {

ActorComposer::ActorComposer() = default;
ActorComposer::~ActorComposer() = default;

void ActorComposer::registerActor(const ActorComposition& composition) {
    actors_[composition.actorName] = composition;
}

void ActorComposer::unregisterActor(const std::string& actorName) {
    actors_.erase(actorName);
}

const ActorComposition* ActorComposer::getActor(const std::string& actorName) const {
    auto it = actors_.find(actorName);
    return (it != actors_.end()) ? &it->second : nullptr;
}

std::vector<std::string> ActorComposer::getActorNames() const {
    std::vector<std::string> names;
    names.reserve(actors_.size());
    for (const auto& [name, _] : actors_) {
        names.push_back(name);
    }
    return names;
}

void ActorComposer::addModuleBinding(const std::string& actorName, const ModuleBinding& binding) {
    auto it = actors_.find(actorName);
    if (it != actors_.end()) {
        it->second.moduleBindings.push_back(binding);
    }
}

void ActorComposer::setRenderBinding(const std::string& actorName, const RenderBinding& binding) {
    auto it = actors_.find(actorName);
    if (it != actors_.end()) {
        it->second.renderBinding = binding;
    }
}

BuildResult ActorComposer::buildActor(const std::string& actorName, BuildMode mode) {
    BuildResult result;
    auto startTime = std::chrono::steady_clock::now();

    auto it = actors_.find(actorName);
    if (it == actors_.end()) {
        result.success = false;
        result.errorMessage = "Actor not found: " + actorName;
        return result;
    }

    auto& actor = it->second;

    // 1. 依存解決
    auto deps = resolveDependencies(actorName);

    // 2. TSロジックをトランスパイル
    for (auto& source : actor.logicSources) {
        auto transpileResult = transpiler_.transpile(source);
        if (!transpileResult.success) {
            result.success = false;
            result.errorMessage = "Transpile failed for " + source.filePath +
                                  ": " + transpileResult.errorMessage;
            return result;
        }
        result.warnings.insert(result.warnings.end(),
                                transpileResult.warnings.begin(),
                                transpileResult.warnings.end());
    }

    // 3. 結合コードを生成
    std::string bindingCode = generateBindingCode(actor, mode);

    // 4. ビルド結果を返す（実際のCMakeビルドはArsが呼び出す）
    result.success = true;
    result.outputPath = actorName + ".compose.h";

    auto endTime = std::chrono::steady_clock::now();
    result.buildTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    return result;
}

std::vector<BuildResult> ActorComposer::buildAll(BuildMode mode) {
    std::vector<BuildResult> results;
    for (const auto& [name, _] : actors_) {
        results.push_back(buildActor(name, mode));
    }
    return results;
}

std::vector<std::string> ActorComposer::resolveDependencies(const std::string& actorName) const {
    // トポロジカルソートで依存順を解決
    std::vector<std::string> sorted;
    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> inStack;

    std::function<bool(const std::string&)> visit = [&](const std::string& name) -> bool {
        if (inStack.count(name)) return false; // 循環依存
        if (visited.count(name)) return true;

        inStack.insert(name);
        visited.insert(name);

        auto it = actors_.find(name);
        if (it != actors_.end()) {
            for (const auto& dep : it->second.dependencies) {
                if (!visit(dep)) return false;
            }
        }

        inStack.erase(name);
        sorted.push_back(name);
        return true;
    };

    visit(actorName);
    return sorted;
}

Transpiler& ActorComposer::transpiler() {
    return transpiler_;
}

std::string ActorComposer::generateBindingCode(const ActorComposition& actor, BuildMode mode) const {
    std::ostringstream code;

    code << "// Auto-generated binding code for actor: " << actor.actorName << "\n";
    code << "#pragma once\n\n";

    // モジュールバインディングの include
    for (const auto& binding : actor.moduleBindings) {
        if (!binding.headerPath.empty()) {
            code << "#include \"" << binding.headerPath << "\"\n";
        }
    }

    // Pictor描画バインディング
    code << "#include \"pictor/object_descriptor.h\"\n\n";

    code << "namespace ars::composed {\n\n";

    // アクタークラス
    code << "class " << actor.actorName << "Composed {\n";
    code << "public:\n";

    // モジュール参照メンバ
    for (const auto& binding : actor.moduleBindings) {
        code << "    // " << binding.moduleName << " (" ;
        switch (binding.domain) {
            case BindingDomain::Logic:   code << "Logic"; break;
            case BindingDomain::Input:   code << "Input"; break;
            case BindingDomain::Physics: code << "Physics"; break;
            case BindingDomain::Audio:   code << "Audio"; break;
            case BindingDomain::Render:  code << "Render"; break;
            case BindingDomain::Network: code << "Network"; break;
            case BindingDomain::Custom:  code << "Custom"; break;
        }
        code << ")\n";
    }

    // 描画設定
    code << "\n    // Pictor render binding\n";
    code << "    // mesh: " << actor.renderBinding.meshId << "\n";
    code << "    // material: " << actor.renderBinding.materialId << "\n";

    // プラットフォーム分岐
    if (mode == BuildMode::Web) {
        code << "\n    // Platform: WebGL + Wasm\n";
    } else {
        code << "\n    // Platform: App + QUIC IPC\n";
    }

    code << "};\n\n";
    code << "} // namespace ars::composed\n";

    return code.str();
}

} // namespace ergo::compose
