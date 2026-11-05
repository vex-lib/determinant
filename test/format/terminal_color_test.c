// Owner: TerminalColor — the packed 0xRRGGBBAA vocabulary and the one operation it
// carries, `TerminalColor_over`. It proves the "set" rule, the two exact ends (a clear
// layer shows what is behind it, an opaque one passes through bit-for-bit), the rounding
// at the half step, per-channel independence, monotonic travel as alpha rises, and that
// a real fade always comes out opaque — which is the property that makes the result safe
// to hand to a device that has no alpha channel at all.
//
// This is the arithmetic a fade is made of, so the case that earns its keep is the one
// nobody would think to write twice: opaque BLACK (0x000000FF) must survive as black and
// never be mistaken for the clear sentinel (0x00000000). They differ by one bit and
// nothing else, and getting it wrong is the difference between black text and no text.
#include "format/terminal_color.h"
#include <assert.h>

// No `determinant/support.h` here on purpose: the composite is TOTAL — every uint32 in,
// an opaque uint32 out — so there is no rejection, no diagnostic to capture, and
// nothing for the harness's assertion helpers to do. `assert.h` comes straight in.

// White, black, and a clear layer, used as the composite's ends.
#define OPAQUE_WHITE 0xFFFFFFFFu
#define OPAQUE_BLACK 0x000000FFu
#define CLEAR_LAYER  0x00000000u

