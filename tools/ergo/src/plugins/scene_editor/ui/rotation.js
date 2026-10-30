// Euler-degree <-> quaternion helpers for the inspector.
//
// The engine stores rotations as quaternions [x, y, z, w] and the wire field
// `transform.rot` carries a full quaternion. Artists edit Euler degrees:
// roll about X, then pitch about Y, then yaw about Z, all about the fixed
// (world) axes — i.e. q = qz * qy * qx.

const DEG = Math.PI / 180;

/** [xDeg, yDeg, zDeg] -> unit quaternion [x, y, z, w]. */
export function eulerDegToQuat([xDeg, yDeg, zDeg]) {
    const hx = (xDeg * DEG) / 2, hy = (yDeg * DEG) / 2, hz = (zDeg * DEG) / 2;
    const cx = Math.cos(hx), sx = Math.sin(hx);
    const cy = Math.cos(hy), sy = Math.sin(hy);
    const cz = Math.cos(hz), sz = Math.sin(hz);
    return [
        sx * cy * cz - cx * sy * sz,
        cx * sy * cz + sx * cy * sz,
        cx * cy * sz - sx * sy * cz,
        cx * cy * cz + sx * sy * sz,
    ];
}

/** Quaternion [x, y, z, w] (any length) -> [xDeg, yDeg, zDeg]. */
export function quatToEulerDeg([x, y, z, w]) {
    const len = Math.hypot(x, y, z, w) || 1;
    x /= len; y /= len; z /= len; w /= len;
    const roll  = Math.atan2(2 * (w * x + y * z), 1 - 2 * (x * x + y * y));
    const sinp  = 2 * (w * y - z * x);
    const pitch = Math.abs(sinp) >= 1 ? Math.sign(sinp) * (Math.PI / 2) : Math.asin(sinp);
    const yaw   = Math.atan2(2 * (w * z + x * y), 1 - 2 * (y * y + z * z));
    return [roll / DEG, pitch / DEG, yaw / DEG];
}
