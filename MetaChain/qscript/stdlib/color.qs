// color: 24-bit RGB colors packed as 0xRRGGBB.
//
//     use color;
//     let orange = rgb(255, 165, 0);
//     print red(orange), luminance(orange), mix(orange, WHITE, 128) == rgb(255, 210, 128);
//
// Channels are 0..255; mix() weights run 0 (all a) .. 255 (all b).
const BLACK = 0x000000;
const WHITE = 0xffffff;

fn clamp_channel(v) { return v < 0 ? 0 : v > 255 ? 255 : v; }
fn rgb(r, g, b) { return (clamp_channel(r) << 16) | (clamp_channel(g) << 8) | clamp_channel(b); }
fn red(c) { return (c >> 16) & 255; }
fn green(c) { return (c >> 8) & 255; }
fn blue(c) { return c & 255; }
// 0xRRGGBBAA.
fn rgba(r, g, b, a) { return (rgb(r, g, b) << 8) | clamp_channel(a); }
fn alpha(c) { return c & 255; }
// Rec. 601 luma, 0..255.
fn luminance(c) { return (299 * red(c) + 587 * green(c) + 114 * blue(c)) / 1000; }
fn gray(c) { let l = luminance(c); return rgb(l, l, l); }
fn invert(c) { return c ^ 0xffffff; }
fn _blend(a, b, t) { return (a * (255 - t) + b * t) / 255; }
fn mix(a, b, t) {
    return rgb(_blend(red(a), red(b), t), _blend(green(a), green(b), t), _blend(blue(a), blue(b), t));
}
fn lighten(c, amount) { return rgb(red(c) + amount, green(c) + amount, blue(c) + amount); }
fn darken(c, amount) { return lighten(c, -amount); }
fn distance2(a, b) {
    let dr = red(a) - red(b);
    let dg = green(a) - green(b);
    let db = blue(a) - blue(b);
    return dr * dr + dg * dg + db * db;
}
