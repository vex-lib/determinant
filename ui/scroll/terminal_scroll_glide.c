#include "vex.h"
#include "ui/scroll/terminal_scroll_glide.h"
#include <math.h>

;;DEFINITION
/** A scroll with MOMENTUM, per axis: a position (`rowShown`/`columnShown`), where
 * it is heading (`rowTarget`/`columnTarget`), and the easing between them.
 *
 * A queue that is spent a fixed step at a time is a conveyor belt - it runs at
 * constant speed until it empties, so there is no friction and nothing to stop.
 * This instead covers a fraction of the remaining distance every frame, which
 * decelerates the closer it gets, and `TerminalScrollGlide_halt` is what a press
 * does: it drops the target onto the position and the motion is over at once.
 *
 * The class owns no cells and paints nothing. A view renders at the whole-cell
 * position `TerminalScrollGlide_step` returns; the panel that clips cells and
 * draws them is a separate owner. Keeping the motion here lets every terminal
 * view share the same feel without each one inventing its own easing. */
;;OVERVIEW
/** ========================================================================
 * CLASS: TerminalScrollGlide (ui/scroll/terminal_scroll_glide.c)
 * STRUCT FIELDS (Mirroring ui/scroll/terminal_scroll_glide.h)
 * d64 rowShown       rows currently displayed, fractional
 * d64 rowTarget      rows headed for
 * d64 columnShown    columns currently displayed, fractional
 * d64 columnTarget   columns headed for
 * d64 ease           fraction of remaining distance covered per step
 * d64 epsilon        remaining distance that snaps to the target
 * SLOT RECORD: TerminalScrollGlidePosition
 * bytes row            whole-cell row to paint
 * bytes column         whole-cell column to paint
 * FUNCTION REGISTRY (exports)
 * Constructors: TerminalScrollGlide_0(), _2(ease, epsilon)
 * Config: set/getEase, set/getEpsilon
 * Motion: isActive, impulse, setTarget, setPosition, setRowPosition,
 *   setColumnPosition, halt, clamp, step
 * Getters: getRowShown, getRowTarget, getColumnShown, getColumnTarget,
 *   getPosition
 * Projections: toString, toStringStruct
 * PRIVATE HELPERS
 * static d64 clampAxis(value, maximum)   clamp one axis into [0, maximum]
 * static d64 gap(shown, target)           absolute remaining distance
 * static bytes roundAxis(value)             nearest whole cell
 * static d64 stepAxis(shown, target, ease, epsilon)
 *   one axis of one frame; returns the new shown position
 * CONTRACT: null mutators no-op and null getters return safe defaults. The cold
 * setters/constructors reject invalid ease (must be in (0,1)) and epsilon
 * (must be positive and finite) with one THROW and preserve prior state. step is
 * the frame path: one null guard, no logging, no allocation; it clamps nothing a
 * validated caller already clamped. Positions are never negative, and a saturated
 * d64 bound converts to SIZE_MAX rather than making an out-of-range cast.
 * ======================================================================== */


