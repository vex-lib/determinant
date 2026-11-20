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
static inline u32 *Canvas_bg(const TerminalCanvas *s) { return (*s).bg; }
;;GETTER
static inline u32 *Canvas_cells(const TerminalCanvas *s) { return (*s).cells; }
;;GETTER
static inline u32 *Canvas_fg(const TerminalCanvas *s) { return (*s).fg; }
;;GETTER
static inline bytes Canvas_gridHeight(const TerminalCanvas *s) { return (*s).gridHeight; }
;;GETTER
static inline bytes Canvas_gridWidth(const TerminalCanvas *s) { return (*s).gridWidth; }
;;GETTER
static inline bytes Canvas_height(const TerminalCanvas *s) { return (*s).height; }
;;GETTER
static inline u16 *Canvas_styles(const TerminalCanvas *s) { return (*s).styles; }
;;GETTER
static inline bytes Canvas_width(const TerminalCanvas *s) { return (*s).width; }
;;GETTER
static inline bytes Canvas_x(const TerminalCanvas *s) { return (*s).x; }
;;GETTER
static inline bytes Canvas_y(const TerminalCanvas *s) { return (*s).y; }

;;GETTER
static inline bool Surf_active(const TerminalSurface *s) { return (*s).active; }
;;SETTER
static inline fn Surf_setActive(TerminalSurface *s, bool v) { (*s).active = v; }
;;GETTER
static inline TerminalCanvas Surf_back(const TerminalSurface *s) { return (*s).back; }
;;GETTER
static inline const TerminalCanvas *Surf_backConst(const TerminalSurface *s) { return &(*s).back; }
;;SETTER
static inline fn Surf_setBack(TerminalSurface *s, TerminalCanvas v) { (*s).back = v; }
;;GETTER
static inline bool Surf_bracketed(const TerminalSurface *s) { return (*s).bracketed; }
;;SETTER
static inline fn Surf_setBracketed(TerminalSurface *s, bool v) { (*s).bracketed = v; }
;;GETTER
static inline bytes Surf_changedCells(const TerminalSurface *s) { return (*s).changedCells; }
;;SETTER
static inline fn Surf_setChangedCells(TerminalSurface *s, bytes v) { (*s).changedCells = v; }
;;GETTER
static inline bytes Surf_changedRuns(const TerminalSurface *s) { return (*s).changedRuns; }
;;SETTER
static inline fn Surf_setChangedRuns(TerminalSurface *s, bytes v) { (*s).changedRuns = v; }
;;GETTER
static inline bytes Surf_emittedCells(const TerminalSurface *s) { return (*s).emittedCells; }
;;SETTER
static inline fn Surf_setEmittedCells(TerminalSurface *s, bytes v) { (*s).emittedCells = v; }
;;GETTER
static inline TerminalFrameFn Surf_frame(const TerminalSurface *s) { return (*s).frame; }
;;SETTER
static inline fn Surf_setFrame(TerminalSurface *s, TerminalFrameFn v) { (*s).frame = v; }
;;GETTER
static inline any Surf_frameContext(const TerminalSurface *s) { return (*s).frameContext; }
;;SETTER
static inline fn Surf_setFrameContext(TerminalSurface *s, any v) { (*s).frameContext = v; }
;;GETTER
static inline TerminalCanvas Surf_front(const TerminalSurface *s) { return (*s).front; }
;;GETTER
static inline const TerminalCanvas *Surf_frontConst(const TerminalSurface *s) { return &(*s).front; }
;;GETTER
static inline TerminalCanvas *Surf_frontMut(TerminalSurface *s) { return &(*s).front; }
;;SETTER
static inline fn Surf_setFront(TerminalSurface *s, TerminalCanvas v) { (*s).front = v; }
;;GETTER
static inline bytes Surf_gapTolerance(const TerminalSurface *s) { return (*s).gapTolerance; }
;;SETTER
static inline fn Surf_setGapTolerance(TerminalSurface *s, bytes v) { (*s).gapTolerance = v; }
;;GETTER
static inline any Surf_hostContext(const TerminalSurface *s) { return (*s).hostContext; }
;;SETTER
static inline fn Surf_setHostContext(TerminalSurface *s, any v) { (*s).hostContext = v; }
;;GETTER
static inline bytes Surf_presentCount(const TerminalSurface *s) { return (*s).presentCount; }
;;SETTER
static inline fn Surf_setPresentCount(TerminalSurface *s, bytes v) { (*s).presentCount = v; }
;;GETTER
static inline bytes Surf_refusalCount(const TerminalSurface *s) { return (*s).refusalCount; }
;;SETTER
static inline fn Surf_setRefusalCount(TerminalSurface *s, bytes v) { (*s).refusalCount = v; }
;;GETTER
static inline TerminalRunFn Surf_sink(const TerminalSurface *s) { return (*s).sink; }
;;SETTER
static inline fn Surf_setSink(TerminalSurface *s, TerminalRunFn v) { (*s).sink = v; }

