#pragma once
/// The operations only this machine may do: refused on the remote gate whatever
/// the Host (spec/feature/tool/ergo.md "口と入口"). They are the ones the
/// Studio already kept to loopback by Host: changing the project list
/// (launcher add / remove) and the mutating calls of the plugins behind
/// mutation_guard (studio: AI instructions, plugin install; lesson: linking to a
/// class; git). Reading stays open (GET / HEAD / OPTIONS).
#include <optional>
#include <string>

namespace ergo::studio::ingress {

/// nullopt = the remote gate may serve it; otherwise why not (403).
std::optional<std::string> remote_refusal(const std::string& method, const std::string& path);

} // namespace ergo::studio::ingress
