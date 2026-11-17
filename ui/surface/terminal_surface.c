#include "vex.h"
#include "ui/surface/terminal_surface.h"
#include <string.h>
#include <stdio.h>

;;DEFINITION
/** The last-paint presenter. TerminalSurface diffs the frame the application just
 * composed (`back`) against the record of what the host was last told (`front`)
 * and hands over only the cells that differ, as contiguous same-row runs. It
 * exists because the four-plane canvas had no way to reach a host sink, and
 * because re-sending an unchanged screen every frame is the terminal's version of
 * re-rendering a clean scene: it costs the device a full repaint, it flickers on
 * slow links, and it destroys the cheap "nothing changed, do nothing" answer that
 * a static interface deserves. The surface owns no memory — both grids are lent
 * by the caller, so a resize is a rebind rather than an internal reallocation —
 * and it never names a terminal or emits an escape: a run is cells, style flags
 * and packed colours, and the host maps it to ncurses, ANSI or a console. */
;;OVERVIEW
/** ========================================================================
 * CLASS: TerminalSurface (ui/surface/terminal_surface.c)
 * STRUCT FIELDS (Mirroring ui/surface/terminal_surface.h)
 * TerminalCanvas back            composed-next grid (borrowed planes)
 * TerminalCanvas front           last-presented grid (borrowed planes)
 * TerminalRunFn sink             borrowed host run sink
 * any hostContext              opaque resident sink state
 * TerminalFrameFn frame          borrowed frame bracket (nullable)
 * any frameContext             opaque bracket state
 * bool bracketed                 a transaction is currently open
 * bytes gapTolerance            max unchanged run inside one emitted run
 * bytes presentCount            completed presents (lifetime)
 * bytes changedCells            differing cells in the last present
 * bytes changedRuns             runs handed to the sink in the last present
 * bytes emittedCells            cells inside those runs in the last present
 * bytes refusalCount            refusals in this epoch
 * bool active                    sink in flight (reentrancy guard)
 * SLOT RECORD: TerminalRun
 * bytes x, y, length            absolute origin and cell count of one run
 * const u32 *cells          borrowed glyph codepoints at the first cell
 * const u16 *styles         borrowed style flags at the first cell (nullable)
 * const u32 *fg, *bg        borrowed packed colours at the first cell (nullable)
 * SLOT RECORD: TerminalPresentResult
 * bool ok                        every changed run was accepted
 * bool changed                   any cell differed from the last paint
 * bool transactional             a frame transaction opened AND closed
 * bytes changedCells            differing cells found by the scan
 * bytes runs                    runs handed to the sink
 * bytes emittedCells            cells inside those runs
 * PRIVATE HELPERS
 * sameGeometry(a,b.sh)      cold admission identity: grid AND region must match
 * samePlanes(a,b.sh)        cold admission: style/colour plane presence must match
 * cellIndex(canvas,x,y)  one row-major index, honouring the region origin
 * cellDiffers(back,front,index)  one bound comparison across the present planes
 * emitRun(self,y,start,last)     hand one same-row run to the sink
 * adoptFront(self)               region copy back -> front after a present
 * FUNCTION REGISTRY (exports)
 * Constructors: TerminalSurface_0(), _2(back, front) [zero/arity chooser]
 * Admission: bool TerminalSurface_bind(self, back, front, sink, hostContext)
 *   bool TerminalSurface_setSink(self, sink, hostContext)
 *   bool TerminalSurface_reset(self)
 *   bool setFrame/getFrame/getFrameContext/isBracketed(self, ...)
 * Configuration: bool setGapTolerance(self, tolerance), bytes getGapTolerance(self)
 * Present: TerminalPresentResult TerminalSurface_present(self)
 * Queries: bool isBound/isActive(self)
 *   TerminalCanvas getBack/getFront(self)
 *   bytes presentCount/changedCells/changedRuns/emittedCells/exhaustionCount(self)
 * Projections: TerminalProjection TerminalSurface_toString/toStringStruct(self)
 * CONTRACT: null/invalid surfaces are safe (queries answer defaults, present
 * refuses); geometry and plane presence are validated once at the cold bind, so
 * the scan carries a single entry guard and never revalidates per cell. Out-of-
 * region cells are never touched. The scan allocates nothing, logs nothing, and
 * a refused sink reports at most ONE diagnostic per epoch while refusalCount
 * counts every refusal; bind and reset open a fresh epoch. A sink failure leaves
 * `front` untouched, so the next present re-delivers the same delta, and only a
 * completely accepted present adopts `back` as the new front. All calls require
 * host external serialization; the sink must not reenter the surface.
 * ======================================================================== */


