#include "ergo/package_manager/rev_resolution.h"

#include "ergo/package_manager/declaration_check.h"

namespace ergo::package_manager {

namespace {

RevResolution fail(ResolveStatus status, const std::string& why) {
    RevResolution r;
    r.status = status;
    r.error  = why;
    return r;
}

const char* const kStrictWarning =
    "the pin has no tag (expect_tag removed): non-STRICT configures accept it, ERGO_PACKAGES_STRICT=ON "
    "does not — pin a tag before turning STRICT on";

} // namespace

RevResolution resolve_rev(const RevRequest& request, IRevResolver& resolver) {
    const std::string& to     = request.to;
    const std::string& branch = request.track_branch;
    if (to.empty()) return fail(ResolveStatus::InputError, "--to is empty");

    bool found = false;
    std::string main_head;
    if (!resolver.branch_head(branch, found, main_head)) {
        return fail(ResolveStatus::IoError, "cannot read the head of '" + branch + "'");
    }
    if (!found) return fail(ResolveStatus::InputError, "the repository has no branch '" + branch + "'");

    RevResolution r;
    if (to == branch) {
        r.rev = main_head;
        r.warnings.push_back(kStrictWarning);
        return r;
    }
    if (is_full_sha(to)) {
        if (!resolver.has_commit(to, found)) return fail(ResolveStatus::IoError, "cannot look up " + to);
        if (!found) return fail(ResolveStatus::InputError, "commit " + to + " is not in the repository");
        r.rev = to;
        r.warnings.push_back(kStrictWarning);
    } else {
        std::string sha;
        if (!resolver.tag_commit(to, found, sha)) {
            return fail(ResolveStatus::IoError, "cannot look up the tag '" + to + "'");
        }
        if (found) {
            r.rev        = sha;
            r.expect_tag = to;
        } else {
            if (!resolver.branch_head(to, found, sha)) {
                return fail(ResolveStatus::IoError, "cannot look up the branch '" + to + "'");
            }
            if (!found) {
                return fail(ResolveStatus::InputError,
                            "'" + to + "' is not a tag, a branch or a 40-hex commit (short SHAs are refused)");
            }
            r.rev = sha;
            r.warnings.push_back(kStrictWarning);
        }
    }

    bool on_main = false;
    if (!resolver.is_ancestor(r.rev, main_head, on_main)) {
        return fail(ResolveStatus::IoError, "cannot tell whether " + r.rev + " is on '" + branch + "'");
    }
    if (!on_main) {
        if (!request.allow_off_main) {
            return fail(ResolveStatus::InputError,
                        r.rev + " is not an ancestor of '" + branch +
                            "' (a squash merge would drop it); pass --allow-off-main to pin it anyway");
        }
        r.off_main = true;
        r.warnings.push_back(r.rev + " is not on '" + branch + "' (off-main): `watch` will keep warning");
    }
    return r;
}

} // namespace ergo::package_manager
