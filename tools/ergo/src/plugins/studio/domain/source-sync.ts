// @implements SPEC-ergo-studio-sources
/// git の状態の読み取りと、早送りしてよいかの判定 (純関数)。

export interface BranchStatus {
    branch:    string | null;   // detached は null
    upstream:  string | null;
    ahead:     number;
    behind:    number;
    changed:   number;          // 追跡ファイルの変更 (staged / unstaged / 衝突)
    untracked: number;
}

/// `git status --porcelain=v2 --branch` の出力を読む。
export function parseBranchStatus(text: string): BranchStatus {
    const s: BranchStatus = { branch: null, upstream: null, ahead: 0, behind: 0, changed: 0, untracked: 0 };
    for (const line of text.split(/\r?\n/)) {
        if (line.startsWith("# branch.head ")) {
            const head = line.slice("# branch.head ".length).trim();
            s.branch = head === "(detached)" ? null : head;
        } else if (line.startsWith("# branch.upstream ")) {
            s.upstream = line.slice("# branch.upstream ".length).trim();
        } else if (line.startsWith("# branch.ab ")) {
            const m = line.match(/\+(\d+)\s+-(\d+)/);
            if (m) {
                s.ahead = Number(m[1]);
                s.behind = Number(m[2]);
            }
        } else if (/^[12u] /.test(line)) {
            s.changed++;
        } else if (line.startsWith("? ")) {
            s.untracked++;
        }
    }
    return s;
}

export type FastForwardDecision =
    | { allowed: true; branch: string }
    | { allowed: false; reason: string };

/// 早送りは「基準ブランチにいて、変更が無く、手元だけのコミットが無く、遅れている」ときだけ。
/// 共有の作業ツリーで他の人の作業を巻き込まないための条件。
export function decideFastForward(s: BranchStatus, baseBranch = "main"): FastForwardDecision {
    if (s.branch !== baseBranch) return { allowed: false, reason: `${baseBranch} 以外のブランチ (${s.branch ?? "detached"}) にいます` };
    if (!s.upstream) return { allowed: false, reason: "追跡ブランチがありません" };
    if (s.changed > 0) return { allowed: false, reason: `未コミットの変更が ${s.changed} 件あります` };
    if (s.ahead > 0) return { allowed: false, reason: `手元だけのコミットが ${s.ahead} 件あります` };
    if (s.behind === 0) return { allowed: false, reason: "すでに最新です" };
    return { allowed: true, branch: baseBranch };
}
