/// In-memory stand-in for a `ws` WebSocket, for driving a plugin's
/// onUpgrade() without a server or a port.
///
/// The plugin sees an open socket (readyState 1 = WS.OPEN). Frames the
/// plugin sends are parsed as JSON and kept in `sent`; `receive()` feeds a
/// frame to the plugin the way `ws` does (a Buffer on "message"), and
/// `close()` fires "close" so the plugin's cleanup runs.

import { EventEmitter } from "node:events";
import type { IncomingMessage } from "node:http";
import type { WebSocket } from "ws";

import type { Plugin, PluginContext } from "../../src/core/plugin.js";

export interface SentFrame {
    op: string;
    [key: string]: unknown;
}

export class FakeWs extends EventEmitter {
    readyState = 1;
    readonly sent: SentFrame[] = [];

    send(text: string): void {
        this.sent.push(JSON.parse(text) as SentFrame);
    }

    /** Deliver a JSON frame to the plugin as if the peer sent it. */
    receive(msg: unknown): void {
        this.emit("message", Buffer.from(JSON.stringify(msg)));
    }

    close(): void {
        if (this.readyState === 3) return;
        this.readyState = 3;
        this.emit("close");
    }

    /** Frames with the given op, in arrival order. */
    ops(op: string): SentFrame[] {
        return this.sent.filter((m) => m.op === op);
    }

    /** The most recent frame with the given op. */
    last(op: string): SentFrame | undefined {
        const all = this.ops(op);
        return all[all.length - 1];
    }

    clear(): void {
        this.sent.length = 0;
    }

    asWs(): WebSocket {
        return this as unknown as WebSocket;
    }
}

/** A context whose log lines are collected instead of printed. */
export function quietContext(lines: string[] = []): PluginContext {
    return { log: (level, msg) => { lines.push(`${level}: ${msg}`); } };
}

/** Open a fake connection to `plugin` (calls onUpgrade with a dummy request). */
export function connect(plugin: Plugin, ctx: PluginContext = quietContext()): FakeWs {
    const ws = new FakeWs();
    if (!plugin.onUpgrade) throw new Error(`plugin '${plugin.id}' has no onUpgrade`);
    plugin.onUpgrade({} as IncomingMessage, ws.asWs(), ctx);
    return ws;
}