static bool sameGeometry(const TerminalCanvas *a, const TerminalCanvas *b) {
    return Canvas_gridWidth(a) == Canvas_gridWidth(b) && Canvas_gridHeight(a) == Canvas_gridHeight(b)
        && Canvas_x(a) == Canvas_x(b) && Canvas_y(a) == Canvas_y(b)
        && Canvas_width(a) == Canvas_width(b) && Canvas_height(a) == Canvas_height(b);
}

static bool samePlanes(const TerminalCanvas *a, const TerminalCanvas *b) {
    return (Canvas_styles(a) != nullptr) == (Canvas_styles(b) != nullptr)
        && (Canvas_fg(a) != nullptr) == (Canvas_fg(b) != nullptr);
}

static bytes cellIndex(const TerminalCanvas *canvas, bytes x, bytes y) {
    return (y + Canvas_y(canvas)) * Canvas_gridWidth(canvas) + Canvas_x(canvas) + x;
}

static bool cellDiffers(const TerminalCanvas *back, const TerminalCanvas *front, bytes index) {
    if (Canvas_cells(back)[index] != Canvas_cells(front)[index])
        return true;
    if (Canvas_styles(back) != nullptr && Canvas_styles(back)[index] != Canvas_styles(front)[index])
        return true;
    if (Canvas_fg(back) != nullptr
        && (Canvas_fg(back)[index] != Canvas_fg(front)[index] || Canvas_bg(back)[index] != Canvas_bg(front)[index]))
        return true;
    return false;
}

static bool emitRun(TerminalSurface *self, bytes y, bytes start, bytes last) {
    if (!Surf_bracketed(self) && Surf_frame(self) != nullptr) {
        Surf_setBracketed(self, true);
        Surf_frame(self)(Surf_frameContext(self), TERMINAL_PRESENT_BEGIN);
    }
    const TerminalCanvas *back = Surf_backConst(self);
    bytes gridY = y + Canvas_y(back);
    bytes first = cellIndex(back, start, y);
    bytes length = last - start + 1;
    TerminalRun run = {0};
    run.x = Canvas_x(back) + start;
    run.y = gridY;
    run.length = length;
    run.cells = &Canvas_cells(back)[first];
    if (Canvas_styles(back) != nullptr)
        run.styles = &Canvas_styles(back)[first];
    if (Canvas_fg(back) != nullptr) {
        run.fg = &Canvas_fg(back)[first];
        run.bg = &Canvas_bg(back)[first];
    }
    Surf_setChangedRuns(self, Surf_changedRuns(self) + 1u);
    Surf_setEmittedCells(self, Surf_emittedCells(self) + length);
    if (Surf_sink(self)(Surf_hostContext(self), run))
        return true;
    if (Surf_refusalCount(self) == 0)
        THROW("TerminalSurface exhausted: sink refused run at %zu,%zu (length %zu, bound %zux%zu)",
              run.x, run.y, run.length, Canvas_width(back), Canvas_height(back));
    if (Surf_refusalCount(self) != SIZE_MAX)
        Surf_setRefusalCount(self, Surf_refusalCount(self) + 1u);
    return false;
}

