/// Value ranges of the numeric ParticleEffectConfig fields — the one table
/// every server-side producer of configs clamps against.
///
/// Readers:
///   - vfx-suggest/sanitize.ts  (clamps LLM output)
///   - vfx-breed/genome.ts      (normalises genes to 0..1 and back)
///
/// Colors are not listed: every channel of an RGBA color is 0..1.

export interface ValueRange {
    lo: number;
    hi: number;
}

export const PARTICLE_RANGES = {
    "emission.rate":                  { lo: 1,    hi: 400  },
    "emission.maxAlive":              { lo: 1,    hi: 2000 },
    "initial.positionRadius":         { lo: 0,    hi: 256  },
    "initial.velocityAngleDeg":       { lo: 0,    hi: 360  },
    "initial.velocityAngleSpreadDeg": { lo: 0,    hi: 360  },
    "initial.speedMin":               { lo: 0,    hi: 600  },
    "initial.speedMax":               { lo: 0,    hi: 600  },
    "initial.lifetimeMin":            { lo: 0.05, hi: 6    },
    "initial.lifetimeMax":            { lo: 0.05, hi: 6    },
    "initial.size":                   { lo: 0,    hi: 64   },
    "overLife.sizeStart":             { lo: 0,    hi: 8    },
    "overLife.sizeEnd":               { lo: 0,    hi: 8    },
    "overLife.velocityDamping":       { lo: 0,    hi: 1    },
    "forces.gravity.x":               { lo: -600, hi: 600  },
    "forces.gravity.y":               { lo: -600, hi: 600  },
} as const satisfies Record<string, ValueRange>;

export type ParticleRangeKey = keyof typeof PARTICLE_RANGES;

export const COLOR_RANGE: ValueRange = { lo: 0, hi: 1 };
