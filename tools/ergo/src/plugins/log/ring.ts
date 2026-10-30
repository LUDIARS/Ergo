/// Bounded line store for the `log` plugin: one ring per app, holding the
/// newest RING_CAPACITY lines. Every pushed line gets the next `seq`
/// (1, 2, 3, ...), so a client can ask for "everything after seq N" and a
/// line that rolled out of the ring is simply no longer returned.

import type { LevelTag, LogLine, StoredLine } from "./protocol.js";

export const RING_CAPACITY = 10_000;

export interface LineFilter {
    /** Keep only these levels (every level when omitted). */
    levels?:   ReadonlySet<LevelTag>;
    /** Keep lines with `seq > since`. */
    since?:    number;
    /** Case-insensitive substring of `msg`. */
    q?:        string;
    /** Inclusive frame range. */
    frameMin?: number;
    frameMax?: number;
}

export function matchesFilter(line: StoredLine, f: LineFilter, needle = f.q?.toLowerCase()): boolean {
    if (f.levels && !f.levels.has(line.lvl)) return false;
    if (f.since !== undefined && line.seq <= f.since) return false;
    if (f.frameMin !== undefined && line.frame < f.frameMin) return false;
    if (f.frameMax !== undefined && line.frame > f.frameMax) return false;
    if (needle && !line.msg.toLowerCase().includes(needle)) return false;
    return true;
}

export class LineRing {
    private readonly buf: Array<StoredLine | undefined>;
    private head    = 0;  // index of the oldest line
    private count   = 0;
    private nextSeq = 1;

    constructor(readonly capacity: number = RING_CAPACITY) {
        if (!Number.isInteger(capacity) || capacity < 1) {
            throw new RangeError(`LineRing capacity must be a positive integer (got ${capacity})`);
        }
        this.buf = new Array<StoredLine | undefined>(capacity);
    }

    /** Lines currently held. */
    get size(): number { return this.count; }
    /** seq of the newest line; 0 before the first push. */
    get lastSeq(): number { return this.nextSeq - 1; }
    /** seq of the oldest line held (lastSeq + 1 when empty). */
    get firstSeq(): number { return this.nextSeq - this.count; }

    /** Append `line`, evicting the oldest when full. Returns the stored line. */
    push(line: LogLine): StoredLine {
        const stored: StoredLine = { ...line, seq: this.nextSeq++ };
        if (this.count < this.capacity) {
            this.buf[(this.head + this.count) % this.capacity] = stored;
            ++this.count;
        } else {
            this.buf[this.head] = stored;
            this.head = (this.head + 1) % this.capacity;
        }
        return stored;
    }

    /**
     * Lines matching `f`, oldest first. When more than `limit` match, only
     * the newest `limit` are returned.
     */
    query(f: LineFilter = {}, limit: number = Number.POSITIVE_INFINITY): StoredLine[] {
        // seq is contiguous inside the ring, so `since` maps to an index.
        let start = 0;
        if (f.since !== undefined) {
            start = Math.max(0, Math.floor(f.since) - this.firstSeq + 1);
        }
        const needle = f.q ? f.q.toLowerCase() : undefined;
        const out: StoredLine[] = [];
        for (let i = this.count - 1; i >= start && out.length < limit; --i) {
            const line = this.buf[(this.head + i) % this.capacity]!;
            if (matchesFilter(line, f, needle)) out.push(line);
        }
        return out.reverse();
    }
}
