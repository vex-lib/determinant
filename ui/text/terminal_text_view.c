#include "ui/text/terminal_text_view.h"
#include "c23/constructor.h"
#include "exception/throw.h"
#include "annotation/annotation.h"
#include <string.h>
#include <stdio.h>

;;DEFINITION
/** A generic text viewport: borrowed lines drawn top-aligned into a region with
 * a scroll offset that defaults to "stick to the tail". No wrapping, no speaker
 * labels, no transcript policy — those are the application's. Owns no memory. */
;;OVERVIEW
/** ========================================================================
 * CLASS: TerminalTextView (ui/text/terminal_text_view.c)
 * STRUCT FIELDS (Mirroring ui/text/terminal_text_view.h)
 * const char *const *lines  borrowed lines (one row each)
 * bytes count              line count
 * bytes scroll             lines scrolled up from the tail
 * bool tail                 keep the newest line visible (default true)
 * TerminalPen pen           text pen
 * FUNCTION REGISTRY (exports)
 * Constructors: TerminalTextView_0(), TerminalTextView_2(lines, count)
 * Config: set/getLines, set/getScroll, setTail/isTail, set/getPen
 * Navigation: bool TerminalTextView_scrollBy(self, delta, visibleRows)
 * Render: TerminalTextView_render(self, canvas)
 * Projections: TerminalProjection TerminalTextView_toString/toStringStruct(self)
 * CONTRACT: null/invalid canvas no-ops; lines past the region clip; a null line
 * renders blank; render never allocates or logs.
 * ======================================================================== */

TerminalTextView TerminalTextView_0() {
    TerminalTextView view = {0};
    view.tail = true;
    view.pen = TerminalPen_0();
    return view;
}

TerminalTextView TerminalTextView_2(const char *const *lines, bytes count) {
    TerminalTextView view = TerminalTextView_0();
    view.lines = (count != 0) ? lines : nullptr;
    view.count = (lines != nullptr) ? count : 0;
    return view;
}

bool TerminalTextView_setLines(TerminalTextView *self, const char *const *lines, bytes count) {
    if (self == nullptr || (count != 0 && lines == nullptr)) {
        THROW("TerminalTextView lines: null list");
        return false;
    }
    (*self).lines = lines;
    (*self).count = count;
    return true;
}
TerminalTextViewLines TerminalTextView_getLines(const TerminalTextView *self) {
    return self ? (TerminalTextViewLines) { (*self).lines, (*self).count }
                : (TerminalTextViewLines) { nullptr, 0u };
}
bool TerminalTextView_setScroll(TerminalTextView *self, bytes scroll) {
    if (self == nullptr) { THROW("TerminalTextView scroll: null"); return false; }
    (*self).scroll = scroll;
    return true;
}
bytes TerminalTextView_getScroll(const TerminalTextView *self) { return self ? (*self).scroll : 0; }
fn TerminalTextView_setTail(TerminalTextView *self, bool tail) { if (self != nullptr) (*self).tail = tail; }
bool TerminalTextView_isTail(const TerminalTextView *self) { return self && (*self).tail; }
fn TerminalTextView_setPen(TerminalTextView *self, TerminalPen pen) { if (self != nullptr) (*self).pen = pen; }
TerminalPen TerminalTextView_getPen(const TerminalTextView *self) { return self ? (*self).pen : TerminalPen_0(); }

bool TerminalTextView_scrollBy(TerminalTextView *self, long delta, bytes visibleRows) {
    (fn) visibleRows;
    if (self == nullptr)
        return false;
    if (delta > 0) {
        if ((*self).scroll == 0)
            return false;
        bytes step = (bytes) delta;
        (*self).scroll = step < (*self).scroll ? (*self).scroll - step : 0;
        return true;
    }
    if (delta < 0) {
        (*self).scroll += (bytes) -delta;
        return true;
    }
    return false;
}

fn TerminalTextView_render(const TerminalTextView *self, TerminalCanvas *canvas) {
    if (self == nullptr || !TerminalCanvas_isValid(canvas))
        return;
    bytes rows = TerminalCanvas_getHeight(canvas);
    bytes first = 0;
    if ((*self).count > rows)
        first = (*self).count - rows;
    first = (*self).scroll <= first ? first - (*self).scroll : 0;
    for (bytes row = 0; row < rows && first + row < (*self).count; ++row) {
        const char *line = (*self).lines[first + row];
        if (line != nullptr)
            TerminalCanvas_text(canvas, 0, row, line, strlen(line), (*self).pen);
    }
}

TerminalProjection TerminalTextView_toString(const TerminalTextView *self) {
    return TerminalTextView_toStringStruct(self);
}

TerminalProjection TerminalTextView_toStringStruct(const TerminalTextView *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format("TerminalTextView{count=%zu,scroll=%zu,tail=%d}",
                                     (*self).count, (*self).scroll, (*self).tail);
}
