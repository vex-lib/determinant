// Owner: TerminalFrame — bare region layout. Regions, bands, rails collapse,
// rules, background paint, small-root degradation, projections.

#include "vex.h"
#include "ui/panel/terminal_frame.h"
#include "determinant/test/support.h"

int main() {
    enum { W = 40, H = 10 };
    static u32 cells[W * H];
    static u16 styles[W * H];
    static u32 fg[W * H];
    static u32 bg[W * H];

    TerminalFrame frame = TerminalFrame(10, 10);
    assert(TerminalFrame_getBackground(&frame) == nullptr);
    TerminalFramePair bands = TerminalFrame_getBands(&frame);
    assert(bands.first == TERMINAL_FRAME_HEADER_ROWS_DEFAULT && bands.second == 0);
    assert(TerminalFrame_setBands(&frame, 2, 3));

    TerminalCanvas root = TerminalCanvas(cells, styles, fg, bg, W, H);
    TerminalCanvas_fill(&root, ' ', TerminalPen());

    TerminalFrameLayout layout = TerminalFrame_getLayout(&frame, &root);
    assert(layout.rails && layout.left == 10 && layout.right == 10);

    TerminalCanvas header = TerminalFrame_header(&frame, &root);
    TerminalCanvas left = TerminalFrame_left(&frame, &root);
    TerminalCanvas center = TerminalFrame_center(&frame, &root);
    TerminalCanvas right = TerminalFrame_right(&frame, &root);
    TerminalCanvas footer = TerminalFrame_footer(&frame, &root);
    assert(TerminalCanvas_getHeight(&header) == 2 && TerminalCanvas_getWidth(&header) == W);
    assert(TerminalCanvas_getWidth(&left) == 10 && TerminalCanvas_getHeight(&left) == 5);
    assert(TerminalCanvas_getWidth(&right) == 10 && TerminalCanvas_getHeight(&right) == 5);
    assert(TerminalCanvas_getWidth(&center) == W - 10 - 10 - 2 && TerminalCanvas_getHeight(&center) == 5);
    assert(TerminalCanvas_getWidth(&footer) == W - 10 - 10 - 2 && TerminalCanvas_getHeight(&footer) == 3);

    // Regions are disjoint views over one grid.
    TerminalCanvas_text(&center, 0, 0, "C", 1, TerminalPen());
    assert(TerminalCanvas_glyphAt(&root, 11, 2) == 'C');
    assert(TerminalCanvas_glyphAt(&left, 0, 0) == ' ');

    TerminalFrame_drawRules(&frame, &root, TerminalPen_1(TERMINAL_STYLE_DIM));
    assert(TerminalCanvas_glyphAt(&root, 10, 1) == '+');       // junction: run meets the left rail
    assert(TerminalCanvas_glyphAt(&root, W - 10 - 1, 1) == '+'); // junction: run meets the right rail
    assert(TerminalCanvas_glyphAt(&root, 5, 1) == '-');        // run between the rails
    assert(TerminalCanvas_glyphAt(&root, 10, 2) == '|');       // rail below the junction
    assert(TerminalCanvas_glyphAt(&root, W - 10 - 1, 2) == '|');

    // A striped connector run, and a custom glyph set.
    TerminalRule striped = TerminalRule('-', '|', '.', 4);
    assert(TerminalFrame_setRule(&frame, striped));
    TerminalRule got = TerminalFrame_getRule(&frame);
    assert(got.stripe == '.' && got.stripeEvery == 4);
    TerminalFrame_drawRules(&frame, &root, TerminalPen_1(TERMINAL_STYLE_DIM));
    assert(TerminalCanvas_glyphAt(&root, 3, 1) == '.');        // every 4th cell is the stripe
    assert(TerminalCanvas_glyphAt(&root, 4, 1) == '-');

    // A very narrow root collapses the rails so the centre stays usable.
    static u32 narrow[6 * 6];
    TerminalCanvas small = TerminalCanvas(narrow, nullptr, 6, 6);
    layout = TerminalFrame_getLayout(&frame, &small);
    assert(!layout.rails && layout.left == 0 && layout.right == 0);
    TerminalCanvas centerSmall = TerminalFrame_center(&frame, &small);
    TerminalCanvas leftSmall = TerminalFrame_left(&frame, &small);
    assert(TerminalCanvas_isValid(&centerSmall) && !TerminalCanvas_isValid(&leftSmall));

    // Background paint + null safety.
    static const u8 white[4] = { 255, 255, 255, 255 };
    TerminalImage image = TerminalImage(white, 1, 1);
    TerminalImage_setRamp(&image, "#");
    assert(TerminalFrame_setBackground(&frame, &image));
    TerminalFrame_paintBackground(&frame, &root);
    assert(TerminalCanvas_glyphAt(&root, W - 1, H - 1) == '#');
    assert(TerminalFrame_setBackground(&frame, nullptr));

    TerminalCanvas none = TerminalFrame_center(nullptr, &root);
    assert(!TerminalCanvas_isValid(&none));
    TerminalCanvas noneRoot = TerminalFrame_left(&frame, nullptr);
    assert(!TerminalCanvas_isValid(&noneRoot));
    TerminalFrame_paintBackground(nullptr, &root);
    TerminalFrame_drawRules(nullptr, &root, TerminalPen());
    REJECT(TerminalFrame_setRails(nullptr, 1, 1), "null self");
    REJECT(TerminalFrame_setRule(nullptr, TerminalRule_0()), "null self");
    got = TerminalFrame_getRule(nullptr);
    TerminalRule_draw(nullptr, &root, 0, TerminalPen());
    TerminalRule_draw(&striped, nullptr, 0, TerminalPen());

    // DETACHED box: real corners, its own verticals, nothing joined to a neighbour.
    {
        static u32 bc[6 * 3];
        TerminalCanvas box = TerminalCanvas(bc, nullptr, 6, 3);
        TerminalCanvas_fill(&box, ' ', TerminalPen());
        // Box glyphs set directly: the frame owns the rule, not the palette.
        TerminalRule boxed = TerminalRule_0();
        boxed.horizontal = 0x2500;        // ─
        boxed.vertical = 0x2502;          // │
        boxed.cornerTopLeft = 0x250C;     // ┌
        boxed.cornerTopRight = 0x2510;    // ┐
        boxed.cornerBottomLeft = 0x2514;  // └
        boxed.cornerBottomRight = 0x2518; // ┘
        boxed.cutLeft = 0x2576;           // ╶
        boxed.cutRight = 0x2574;          // ╴
        TerminalRule_drawBox(&boxed, &box, TerminalPen_1(TERMINAL_STYLE_DIM));
        assert(TerminalCanvas_glyphAt(&box, 0, 0) == 0x250C);            // ┌
        assert(TerminalCanvas_glyphAt(&box, 5, 0) == 0x2510);            // ┐
        assert(TerminalCanvas_glyphAt(&box, 0, 2) == 0x2514);            // └
        assert(TerminalCanvas_glyphAt(&box, 5, 2) == 0x2518);            // ┘
        assert(TerminalCanvas_glyphAt(&box, 0, 1) == 0x2502);            // │
        assert(TerminalCanvas_glyphAt(&box, 3, 0) == 0x2500);            // ─
        TerminalRule_drawBox(&boxed, nullptr, TerminalPen());            // no-op
        TerminalRule_drawBox(nullptr, &box, TerminalPen());              // no-op
        // A rule with no corner glyphs falls back to `corner` (the '+ ' default).
        TerminalRule plain = TerminalRule_0();
        TerminalCanvas_fill(&box, ' ', TerminalPen());
        TerminalRule_drawBox(&plain, &box, TerminalPen_1(TERMINAL_STYLE_DIM));
        assert(TerminalCanvas_glyphAt(&box, 0, 0) == '+');
        assert(TerminalCanvas_glyphAt(&box, 5, 2) == '+');
        // A 1-cell region cannot hold a box; it must no-op rather than scribble.
        TerminalCanvas tiny = TerminalCanvas(bc, nullptr, 1, 1);
        TerminalCanvas_fill(&tiny, 'x', TerminalPen());
        TerminalRule_drawBox(&boxed, &tiny, TerminalPen());
        assert(TerminalCanvas_glyphAt(&tiny, 0, 0) == 'x');
    }

    // CUT run: it stops short of both edges and ends in the endpoint glyphs, so a
    // divider never runs into the panel's verticals.
    {
        static u32 cut[10];
        TerminalCanvas line = TerminalCanvas(cut, nullptr, 10, 1);
        TerminalCanvas_fill(&line, ' ', TerminalPen());
        TerminalRule boxed = TerminalRule_0();
        boxed.horizontal = 0x2500;        // ─
        boxed.cutLeft = 0x2576;           // ╶
        boxed.cutRight = 0x2574;          // ╴
        TerminalRule_drawCut(&boxed, &line, 0, 1, TerminalPen_1(TERMINAL_STYLE_DIM));
        assert(TerminalCanvas_glyphAt(&line, 0, 0) == ' ');              // inset, untouched
        assert(TerminalCanvas_glyphAt(&line, 1, 0) == 0x2576);           // ╶ cut end
        assert(TerminalCanvas_glyphAt(&line, 4, 0) == 0x2500);           // ─ run
        assert(TerminalCanvas_glyphAt(&line, 8, 0) == 0x2574);           // ╴ cut end
        assert(TerminalCanvas_glyphAt(&line, 9, 0) == ' ');
        TerminalRule_drawCut(&boxed, &line, 0, 5, TerminalPen());        // too narrow: no-op
        assert(TerminalCanvas_glyphAt(&line, 1, 0) == 0x2576);            // unchanged
        // With no cut glyphs the run simply stops short, leaving blank ends.
        TerminalRule plain = TerminalRule_0();
        TerminalCanvas_fill(&line, ' ', TerminalPen());
        TerminalRule_drawCut(&plain, &line, 0, 1, TerminalPen_1(TERMINAL_STYLE_DIM));
        assert(TerminalCanvas_glyphAt(&line, 0, 0) == ' ');
        assert(TerminalCanvas_glyphAt(&line, 1, 0) == '-');
        assert(TerminalCanvas_glyphAt(&line, 8, 0) == '-');
        assert(TerminalCanvas_glyphAt(&line, 9, 0) == ' ');
        TerminalRule_drawCut(&boxed, nullptr, 0, 1, TerminalPen());
        TerminalRule_drawCut(nullptr, &line, 0, 1, TerminalPen());
    }
    (fn) TerminalFrame_getRails(nullptr);
    (fn) TerminalFrame_getBands(nullptr);

    TERMINAL_PROJECT(TerminalFrame, &frame);
    return 0;
}
