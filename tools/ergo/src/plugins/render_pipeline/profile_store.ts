/// Disk-backed store for Pictor pipeline profiles (`*.profile.json`).
///
/// Unlike the particle / variable plugins (which keep an in-memory config
/// only), the pipeline profile *files* are the source of truth — they are
/// consumed by Pictor at runtime via `register_presets_from_dir()`. This
/// store therefore reads from and writes to a real directory of
/// `<name>.profile.json` files.
///
/// Default directory resolution (first hit wins):
///   1. env  ERGO_PICTOR_PROFILE_DIR
///   2. <cwd>/../../../Pictor/profiles      (ergo & Pictor as siblings)
///   3. <cwd>/profiles                      (last-resort fallback)
///
/// `cwd` is `tools/ergo` when the server runs from there, so option 2
/// resolves to `E:/Document/Ars/Pictor/profiles` in the standard layout.

import { readFileSync, writeFileSync, readdirSync, existsSync, mkdirSync, statSync } from "node:fs";
import { resolve, join, isAbsolute } from "node:path";

import {
    normalizeProfile,
    serializeProfile,
    profileFileName,
    type PipelineProfileDef,
} from "./profile_schema.js";

const PROFILE_SUFFIX = ".profile.json";

/** Resolve the directory that holds `*.profile.json` files. */
export function resolveProfileDir(): string {
    const fromEnv = process.env.ERGO_PICTOR_PROFILE_DIR;
    if (fromEnv && fromEnv.trim()) {
        return isAbsolute(fromEnv) ? fromEnv : resolve(process.cwd(), fromEnv);
    }
    // ergo & Pictor checked out side by side: tools/ergo -> ../../../Pictor
    const sibling = resolve(process.cwd(), "..", "..", "..", "Pictor", "profiles");
    if (existsSync(sibling)) return sibling;
    return resolve(process.cwd(), "profiles");
}

export interface ProfileListEntry {
    /** File name, e.g. `standard.profile.json`. */
    file:          string;
    /** `profile_name` field from inside the file (falls back to file stem). */
    profile_name:  string;
    /** Bytes on disk. */
    size:          number;
    /** ISO mtime. */
    mtime:         string;
}

export interface ProfileLoadResult {
    file:    string;
    profile: PipelineProfileDef;
    /** Raw mtime so the UI can warn on concurrent external edits. */
    mtime:   string;
}

export interface ProfileSaveResult {
    file:    string;
    profile: PipelineProfileDef;
    mtime:   string;
}

/** Reject path traversal — only bare `*.profile.json` names are allowed. */
function assertSafeFile(file: string): void {
    if (
        !file.endsWith(PROFILE_SUFFIX) ||
        file.includes("/") || file.includes("\\") ||
        file.includes("..")
    ) {
        throw new Error(`invalid profile file name: ${file}`);
    }
}

/** List every `*.profile.json` in the profile directory. */
export function listProfiles(dir = resolveProfileDir()): ProfileListEntry[] {
    if (!existsSync(dir)) return [];
    const out: ProfileListEntry[] = [];
    for (const name of readdirSync(dir)) {
        if (!name.endsWith(PROFILE_SUFFIX)) continue;
        const full = join(dir, name);
        let st;
        try { st = statSync(full); } catch { continue; }
        if (!st.isFile()) continue;
        let profileName = name.slice(0, -PROFILE_SUFFIX.length);
        try {
            const parsed = JSON.parse(readFileSync(full, "utf-8"));
            if (parsed && typeof parsed.profile_name === "string" && parsed.profile_name) {
                profileName = parsed.profile_name;
            }
        } catch { /* keep the file-stem fallback */ }
        out.push({
            file:         name,
            profile_name: profileName,
            size:         st.size,
            mtime:        st.mtime.toISOString(),
        });
    }
    out.sort((a, b) => a.file.localeCompare(b.file));
    return out;
}

/** Read one profile file and normalize it to a full `PipelineProfileDef`. */
export function loadProfile(file: string, dir = resolveProfileDir()): ProfileLoadResult {
    assertSafeFile(file);
    const full = join(dir, file);
    if (!existsSync(full)) throw new Error(`profile not found: ${file}`);
    const text = readFileSync(full, "utf-8");
    let parsed: unknown;
    try {
        parsed = JSON.parse(text);
    } catch (e) {
        throw new Error(`profile JSON parse failed (${file}): ${(e as Error).message}`);
    }
    const profile = normalizeProfile(parsed);
    return { file, profile, mtime: statSync(full).mtime.toISOString() };
}

/**
 * Write a profile to disk.
 *
 * `file` is optional — when omitted the canonical
 * `<lowercased-profile_name>.profile.json` name is derived. The directory
 * is created if missing.
 */
export function saveProfile(
    profile: PipelineProfileDef,
    file?: string,
    dir = resolveProfileDir(),
): ProfileSaveResult {
    const target = file ?? profileFileName(profile.profile_name);
    assertSafeFile(target);
    if (!existsSync(dir)) mkdirSync(dir, { recursive: true });
    const full = join(dir, target);
    const text = serializeProfile(normalizeProfile(profile));
    writeFileSync(full, text, "utf-8");
    return {
        file:    target,
        profile: normalizeProfile(profile),
        mtime:   statSync(full).mtime.toISOString(),
    };
}
