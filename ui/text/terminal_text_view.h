#ifndef DETERMINANT_TERMINAL_TEXT_VIEW_H
#define DETERMINANT_TERMINAL_TEXT_VIEW_H

#include <stdbool.h>
#include <stddef.h>

#include "ui/surface/terminal_canvas.h"
#include "ui/surface/terminal_projection.h"
#include "util/constructor.h"

// ui/text/terminal_text_view.h — a bare, generic text viewport.
//
// Draws borrowed lines top-aligned into a region with a scroll offset. It holds
// no transcript policy, no wrapping and no speaker labels: those are application
// concerns. One line is one row; the caller owns and wraps the lines.

typedef struct TerminalTextView {
    const char *const *lines;   // borrowed lines
    bytes count;
    bytes scroll;              // lines scrolled up from the tail
    bool tail;                  // keep the newest line visible (default true)
    TerminalPen pen;
} TerminalTextView;
typedef struct TerminalTextViewLines { const char *const *data; bytes count; } TerminalTextViewLines;

TerminalTextView TerminalTextView_0();
TerminalTextView TerminalTextView_2(const char *const *lines, bytes count);
#define TerminalTextView(...) CONSTRUCTOR_DISPATCH(TerminalTextView, __VA_ARGS__)
#define TerminalTextView_zero() TerminalTextView_0()

bool TerminalTextView_setLines(TerminalTextView *self, const char *const *lines, bytes count);
TerminalTextViewLines TerminalTextView_getLines(const TerminalTextView *self);
bool TerminalTextView_setScroll(TerminalTextView *self, bytes scroll);
bytes TerminalTextView_getScroll(const TerminalTextView *self);
fn TerminalTextView_setTail(TerminalTextView *self, bool tail);
bool TerminalTextView_isTail(const TerminalTextView *self);
fn TerminalTextView_setPen(TerminalTextView *self, TerminalPen pen);
TerminalPen TerminalTextView_getPen(const TerminalTextView *self);
// Scroll by rows (positive = toward newer at the tail); false at an end.
bool TerminalTextView_scrollBy(TerminalTextView *self, long delta, bytes visibleRows);

fn TerminalTextView_render(const TerminalTextView *self, TerminalCanvas *canvas);

TerminalProjection TerminalTextView_toString(const TerminalTextView *self);
TerminalProjection TerminalTextView_toStringStruct(const TerminalTextView *self);

#endif
