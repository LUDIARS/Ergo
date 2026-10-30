// @implements SPEC-ergo-studio-server-side
/// 外部コマンドの起動 (git / node / npm)。shell を通さず引数配列で渡す。

import { execFile } from "node:child_process";

export interface ExecResult {
    ok:     boolean;
    code:   number | null;
    stdout: string;
    stderr: string;
    /** 起動できなかった (コマンドが無い等)。 */
    spawnError?: string;
}

export function runFile(cmd: string, args: string[], opts: { cwd?: string; timeoutMs: number }): Promise<ExecResult> {
    return new Promise((resolve) => {
        execFile(cmd, args, {
            cwd: opts.cwd,
            timeout: opts.timeoutMs,
            windowsHide: true,
            maxBuffer: 32 * 1024 * 1024,
            encoding: "utf8",
            // Windows の npm は .cmd なので shell 無しでは起動できない。cmd 名で切り替える。
            shell: process.platform === "win32" && cmd === "npm",
        }, (err, stdout, stderr) => {
            if (err && (err as NodeJS.ErrnoException).code === "ENOENT") {
                resolve({ ok: false, code: null, stdout: "", stderr: "", spawnError: `${cmd} が見つかりません` });
                return;
            }
            const code = err ? (typeof (err as { code?: unknown }).code === "number" ? (err as { code: number }).code : 1) : 0;
            resolve({ ok: !err, code, stdout: String(stdout), stderr: String(stderr) });
        });
    });
}
