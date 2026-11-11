// Owner: TerminalStatusBar — generic one-row status. Left/right, collision drop.
#include "ui/text/terminal_status_bar.h"
#include "determinant/test/support.h"

int main(void) {
    enum { W = 12, H = 1 };
    static u32 cells[W * H];
    static u32 fg[W * H];
    TerminalCanvas wide = TerminalCanvas(cells, nullptr, fg, nullptr, W, H);
    TerminalCanvas_fill(&wide, ' ', TerminalPen());

    TerminalStatusBar bar = TerminalStatusBar("left");
    assert(strcmp(TerminalStatusBar_getLeft(&bar), "left") == 0);
    assert(TerminalStatusBar_getRight(&bar) == nullptr);
    assert(TerminalStatusBar_setRight(&bar, "[on]"));
    TerminalStatusBar_setPens(&bar, TerminalPen(0, TERMINAL_RGB(1, 1, 1)), TerminalPen(0, TERMINAL_RGB(2, 2, 2)));
    TerminalStatusBar_render(&bar, &wide);
    assert(TerminalCanvas_glyphAt(&wide, 0, 0) == 'l');
    assert(TerminalCanvas_glyphAt(&wide, 8, 0) == '[');        // right string, right-aligned
    assert(TerminalCanvas_fgAt(&wide, 8, 0) == TERMINAL_RGB(2, 2, 2));
    TerminalStatusBarPens pens = TerminalStatusBar_getPens(&bar);
    assert(pens.left.fg == TERMINAL_RGB(1, 1, 1) && pens.right.fg == TERMINAL_RGB(2, 2, 2));

    // Collision: a right string that would touch the left is dropped, not drawn.
    static u32 narrow[8 * 1];
    TerminalCanvas tight = TerminalCanvas(narrow, nullptr, nullptr, nullptr, 8, 1);
    TerminalCanvas_fill(&tight, ' ', TerminalPen());
    assert(TerminalStatusBar_setLeft(&bar, "leftside"));      // 8 wide
    TerminalStatusBar_render(&bar, &tight);
    assert(TerminalCanvas_glyphAt(&tight, 0, 0) == 'l');
    assert(TerminalCanvas_glyphAt(&tight, 7, 0) == 'e');       // no '[on]' overwrite

    // Null / invalid safety.
    TerminalStatusBar_render(nullptr, &wide);
    TerminalStatusBar_render(&bar, nullptr);
    assert(TerminalStatusBar_getLeft(nullptr) == nullptr && TerminalStatusBar_getRight(nullptr) == nullptr);
    TerminalStatusBar_setPens(nullptr, TerminalPen_0(), TerminalPen_0());
    (void) TerminalStatusBar_getPens(nullptr);
    REJECT(TerminalStatusBar_setLeft(nullptr, "x"), "null self");
    REJECT(TerminalStatusBar_setRight(nullptr, "x"), "null self");
    TerminalStatusBar empty = TerminalStatusBar_0();
    TerminalStatusBar_render(&empty, &wide);

    char longStatus[600];
    memset(longStatus, 'x', sizeof longStatus - 1u);
    longStatus[sizeof longStatus - 1u] = '\0';
    TerminalStatusBar_setLeft(&bar, longStatus);
    TerminalProjection boundedProjection = TerminalStatusBar_toString(&bar);
    assert(boundedProjection.truncated && boundedProjection.length == sizeof boundedProjection.text - 1u);
    TerminalStatusBar_setLeft(&bar, "left-status-goes-here");
    TERMINAL_PROJECT(TerminalStatusBar, &bar);
    return 0;
}