;;GETTER
static inline u32 * TerminalCanvas_bg(const TerminalCanvas *s) { return (*s).bg; }
;;GETTER
static inline u32 * *TerminalCanvas_bgMut(TerminalCanvas *s) { return &(*s).bg; }
;;SETTER
static inline fn TerminalCanvas_setBg(TerminalCanvas *s, u32 * v) { (*s).bg = v; }
;;GETTER
static inline u32 * TerminalCanvas_cells(const TerminalCanvas *s) { return (*s).cells; }
;;GETTER
static inline u32 * *TerminalCanvas_cellsMut(TerminalCanvas *s) { return &(*s).cells; }
;;SETTER
static inline fn TerminalCanvas_setCells(TerminalCanvas *s, u32 * v) { (*s).cells = v; }
;;GETTER
static inline u32 * TerminalCanvas_fg(const TerminalCanvas *s) { return (*s).fg; }
;;GETTER
static inline u32 * *TerminalCanvas_fgMut(TerminalCanvas *s) { return &(*s).fg; }
;;SETTER
static inline fn TerminalCanvas_setFg(TerminalCanvas *s, u32 * v) { (*s).fg = v; }
;;GETTER
static inline bytes TerminalCanvas_gridHeight(const TerminalCanvas *s) { return (*s).gridHeight; }
;;GETTER
static inline bytes *TerminalCanvas_gridHeightMut(TerminalCanvas *s) { return &(*s).gridHeight; }
;;SETTER
static inline fn TerminalCanvas_setGridHeight(TerminalCanvas *s, bytes v) { (*s).gridHeight = v; }
;;GETTER
static inline bytes TerminalCanvas_gridWidth(const TerminalCanvas *s) { return (*s).gridWidth; }
;;GETTER
static inline bytes *TerminalCanvas_gridWidthMut(TerminalCanvas *s) { return &(*s).gridWidth; }
;;SETTER
static inline fn TerminalCanvas_setGridWidth(TerminalCanvas *s, bytes v) { (*s).gridWidth = v; }
;;GETTER
static inline bytes TerminalCanvas_height(const TerminalCanvas *s) { return (*s).height; }
;;GETTER
static inline bytes *TerminalCanvas_heightMut(TerminalCanvas *s) { return &(*s).height; }
;;SETTER
static inline fn TerminalCanvas_setHeight(TerminalCanvas *s, bytes v) { (*s).height = v; }
;;GETTER
static inline u16 * TerminalCanvas_styles(const TerminalCanvas *s) { return (*s).styles; }
;;GETTER
static inline u16 * *TerminalCanvas_stylesMut(TerminalCanvas *s) { return &(*s).styles; }
;;SETTER
static inline fn TerminalCanvas_setStyles(TerminalCanvas *s, u16 * v) { (*s).styles = v; }
;;GETTER
static inline bytes TerminalCanvas_width(const TerminalCanvas *s) { return (*s).width; }
;;GETTER
static inline bytes *TerminalCanvas_widthMut(TerminalCanvas *s) { return &(*s).width; }
;;SETTER
static inline fn TerminalCanvas_setWidth(TerminalCanvas *s, bytes v) { (*s).width = v; }
;;GETTER
static inline bytes TerminalCanvas_x(const TerminalCanvas *s) { return (*s).x; }
;;GETTER
static inline bytes *TerminalCanvas_xMut(TerminalCanvas *s) { return &(*s).x; }
;;SETTER
static inline fn TerminalCanvas_setX(TerminalCanvas *s, bytes v) { (*s).x = v; }
;;GETTER
static inline bytes TerminalCanvas_y(const TerminalCanvas *s) { return (*s).y; }
;;GETTER
static inline bytes *TerminalCanvas_yMut(TerminalCanvas *s) { return &(*s).y; }
;;SETTER
static inline fn TerminalCanvas_setY(TerminalCanvas *s, bytes v) { (*s).y = v; }

