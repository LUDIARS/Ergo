/// Path containment guard shared by plugins that read / write files under
/// a configured root (visus, ui_layout).

import path from "node:path";

/** The subset of node:path the guard uses (`path.posix` / `path.win32` in tests). */
export type PathModule = Pick<typeof path, "resolve" | "relative" | "isAbsolute" | "sep">;

/// Resolve `rel` against `root` and return the absolute path, or null when
/// the result is not inside `root` (the root itself counts as inside).
///
/// `rel` may be absolute, as long as it points inside `root`. A result is
/// rejected when relative(root, target) is "..", starts with ".." + sep,
/// or is itself absolute. The last case is the Windows cross-drive /
/// UNC escape: relative("C:\\proj", "D:\\x") returns "D:\\x", which starts
/// with neither ".." nor a separator.
export function resolveWithin(root: string, rel: string, pathMod: PathModule = path): string | null {
    const base   = pathMod.resolve(root);
    const target = pathMod.resolve(base, rel);
    const back   = pathMod.relative(base, target);
    if (back === ".." || back.startsWith(".." + pathMod.sep) || pathMod.isAbsolute(back)) {
        return null;
    }
    return target;
}
