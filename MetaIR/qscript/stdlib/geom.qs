// geom: integer 2-D geometry on separate x, y coordinates.
//
//     use geom;
//     print manhattan(0, 0, 3, 4), dist(0, 0, 3, 4), orient(0, 0, 1, 0, 0, 1);  // 7 5 1
//
// Exact integer predicates (orientation, intersection) are robust; dist()
// is the floor of the Euclidean distance.
use math;

fn manhattan(x1, y1, x2, y2) { return abs(x2 - x1) + abs(y2 - y1); }
fn chebyshev(x1, y1, x2, y2) { return max(abs(x2 - x1), abs(y2 - y1)); }
fn dist2(x1, y1, x2, y2) { let dx = x2 - x1; let dy = y2 - y1; return dx * dx + dy * dy; }
fn dist(x1, y1, x2, y2) { return isqrt(dist2(x1, y1, x2, y2)); }
fn dot(ax, ay, bx, by) { return ax * bx + ay * by; }
fn cross(ax, ay, bx, by) { return ax * by - ay * bx; }
// 1 if a, b, c turn counter-clockwise, -1 if clockwise, 0 if collinear.
fn orient(ax, ay, bx, by, cx, cy) { return sign(cross(bx - ax, by - ay, cx - ax, cy - ay)); }
// Twice the area of triangle abc.
fn area2(ax, ay, bx, by, cx, cy) { return abs(cross(bx - ax, by - ay, cx - ax, cy - ay)); }
fn in_rect(x, y, x0, y0, x1, y1) { return x >= min(x0, x1) && x <= max(x0, x1) && y >= min(y0, y1) && y <= max(y0, y1); }
fn in_circle(x, y, cx, cy, r) { return dist2(x, y, cx, cy) <= r * r; }
// Lattice points on the closed segment from (x1, y1) to (x2, y2).
fn lattice_points(x1, y1, x2, y2) { return gcd(x2 - x1, y2 - y1) + 1; }
// Do closed segments ab and cd intersect?
fn segments_intersect(ax, ay, bx, by, cx, cy, dx, dy) {
    let d1 = orient(cx, cy, dx, dy, ax, ay);
    let d2 = orient(cx, cy, dx, dy, bx, by);
    let d3 = orient(ax, ay, bx, by, cx, cy);
    let d4 = orient(ax, ay, bx, by, dx, dy);
    if (d1 * d2 < 0 && d3 * d4 < 0) return 1;
    if (d1 == 0 && in_rect(ax, ay, cx, cy, dx, dy)) return 1;
    if (d2 == 0 && in_rect(bx, by, cx, cy, dx, dy)) return 1;
    if (d3 == 0 && in_rect(cx, cy, ax, ay, bx, by)) return 1;
    if (d4 == 0 && in_rect(dx, dy, ax, ay, bx, by)) return 1;
    return 0;
}