;;GETTER
static inline bool TerminalSurface_active(const TerminalSurface *s) { return (*s).active; }
;;GETTER
static inline bool *TerminalSurface_activeMut(TerminalSurface *s) { return &(*s).active; }
;;SETTER
static inline fn TerminalSurface_setActive(TerminalSurface *s, bool v) { (*s).active = v; }
;;GETTER
static inline TerminalCanvas TerminalSurface_back(const TerminalSurface *s) { return (*s).back; }
;;GETTER
static inline TerminalCanvas *TerminalSurface_backMut(TerminalSurface *s) { return &(*s).back; }
;;SETTER
static inline fn TerminalSurface_setBack(TerminalSurface *s, TerminalCanvas v) { (*s).back = v; }
;;GETTER
static inline bool TerminalSurface_bracketed(const TerminalSurface *s) { return (*s).bracketed; }
;;GETTER
static inline bool *TerminalSurface_bracketedMut(TerminalSurface *s) { return &(*s).bracketed; }
;;SETTER
static inline fn TerminalSurface_setBracketed(TerminalSurface *s, bool v) { (*s).bracketed = v; }
;;GETTER
static inline bytes TerminalSurface_changedCells(const TerminalSurface *s) { return (*s).changedCells; }
;;GETTER
static inline bytes *TerminalSurface_changedCellsMut(TerminalSurface *s) { return &(*s).changedCells; }
;;SETTER
static inline fn TerminalSurface_setChangedCells(TerminalSurface *s, bytes v) { (*s).changedCells = v; }
;;GETTER
static inline bytes TerminalSurface_changedRuns(const TerminalSurface *s) { return (*s).changedRuns; }
;;GETTER
static inline bytes *TerminalSurface_changedRunsMut(TerminalSurface *s) { return &(*s).changedRuns; }
;;SETTER
static inline fn TerminalSurface_setChangedRuns(TerminalSurface *s, bytes v) { (*s).changedRuns = v; }
;;GETTER
static inline bytes TerminalSurface_emittedCells(const TerminalSurface *s) { return (*s).emittedCells; }
;;GETTER
static inline bytes *TerminalSurface_emittedCellsMut(TerminalSurface *s) { return &(*s).emittedCells; }
;;SETTER
static inline fn TerminalSurface_setEmittedCells(TerminalSurface *s, bytes v) { (*s).emittedCells = v; }
;;GETTER
static inline TerminalFrameFn TerminalSurface_frame(const TerminalSurface *s) { return (*s).frame; }
;;GETTER
static inline TerminalFrameFn *TerminalSurface_frameMut(TerminalSurface *s) { return &(*s).frame; }
;;SETTER
static inline fn TerminalSurface_setFrame(TerminalSurface *s, TerminalFrameFn v) { (*s).frame = v; }
;;GETTER
static inline any TerminalSurface_frameContext(const TerminalSurface *s) { return (*s).frameContext; }
;;GETTER
static inline any *TerminalSurface_frameContextMut(TerminalSurface *s) { return &(*s).frameContext; }
;;SETTER
static inline fn TerminalSurface_setFrameContext(TerminalSurface *s, any v) { (*s).frameContext = v; }
;;GETTER
static inline TerminalCanvas TerminalSurface_front(const TerminalSurface *s) { return (*s).front; }
;;GETTER
static inline TerminalCanvas *TerminalSurface_frontMut(TerminalSurface *s) { return &(*s).front; }
;;SETTER
static inline fn TerminalSurface_setFront(TerminalSurface *s, TerminalCanvas v) { (*s).front = v; }
;;GETTER
static inline bytes TerminalSurface_gapTolerance(const TerminalSurface *s) { return (*s).gapTolerance; }
;;GETTER
static inline bytes *TerminalSurface_gapToleranceMut(TerminalSurface *s) { return &(*s).gapTolerance; }
;;SETTER
static inline fn TerminalSurface_setGapTolerance(TerminalSurface *s, bytes v) { (*s).gapTolerance = v; }
;;GETTER
static inline any TerminalSurface_hostContext(const TerminalSurface *s) { return (*s).hostContext; }
;;GETTER
static inline any *TerminalSurface_hostContextMut(TerminalSurface *s) { return &(*s).hostContext; }
;;SETTER
static inline fn TerminalSurface_setHostContext(TerminalSurface *s, any v) { (*s).hostContext = v; }
;;GETTER
static inline bytes TerminalSurface_presentCount(const TerminalSurface *s) { return (*s).presentCount; }
;;GETTER
static inline bytes *TerminalSurface_presentCountMut(TerminalSurface *s) { return &(*s).presentCount; }
;;SETTER
static inline fn TerminalSurface_setPresentCount(TerminalSurface *s, bytes v) { (*s).presentCount = v; }
;;GETTER
static inline bytes TerminalSurface_refusalCount(const TerminalSurface *s) { return (*s).refusalCount; }
;;GETTER
static inline bytes *TerminalSurface_refusalCountMut(TerminalSurface *s) { return &(*s).refusalCount; }
;;SETTER
static inline fn TerminalSurface_setRefusalCount(TerminalSurface *s, bytes v) { (*s).refusalCount = v; }
;;GETTER
static inline TerminalRunFn TerminalSurface_sink(const TerminalSurface *s) { return (*s).sink; }
;;GETTER
static inline TerminalRunFn *TerminalSurface_sinkMut(TerminalSurface *s) { return &(*s).sink; }
;;SETTER
static inline fn TerminalSurface_setSink(TerminalSurface *s, TerminalRunFn v) { (*s).sink = v; }

