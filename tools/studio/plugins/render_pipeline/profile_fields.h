#pragma once
/// Field tables of Pictor's PipelineProfileDef (schema v2), the data behind
/// normalisation and serialisation (port of render_pipeline/profile_schema.ts).
/// Keep in sync with Pictor/spec/pipeline-profile-config.md and
/// pictor::PipelineProfileDef. Table order = key order in written files.
#include <array>
#include <string>
#include <vector>

namespace ergo::studio::plugins::render_pipeline {

using Choices = std::vector<std::string>;

struct Section;

enum class FieldType { Number, Bool, String, Enum, Vec3, StringArray, Section };

struct Field {
    std::string           key;
    FieldType             type = FieldType::Number;
    double                num  = 0;
    bool                  flag = false;
    std::string           text;
    const Choices*        choices = nullptr;
    std::array<double, 3> vec{};
    const Section*        section = nullptr;
};

struct Section {
    std::vector<Field> fields;
};

// ---- enums ----
const Choices& rendering_paths();
const Choices& pass_types();
const Choices& sort_modes();
const Choices& shadow_filter_modes();
const Choices& overlay_modes();
const Choices& post_process_kinds();
const Choices& tone_map_operators();
const Choices& attachment_kinds();
const Choices& attachment_sizings();
const Choices& attachment_formats();
const Choices& attachment_usage_flags();
const Choices& attachment_load_ops();
const Choices& attachment_store_ops();
const Choices& image_layouts();

// ---- sections ----
const Section& profile_scalars();   ///< version .. compute_update_enabled
const Section& render_pass_section();   ///< without attachment_ops
const Section& attachment_ops_section();
const Section& bloom_section();
const Section& tone_mapping_section();
const Section& vignette_section();
const Section& color_grading_section();
const Section& depth_of_field_section();
const Section& shadow_section();
const Section& gi_section();        ///< includes shadow / ssao / probes
const Section& memory_section();    ///< includes gpu
const Section& gpu_driven_section();
const Section& update_section();
const Section& profiler_section();

/// Top-level object sections after post_process, in file order.
struct NamedSection {
    const char*    key;
    const Section* section;
};
const std::vector<NamedSection>& tail_sections();

} // namespace ergo::studio::plugins::render_pipeline
