#include "vex.h"
#include "ui/text/terminal_text_view.h"
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


;;GETTER
static inline bytes TerminalTextView_count(const TerminalTextView *s) { return (*s).count; }
;;GETTER
static inline bytes *TerminalTextView_countMut(TerminalTextView *s) { return &(*s).count; }
;;SETTER
static inline fn TerminalTextView_setCount(TerminalTextView *s, bytes v) { (*s).count = v; }
;;GETTER
static inline const char *const * TerminalTextView_lines(const TerminalTextView *s) { return (*s).lines; }
;;GETTER
static inline const char *const * *TerminalTextView_linesMut(TerminalTextView *s) { return &(*s).lines; }
;;SETTER
static inline fn TerminalTextView_setLines(TerminalTextView *s, const char *const * v) { (*s).lines = v; }
;;GETTER
static inline TerminalPen TerminalTextView_pen(const TerminalTextView *s) { return (*s).pen; }
;;GETTER
static inline TerminalPen *TerminalTextView_penMut(TerminalTextView *s) { return &(*s).pen; }
;;SETTER
static inline fn TerminalTextView_setPen(TerminalTextView *s, TerminalPen v) { (*s).pen = v; }
;;GETTER
static inline bytes TerminalTextView_scroll(const TerminalTextView *s) { return (*s).scroll; }
;;GETTER
static inline bytes *TerminalTextView_scrollMut(TerminalTextView *s) { return &(*s).scroll; }
;;SETTER
static inline fn TerminalTextView_setScroll(TerminalTextView *s, bytes v) { (*s).scroll = v; }
;;GETTER
static inline bool TerminalTextView_tail(const TerminalTextView *s) { return (*s).tail; }
;;GETTER
static inline bool *TerminalTextView_tailMut(TerminalTextView *s) { return &(*s).tail; }
;;SETTER
static inline fn TerminalTextView_setTail(TerminalTextView *s, bool v) { (*s).tail = v; }

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
    TerminalTextView_setLines(self, lines);
    TerminalTextView_setCount(self, count);
    return true;
}
TerminalTextViewLines TerminalTextView_getLines(const TerminalTextView *self) {
    return self ? (TerminalTextViewLines) { TerminalTextView_lines(self), TerminalTextView_count(self) }
                : (TerminalTextViewLines) { nullptr, 0u };
}
bool TerminalTextView_setScroll(TerminalTextView *self, bytes scroll) {
    if (self == nullptr) { THROW("TerminalTextView scroll: null"); return false; }
    TerminalTextView_setScroll(self, scroll);
    return true;
}
bytes TerminalTextView_getScroll(const TerminalTextView *self) { return self ? TerminalTextView_scroll(self) : 0; }
fn TerminalTextView_setTail(TerminalTextView *self, bool tail) { if (self != nullptr) (*TerminalTextView_tailMut(self)) = tail; }
bool TerminalTextView_isTail(const TerminalTextView *self) { return self && TerminalTextView_tail(self); }
fn TerminalTextView_setPen(TerminalTextView *self, TerminalPen pen) { if (self != nullptr) (*TerminalTextView_penMut(self)) = pen; }
TerminalPen TerminalTextView_getPen(const TerminalTextView *self) { return self ? TerminalTextView_pen(self) : TerminalPen_0(); }

bool TerminalTextView_scrollBy(TerminalTextView *self, i64 delta, bytes visibleRows) {
    (discard) visibleRows;
    if (self == nullptr)
        return false;
    if (delta > 0) {
        if (TerminalTextView_scroll(self) == 0)
            return false;
        bytes step = (bytes) delta;
        TerminalTextView_setScroll(self, step < TerminalTextView_scroll(self) ? TerminalTextView_scroll(self) - step : 0);
        return true;
    }
    if (delta < 0) {
        TerminalTextView_setScroll(self, TerminalTextView_scroll(self) + ((bytes) -delta));
        return true;
    }
    return false;
}

fn TerminalTextView_render(const TerminalTextView *self, TerminalCanvas *canvas) {
    if (self == nullptr || !TerminalCanvas_isValid(canvas))
        return;
    bytes rows = TerminalCanvas_getHeight(canvas);
    bytes first = 0;
    if (TerminalTextView_count(self) > rows)
        first = TerminalTextView_count(self) - rows;
    first = TerminalTextView_scroll(self) <= first ? first - TerminalTextView_scroll(self) : 0;
    for (bytes row = 0; row < rows && first + row < TerminalTextView_count(self); ++row) {
        const char *line = TerminalTextView_lines(self)[first + row];
        if (line != nullptr)
            TerminalCanvas_text(canvas, 0, row, line, strlen(line), TerminalTextView_pen(self));
    }
}

TerminalProjection TerminalTextView_toString(const TerminalTextView *self) {
    return TerminalTextView_toStringStruct(self);
}

TerminalProjection TerminalTextView_toStringStruct(const TerminalTextView *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format("TerminalTextView{count=%zu,scroll=%zu,tail=%d}",
                                     TerminalTextView_count(self), TerminalTextView_scroll(self), TerminalTextView_tail(self));
}