static bool sameGeometry(const TerminalCanvas *a, const TerminalCanvas *b) {
    return TerminalCanvas_gridWidth(a) == TerminalCanvas_gridWidth(b) && TerminalCanvas_gridHeight(a) == TerminalCanvas_gridHeight(b)
        && TerminalCanvas_x(a) == TerminalCanvas_x(b) && TerminalCanvas_y(a) == TerminalCanvas_y(b)
        && TerminalCanvas_width(a) == TerminalCanvas_width(b) && TerminalCanvas_height(a) == TerminalCanvas_height(b);
}

static bool samePlanes(const TerminalCanvas *a, const TerminalCanvas *b) {
    return (TerminalCanvas_styles(a) != nullptr) == (TerminalCanvas_styles(b) != nullptr)
        && (TerminalCanvas_fg(a) != nullptr) == (TerminalCanvas_fg(b) != nullptr);
}

static bytes cellIndex(const TerminalCanvas *canvas, bytes x, bytes y) {
    return (y + TerminalCanvas_y(canvas)) * TerminalCanvas_gridWidth(canvas) + TerminalCanvas_x(canvas) + x;
}

// One index, compared across every plane the pair actually carries. A plane the
// canvases do not carry is not a dimension of difference, which is exactly why
// bind demands matching presence: a difference can never be invisible.
static bool cellDiffers(const TerminalCanvas *back, const TerminalCanvas *front, bytes index) {
    if (TerminalCanvas_cells(back)[index] != TerminalCanvas_cells(front)[index])
        return true;
    if (TerminalCanvas_styles(back) != nullptr && TerminalCanvas_styles(back)[index] != TerminalCanvas_styles(front)[index])
        return true;
    if (TerminalCanvas_fg(back) != nullptr
        && (TerminalCanvas_fg(back)[index] != TerminalCanvas_fg(front)[index] || TerminalCanvas_bg(back)[index] != TerminalCanvas_bg(front)[index]))
        return true;
    return false;
}

