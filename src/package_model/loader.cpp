#include "ergo/package_model/loader.h"

#include "ergo/package_model/attribution.h"
#include "ergo/package_model/layers.h"
#include "ergo/package_model/resolved_reader.h"

#include <filesystem>

namespace ergo::package_model {

namespace {

void note(LoadReport* report, std::string text) {
    if (report) report->notes.push_back(std::move(text));
}

} // namespace

bool load_package_model(const LoadOptions& o, PackageModel& model, LoadReport* report,
                        std::string* error) {
    model = PackageModel{};
    const std::string resolved = o.resolved_path.empty()
                                     ? normalize_path("ergo-packages.resolved.json", o.build_dir)
                                     : o.resolved_path;
    if (!read_resolved_file(resolved, model, error)) return false;

    std::string api_error;
    if (!read_codemodel(o.build_dir, model, &api_error, o.codemodel)) {
        if (o.require_codemodel) {
            if (error) *error = api_error;
            return false;
        }
        model.has_codemodel = false;
        model.targets.clear();
        note(report, "File API を読めないため実リンクなし: " + api_error);
    }
    attribute_targets(model);

    if (!model.has_codemodel) return true;
    std::string layers = o.layers_path;
    if (layers.empty() && !model.ergo_root.empty()) {
        layers = normalize_path(".anatomia/layers.json", model.ergo_root);
    }
    std::error_code ec;
    if (layers.empty() || !std::filesystem::exists(std::filesystem::u8path(layers), ec)) {
        note(report, "層宣言 (.anatomia/layers.json) が無いため層は未分類");
        return true;
    }
    std::vector<LayerRule> rules;
    std::string layer_error;
    if (!read_layers_file(layers, rules, &layer_error)) {
        if (error) *error = layer_error;
        return false;
    }
    assign_layers(model, rules);
    return true;
}

} // namespace ergo::package_model
