#ifndef DETERMINANT_TERMINAL_SCROLL_GLIDE_H
#define DETERMINANT_TERMINAL_SCROLL_GLIDE_H

#include <stdbool.h>
#include <stddef.h>

#include "ui/surface/terminal_projection.h"
#include "util/constructor.h"

// ui/scroll/terminal_scroll_glide.h — per-axis viewport momentum.
//
// A scroll is two positions (row, column): where the view is shown, where it is
// heading, and the easing between them. Input moves the target; each frame covers
// a fraction of the remaining distance and snaps once the rest is imperceptible.
// The class owns no cells and paints nothing: a view renders at the returned
// whole-cell position. That is the half of a scroll panel that is independent of
// what is being scrolled.

#define TERMINAL_SCROLL_EASE_DEFAULT 0.18
#define TERMINAL_SCROLL_EPSILON_DEFAULT 0.4

typedef struct TerminalScrollGlide {
    d64 rowShown;      // rows currently displayed, fractional
    d64 rowTarget;     // rows headed for
    d64 columnShown;   // columns currently displayed, fractional
    d64 columnTarget;  // columns headed for
    d64 ease;          // fraction of remaining distance covered per step
    d64 epsilon;       // remaining distance that snaps to the target
} TerminalScrollGlide;

// SLOT RECORD: TerminalScrollGlidePosition (no behaviour of its own)
typedef struct TerminalScrollGlidePosition {
    bytes row;
    bytes column;
} TerminalScrollGlidePosition;

TerminalScrollGlide TerminalScrollGlide_0();
TerminalScrollGlide TerminalScrollGlide_2(d64 ease, d64 epsilon);
#define TerminalScrollGlide(...) CONSTRUCTOR_DISPATCH(TerminalScrollGlide, __VA_ARGS__)
#define TerminalScrollGlide_zero() TerminalScrollGlide_0()

bool TerminalScrollGlide_setEase(TerminalScrollGlide *self, d64 ease);
d64 TerminalScrollGlide_getEase(const TerminalScrollGlide *self);
bool TerminalScrollGlide_setEpsilon(TerminalScrollGlide *self, d64 epsilon);
d64 TerminalScrollGlide_getEpsilon(const TerminalScrollGlide *self);

// Still travelling? Drives the frame cadence: true while either axis is farther
// than epsilon from its target.
bool TerminalScrollGlide_isActive(const TerminalScrollGlide *self);

// Input moves only the target. Both bounds are inclusive maxima; the target is
// clamped into [0, max] so the glide cannot aim past a bound.
fn TerminalScrollGlide_impulse(TerminalScrollGlide *self, long rows, long columns,
                                 bytes maxRows, bytes maxColumns);
fn TerminalScrollGlide_setTarget(TerminalScrollGlide *self, bytes row, bytes column,
                                   bytes maxRows, bytes maxColumns);

// Snap one or both axes: both shown and target land on the supplied position(s).
fn TerminalScrollGlide_setPosition(TerminalScrollGlide *self, bytes row, bytes column,
                                     bytes maxRows, bytes maxColumns);
fn TerminalScrollGlide_setRowPosition(TerminalScrollGlide *self, bytes row, bytes maxRows);
fn TerminalScrollGlide_setColumnPosition(TerminalScrollGlide *self, bytes column,
                                           bytes maxColumns);

// Stop where we are: drop the target onto the shown position.
fn TerminalScrollGlide_halt(TerminalScrollGlide *self);
// Clamp both shown and target after a layout or extent change.
fn TerminalScrollGlide_clamp(TerminalScrollGlide *self, bytes maxRows, bytes maxColumns);

// Advance one frame and return the whole-cell position to paint. Call once per
// frame; a settled glide returns the same position and costs two comparisons.
TerminalScrollGlidePosition TerminalScrollGlide_step(TerminalScrollGlide *self);

d64 TerminalScrollGlide_getRowShown(const TerminalScrollGlide *self);
d64 TerminalScrollGlide_getRowTarget(const TerminalScrollGlide *self);
d64 TerminalScrollGlide_getColumnShown(const TerminalScrollGlide *self);
d64 TerminalScrollGlide_getColumnTarget(const TerminalScrollGlide *self);
TerminalScrollGlidePosition TerminalScrollGlide_getPosition(const TerminalScrollGlide *self);

TerminalProjection TerminalScrollGlide_toString(const TerminalScrollGlide *self);
TerminalProjection TerminalScrollGlide_toStringStruct(const TerminalScrollGlide *self);

#endif