;;GETTER
static inline d64 TerminalScrollGlide_columnShown(const TerminalScrollGlide *s) { return (*s).columnShown; }
;;GETTER
static inline d64 *TerminalScrollGlide_columnShownMut(TerminalScrollGlide *s) { return &(*s).columnShown; }
;;SETTER
static inline fn TerminalScrollGlide_setColumnShown(TerminalScrollGlide *s, d64 v) { (*s).columnShown = v; }
;;GETTER
static inline d64 TerminalScrollGlide_columnTarget(const TerminalScrollGlide *s) { return (*s).columnTarget; }
;;GETTER
static inline d64 *TerminalScrollGlide_columnTargetMut(TerminalScrollGlide *s) { return &(*s).columnTarget; }
;;SETTER
static inline fn TerminalScrollGlide_setColumnTarget(TerminalScrollGlide *s, d64 v) { (*s).columnTarget = v; }
;;GETTER
static inline d64 TerminalScrollGlide_ease(const TerminalScrollGlide *s) { return (*s).ease; }
;;GETTER
static inline d64 *TerminalScrollGlide_easeMut(TerminalScrollGlide *s) { return &(*s).ease; }
;;SETTER
static inline fn TerminalScrollGlide_setEase(TerminalScrollGlide *s, d64 v) { (*s).ease = v; }
;;GETTER
static inline d64 TerminalScrollGlide_epsilon(const TerminalScrollGlide *s) { return (*s).epsilon; }
;;GETTER
static inline d64 *TerminalScrollGlide_epsilonMut(TerminalScrollGlide *s) { return &(*s).epsilon; }
;;SETTER
static inline fn TerminalScrollGlide_setEpsilon(TerminalScrollGlide *s, d64 v) { (*s).epsilon = v; }
;;GETTER
static inline d64 TerminalScrollGlide_rowShown(const TerminalScrollGlide *s) { return (*s).rowShown; }
;;GETTER
static inline d64 *TerminalScrollGlide_rowShownMut(TerminalScrollGlide *s) { return &(*s).rowShown; }
;;SETTER
static inline fn TerminalScrollGlide_setRowShown(TerminalScrollGlide *s, d64 v) { (*s).rowShown = v; }
;;GETTER
static inline d64 TerminalScrollGlide_rowTarget(const TerminalScrollGlide *s) { return (*s).rowTarget; }
;;GETTER
static inline d64 *TerminalScrollGlide_rowTargetMut(TerminalScrollGlide *s) { return &(*s).rowTarget; }
;;SETTER
static inline fn TerminalScrollGlide_setRowTarget(TerminalScrollGlide *s, d64 v) { (*s).rowTarget = v; }

static d64 clampAxis(d64 value, bytes maximum) {
    d64 top = (d64) maximum;
    if (value < 0.0)
        return 0.0;
    if (value > top)
        return top;
    return value;
}

static d64 gap(d64 shown, d64 target) {
    d64 difference = target - shown;
    return difference < 0.0 ? -difference : difference;
}

static bytes roundAxis(d64 value) {
    if (!(value > 0.0))
        return 0u;
    // (d64) SIZE_MAX rounds up to 2^64, so a value at or beyond it would make
    // the cast below out of range. Clamp before converting; the public bounds are
    // already bytes, so this only catches a saturated d64, never a real cell.
    if (value >= (d64) SIZE_MAX)
        return SIZE_MAX;
    return (bytes) (value + 0.5);
}

static d64 stepAxis(d64 *shown, d64 target, d64 ease, d64 epsilon) {
    if (gap(*shown, target) < epsilon) {
        *shown = target;
        return target;
    }
    *shown += (target - *shown) * ease;
    return *shown;
}

TerminalScrollGlide TerminalScrollGlide_0() {
    TerminalScrollGlide glide = {0};
    glide.ease = TERMINAL_SCROLL_EASE_DEFAULT;
    glide.epsilon = TERMINAL_SCROLL_EPSILON_DEFAULT;
    return glide;
}

TerminalScrollGlide TerminalScrollGlide_2(d64 ease, d64 epsilon) {
    TerminalScrollGlide glide = TerminalScrollGlide_0();
    if (!(ease > 0.0 && ease < 1.0)) {
        THROW("TerminalScrollGlide ease: %.6f outside (0,1)", ease);
        return glide;
    }
    if (!(epsilon > 0.0) || !isfinite(epsilon)) {
        THROW("TerminalScrollGlide epsilon: %.6f not positive", epsilon);
        return glide;
    }
    glide.ease = ease;
    glide.epsilon = epsilon;
    return glide;
}

bool TerminalScrollGlide_setEase(TerminalScrollGlide *self, d64 ease) {
    if (self == nullptr) {
        THROW("TerminalScrollGlide ease: null");
        return false;
    }
    if (!(ease > 0.0 && ease < 1.0)) {
        THROW("TerminalScrollGlide ease: %.6f outside (0,1)", ease);
        return false;
    }
    TerminalScrollGlide_setEase(self, ease);
    return true;
}

