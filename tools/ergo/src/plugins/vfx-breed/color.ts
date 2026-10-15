/// RGB(A) <-> HSV(A) conversion. Colors breed in HSV so a mutation moves
/// hue / saturation / brightness separately instead of muddying channels.
/// Every component is 0..1 (hue too: 1 == 360°).

export type Rgba = [number, number, number, number];
export type Hsva = [number, number, number, number];

export function rgbaToHsva([r, g, b, a]: Rgba): Hsva {
    const max = Math.max(r, g, b);
    const min = Math.min(r, g, b);
    const d   = max - min;
    let h = 0;
    if (d > 0) {
        if (max === r)      h = ((g - b) / d) % 6;
        else if (max === g) h = (b - r) / d + 2;
        else                h = (r - g) / d + 4;
        h /= 6;
        if (h < 0) h += 1;
    }
    const s = max > 0 ? d / max : 0;
    return [h, s, max, a];
}

export function hsvaToRgba([h, s, v, a]: Hsva): Rgba {
    const hh = ((h % 1) + 1) % 1 * 6;
    const i  = Math.floor(hh);
    const f  = hh - i;
    const p  = v * (1 - s);
    const q  = v * (1 - s * f);
    const t  = v * (1 - s * (1 - f));
    const [r, g, b] =
        i === 0 ? [v, t, p] :
        i === 1 ? [q, v, p] :
        i === 2 ? [p, v, t] :
        i === 3 ? [p, q, v] :
        i === 4 ? [t, p, v] :
                  [v, p, q];
    return [r, g, b, a];
}
