// Owner: TerminalTextView — generic text viewport. Tail stick, scroll, clip.
#include "ui/text/terminal_text_view.h"
#include "determinant/test/support.h"

int main(void) {
    static const char *lines[] = { "one", "two", "three", "four", "five" };
    enum { W = 10, H = 3 };
    static uint32_t cells[W * H];
    static uint32_t fg[W * H];
    TerminalCanvas canvas = TerminalCanvas(cells, nullptr, fg, nullptr, W, H);
    TerminalCanvas_fill(&canvas, ' ', TerminalPen());

    TerminalTextView view = TerminalTextView(lines, 5);
    TerminalTextViewLines viewLines = TerminalTextView_getLines(&view);
    assert(viewLines.data == lines && viewLines.count == 5u);
    assert(TerminalTextView_isTail(&view));
    assert(TerminalTextView_getScroll(&view) == 0);

    // Tail: the newest three lines are shown.
    TerminalTextView_render(&view, &canvas);
    assert(TerminalCanvas_glyphAt(&canvas, 0, 0) == 't');   // "three"
    assert(TerminalCanvas_glyphAt(&canvas, 0, 2) == 'f');   // "five"

    // Scroll up one row: "two" becomes the first visible line.
    assert(TerminalTextView_scrollBy(&view, -1, 3));
    assert(TerminalTextView_getScroll(&view) == 1);
    TerminalTextView_render(&view, &canvas);
    assert(TerminalCanvas_glyphAt(&canvas, 0, 0) == 't' && TerminalCanvas_glyphAt(&canvas, 1, 0) == 'w');
    assert(TerminalCanvas_glyphAt(&canvas, 0, 2) == 'f');

    // Scroll back to the tail is bounded.
    assert(TerminalTextView_scrollBy(&view, 1, 3));
    assert(TerminalTextView_getScroll(&view) == 0);
    assert(!TerminalTextView_scrollBy(&view, 1, 3));

    // Pen colour lands.
    TerminalTextView_setPen(&view, TerminalPen(0, TERMINAL_RGB(9, 8, 7)));
    TerminalTextView_render(&view, &canvas);
    assert(TerminalCanvas_fgAt(&canvas, 0, 0) == TERMINAL_RGB(9, 8, 7));
    TerminalPen p = TerminalTextView_getPen(&view);
    assert(p.fg == TERMINAL_RGB(9, 8, 7));

    // Empty + null safety.
    TerminalTextView empty = TerminalTextView_0();
    TerminalTextView_render(&empty, &canvas);
    TerminalTextView_render(nullptr, &canvas);
    TerminalTextView_render(&view, nullptr);
    assert(TerminalTextView_getScroll(nullptr) == 0 && !TerminalTextView_isTail(nullptr));
    viewLines = TerminalTextView_getLines(nullptr);
    assert(viewLines.data == nullptr && viewLines.count == 0u);
    REJECT(TerminalTextView_setLines(&view, nullptr, 2), "null list");
    REJECT(TerminalTextView_setScroll(nullptr, 1), "null");

    TERMINAL_PROJECT(TerminalTextView, &view);
    return 0;
}