d64 TerminalScrollGlide_getEase(const TerminalScrollGlide *self) {
    return self ? TerminalScrollGlide_ease(self) : TERMINAL_SCROLL_EASE_DEFAULT;
}

bool TerminalScrollGlide_setEpsilon(TerminalScrollGlide *self, d64 epsilon) {
    if (self == nullptr) {
        THROW("TerminalScrollGlide epsilon: null");
        return false;
    }
    if (!(epsilon > 0.0) || !isfinite(epsilon)) {
        THROW("TerminalScrollGlide epsilon: %.6f not positive", epsilon);
        return false;
    }
    TerminalScrollGlide_setEpsilon(self, epsilon);
    return true;
}

d64 TerminalScrollGlide_getEpsilon(const TerminalScrollGlide *self) {
    return self ? TerminalScrollGlide_epsilon(self) : TERMINAL_SCROLL_EPSILON_DEFAULT;
}

bool TerminalScrollGlide_isActive(const TerminalScrollGlide *self) {
    if (self == nullptr)
        return false;
    return gap(TerminalScrollGlide_rowShown(self), TerminalScrollGlide_rowTarget(self)) >= TerminalScrollGlide_epsilon(self)
        || gap(TerminalScrollGlide_columnShown(self), TerminalScrollGlide_columnTarget(self)) >= TerminalScrollGlide_epsilon(self);
}

fn TerminalScrollGlide_impulse(TerminalScrollGlide *self, i64 rows, i64 columns,
                                 bytes maxRows, bytes maxColumns) {
    if (self == nullptr)
        return;
    TerminalScrollGlide_setRowTarget(self, clampAxis(TerminalScrollGlide_rowTarget(self) + (d64) rows, maxRows));
    TerminalScrollGlide_setColumnTarget(self, clampAxis(TerminalScrollGlide_columnTarget(self) + (d64) columns, maxColumns));
}

fn TerminalScrollGlide_setTarget(TerminalScrollGlide *self, bytes row, bytes column,
                                   bytes maxRows, bytes maxColumns) {
    if (self == nullptr)
        return;
    TerminalScrollGlide_setRowTarget(self, clampAxis((d64) row, maxRows));
    TerminalScrollGlide_setColumnTarget(self, clampAxis((d64) column, maxColumns));
}

fn TerminalScrollGlide_setPosition(TerminalScrollGlide *self, bytes row, bytes column,
                                     bytes maxRows, bytes maxColumns) {
    if (self == nullptr)
        return;
    d64 clampedRow = clampAxis((d64) row, maxRows);
    d64 clampedColumn = clampAxis((d64) column, maxColumns);
    TerminalScrollGlide_setRowShown(self, clampedRow);
    TerminalScrollGlide_setRowTarget(self, clampedRow);
    TerminalScrollGlide_setColumnShown(self, clampedColumn);
    TerminalScrollGlide_setColumnTarget(self, clampedColumn);
}

fn TerminalScrollGlide_setRowPosition(TerminalScrollGlide *self, bytes row, bytes maxRows) {
    if (self == nullptr)
        return;
    d64 clampedRow = clampAxis((d64) row, maxRows);
    TerminalScrollGlide_setRowShown(self, clampedRow);
    TerminalScrollGlide_setRowTarget(self, clampedRow);
}

fn TerminalScrollGlide_setColumnPosition(TerminalScrollGlide *self, bytes column,
                                           bytes maxColumns) {
    if (self == nullptr)
        return;
    d64 clampedColumn = clampAxis((d64) column, maxColumns);
    TerminalScrollGlide_setColumnShown(self, clampedColumn);
    TerminalScrollGlide_setColumnTarget(self, clampedColumn);
}

