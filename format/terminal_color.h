#ifndef DETERMINANT_TERMINAL_COLOR_H
#define DETERMINANT_TERMINAL_COLOR_H

#include <stdint.h>

// Determinant terminal colour — packed 0xRRGGBBAA, the ecosystem's Strict
// 0xRRGGBBAA Color Law applied to cells. Red is the high byte, alpha the low
// byte, so a hex literal reads R,G,B,A left to right and the same value means
// the same thing in a terminal cell, a graphvex Image or a brush.
//
// TERMINAL_COLOR_DEFAULT (fully transparent, 0x00000000) is the "unset" sentinel:
// the device backend renders its own default fg/bg. Opaque black is
// 0x000000FF and is NOT the sentinel, so black text is expressible.
//
// Any alpha BETWEEN those two ends is REAL: TERMINAL_RGBA(r, g, b.sh, 0x60) is the same
// colour at 38% opacity, which is how a fade is written down. A terminal has no alpha
// channel — SGR sets ONE colour — so a translucent value only means something once it
// has been composited over whatever is behind it. `TerminalColor_over` below is that
// step, and it lives here, once, because the colour contract is the toolkit's: a host
// that draws cells calls it rather than inventing its own blend.

#define TERMINAL_COLOR_DEFAULT 0x00000000u

#define TERMINAL_RGBA(r, g, b, a) \
    ((((u32) (r) & 0xFFu) << 24) | (((u32) (g) & 0xFFu) << 16) | \
     (((u32) (b) & 0xFFu) << 8) | ((u32) (a) & 0xFFu))

#define TERMINAL_RGB(r, g, b) TERMINAL_RGBA(r, g, b, 0xFFu)

#define TERMINAL_COLOR_RED(c)   ((u8) (((c) >> 24) & 0xFFu))
#define TERMINAL_COLOR_GREEN(c) ((u8) (((c) >> 16) & 0xFFu))
#define TERMINAL_COLOR_BLUE(c)  ((u8) (((c) >> 8) & 0xFFu))
#define TERMINAL_COLOR_ALPHA(c) ((u8) ((c) & 0xFFu))
// A colour is "set" when its alpha is non-zero; the default has alpha 0.
#define TERMINAL_COLOR_IS_SET(c) (((c) & 0xFFu) != 0u)

// Composite `top` over `bottom` (source-over) and return the opaque result. `bottom` is
// what is ALREADY there — a resolved device background, or a background that has been
// composited itself — and is expected to be opaque.
//
// The two ends are exact and free: a clear `top` returns `bottom` untouched, and an
// opaque `top` returns itself, so fully opaque artwork passes through bit-for-bit and
// costs one comparison.
//
// Note the asymmetry a caller must respect, because it is not an accident of this
// function: for a BACKGROUND PLANE, "clear" and "unset" really are the same thing (a
// transparent background shows what is behind it), so an unset background composites to
// the backdrop and stays correct. For a FOREGROUND PLANE they are NOT the same: an unset
// foreground means "the device's own default ink", which is certainly not "nothing", so
// feeding an unset foreground through here would silently paint the ink in the backdrop
// colour and make the text vanish. A caller keeps the planes distinct — composite what
// is set, pass what is unset straight through.
static inline u32 TerminalColor_over(u32 top, u32 bottom) {
    u32 alpha = (u32) TERMINAL_COLOR_ALPHA(top);
    if (alpha == 0u)
        return bottom;
    if (alpha == 0xFFu)
        return top;
    u32 keep = 0xFFu - alpha;
    // +127 rounds to nearest, so the two ends stay exact and no channel drifts by a step
    // when a fade passes through the middle.
    u32 red = ((u32) TERMINAL_COLOR_RED(top) * alpha
                    + (u32) TERMINAL_COLOR_RED(bottom) * keep + 127u) / 255u;
    u32 green = ((u32) TERMINAL_COLOR_GREEN(top) * alpha
                      + (u32) TERMINAL_COLOR_GREEN(bottom) * keep + 127u) / 255u;
    u32 blue = ((u32) TERMINAL_COLOR_BLUE(top) * alpha
                     + (u32) TERMINAL_COLOR_BLUE(bottom) * keep + 127u) / 255u;
    return TERMINAL_RGB(red, green, blue);
}

#endif
