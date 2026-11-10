#pragma once
/// Prompt text for LLM VFX generation (port of vfx-suggest/prompt.ts; the
/// wording is kept identical so results stay comparable).
#include <string>

namespace ergo::studio::plugins::vfx_suggest {

std::string build_system_prompt(const std::string& game);
std::string build_user_content(const std::string& game, const std::string& scene, int count);

} // namespace ergo::studio::plugins::vfx_suggest