fn TerminalScrollGlide_halt(TerminalScrollGlide *self) {
    if (self == nullptr)
        return;
    TerminalScrollGlide_setRowTarget(self, TerminalScrollGlide_rowShown(self));
    TerminalScrollGlide_setColumnTarget(self, TerminalScrollGlide_columnShown(self));
}

fn TerminalScrollGlide_clamp(TerminalScrollGlide *self, bytes maxRows, bytes maxColumns) {
    if (self == nullptr)
        return;
    TerminalScrollGlide_setRowTarget(self, clampAxis(TerminalScrollGlide_rowTarget(self), maxRows));
    TerminalScrollGlide_setRowShown(self, clampAxis(TerminalScrollGlide_rowShown(self), maxRows));
    TerminalScrollGlide_setColumnTarget(self, clampAxis(TerminalScrollGlide_columnTarget(self), maxColumns));
    TerminalScrollGlide_setColumnShown(self, clampAxis(TerminalScrollGlide_columnShown(self), maxColumns));
}

;;INTENTION("step runs once per frame; its success path is a null guard, two comparisons and two arithmetic steps, with no logging, allocation or per-cell revalidation")
;;HOTCODE
TerminalScrollGlidePosition TerminalScrollGlide_step(TerminalScrollGlide *self) {
    TerminalScrollGlidePosition position = {0};
    if (self == nullptr)
        return position;
    d64 row = stepAxis(TerminalScrollGlide_rowShownMut(self), TerminalScrollGlide_rowTarget(self), TerminalScrollGlide_ease(self), TerminalScrollGlide_epsilon(self));
    d64 column = stepAxis(TerminalScrollGlide_columnShownMut(self), TerminalScrollGlide_columnTarget(self), TerminalScrollGlide_ease(self),
                             TerminalScrollGlide_epsilon(self));
    position.row = roundAxis(row);
    position.column = roundAxis(column);
    return position;
}

d64 TerminalScrollGlide_getRowShown(const TerminalScrollGlide *self) {
    return self ? TerminalScrollGlide_rowShown(self) : 0.0;
}

d64 TerminalScrollGlide_getRowTarget(const TerminalScrollGlide *self) {
    return self ? TerminalScrollGlide_rowTarget(self) : 0.0;
}

d64 TerminalScrollGlide_getColumnShown(const TerminalScrollGlide *self) {
    return self ? TerminalScrollGlide_columnShown(self) : 0.0;
}

d64 TerminalScrollGlide_getColumnTarget(const TerminalScrollGlide *self) {
    return self ? TerminalScrollGlide_columnTarget(self) : 0.0;
}

TerminalScrollGlidePosition TerminalScrollGlide_getPosition(const TerminalScrollGlide *self) {
    TerminalScrollGlidePosition position = {0};
    if (self == nullptr)
        return position;
    position.row = roundAxis(TerminalScrollGlide_rowShown(self));
    position.column = roundAxis(TerminalScrollGlide_columnShown(self));
    return position;
}

TerminalProjection TerminalScrollGlide_toString(const TerminalScrollGlide *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format("TerminalScrollGlide{row=%.2f->%.2f,column=%.2f->%.2f}",
                                     TerminalScrollGlide_rowShown(self), TerminalScrollGlide_rowTarget(self),
                                     TerminalScrollGlide_columnShown(self), TerminalScrollGlide_columnTarget(self));
}

TerminalProjection TerminalScrollGlide_toStringStruct(const TerminalScrollGlide *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format(
        "TerminalScrollGlide{rowShown=%.6f,rowTarget=%.6f,columnShown=%.6f,"
        "columnTarget=%.6f,ease=%.6f,epsilon=%.6f}",
        TerminalScrollGlide_rowShown(self), TerminalScrollGlide_rowTarget(self), TerminalScrollGlide_columnShown(self),
        TerminalScrollGlide_columnTarget(self), TerminalScrollGlide_ease(self), TerminalScrollGlide_epsilon(self));
}
