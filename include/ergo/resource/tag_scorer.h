#pragma once

/// Tag-match scoring — pure logic, no I/O, no Ergo dependencies.
///
/// Mirrors MUSA Clio's `ClioTagScorer` weights so Unity-side and
/// engine-side selection agree: tags decide, names only assist.

#include <string>
#include <vector>

namespace ergo::resource {

/// Score for each wanted tag the candidate also carries.
inline constexpr int kTagMatchScore = 10;

/// Bonus when the candidate name equals the hint (case-insensitive).
inline constexpr int kNameExactScore = 5;

/// Bonus per hint token contained in the candidate name.
inline constexpr int kNameTokenScore = 2;

/// Compute the match score of one candidate. Comparison is
/// case-insensitive; blank tags are ignored. Returns 0 when nothing
/// matches (callers drop such candidates).
int score(const std::vector<std::string>& candidate_tags,
          const std::vector<std::string>& wanted_tags,
          const std::string& candidate_name,
          const std::string& name_hint);

/// Derive a name hint from a scene-object name: strips a trailing
/// " (N)" duplicate counter and returns "" for bare placeholder names
/// ("cube", any case) so default primitives don't skew matching.
std::string extract_name_hint(const std::string& object_name);

} // namespace ergo::resource
