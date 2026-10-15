/// Seeded deterministic RNG (mulberry32) — the same seed always yields the
/// same sequence, so a generation request with a fixed seed reproduces the
/// same children.

export interface Rng {
    /** Uniform in [0, 1). */
    next(): number;
    /** Standard normal sample (Box–Muller). */
    gaussian(): number;
    /** Uniform integer in [0, n). */
    int(n: number): number;
}

export function makeRng(seed: number): Rng {
    let s = (Math.floor(seed) >>> 0) || 0x9e3779b9;

    const next = (): number => {
        s = (s + 0x6d2b79f5) >>> 0;
        let t = s;
        t = Math.imul(t ^ (t >>> 15), t | 1);
        t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
        return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };

    return {
        next,
        gaussian() {
            const u = 1 - next();   // (0, 1] — keeps log() finite
            const v = next();
            return Math.sqrt(-2 * Math.log(u)) * Math.cos(2 * Math.PI * v);
        },
        int(n: number) {
            return Math.floor(next() * n);
        },
    };
}

/// Seed for a request that did not pass one. Returned to the client so the
/// generation can be reproduced.
export function freshSeed(): number {
    return Math.floor(Math.random() * 0xffffffff) >>> 0;
}
