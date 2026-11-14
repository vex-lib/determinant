// Owner: TerminalText — the addressable leaf. Constructors and arity, borrowed
// glyph admission, pen/style/colour round trips, brackets, alignment, cell
// measurement (wide glyphs count two), paint with alignment and clipping, pen
// inheritance from the parent, and the projections.
//
// Scope note: single-line by contract, so multi-row behaviour is not claimed.
// No negative-arity case here (the runner owns the shared arity battery).

#include "vex.h"
#include "ui/text/terminal_text.h"
#include "format/terminal_color.h"
#include "determinant/test/support.h"

enum { W = 12, H = 1 };

int main() {
    u32 cells[W * H];
    u16 styles[W * H];
    u32 fg[W * H];
    u32 bg[W * H];
    TerminalCanvas canvas = TerminalCanvas(cells, styles, fg, bg, W, H);

    // --- Constructors: zero / one / two, and the chooser ---
    TerminalText *empty = TerminalText_0();
    assert(empty != nullptr);
    assert(TerminalText_getLength(empty) == 0 && TerminalText_measure(empty) == 0);
    assert(TerminalText_getAlign(empty) == TERMINAL_TEXT_ALIGN_LEFT);
    assert(TerminalText_getStyle(empty) == TERMINAL_STYLE_NONE);
    assert(TerminalText_getForegroundColor(empty) == TERMINAL_COLOR_DEFAULT);
    assert(TerminalText_getBracketLeft(empty) == 0 && TerminalText_getBracketRight(empty) == 0);
    TerminalText *zeroed = TerminalText_zero();
    assert(zeroed != nullptr && TerminalText_getLength(zeroed) == 0);
    TerminalText_free(zeroed);
    TerminalText *one = TerminalText("abc");
    assert(one != nullptr && TerminalText_getLength(one) == 3);
    assert(strcmp(TerminalText_getText(one), "abc") == 0);
    TerminalText *two = TerminalText("abcdef", 3);      // an explicit prefix
    assert(two != nullptr && TerminalText_getLength(two) == 3);
    assert(strncmp(TerminalText_getText(two), "abc", 3) == 0);
    REJECT(TerminalText_2(nullptr, 5) != nullptr, "glyph span");
    assert(TerminalText_2(nullptr, 0) != nullptr);      // a null span with no length is empty

    // --- Text, pen and colours round-trip; rejection preserves state ---
    assert(TerminalText_setText(two, "xy", 2));
    assert(TerminalText_getLength(two) == 2 && strncmp(TerminalText_getText(two), "xy", 2) == 0);
    assert(TerminalText_setStyle(two, TERMINAL_STYLE_BOLD));
    assert(TerminalText_getStyle(two) == TERMINAL_STYLE_BOLD);
    assert(TerminalText_setForegroundColor(two, TERMINAL_RGB(1, 2, 3)));
    assert(TerminalText_getForegroundColor(two) == TERMINAL_RGB(1, 2, 3));
    assert(TerminalText_setBackgroundColor(two, TERMINAL_RGB(4, 5, 6)));
    assert(TerminalText_getBackgroundColor(two) == TERMINAL_RGB(4, 5, 6));
    TerminalPen pen = TerminalPen(TERMINAL_STYLE_ACCENT, TERMINAL_RGB(7, 8, 9), TERMINAL_RGB(10, 11, 12));
    assert(TerminalText_setPen(two, pen));
    TerminalPen readBack = TerminalText_getPen(two);
    assert(readBack.style == TERMINAL_STYLE_ACCENT && readBack.fg == TERMINAL_RGB(7, 8, 9));
    assert(readBack.bg == TERMINAL_RGB(10, 11, 12));
    REJECT(TerminalText_setText(nullptr, "x", 1), "null text");
    REJECT(TerminalText_setStyle(nullptr, 0), "style");
    REJECT(TerminalText_setForegroundColor(nullptr, 0), "foreground");
    REJECT(TerminalText_setBackgroundColor(nullptr, 0), "background");
    REJECT(TerminalText_setPen(nullptr, pen), "pen");
    assert(TerminalText_getStyle(two) == TERMINAL_STYLE_ACCENT);   // preserved

    // --- Brackets are codepoints, so real bracket glyphs work ---
    assert(TerminalText_setBrackets(two, '[', ']'));
    assert(TerminalText_getBracketLeft(two) == '[' && TerminalText_getBracketRight(two) == ']');
    assert(TerminalText_measure(two) == 4);                         // [ + xy + ]
    assert(TerminalText_setBrackets(two, 0x3010u, 0x3011u));        // 【 】
    assert(TerminalText_getBracketLeft(two) == 0x3010u);
    assert(TerminalText_setBrackets(two, 0, 0));
    assert(TerminalText_measure(two) == 2);
    REJECT(TerminalText_setBrackets(nullptr, 0, 0), "brackets");

    // --- Alignment: round trip, unknown rejected, defaults safe ---
    assert(TerminalText_setAlign(two, TERMINAL_TEXT_ALIGN_CENTER));
    assert(TerminalText_getAlign(two) == TERMINAL_TEXT_ALIGN_CENTER);
    REJECT(TerminalText_setAlign(two, (TerminalTextAlign) 7), "align");
    assert(TerminalText_getAlign(two) == TERMINAL_TEXT_ALIGN_CENTER);
    REJECT(TerminalText_setAlign(nullptr, TERMINAL_TEXT_ALIGN_LEFT), "align");
    assert(TerminalText_getAlign(nullptr) == TERMINAL_TEXT_ALIGN_LEFT);

    // --- Measurement: cells, not bytes ---
    TerminalText *wide = TerminalText("\xE6\xBC\xA2");              // 漢
    assert(wide != nullptr && TerminalText_measure(wide) == 2);      // one wide glyph, two cells
    assert(TerminalText_getLength(wide) == 3);                      // ...three bytes
    TerminalText *broken = TerminalText("\xFF\xFF");
    assert(broken != nullptr && TerminalText_measure(broken) == 2);  // malformed bytes count one cell each
    assert(TerminalText_measure(empty) == 0);
    assert(TerminalText_measure(nullptr) == 0);

    // --- Paint: alignment, brackets, clipping ---
    TerminalCanvas_fill(&canvas, ' ', TerminalPen());
    assert(TerminalText_setAlign(one, TERMINAL_TEXT_ALIGN_LEFT));
    TerminalText_paint(one, &canvas, TerminalPen());
    assert(TerminalCanvas_glyphAt(&canvas, 0, 0) == 'a');
    assert(TerminalCanvas_glyphAt(&canvas, 1, 0) == 'b');
    assert(TerminalCanvas_glyphAt(&canvas, 2, 0) == 'c');
    assert(TerminalCanvas_glyphAt(&canvas, 3, 0) == ' ');

    TerminalCanvas_fill(&canvas, ' ', TerminalPen());
    assert(TerminalText_setAlign(one, TERMINAL_TEXT_ALIGN_CENTER));
    TerminalText_paint(one, &canvas, TerminalPen());
    assert(TerminalCanvas_glyphAt(&canvas, 4, 0) == 'a');           // (12-3)/2 = 4

    TerminalCanvas_fill(&canvas, ' ', TerminalPen());
    assert(TerminalText_setAlign(one, TERMINAL_TEXT_ALIGN_RIGHT));
    TerminalText_paint(one, &canvas, TerminalPen());
    assert(TerminalCanvas_glyphAt(&canvas, 9, 0) == 'a');           // 12-3 = 9
    assert(TerminalCanvas_glyphAt(&canvas, 11, 0) == 'c');

    TerminalCanvas_fill(&canvas, ' ', TerminalPen());
    assert(TerminalText_setAlign(one, TERMINAL_TEXT_ALIGN_LEFT));
    assert(TerminalText_setBrackets(one, '[', ']'));
    TerminalText_paint(one, &canvas, TerminalPen());
    assert(TerminalCanvas_glyphAt(&canvas, 0, 0) == '[');
    assert(TerminalCanvas_glyphAt(&canvas, 4, 0) == ']');

    // A text wider than its region is cut at the edge, not wrapped or faulted.
    TerminalCanvas narrow = TerminalCanvas_sub(&canvas, 0, 0, 3, 1);
    TerminalCanvas_fill(&canvas, ' ', TerminalPen());
    assert(TerminalText_setBrackets(one, 0, 0));
    TerminalCanvas_text(&narrow, 0, 0, "abcdef", 6, TerminalPen());
    assert(TerminalCanvas_glyphAt(&canvas, 2, 0) == 'c');
    assert(TerminalCanvas_glyphAt(&canvas, 3, 0) == ' ');           // clipped, not spilled

    // A wide glyph marks its continuation cell, exactly as the canvas does.
    TerminalCanvas_fill(&canvas, ' ', TerminalPen());
    TerminalText_paint(wide, &canvas, TerminalPen());
    assert(TerminalCanvas_glyphAt(&canvas, 0, 0) == 0x6F22u);
    assert(TerminalCanvas_glyphAt(&canvas, 1, 0) == 0u);

    // --- Inheritance: the parent's pen fills what the text leaves unset ---
    TerminalCanvas_fill(&canvas, ' ', TerminalPen());
    TerminalPen parent = TerminalPen(TERMINAL_STYLE_DIM, TERMINAL_RGB(0xAA, 0xBB, 0xCC), TERMINAL_COLOR_DEFAULT);
    assert(TerminalText_setPen(one, TerminalPen_0()));              // the text sets nothing
    TerminalText_paint(one, &canvas, parent);
    assert(TerminalCanvas_fgAt(&canvas, 0, 0) == TERMINAL_RGB(0xAA, 0xBB, 0xCC));
    assert(TerminalCanvas_styleAt(&canvas, 0, 0) == TERMINAL_STYLE_DIM);
    // The text's own colour wins, and styles combine.
    TerminalPen own = TerminalPen(TERMINAL_STYLE_BOLD, TERMINAL_RGB(0x11, 0x22, 0x33), TERMINAL_COLOR_DEFAULT);
    TerminalCanvas_fill(&canvas, ' ', TerminalPen());
    assert(TerminalText_setPen(one, own));
    TerminalText_paint(one, &canvas, parent);
    assert(TerminalCanvas_fgAt(&canvas, 0, 0) == TERMINAL_RGB(0x11, 0x22, 0x33));
    assert(TerminalCanvas_styleAt(&canvas, 0, 0) == (TERMINAL_STYLE_BOLD | TERMINAL_STYLE_DIM));

    // --- Null safety across the public surface ---
    beginDiagnostics();
    TerminalText_paint(nullptr, &canvas, TerminalPen());
    TerminalText_paint(one, nullptr, TerminalPen());
    TerminalCanvas invalid = TerminalCanvas_0();
    TerminalText_paint(one, &invalid, TerminalPen());
    endDiagnostics(0, nullptr);                                     // paint is quiet, never loud
    assert(TerminalText_getText(nullptr) == nullptr);
    assert(TerminalText_getLength(nullptr) == 0);
    assert(TerminalText_getStyle(nullptr) == TERMINAL_STYLE_NONE);
    assert(TerminalText_getForegroundColor(nullptr) == TERMINAL_COLOR_DEFAULT);
    assert(TerminalText_getBackgroundColor(nullptr) == TERMINAL_COLOR_DEFAULT);
    assert(TerminalText_getBracketLeft(nullptr) == 0 && TerminalText_getBracketRight(nullptr) == 0);
    TerminalPen nullPen = TerminalText_getPen(nullptr);
    assert(nullPen.style == TERMINAL_STYLE_NONE && nullPen.fg == TERMINAL_COLOR_DEFAULT);
    TerminalText_free(nullptr);                                     // harmless

    // --- Projections: value string and a struct dump mirroring STRUCT FIELDS ---
    TERMINAL_PROJECT(TerminalText, one);
    TerminalProjection structured = TerminalText_toStringStruct(one);
    assert(strstr(structured.text, "glyphs=<borrowed:") != nullptr);
    assert(strstr(structured.text, "bracketLeft=") != nullptr);
    assert(strstr(structured.text, "align=") != nullptr);
    assert(strstr(TerminalText_toString(one).text, "TerminalText{cells=") != nullptr);

    TerminalText_free(empty);
    TerminalText_free(one);
    TerminalText_free(two);
    TerminalText_free(wide);
    TerminalText_free(broken);
    return 0;
}