static bool emitRun(TerminalSurface *self, bytes y, bytes start, bytes last) {
    // The transaction opens on the first run that is actually going to be handed over,
    // so a frame carrying no change never opens one.
    if (!TerminalSurface_bracketed(self) && TerminalSurface_frame(self) != nullptr) {
        TerminalSurface_setBracketed(self, true);
        TerminalSurface_frame(self)(TerminalSurface_frameContext(self), TERMINAL_PRESENT_BEGIN);
    }
    const TerminalCanvas *back = TerminalSurface_backMut(self);
    bytes gridY = y + TerminalCanvas_y(back);
    bytes first = cellIndex(back, start, y);
    bytes length = last - start + 1;
    TerminalRun run = {0};
    run.x = TerminalCanvas_x(back) + start;
    run.y = gridY;
    run.length = length;
    run.cells = &TerminalCanvas_cellsMut(back)[first];
    if (TerminalCanvas_styles(back) != nullptr)
        run.styles = &TerminalCanvas_stylesMut(back)[first];
    if (TerminalCanvas_fg(back) != nullptr) {
        run.fg = &TerminalCanvas_fgMut(back)[first];
        run.bg = &TerminalCanvas_bgMut(back)[first];
    }
    TerminalSurface_setChangedRuns(self, TerminalSurface_changedRuns(self) + 1u);
    TerminalSurface_setEmittedCells(self, TerminalSurface_emittedCells(self) + (length));
    if (TerminalSurface_sink(self)(TerminalSurface_hostContext(self), run))
        return true;
    if (TerminalSurface_refusalCount(self) == 0)
        THROW("TerminalSurface exhausted: sink refused run at %zu,%zu (length %zu, bound %zux%zu)",
              run.x, run.y, run.length, TerminalCanvas_width(back), TerminalCanvas_height(back));
    if (TerminalSurface_refusalCount(self) != SIZE_MAX)
        TerminalSurface_setRefusalCount(self, TerminalSurface_refusalCount(self) + 1u);
    return false;
}

// Copy the region from back to front, one row at a time so cells outside the
// region are never touched. Styles and colours are guarded SEPARATELY: a canvas
// may carry a style plane and no colour planes at all.
static fn adoptFront(TerminalSurface *self) {
    const TerminalCanvas *back = TerminalSurface_backMut(self);
    TerminalCanvas *front = TerminalSurface_frontMut(self);
    bytes width = TerminalCanvas_width(back);
    for (bytes y = 0; y < TerminalCanvas_height(back); ++y) {
        bytes origin = TerminalCanvas_x(back) + (y + TerminalCanvas_y(back)) * TerminalCanvas_gridWidth(back);
        memcpy(&TerminalCanvas_cellsMut(front)[origin], &TerminalCanvas_cellsMut(back)[origin], width * sizeof(u32));
        if (TerminalCanvas_styles(back) != nullptr)
            memcpy(&TerminalCanvas_stylesMut(front)[origin], &TerminalCanvas_stylesMut(back)[origin], width * sizeof(u16));
        if (TerminalCanvas_fg(back) != nullptr) {
            memcpy(&TerminalCanvas_fgMut(front)[origin], &TerminalCanvas_fgMut(back)[origin], width * sizeof(u32));
            memcpy(&TerminalCanvas_bgMut(front)[origin], &TerminalCanvas_bgMut(back)[origin], width * sizeof(u32));
        }
    }
}

TerminalSurface TerminalSurface_0() {
    return (TerminalSurface) { .gapTolerance = TERMINAL_SURFACE_GAP_DEFAULT };
}

TerminalSurface TerminalSurface_2(TerminalCanvas back, TerminalCanvas front) {
    TerminalSurface self = TerminalSurface_0();
    self.back = back;
    self.front = front;
    return self;
}

bool TerminalSurface_bind(TerminalSurface *self, const TerminalCanvas *back,
                          const TerminalCanvas *front, TerminalRunFn sink, any hostContext) {
    if (self == nullptr || back == nullptr || front == nullptr || sink == nullptr
        || !TerminalCanvas_isValid(back) || !TerminalCanvas_isValid(front)
        || !sameGeometry(back, front) || !samePlanes(back, front) || TerminalSurface_active(self)) {
        THROW("TerminalSurface bind: invalid canvases/sink, mismatched geometry or planes, or busy");
        return false;
    }
    TerminalSurface_setBack(self, *back);
    TerminalSurface_setFront(self, *front);
    TerminalSurface_setSink(self, sink);
    TerminalSurface_setHostContext(self, hostContext);
    TerminalSurface_setPresentCount(self, 0);
    TerminalSurface_setChangedCells(self, 0);
    TerminalSurface_setChangedRuns(self, 0);
    TerminalSurface_setEmittedCells(self, 0);
    TerminalSurface_setRefusalCount(self, 0);
    return true;
}

