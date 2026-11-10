#pragma once
/// Pipeline profile normalisation and serialisation (port of
/// render_pipeline/profile_schema.ts). Forward compatible like Pictor's
/// serializer: unknown keys are dropped, missing keys and unknown enum
/// strings take the defaults. A v1 profile without attachments[] gets the
/// built-in three (scene_hdr_color / scene_depth / swapchain).
#include "../common/json_util.h"

#include <string>

namespace ergo::studio::plugins::render_pipeline {

constexpr int kProfileSchemaVersion = 2;

/// Case-insensitive effect name -> PostProcessKind ("Unknown" when unmapped);
/// mirrors pictor post_process_kind_from_name().
std::string post_process_kind_from_name(const std::string& name);

/// Bit-equivalent to Pictor's default_attachments().
JsonValue default_attachments();

/// Any parsed JSON -> a fully populated profile object.
JsonValue normalize_profile(const JsonValue& raw);

/// File text of a normalised profile: canonical key order (version first),
/// two-space indentation, trailing newline; each post_process entry keeps
/// only the parameter block of its kind; _editor only when non-empty.
std::string serialize_profile(const JsonValue& normalized);

/// "<lower-cased profile_name stripped to [a-z0-9_-]>.profile.json"
/// ("untitled" when nothing is left).
std::string profile_file_name(const std::string& profile_name);

} // namespace ergo::studio::plugins::render_pipeline
