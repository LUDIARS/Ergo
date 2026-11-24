#include "static_model_source.h"

#include "import_assets.h"

#include <stdexcept>

namespace ergo::studio::plugins::asset_import {

pictor::ModelImportResult load_static_model(const std::filesystem::path& source,
                                           const std::string& bytes) {
    auto model = pictor::ModelImporter{}.import_file(source);
    if (!model.success) throw std::runtime_error("Pictor: " + model.error_message);
    // Pictor opens the source to resolve sidecars. Check that the primary file
    // still matches the bounded read used for the import or capture revision.
    const auto current = read_bounded(source, kMaxModelBytes);
    if (!current || *current != bytes) throw std::runtime_error("model changed during import");
    return model;
}

} // namespace ergo::studio::plugins::asset_import
