// Owner: TerminalListView — generic list. Marker, selection, scroll, navigation,
// pens, clipping, projections.
#include "ui/list/terminal_list_view.h"
#include "determinant/test/support.h"

int main(void) {
    static const char *labels[] = { "alpha", "beta", "gamma", "delta" };
    enum { W = 16, H = 3 };
    static uint32_t cells[W * H];
    static uint32_t fg[W * H];
    TerminalCanvas canvas = TerminalCanvas(cells, nullptr, fg, nullptr, W, H);
    TerminalCanvas_fill(&canvas, ' ', TerminalPen());

    TerminalListView view = TerminalListView(labels, 4);
    TerminalListViewLabels viewLabels = TerminalListView_getLabels(&view);
    assert(viewLabels.data == labels && viewLabels.count == 4u);
    assert(strcmp(TerminalListView_getMarker(&view), "> ") == 0);
    assert(TerminalListView_getSelection(&view) == 0 && TerminalListView_getScroll(&view) == 0);

    TerminalListView_render(&view, &canvas);
    assert(TerminalCanvas_glyphAt(&canvas, 0, 0) == '>' && TerminalCanvas_glyphAt(&canvas, 2, 0) == 'a');
    assert(TerminalCanvas_glyphAt(&canvas, 0, 1) == ' ' && TerminalCanvas_glyphAt(&canvas, 2, 1) == 'b');

    // Selection moves and keeps visible; false at an edge.
    assert(TerminalListView_move(&view, 1, 3));
    assert(TerminalListView_getSelection(&view) == 1);
    assert(TerminalListView_move(&view, 1, 3));
    assert(TerminalListView_getSelection(&view) == 2);
    assert(TerminalListView_getSelection(&view) == 2 && TerminalListView_getScroll(&view) == 0);
    assert(TerminalListView_move(&view, 1, 3));
    assert(TerminalListView_getSelection(&view) == 3 && TerminalListView_getScroll(&view) == 1);
    assert(!TerminalListView_move(&view, 1, 3));      // already at the last row
    assert(TerminalListView_move(&view, -3, 3));
    assert(TerminalListView_getSelection(&view) == 0 && TerminalListView_getScroll(&view) == 0);
    assert(!TerminalListView_move(&view, -1, 3));     // already at the first row

    // Pens: colour lands on the selected row.
    TerminalListView_setPens(&view, TerminalPen(0, TERMINAL_RGB(1, 1, 1)),
                             TerminalPen(TERMINAL_STYLE_REVERSE, TERMINAL_RGB(2, 2, 2)));
    TerminalListView_render(&view, &canvas);
    assert(TerminalCanvas_fgAt(&canvas, 2, 0) == TERMINAL_RGB(2, 2, 2));
    assert(TerminalCanvas_styleAt(&canvas, 2, 0) == 0);   // styles plane absent here
    TerminalListViewPens pens = TerminalListView_getPens(&view);
    assert(pens.normal.fg == TERMINAL_RGB(1, 1, 1) && pens.selected.fg == TERMINAL_RGB(2, 2, 2));

    // Empty / overflow / null safety.
    TerminalListView empty = TerminalListView_0();
    viewLabels = TerminalListView_getLabels(nullptr);
    assert(viewLabels.data == nullptr && viewLabels.count == 0u);
    TerminalListView_render(&empty, &canvas);
    assert(!TerminalListView_move(&empty, 1, 3));
    TerminalListView_render(nullptr, &canvas);
    TerminalListView_render(&view, nullptr);
    assert(TerminalListView_getSelection(nullptr) == 0);
    assert(strcmp(TerminalListView_getMarker(nullptr), "> ") == 0);
    REJECT(TerminalListView_setLabels(&view, nullptr, 3), "null list");
    REJECT(TerminalListView_setSelection(nullptr, 1), "null");

    TERMINAL_PROJECT(TerminalListView, &view);
    return 0;
}