bool TerminalSurface_setFrame(TerminalSurface *self, TerminalFrameFn frame, any frameContext) {
    if (self == nullptr || frame == nullptr || TerminalSurface_active(self) || TerminalSurface_bracketed(self)) {
        THROW("TerminalSurface setFrame: null bracket, or a transaction is open");
        return false;
    }
    TerminalSurface_setFrame(self, frame);
    TerminalSurface_setFrameContext(self, frameContext);
    return true;
}
TerminalFrameFn TerminalSurface_getFrame(const TerminalSurface *self) { return self ? TerminalSurface_frame(self) : nullptr; }
any TerminalSurface_getFrameContext(const TerminalSurface *self) { return self ? TerminalSurface_frameContext(self) : nullptr; }
bool TerminalSurface_isBracketed(const TerminalSurface *self) { return self != nullptr && TerminalSurface_bracketed(self); }

bool TerminalSurface_setSink(TerminalSurface *self, TerminalRunFn sink, any hostContext) {
    if (self == nullptr || sink == nullptr || TerminalSurface_active(self)) {
        THROW("TerminalSurface setSink: null sink or busy");
        return false;
    }
    TerminalSurface_setSink(self, sink);
    TerminalSurface_setHostContext(self, hostContext);
    return true;
}

bool TerminalSurface_setGapTolerance(TerminalSurface *self, bytes tolerance) {
    if (self == nullptr || TerminalSurface_active(self)) {
        THROW("TerminalSurface gapTolerance: null surface or busy");
        return false;
    }
    TerminalSurface_setGapTolerance(self, tolerance);
    return true;
}

bytes TerminalSurface_getGapTolerance(const TerminalSurface *self) {
    return self ? TerminalSurface_gapTolerance(self) : TERMINAL_SURFACE_GAP_DEFAULT;
}

bool TerminalSurface_isBound(const TerminalSurface *self) {
    return self != nullptr && TerminalSurface_sink(self) != nullptr
        && TerminalCanvas_isValid(TerminalSurface_backMut(self)) && TerminalCanvas_isValid(TerminalSurface_frontMut(self));
}

bool TerminalSurface_isActive(const TerminalSurface *self) {
    return self != nullptr && TerminalSurface_active(self);
}

TerminalCanvas TerminalSurface_getBack(const TerminalSurface *self) {
    return self ? TerminalSurface_back(self) : TerminalCanvas_0();
}

TerminalCanvas TerminalSurface_getFront(const TerminalSurface *self) {
    return self ? TerminalSurface_front(self) : TerminalCanvas_0();
}

bytes TerminalSurface_presentCount(const TerminalSurface *self) { return self ? TerminalSurface_presentCount(self) : 0; }
bytes TerminalSurface_changedCells(const TerminalSurface *self) { return self ? TerminalSurface_changedCells(self) : 0; }
bytes TerminalSurface_changedRuns(const TerminalSurface *self) { return self ? TerminalSurface_changedRuns(self) : 0; }
bytes TerminalSurface_emittedCells(const TerminalSurface *self) { return self ? TerminalSurface_emittedCells(self) : 0; }
bytes TerminalSurface_exhaustionCount(const TerminalSurface *self) { return self ? TerminalSurface_refusalCount(self) : 0; }

