// Owner: TerminalCanvas — the clipped region primitive. Admission, sub-region
// clipping, text/line/box primitives, style + colour planes, pen, row projection.
#include "ui/surface/terminal_canvas.h"
#include "format/terminal_color.h"
#include "determinant/test/support.h"

int main(void) {
    enum { W = 12, H = 4 };
    u32 cells[W * H];
    u16 styles[W * H];
    u32 fg[W * H];
    u32 bg[W * H];

    // Pen constructors.
    TerminalPen p0 = TerminalPen();
    assert(p0.style == TERMINAL_STYLE_NONE && p0.fg == TERMINAL_COLOR_DEFAULT && p0.bg == TERMINAL_COLOR_DEFAULT);
    TerminalPen p3 = TerminalPen(TERMINAL_STYLE_BOLD, TERMINAL_RGB(1, 2, 3), TERMINAL_RGBA(4, 5, 6, 255));
    assert(p3.fg == TERMINAL_RGB(1, 2, 3) && TERMINAL_COLOR_RED(p3.fg) == 1 && TERMINAL_COLOR_GREEN(p3.fg) == 2 &&
           TERMINAL_COLOR_BLUE(p3.fg) == 3 && TERMINAL_COLOR_ALPHA(p3.fg) == 255);
    assert(TERMINAL_COLOR_IS_SET(p3.fg) && !TERMINAL_COLOR_IS_SET(TERMINAL_COLOR_DEFAULT));

    // Style-only canvas (4-arg): no colour planes.
    TerminalCanvas c = TerminalCanvas(cells, styles, W, H);
    assert(TerminalCanvas_isValid(&c));
    assert(TerminalCanvas_hasStylePlane(&c) && !TerminalCanvas_hasColorPlane(&c));
    assert(TerminalCanvas_fgAt(&c, 0, 0) == TERMINAL_COLOR_DEFAULT);

    TerminalCanvas_fill(&c, '.', TerminalPen_1(TERMINAL_STYLE_MUTED));
    assert(TerminalCanvas_glyphAt(&c, 0, 0) == '.' && TerminalCanvas_styleAt(&c, 0, 0) == TERMINAL_STYLE_MUTED);

    TerminalCanvas_text(&c, 10, 0, "abcdef", 6, TerminalPen_1(TERMINAL_STYLE_BOLD));
    assert(TerminalCanvas_glyphAt(&c, 10, 0) == 'a' && TerminalCanvas_glyphAt(&c, 11, 0) == 'b');
    assert(TerminalCanvas_styleAt(&c, 10, 0) == TERMINAL_STYLE_BOLD);
    TerminalCanvas_text(&c, 0, 1, "\x1b", 1, TerminalPen());   // non-printable -> '?'
    assert(TerminalCanvas_glyphAt(&c, 0, 1) == '?');

    TerminalCanvas_hline(&c, 2, '-', TerminalPen_1(TERMINAL_STYLE_DIM));
    assert(TerminalCanvas_glyphAt(&c, 5, 2) == '-');
    TerminalCanvas_vline(&c, 3, '|', TerminalPen_1(TERMINAL_STYLE_DIM));
    assert(TerminalCanvas_glyphAt(&c, 3, 0) == '|');
    TerminalCanvas_box(&c, TerminalPen_1(TERMINAL_STYLE_DIM));
    assert(TerminalCanvas_glyphAt(&c, 0, 0) == '+' && TerminalCanvas_glyphAt(&c, W - 1, 0) == '+');

    // Sub-region clips to the parent and grid.
    TerminalCanvas s = TerminalCanvas_sub(&c, 10, 3, 100, 100);
    assert(TerminalCanvas_isValid(&s) && TerminalCanvas_getWidth(&s) == 2 && TerminalCanvas_getHeight(&s) == 1);
    TerminalCanvas outside = TerminalCanvas_sub(&c, 99, 0, 4, 4);
    assert(!TerminalCanvas_isValid(&outside));
    TerminalCanvas_fill(&s, '#', TerminalPen());
    assert(TerminalCanvas_glyphAt(&c, 11, 3) == '#' && TerminalCanvas_glyphAt(&c, 9, 3) != '#');

    // Full four-plane canvas: colours land and read back (separate buffers so the
    // earlier glyph assertions stand).
    static u32 colorCells[W * H];
    static u16 colorStyles[W * H];
    memset(fg, 0, sizeof fg);
    memset(bg, 0, sizeof bg);
    TerminalCanvas col = TerminalCanvas(colorCells, colorStyles, fg, bg, W, H);
    assert(TerminalCanvas_hasColorPlane(&col));
    TerminalCanvas_fill(&col, ' ', TerminalPen());
    TerminalPen deep = TerminalPen(TERMINAL_STYLE_ACCENT, TERMINAL_RGB(10, 20, 30), TERMINAL_RGB(40, 50, 60));
    TerminalCanvas_text(&col, 1, 1, "hi", 2, deep);
    assert(TerminalCanvas_fgAt(&col, 1, 1) == TERMINAL_RGB(10, 20, 30));
    assert(TerminalCanvas_bgAt(&col, 1, 1) == TERMINAL_RGB(40, 50, 60));
    assert(TerminalCanvas_styleAt(&col, 1, 1) == TERMINAL_STYLE_ACCENT);
    assert(TerminalCanvas_fgAt(&col, 1, 3) == TERMINAL_COLOR_DEFAULT);
    // A sub-region inherits the colour planes.
    TerminalCanvas cs = TerminalCanvas_sub(&col, 1, 1, 2, 1);
    assert(TerminalCanvas_fgAt(&cs, 0, 0) == TERMINAL_RGB(10, 20, 30));

    // Row projection + truncation.
    TerminalCanvasTextRow row = TerminalCanvas_copyRow(&c, 2);
    assert(!row.truncated && row.data[5] == '-');
    static u32 wideRowCells[1100];
    TerminalCanvas wideRowCanvas = TerminalCanvas(wideRowCells, nullptr, 1100u, 1u);
    TerminalCanvas_fill(&wideRowCanvas, 'x', TerminalPen());
    TerminalCanvasTextRow boundedRow = TerminalCanvas_copyRow(&wideRowCanvas, 0u);
    assert(boundedRow.truncated && boundedRow.bytes == sizeof boundedRow.data - 1u &&
           boundedRow.data[boundedRow.bytes] == '\0');

    // Null / invalid safety.
    assert(!TerminalCanvas_isValid(nullptr));
    assert(TerminalCanvas_glyphAt(nullptr, 0, 0) == '\0');
    assert(TerminalCanvas_fgAt(nullptr, 0, 0) == TERMINAL_COLOR_DEFAULT);
    assert(!TerminalCanvas_hasColorPlane(nullptr));
    TerminalCanvas_fill(nullptr, 'x', TerminalPen());
    TerminalCanvas_text(nullptr, 0, 0, "x", 1, TerminalPen());
    TerminalCanvas zero = TerminalCanvas_0();
    TerminalCanvas rejected = TerminalCanvas(cells, styles, 0, 4);
    assert(!TerminalCanvas_isValid(&zero) && !TerminalCanvas_isValid(&rejected));
    assert(TerminalCanvas_copyRow(nullptr, 0).bytes == 0);

    // UTF-8: decode, wide-glyph continuation, malformed input, re-encode.
    TerminalCanvasDecodedScalar decoded = TerminalCanvas_utf8Decode("A", 1);
    assert(decoded.bytes == 1 && decoded.codepoint == 'A');
    decoded = TerminalCanvas_utf8Decode("\xC3\xA9", 2);
    assert(decoded.bytes == 2 && decoded.codepoint == 0xE9u);        // é
    decoded = TerminalCanvas_utf8Decode("\xE6\xBC\xA2", 3);
    assert(decoded.bytes == 3 && decoded.codepoint == 0x6F22u);  // 漢 (wide)
    decoded = TerminalCanvas_utf8Decode("\xFF", 1);
    assert(decoded.bytes == 0);                           // malformed
    assert(TerminalCanvas_utf8Decode("", 0).bytes == 0);

    TerminalCanvas utf = TerminalCanvas(cells, styles, W, H);
    TerminalCanvas_fill(&utf, ' ', TerminalPen());
    TerminalCanvas_text(&utf, 0, 0, "\xE6\xBC\xA2", 3, TerminalPen());   // 漢 spans two cells
    assert(TerminalCanvas_glyphAt(&utf, 0, 0) == 0x6F22u);
    assert(TerminalCanvas_glyphAt(&utf, 1, 0) == 0);                     // continuation marker
    TerminalCanvas_text(&utf, 3, 0, "\xC3\xA9!", 3, TerminalPen());      // é ! = two cells
    assert(TerminalCanvas_glyphAt(&utf, 3, 0) == 0xE9u && TerminalCanvas_glyphAt(&utf, 4, 0) == '!');
    TerminalCanvas_text(&utf, 0, 1, "\xFF", 1, TerminalPen());           // malformed -> '?'
    assert(TerminalCanvas_glyphAt(&utf, 0, 1) == '?');
    TerminalCanvasTextRow utfRow = TerminalCanvas_copyRow(&utf, 0);
    assert(!utfRow.truncated);
    assert(strncmp(utfRow.data, "\xE6\xBC\xA2  \xC3\xA9!", 8) == 0); // re-encoded, continuation as space

    TERMINAL_PROJECT(TerminalCanvas, &c);
    return 0;
}