int main(void) {
    // --- The vocabulary itself: channel order, and the sentinel ---
    uint32_t packed = TERMINAL_RGBA(0x12, 0x34, 0x56, 0x78);
    assert(TERMINAL_COLOR_RED(packed) == 0x12u);
    assert(TERMINAL_COLOR_GREEN(packed) == 0x34u);
    assert(TERMINAL_COLOR_BLUE(packed) == 0x56u);
    assert(TERMINAL_COLOR_ALPHA(packed) == 0x78u);
    assert(TERMINAL_COLOR_DEFAULT == 0x00000000u);
    assert(!TERMINAL_COLOR_IS_SET(TERMINAL_COLOR_DEFAULT));
    // Opaque black differs from the sentinel by one bit, and must read as SET: this is
    // the assertion that keeps black text expressible.
    assert(TERMINAL_COLOR_IS_SET(OPAQUE_BLACK));
    assert(TERMINAL_COLOR_ALPHA(TERMINAL_RGB(1, 2, 3)) == 0xFFu);
    // Any non-zero alpha is a real fade and counts as set.
    assert(TERMINAL_COLOR_IS_SET(0x12345601u));

    // --- The clear end: what is behind shows through ---
    // The RGB of a clear layer is irrelevant, which is exactly why an UNSET BACKGROUND
    // is correct purely by passing it through untouched.
    assert(TerminalColor_over(TERMINAL_COLOR_DEFAULT, 0x112233FFu) == 0x112233FFu);
    assert(TerminalColor_over(0xFF00FF00u, 0x112233FFu) == 0x112233FFu);
    assert(TerminalColor_over(CLEAR_LAYER, OPAQUE_WHITE) == OPAQUE_WHITE);

    // --- The opaque end: the layer is itself, bit-for-bit ---
    assert(TerminalColor_over(OPAQUE_BLACK, 0x112233FFu) == OPAQUE_BLACK);
    assert(TerminalColor_over(0xABCDEFFFu, 0x112233FFu) == 0xABCDEFFFu);
    assert(TerminalColor_over(OPAQUE_WHITE, OPAQUE_BLACK) == OPAQUE_WHITE);

    // --- The half step, rounded to nearest ---
    // White at 50% over black is very nearly mid grey. The two halves land one step
    // apart (0x80 going one way, 0x7F coming back) because the weights are alpha and
    // 255-alpha: they sum to exactly one, and rounding therefore cannot land dead on
    // .5 in both directions. One palette step either way is invisible, and both ENDS
    // stay exact, which is the part that matters.
    assert(TerminalColor_over(0xFFFFFF80u, OPAQUE_BLACK) == 0x808080FFu);
    assert(TerminalColor_over(0x00000080u, OPAQUE_WHITE) == 0x7F7F7FFF);
    // A fade of a quarter: 255*0x40/255 = 0x40 exactly.
    assert(TerminalColor_over(0xFFFFFF40u, OPAQUE_BLACK) == 0x404040FFu);
    // The smallest and largest real fades stay inside the range rather than wrapping.
    assert(TerminalColor_over(0xFFFFFF01u, OPAQUE_BLACK) == 0x010101FFu);
    assert(TerminalColor_over(0xFFFFFFFEu, OPAQUE_BLACK) == 0xFEFEFEFFu);

    // --- Per-channel independence ---
    // Red at half over green: each channel carries over on its own, and a channel that
    // is zero on BOTH ends stays zero. Note the bottom's blue here is 0x00, which is
    // what makes this a real per-channel check rather than three copies of one sum.
    assert(TerminalColor_over(0xFF000080u, 0x00FF00FFu) == 0x807F00FFu);
    assert(TerminalColor_over(0x0000FF80u, 0x00FF00FFu) == 0x007F80FFu);

    // --- A real fade always comes out opaque ---
    // This is what makes the result safe for a device with no alpha channel: there is
    // nothing left to composite after this call.
    for (uint32_t alpha = 0x01u; alpha < 0xFFu; ++alpha) {
        uint32_t blended = TerminalColor_over(TERMINAL_RGBA(0x20, 0x60, 0xA0, alpha), OPAQUE_BLACK);
        assert(TERMINAL_COLOR_ALPHA(blended) == 0xFFu);
    }

    // --- Monotone travel, and an exact landing at both ends ---
    uint32_t previous = 0u;
    for (uint32_t alpha = 0x00u; alpha <= 0xFFu; ++alpha) {
        uint32_t blended = TerminalColor_over(TERMINAL_RGBA(0xFF, 0xFF, 0xFF, alpha), OPAQUE_BLACK);
        uint32_t red = TERMINAL_COLOR_RED(blended);
        assert(red >= previous);                       // never goes backwards
        assert(TERMINAL_COLOR_RED(blended) == TERMINAL_COLOR_GREEN(blended)
               && TERMINAL_COLOR_GREEN(blended) == TERMINAL_COLOR_BLUE(blended));
        previous = red;
    }
    assert(previous == 0xFFu);                          // the top of the ramp is reached

    // --- Re-compositing an opaque result does not drift ---
    // A composite becomes the backdrop for the next layer (a faded highlight, then ink
    // over it), and that must be a fixed point rather than a slow decay.
    uint32_t band = TerminalColor_over(TERMINAL_RGBA(0x7A, 0xC8, 0xFF, 0x38), OPAQUE_BLACK);
    assert(TerminalColor_over(band, OPAQUE_BLACK) == band);
    assert(TerminalColor_over(TERMINAL_COLOR_DEFAULT, band) == band);
    // Ink over the band is not the ink on its own: the layering is real.
    uint32_t ink = TerminalColor_over(TERMINAL_RGBA(0xE6, 0xE6, 0xE6, 0x80), band);
    assert(ink != band && TERMINAL_COLOR_ALPHA(ink) == 0xFFu);

    // --- A fade toward a light backdrop moves toward that backdrop ---
    // The same translucent colour lands light on a light theme and dark on a dark one,
    // which is the whole reason a fade is worth writing down instead of a fixed grey.
    uint32_t onDark = TerminalColor_over(TERMINAL_RGBA(0xAA, 0xAA, 0xC4, 0x7A), OPAQUE_BLACK);
    uint32_t onLight = TerminalColor_over(TERMINAL_RGBA(0xAA, 0xAA, 0xC4, 0x7A), OPAQUE_WHITE);
    assert(TERMINAL_COLOR_RED(onLight) > TERMINAL_COLOR_RED(onDark));
    assert(TERMINAL_COLOR_BLUE(onLight) > TERMINAL_COLOR_BLUE(onDark));
    return 0;
}