static fn adoptFront(TerminalSurface *self) {
    const TerminalCanvas *back = Surf_backConst(self);
    TerminalCanvas *front = Surf_frontMut(self);
    bytes width = Canvas_width(back);
    for (bytes y = 0; y < Canvas_height(back); ++y) {
        bytes origin = Canvas_x(back) + (y + Canvas_y(back)) * Canvas_gridWidth(back);
        memcpy(&Canvas_cells(front)[origin], &Canvas_cells(back)[origin], width * sizeof(u32));
        if (Canvas_styles(back) != nullptr)
            memcpy(&Canvas_styles(front)[origin], &Canvas_styles(back)[origin], width * sizeof(u16));
        if (Canvas_fg(back) != nullptr) {
            memcpy(&Canvas_fg(front)[origin], &Canvas_fg(back)[origin], width * sizeof(u32));
            memcpy(&Canvas_bg(front)[origin], &Canvas_bg(back)[origin], width * sizeof(u32));
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
        || !sameGeometry(back, front) || !samePlanes(back, front) || Surf_active(self)) {
        THROW("TerminalSurface bind: invalid canvases/sink, mismatched geometry or planes, or busy");
        return false;
    }
    Surf_setBack(self, *back);
    Surf_setFront(self, *front);
    Surf_setSink(self, sink);
    Surf_setHostContext(self, hostContext);
    Surf_setPresentCount(self, 0);
    Surf_setChangedCells(self, 0);
    Surf_setChangedRuns(self, 0);
    Surf_setEmittedCells(self, 0);
    Surf_setRefusalCount(self, 0);
    return true;
}

bool TerminalSurface_setFrame(TerminalSurface *self, TerminalFrameFn frame, any frameContext) {
    if (self == nullptr || frame == nullptr || Surf_active(self) || Surf_bracketed(self)) {
        THROW("TerminalSurface setFrame: null bracket, or a transaction is open");
        return false;
    }
    Surf_setFrame(self, frame);
    Surf_setFrameContext(self, frameContext);
    return true;
}
TerminalFrameFn TerminalSurface_getFrame(const TerminalSurface *self) { return self ? Surf_frame(self) : nullptr; }
any TerminalSurface_getFrameContext(const TerminalSurface *self) { return self ? Surf_frameContext(self) : nullptr; }
bool TerminalSurface_isBracketed(const TerminalSurface *self) { return self != nullptr && Surf_bracketed(self); }

bool TerminalSurface_setSink(TerminalSurface *self, TerminalRunFn sink, any hostContext) {
    if (self == nullptr || sink == nullptr || Surf_active(self)) {
        THROW("TerminalSurface setSink: null sink or busy");
        return false;
    }
    Surf_setSink(self, sink);
    Surf_setHostContext(self, hostContext);
    return true;
}

bool TerminalSurface_setGapTolerance(TerminalSurface *self, bytes tolerance) {
    if (self == nullptr || Surf_active(self)) {
        THROW("TerminalSurface gapTolerance: null surface or busy");
        return false;
    }
    Surf_setGapTolerance(self, tolerance);
    return true;
}

bytes TerminalSurface_getGapTolerance(const TerminalSurface *self) {
    return self ? Surf_gapTolerance(self) : TERMINAL_SURFACE_GAP_DEFAULT;
}

bool TerminalSurface_isBound(const TerminalSurface *self) {
    return self != nullptr && Surf_sink(self) != nullptr
        && TerminalCanvas_isValid(Surf_backConst(self)) && TerminalCanvas_isValid(Surf_frontConst(self));
}

bool TerminalSurface_isActive(const TerminalSurface *self) {
    return self != nullptr && Surf_active(self);
}

TerminalCanvas TerminalSurface_getBack(const TerminalSurface *self) {
    return self ? Surf_back(self) : TerminalCanvas_0();
}

TerminalCanvas TerminalSurface_getFront(const TerminalSurface *self) {
    return self ? Surf_front(self) : TerminalCanvas_0();
}

bytes TerminalSurface_presentCount(const TerminalSurface *self) { return self ? Surf_presentCount(self) : 0; }
bytes TerminalSurface_changedCells(const TerminalSurface *self) { return self ? Surf_changedCells(self) : 0; }
bytes TerminalSurface_changedRuns(const TerminalSurface *self) { return self ? Surf_changedRuns(self) : 0; }
bytes TerminalSurface_emittedCells(const TerminalSurface *self) { return self ? Surf_emittedCells(self) : 0; }
bytes TerminalSurface_exhaustionCount(const TerminalSurface *self) { return self ? Surf_refusalCount(self) : 0; }

;;INTENTION("present is a hot path; per the Exhaustion Loudness Law's failure-only waiver its success path adds no logging, allocation or per-cell revalidation, and only a refused run reports — at most once in the epoch")
;;HOTCODE
TerminalPresentResult TerminalSurface_present(TerminalSurface *self) {
    TerminalPresentResult result = {0};
    if (self == nullptr) {
        THROW("TerminalSurface present: null surface");
        return result;
    }
    if (!TerminalSurface_isBound(self) || Surf_active(self)) {
        if (Surf_refusalCount(self) == 0)
            THROW("TerminalSurface refused present: unbound or reentrant");
        if (Surf_refusalCount(self) != SIZE_MAX)
            Surf_setRefusalCount(self, Surf_refusalCount(self) + 1u);
        return result;
    }
    const TerminalCanvas *back = Surf_backConst(self);
    const TerminalCanvas *front = Surf_frontConst(self);
    bytes width = Canvas_width(back);
    bytes height = Canvas_height(back);
    bytes tolerance = Surf_gapTolerance(self);
    Surf_setChangedCells(self, 0);
    Surf_setChangedRuns(self, 0);
    Surf_setEmittedCells(self, 0);
    Surf_setActive(self, true);
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
    if (Surf_bracketed(self)) {
        Surf_setBracketed(self, false);
        Surf_frame(self)(Surf_frameContext(self), TERMINAL_PRESENT_END);
        result.transactional = true;
    }
    Surf_setActive(self, false);
    Surf_setChangedCells(self, changed);
    result.changedCells = changed;
    result.runs = Surf_changedRuns(self);
    result.emittedCells = Surf_emittedCells(self);
    result.changed = changed != 0;
    if (accepted) {
        if (result.runs > 0)
            adoptFront(self);
        Surf_setPresentCount(self, Surf_presentCount(self) + 1u);
        result.ok = true;
    }
    return result;
}

bool TerminalSurface_reset(TerminalSurface *self) {
    if (self == nullptr || !TerminalSurface_isBound(self) || Surf_active(self)) {
        THROW("TerminalSurface reset: unbound or busy");
        return false;
    }
    adoptFront(self);
    Surf_setRefusalCount(self, 0);
    Surf_setChangedCells(self, 0);
    Surf_setChangedRuns(self, 0);
    Surf_setEmittedCells(self, 0);
    return true;
}

TerminalProjection TerminalSurface_toString(const TerminalSurface *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format(
        "TerminalSurface{presented=%zu,changed=%zu,runs=%zu,emitted=%zu,bound=%d,active=%d}",
        Surf_presentCount(self), Surf_changedCells(self), Surf_changedRuns(self), Surf_emittedCells(self),
        TerminalSurface_isBound(self), Surf_active(self));
}

TerminalProjection TerminalSurface_toStringStruct(const TerminalSurface *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    TerminalCanvas back = Surf_back(self);
    TerminalCanvas front = Surf_front(self);
    return TerminalProjection_format(
        "TerminalSurface{back=%zux%zu@%zu,%zu,front=%zux%zu@%zu,%zu,sink=%s,gapTolerance=%zu,presentCount=%zu,changedCells=%zu,changedRuns=%zu,emittedCells=%zu,refusalCount=%zu,active=%d}",
        back.width, back.height, back.x, back.y,
        front.width, front.height, front.x, front.y,
        Surf_sink(self) ? "bound" : "none", Surf_gapTolerance(self),
        Surf_presentCount(self), Surf_changedCells(self), Surf_changedRuns(self), Surf_emittedCells(self),
        Surf_refusalCount(self), Surf_active(self));
}