;;INTENTION("present is a hot path; per the Exhaustion Loudness Law's failure-only waiver its success path adds no logging, allocation or per-cell revalidation, and only a refused run reports — at most once in the epoch")
;;HOTCODE
TerminalPresentResult TerminalSurface_present(TerminalSurface *self) {
    TerminalPresentResult result = {0};
    if (self == nullptr) {
        THROW("TerminalSurface present: null surface");
        return result;
    }
    if (!TerminalSurface_isBound(self) || TerminalSurface_active(self)) {
        if (TerminalSurface_refusalCount(self) == 0)
            THROW("TerminalSurface refused present: unbound or reentrant");
        if (TerminalSurface_refusalCount(self) != SIZE_MAX)
            TerminalSurface_setRefusalCount(self, TerminalSurface_refusalCount(self) + 1u);
        return result;
    }
    const TerminalCanvas *back = TerminalSurface_backMut(self);
    const TerminalCanvas *front = TerminalSurface_frontMut(self);
    bytes width = TerminalCanvas_width(back);
    bytes height = TerminalCanvas_height(back);
    bytes tolerance = TerminalSurface_gapTolerance(self);
    TerminalSurface_setChangedCells(self, 0);
    TerminalSurface_setChangedRuns(self, 0);
    TerminalSurface_setEmittedCells(self, 0);
    TerminalSurface_setActive(self, true);
    bytes changed = 0;
    bool accepted = true;
    for (bytes y = 0; y < height && accepted; ++y) {
        bytes start = 0;
        bytes last = 0;
        bool open = false;
        for (bytes x = 0; x < width; ++x) {
            if (cellDiffers(back, front, cellIndex(back, x, y))) {
                if (!open) {
                    start = x;
                    open = true;
                }
                last = x;
                ++changed;
                continue;
            }
            if (!open || x - last <= tolerance)
                continue;
            open = false;
            if (!emitRun(self, y, start, last)) {
                accepted = false;
                break;
            }
        }
        if (accepted && open)
            accepted = emitRun(self, y, start, last);
    }
    // Close the transaction BEFORE anything else is reported. It closes even when a
    // sink refused part-way: leaving a device inside a half-finished transaction is
    // how a screen ends up frozen.
    if (TerminalSurface_bracketed(self)) {
        TerminalSurface_setBracketed(self, false);
        TerminalSurface_frame(self)(TerminalSurface_frameContext(self), TERMINAL_PRESENT_END);
        result.transactional = true;
    }
    TerminalSurface_setActive(self, false);
    TerminalSurface_setChangedCells(self, changed);
    result.changedCells = changed;
    result.runs = TerminalSurface_changedRuns(self);
    result.emittedCells = TerminalSurface_emittedCells(self);
    result.changed = changed != 0;
    if (accepted) {
        // An unchanged frame needs no adopt: front already equals back, so an
        // idle frame costs one scan and zero writes.
        if (result.runs > 0)
            adoptFront(self);
        TerminalSurface_setPresentCount(self, TerminalSurface_presentCount(self) + 1u);
        result.ok = true;
    }
    return result;
}

bool TerminalSurface_reset(TerminalSurface *self) {
    if (self == nullptr || !TerminalSurface_isBound(self) || TerminalSurface_active(self)) {
        THROW("TerminalSurface reset: unbound or busy");
        return false;
    }
    adoptFront(self);
    TerminalSurface_setRefusalCount(self, 0);
    TerminalSurface_setChangedCells(self, 0);
    TerminalSurface_setChangedRuns(self, 0);
    TerminalSurface_setEmittedCells(self, 0);
    return true;
}

TerminalProjection TerminalSurface_toString(const TerminalSurface *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format(
        "TerminalSurface{presented=%zu,changed=%zu,runs=%zu,emitted=%zu,bound=%d,active=%d}",
        TerminalSurface_presentCount(self), TerminalSurface_changedCells(self), TerminalSurface_changedRuns(self), TerminalSurface_emittedCells(self),
        TerminalSurface_isBound(self), TerminalSurface_active(self));
}

TerminalProjection TerminalSurface_toStringStruct(const TerminalSurface *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    TerminalCanvas back = TerminalSurface_back(self);
    TerminalCanvas front = TerminalSurface_front(self);
    return TerminalProjection_format(
        "TerminalSurface{back=%zux%zu@%zu,%zu,front=%zux%zu@%zu,%zu,sink=%s,gapTolerance=%zu,presentCount=%zu,changedCells=%zu,changedRuns=%zu,emittedCells=%zu,refusalCount=%zu,active=%d}",
        back.width, back.height, back.x, back.y,
        front.width, front.height, front.x, front.y,
        TerminalSurface_sink(self) ? "bound" : "none", TerminalSurface_gapTolerance(self),
        TerminalSurface_presentCount(self), TerminalSurface_changedCells(self), TerminalSurface_changedRuns(self), TerminalSurface_emittedCells(self),
        TerminalSurface_refusalCount(self), TerminalSurface_active(self));
}
