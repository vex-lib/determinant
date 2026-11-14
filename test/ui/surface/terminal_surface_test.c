// Owner: TerminalSurface — the last-paint presenter. Cold admission identity
// (grid, region and plane presence), delta scanning into same-row runs, gap
// tolerance, adoption, failure preservation with retry, the once-per-epoch
// refusal report with an always-climbing count, reentrant refusal, and a quiet
// bounded hot path. Determinism is asserted by two identically prepared surfaces
// producing identical runs.
//
// Scope note: this owner proves the surface's own contract on borrowed grids. It
// does not compile a negative-arity case (no determinant owner does) and cannot
// prove physical device output, which stays the host's and the user's acceptance.
#include "ui/surface/terminal_surface.h"
#include "ui/surface/terminal_canvas.h"
#include "format/terminal_color.h"
#include "determinant/test/support.h"

enum { W = 8, H = 3 };

typedef struct RunLog {
    bytes calls;            // sink invocations
    bytes runs;             // accepted runs
    bytes cells;            // accepted cells
    bytes failFromCall;     // 0 = never refuse; else refuse every call from here
    TerminalRun first;       // first accepted run
    TerminalRun last;        // last accepted run
    u32 firstGlyphs[W];
    u32 lastGlyphs[W];
    TerminalSurface *surface; // reentrancy probe target
    bytes reentered;        // nested presents observed as a clean refusal
} RunLog;

static fn copyGlyphs(u32 *destination, TerminalRun run) {
    if (run.cells == nullptr)
        return;
    bytes bounded = run.length < (bytes) W ? run.length : (bytes) W;
    for (bytes i = 0; i < bounded; ++i)
        destination[i] = run.cells[i];
}

// Records the frame bracket: how many times each phase arrived, and in what order.
typedef struct FrameLog {
    bytes begins;
    bytes ends;
    bytes order;        // 0 nothing, 1 inside a transaction, 2 closed
    bool outOfOrder;     // a phase arrived where none could be expected
    TerminalSurface *probe;   // when set, BEGIN tries to swap the bracket mid-frame
    bool swapRefused;
} FrameLog;

static fn frameBracket(any frameContext, TerminalPresentPhase phase) {
    FrameLog *frames = frameContext;
    if (phase == TERMINAL_PRESENT_BEGIN) {
        if ((*frames).order != 0)
            (*frames).outOfOrder = true;
        (*frames).order = 1;
        ++(*frames).begins;
        // Swapping the bracket while a transaction is open must be refused: the host
        // must not change who is being told mid-frame.
        if ((*frames).probe != nullptr)
            (*frames).swapRefused = !TerminalSurface_setFrame((*frames).probe, frameBracket, nullptr);
    } else {
        if ((*frames).order != 1)
            (*frames).outOfOrder = true;
        (*frames).order = 2;
        ++(*frames).ends;
    }
}

static bool sink(any hostContext, TerminalRun run) {
    RunLog *log = hostContext;
    ++(*log).calls;
    if ((*log).failFromCall != 0 && (*log).calls >= (*log).failFromCall)
        return false;
    if ((*log).runs == 0) {
        (*log).first = run;
        copyGlyphs((*log).firstGlyphs, run);
    }
    (*log).last = run;
    copyGlyphs((*log).lastGlyphs, run);
    ++(*log).runs;
    (*log).cells += run.length;
    return true;
}

// A sink that reaches back into the surface while it is presenting: the nested
// present must refuse as reentrant and the outer run must still land.
static bool reentrantSink(any hostContext, TerminalRun run) {
    RunLog *log = hostContext;
    ++(*log).calls;
    TerminalPresentResult nested = TerminalSurface_present((*log).surface);
    if (!nested.ok && !nested.changed && nested.runs == 0)
        ++(*log).reentered;
    (fn) run;
    ++(*log).runs;
    return true;
}

// A sink that probes the cold doors from inside the hot path.
static bool busyProbeSink(any hostContext, TerminalRun run) {
    RunLog *log = hostContext;
    ++(*log).calls;
    assert(!TerminalSurface_reset((*log).surface));
    assert(!TerminalSurface_setGapTolerance((*log).surface, 2));
    (fn) run;
    ++(*log).runs;
    return true;
}

static fn clearBoth(TerminalCanvas *back, TerminalCanvas *front) {
    TerminalCanvas_fill(back, ' ', TerminalPen());
    TerminalCanvas_fill(front, ' ', TerminalPen());
}

int main() {
    enum { CELLS = W * H };
    u32 backCells[CELLS], frontCells[CELLS];
    u16 backStyles[CELLS], frontStyles[CELLS];
    u32 backFg[CELLS], frontFg[CELLS], backBg[CELLS], frontBg[CELLS];
    TerminalCanvas back = TerminalCanvas(backCells, backStyles, backFg, backBg, W, H);
    TerminalCanvas front = TerminalCanvas(frontCells, frontStyles, frontFg, frontBg, W, H);
    clearBoth(&back, &front);

    // --- Constructors and the zero/arity chooser ---
    TerminalSurface zeroSurface = TerminalSurface_0();
    assert(!TerminalSurface_isBound(&zeroSurface) && !TerminalSurface_isActive(&zeroSurface));
    assert(TerminalSurface_getGapTolerance(&zeroSurface) == TERMINAL_SURFACE_GAP_DEFAULT);
    TerminalSurface chosen = TerminalSurface();
    assert(!TerminalSurface_isBound(&chosen));
    TerminalSurface pair = TerminalSurface(back, front);
    assert(!TerminalSurface_isBound(&pair));                       // canvases, no sink yet
    assert(TerminalSurface_getBack(&pair).gridWidth == W && TerminalSurface_getBack(&pair).height == H);
    assert(TerminalSurface_getFront(&pair).gridWidth == W);
    TerminalSurface zeroed = TerminalSurface_zero();
    assert(!TerminalSurface_isBound(&zeroed));

    // --- Null safety across the whole public surface ---
    assert(!TerminalSurface_isBound(nullptr) && !TerminalSurface_isActive(nullptr));
    assert(TerminalSurface_getGapTolerance(nullptr) == TERMINAL_SURFACE_GAP_DEFAULT);
    assert(TerminalSurface_getBack(nullptr).cells == nullptr);
    assert(TerminalSurface_getFront(nullptr).cells == nullptr);
    assert(TerminalSurface_presentCount(nullptr) == 0);
    assert(TerminalSurface_changedCells(nullptr) == 0);
    assert(TerminalSurface_changedRuns(nullptr) == 0);
    assert(TerminalSurface_emittedCells(nullptr) == 0);
    assert(TerminalSurface_exhaustionCount(nullptr) == 0);
    REJECT(TerminalSurface_present(nullptr).ok, "null surface");

    // --- Cold admission rejects loud and preserves the unbound state ---
    RunLog log = {0};
    TerminalSurface surface = TerminalSurface_0();
    REJECT(TerminalSurface_bind(nullptr, &back, &front, sink, &log), "bind");
    REJECT(TerminalSurface_bind(&surface, nullptr, &front, sink, &log), "bind");
    REJECT(TerminalSurface_bind(&surface, &back, nullptr, sink, &log), "bind");
    REJECT(TerminalSurface_bind(&surface, &back, &front, nullptr, &log), "bind");
    TerminalCanvas empty = TerminalCanvas_0();
    REJECT(TerminalSurface_bind(&surface, &empty, &empty, sink, &log), "bind");
    REJECT(TerminalSurface_bind(&surface, &back, &empty, sink, &log), "bind");
    TerminalCanvas subBack = TerminalCanvas_sub(&back, 0, 0, W - 2, H);
    REJECT(TerminalSurface_bind(&surface, &subBack, &front, sink, &log), "bind");   // region differs
    u32 thinCells[CELLS];
    u16 thinStyles[CELLS];
    TerminalCanvas thin = TerminalCanvas(thinCells, thinStyles, W, H);              // no colour planes
    REJECT(TerminalSurface_bind(&surface, &back, &thin, sink, &log), "bind");       // planes differ
    assert(!TerminalSurface_isBound(&surface));
    assert(TerminalSurface_presentCount(&surface) == 0);
    REJECT(TerminalSurface_reset(&surface), "unbound");

    // --- First paint: the whole delta, one run per row ---
    assert(TerminalSurface_setSink(&surface, sink, &log));
    assert(TerminalSurface_setGapTolerance(&surface, TERMINAL_SURFACE_GAP_DEFAULT));
    clearBoth(&back, &front);
    TerminalCanvas_text(&back, 1, 0, "abc", 3, TerminalPen());
    TerminalCanvas_text(&back, 4, 2, "xy", 2, TerminalPen());
    assert(TerminalSurface_bind(&surface, &back, &front, sink, &log));
    assert(TerminalSurface_isBound(&surface));
    assert(TerminalSurface_getBack(&surface).cells == backCells);
    assert(TerminalSurface_getGapTolerance(&surface) == TERMINAL_SURFACE_GAP_DEFAULT);

    TerminalPresentResult first = TerminalSurface_present(&surface);
    assert(first.ok && first.changed);
    assert(first.changedCells == 5 && first.runs == 2 && first.emittedCells == 5);
    assert(log.calls == 2 && log.runs == 2 && log.cells == 5);
    assert(log.first.x == 1 && log.first.y == 0 && log.first.length == 3);
    assert(log.firstGlyphs[0] == 'a' && log.firstGlyphs[1] == 'b' && log.firstGlyphs[2] == 'c');
    assert(log.first.styles != nullptr && log.first.fg != nullptr && log.first.bg != nullptr);
    assert(log.last.x == 4 && log.last.y == 2 && log.last.length == 2);
    assert(log.lastGlyphs[0] == 'x' && log.lastGlyphs[1] == 'y');
    assert(TerminalSurface_presentCount(&surface) == 1);
    assert(TerminalSurface_changedCells(&surface) == 5 && TerminalSurface_changedRuns(&surface) == 2);
    // Adoption: the front now records what the host was told.
    assert(TerminalCanvas_glyphAt(&front, 1, 0) == 'a' && TerminalCanvas_glyphAt(&front, 5, 2) == 'y');
    assert(TerminalCanvas_glyphAt(&front, 0, 0) == ' ');

    // --- A clean frame costs the host nothing at all ---
    RunLog idle = {0};
    assert(TerminalSurface_setSink(&surface, sink, &idle));
    TerminalPresentResult quiet = TerminalSurface_present(&surface);
    assert(quiet.ok && !quiet.changed && quiet.runs == 0 && quiet.emittedCells == 0);
    assert(idle.calls == 0 && idle.runs == 0);
    assert(TerminalSurface_changedCells(&surface) == 0 && TerminalSurface_changedRuns(&surface) == 0);

    // --- Minimal delta: one cell, one run, exact coordinates ---
    TerminalCanvas_glyph(&back, 7, 1, 'Z', TerminalPen());
    RunLog one = {0};
    assert(TerminalSurface_setSink(&surface, sink, &one));
    TerminalPresentResult single = TerminalSurface_present(&surface);
    assert(single.ok && single.changed && single.changedCells == 1);
    assert(single.runs == 1 && single.emittedCells == 1);
    assert(one.last.x == 7 && one.last.y == 1 && one.last.length == 1 && one.lastGlyphs[0] == 'Z');

    // --- Gap tolerance batches; it never changes content ---
    // Two changes with one unchanged cell between them: tight splits, 1 joins.
    clearBoth(&back, &front);
    assert(TerminalSurface_reset(&surface));                     // front := back, clean grid
    TerminalCanvas_glyph(&back, 0, 0, 'A', TerminalPen());
    TerminalCanvas_glyph(&back, 2, 0, 'B', TerminalPen());
    assert(TerminalSurface_setGapTolerance(&surface, 0));
    RunLog tight = {0};
    assert(TerminalSurface_setSink(&surface, sink, &tight));
    TerminalPresentResult tightResult = TerminalSurface_present(&surface);
    assert(tightResult.changedCells == 2 && tightResult.runs == 2 && tightResult.emittedCells == 2);
    // Revert, adopt, then re-apply the same two changes with tolerance 1.
    TerminalCanvas_glyph(&back, 0, 0, ' ', TerminalPen());
    TerminalCanvas_glyph(&back, 2, 0, ' ', TerminalPen());
    assert(TerminalSurface_present(&surface).changedCells == 2);
    TerminalCanvas_glyph(&back, 0, 0, 'A', TerminalPen());
    TerminalCanvas_glyph(&back, 2, 0, 'B', TerminalPen());
    assert(TerminalSurface_setGapTolerance(&surface, 1));
    RunLog merged = {0};
    assert(TerminalSurface_setSink(&surface, sink, &merged));
    TerminalPresentResult mergedResult = TerminalSurface_present(&surface);
    assert(mergedResult.changedCells == 2 && mergedResult.runs == 1 && mergedResult.emittedCells == 3);
    assert(merged.last.x == 0 && merged.last.y == 0 && merged.last.length == 3);
    // The unchanged gap cell rides along carrying its OWN existing value, which is
    // why a widened run is an idempotent re-send and never a content change.
    assert(merged.lastGlyphs[1] == TerminalCanvas_glyphAt(&back, 1, 0));
    assert(merged.lastGlyphs[0] == 'A' && merged.lastGlyphs[1] == ' ' && merged.lastGlyphs[2] == 'B');
    // A huge tolerance still never joins two rows.
    assert(TerminalSurface_setGapTolerance(&surface, 64));
    assert(TerminalSurface_getGapTolerance(&surface) == 64);
    TerminalCanvas_glyph(&back, 3, 1, 'p', TerminalPen());
    TerminalCanvas_glyph(&back, 3, 2, 'q', TerminalPen());
    RunLog rows = {0};
    assert(TerminalSurface_setSink(&surface, sink, &rows));
    TerminalPresentResult rowResult = TerminalSurface_present(&surface);
    assert(rowResult.changedCells == 2 && rowResult.runs == 2 && rowResult.emittedCells == 2);
    assert(rows.first.y == 1 && rows.last.y == 2);

    // --- Plane diffing: style and colour count as change, glyph-only too ---
    clearBoth(&back, &front);
    assert(TerminalSurface_reset(&surface));
    TerminalCanvas_glyph(&back, 2, 1, ' ', TerminalPen(TERMINAL_STYLE_BOLD, TERMINAL_RGB(9, 8, 7), TERMINAL_RGB(6, 5, 4)));
    RunLog styled = {0};
    assert(TerminalSurface_setSink(&surface, sink, &styled));
    TerminalPresentResult styledResult = TerminalSurface_present(&surface);
    assert(styledResult.ok && styledResult.changedCells == 1 && styledResult.runs == 1);
    assert(styled.last.cells[0] == ' ');                       // same glyph...
    assert(styled.last.styles[0] == TERMINAL_STYLE_BOLD);      // ...different style
    assert(styled.last.fg[0] == TERMINAL_RGB(9, 8, 7));
    assert(styled.last.bg[0] == TERMINAL_RGB(6, 5, 4));

    // --- Wide glyph: the continuation cell is a cell too ---
    assert(TerminalSurface_reset(&surface));
    clearBoth(&back, &front);
    TerminalCanvas_text(&back, 0, 0, "\xE6\xBC\xA2", 3, TerminalPen());   // 漢 spans two cells
    RunLog wide = {0};
    assert(TerminalSurface_setSink(&surface, sink, &wide));
    TerminalPresentResult wideResult = TerminalSurface_present(&surface);
    assert(wideResult.changedCells == 2 && wideResult.runs == 1 && wideResult.emittedCells == 2);
    assert(wide.last.cells[0] == 0x6F22u && wide.last.cells[1] == 0u);

    // --- A plane the pair does not carry is not a dimension (thin canvas) ---
    u32 thinBackCells[CELLS], thinFrontCells[CELLS];
    u16 thinBackStyles[CELLS], thinFrontStyles[CELLS];
    TerminalCanvas thinBack = TerminalCanvas(thinBackCells, thinBackStyles, W, H);
    TerminalCanvas thinFront = TerminalCanvas(thinFrontCells, thinFrontStyles, W, H);
    clearBoth(&thinBack, &thinFront);
    TerminalCanvas_glyph(&thinBack, 0, 0, 'k', TerminalPen(TERMINAL_STYLE_ACCENT));
    TerminalSurface thinSurface = TerminalSurface_0();
    RunLog thinLog = {0};
    assert(TerminalSurface_bind(&thinSurface, &thinBack, &thinFront, sink, &thinLog));
    TerminalPresentResult thinResult = TerminalSurface_present(&thinSurface);
    assert(thinResult.ok && thinResult.changedCells == 1 && thinResult.runs == 1);
    assert(thinLog.last.styles != nullptr && thinLog.last.styles[0] == TERMINAL_STYLE_ACCENT);
    assert(thinLog.last.fg == nullptr && thinLog.last.bg == nullptr);
    // Adoption ran over a style plane with NO colour planes present: the front now
    // records the glyph, which is the exact path that must not touch a null plane.
    assert(TerminalCanvas_glyphAt(&thinFront, 0, 0) == 'k');
    assert(TerminalCanvas_styleAt(&thinFront, 0, 0) == TERMINAL_STYLE_ACCENT);

    // --- Region bind: only the region is scanned, coordinates stay absolute ---
    enum { BW = 16, BH = 6 };
    u32 bigBackCells[BW * BH], bigFrontCells[BW * BH];
    u16 bigBackStyles[BW * BH], bigFrontStyles[BW * BH];
    TerminalCanvas bigBack = TerminalCanvas(bigBackCells, bigBackStyles, BW, BH);
    TerminalCanvas bigFront = TerminalCanvas(bigFrontCells, bigFrontStyles, BW, BH);
    clearBoth(&bigBack, &bigFront);
    TerminalCanvas_glyph(&bigBack, 0, 0, '@', TerminalPen());   // OUTSIDE the region, differing
    TerminalCanvas bigBackRegion = TerminalCanvas_sub(&bigBack, 2, 1, 6, 3);
    TerminalCanvas bigFrontRegion = TerminalCanvas_sub(&bigFront, 2, 1, 6, 3);
    TerminalCanvas_glyph(&bigBackRegion, 0, 0, '#', TerminalPen());
    TerminalSurface regionSurface = TerminalSurface_0();
    RunLog regionLog = {0};
    assert(TerminalSurface_bind(&regionSurface, &bigBackRegion, &bigFrontRegion, sink, &regionLog));
    TerminalPresentResult regionResult = TerminalSurface_present(&regionSurface);
    assert(regionResult.ok && regionResult.changedCells == 1 && regionResult.runs == 1);
    assert(regionLog.last.x == 2 && regionLog.last.y == 1);     // absolute, offset by the region
    assert(regionLog.lastGlyphs[0] == '#');
    assert(TerminalCanvas_glyphAt(&bigFront, 0, 0) == ' ');     // outside stayed untouched

    // --- Failure atomicity: a refused run preserves front and is re-offered ---
    clearBoth(&back, &front);
    assert(TerminalSurface_reset(&surface));                     // front := back, fresh epoch
    TerminalCanvas_glyph(&back, 0, 0, 'X', TerminalPen());
    TerminalCanvas_glyph(&back, 4, 1, 'Y', TerminalPen());
    RunLog failing = {0};
    failing.failFromCall = 2;                                   // refuse the second run
    assert(TerminalSurface_setSink(&surface, sink, &failing));
    beginDiagnostics();
    TerminalPresentResult refused = TerminalSurface_present(&surface);
    endDiagnostics(1, "sink refused");
    assert(!refused.ok && refused.changed && refused.changedCells == 2 && refused.runs == 2);
    assert(!TerminalSurface_isActive(&surface));
    assert(TerminalSurface_exhaustionCount(&surface) == 1);
    // Nothing was adopted, so the very same delta is offered on the retry.
    assert(TerminalCanvas_glyphAt(&front, 0, 0) == ' ' && TerminalCanvas_glyphAt(&front, 4, 1) == ' ');
    failing.failFromCall = 0;                                   // the sink recovers
    TerminalPresentResult retried = TerminalSurface_present(&surface);
    assert(retried.ok && retried.changedCells == 2 && retried.runs == 2);
    assert(TerminalCanvas_glyphAt(&front, 0, 0) == 'X' && TerminalCanvas_glyphAt(&front, 4, 1) == 'Y');
    assert(TerminalSurface_exhaustionCount(&surface) == 1);      // a healthy retry adds no refusal

    // --- Exhaustion Loudness: report once per epoch, count every refusal ---
    assert(TerminalSurface_reset(&surface));                     // fresh epoch
    assert(TerminalSurface_exhaustionCount(&surface) == 0);
    TerminalCanvas_glyph(&back, 1, 0, 'Q', TerminalPen());
    RunLog always = {0};
    always.failFromCall = 1;
    assert(TerminalSurface_setSink(&surface, sink, &always));
    beginDiagnostics();
    assert(!TerminalSurface_present(&surface).ok);
    assert(!TerminalSurface_present(&surface).ok);               // same epoch: no second line
    endDiagnostics(1, "sink refused");
    assert(TerminalSurface_exhaustionCount(&surface) == 2);      // but both are counted
    assert(TerminalSurface_changedCells(&surface) == 1);         // the scan still reports the delta

    // --- Reentrancy: a nested present refuses; the outer run still lands ---
    assert(TerminalSurface_reset(&surface));
    TerminalCanvas_glyph(&back, 0, 0, 'R', TerminalPen());
    RunLog nested = {0};
    nested.surface = &surface;
    assert(TerminalSurface_setSink(&surface, reentrantSink, &nested));
    beginDiagnostics();
    TerminalPresentResult outer = TerminalSurface_present(&surface);
    endDiagnostics(1, "unbound or reentrant");
    assert(outer.ok && outer.runs == 1 && nested.reentered == 1);
    assert(TerminalSurface_exhaustionCount(&surface) == 1);
    assert(!TerminalSurface_isActive(&surface));

    // --- The cold doors refuse while the hot path runs ---
    assert(TerminalSurface_reset(&surface));
    TerminalCanvas_glyph(&back, 0, 0, 'S', TerminalPen());
    RunLog busy = {0};
    busy.surface = &surface;
    assert(TerminalSurface_setSink(&surface, busyProbeSink, &busy));
    beginDiagnostics();
    assert(TerminalSurface_present(&surface).ok);
    endDiagnostics(2, "busy");
    assert(busy.calls == 1);

    // --- Unbound refusals count and report once per epoch ---
    TerminalSurface unbound = TerminalSurface_0();
    beginDiagnostics();
    TerminalPresentResult nothing = TerminalSurface_present(&unbound);
    endDiagnostics(1, "unbound or reentrant");
    assert(!nothing.ok && !nothing.changed && nothing.runs == 0);
    assert(TerminalSurface_exhaustionCount(&unbound) == 1);
    beginDiagnostics();
    assert(!TerminalSurface_present(&unbound).ok);
    endDiagnostics(0, nullptr);
    assert(TerminalSurface_exhaustionCount(&unbound) == 2);

    // --- The hot path is quiet and bounded over many clean frames ---
    assert(TerminalSurface_reset(&surface));
    RunLog silent = {0};
    assert(TerminalSurface_setSink(&surface, sink, &silent));
    bytes presentsBefore = TerminalSurface_presentCount(&surface);
    beginDiagnostics();
    for (bytes i = 0; i < 1000; ++i) {
        TerminalPresentResult clean = TerminalSurface_present(&surface);
        assert(clean.ok && !clean.changed && clean.runs == 0);
    }
    endDiagnostics(0, nullptr);
    assert(silent.calls == 0);
    assert(TerminalSurface_presentCount(&surface) == presentsBefore + 1000);

    // --- Determinism: identically prepared surfaces agree exactly ---
    u32 detBackA[CELLS], detFrontA[CELLS], detBackB[CELLS], detFrontB[CELLS];
    u16 detBackStyleA[CELLS], detFrontStyleA[CELLS], detBackStyleB[CELLS], detFrontStyleB[CELLS];
    TerminalCanvas detA = TerminalCanvas(detBackA, detBackStyleA, W, H);
    TerminalCanvas detAFront = TerminalCanvas(detFrontA, detFrontStyleA, W, H);
    TerminalCanvas detB = TerminalCanvas(detBackB, detBackStyleB, W, H);
    TerminalCanvas detBFront = TerminalCanvas(detFrontB, detFrontStyleB, W, H);
    clearBoth(&detA, &detAFront);
    clearBoth(&detB, &detBFront);
    TerminalCanvas_text(&detA, 2, 1, "xy", 2, TerminalPen());
    TerminalCanvas_text(&detB, 2, 1, "xy", 2, TerminalPen());
    TerminalSurface surfaceA = TerminalSurface_0();
    TerminalSurface surfaceB = TerminalSurface_0();
    RunLog logA = {0}, logB = {0};
    assert(TerminalSurface_bind(&surfaceA, &detA, &detAFront, sink, &logA));
    assert(TerminalSurface_bind(&surfaceB, &detB, &detBFront, sink, &logB));
    TerminalPresentResult repeatA = TerminalSurface_present(&surfaceA);
    TerminalPresentResult repeatB = TerminalSurface_present(&surfaceB);
    assert(repeatA.changedCells == repeatB.changedCells && repeatA.runs == repeatB.runs
           && repeatA.emittedCells == repeatB.emittedCells);
    assert(logA.last.x == logB.last.x && logA.last.y == logB.last.y && logA.last.length == logB.last.length);

    // --- A present is a TRANSACTION: bracketed, lazily, and always closed ---
    u32 txnBackCells[CELLS], txnFrontCells[CELLS];
    u16 txnBackStyles[CELLS], txnFrontStyles[CELLS];
    TerminalCanvas txnBack = TerminalCanvas(txnBackCells, txnBackStyles, W, H);
    TerminalCanvas txnFront = TerminalCanvas(txnFrontCells, txnFrontStyles, W, H);
    TerminalCanvas_fill(&txnBack, ' ', TerminalPen());
    TerminalCanvas_fill(&txnFront, ' ', TerminalPen());
    TerminalSurface txn = TerminalSurface_0();
    RunLog txnLog = {0};
    assert(TerminalSurface_bind(&txn, &txnBack, &txnFront, sink, &txnLog));
    FrameLog frames = {0};
    assert(TerminalSurface_setFrame(&txn, frameBracket, &frames));
    assert(TerminalSurface_getFrame(&txn) == frameBracket);
    assert(TerminalSurface_getFrameContext(&txn) == &frames);
    assert(!TerminalSurface_isBracketed(&txn));

    TerminalCanvas_glyph(&txnBack, 0, 0, 'x', TerminalPen());
    beginDiagnostics();
    TerminalPresentResult txnResult = TerminalSurface_present(&txn);
    endDiagnostics(0, nullptr);
    assert(txnResult.ok && txnResult.changed && txnResult.transactional);
    assert(frames.begins == 1 && frames.ends == 1 && !frames.outOfOrder);
    assert(!TerminalSurface_isBracketed(&txn));   // and closed again afterwards

    // A frame that changes NOTHING opens no transaction at all, so an idle frame stays
    // completely silent rather than bracketing empty work.
    TerminalPresentResult txnQuiet = TerminalSurface_present(&txn);
    assert(txnQuiet.ok && !txnQuiet.changed && !txnQuiet.transactional);
    assert(frames.begins == 1 && frames.ends == 1);

    // A sink that refuses part-way must STILL close the transaction: a device left
    // inside a half-finished frame is a device left frozen.
    TerminalCanvas_glyph(&txnBack, 1, 0, 'y', TerminalPen());
    RunLog refusing = {0};
    refusing.failFromCall = 1;
    assert(TerminalSurface_setSink(&txn, sink, &refusing));
    frames.begins = frames.ends = frames.order = 0;
    beginDiagnostics();
    TerminalPresentResult txnRefused = TerminalSurface_present(&txn);
    endDiagnostics(1, "sink refused");
    assert(!txnRefused.ok && txnRefused.transactional);
    assert(frames.begins == 1 && frames.ends == 1 && !frames.outOfOrder);
    assert(!TerminalSurface_isBracketed(&txn));

    // The bracket cannot be swapped while a transaction is open.
    FrameLog probing = {0};
    probing.probe = &txn;
    assert(TerminalSurface_setFrame(&txn, frameBracket, &probing));
    assert(TerminalSurface_setSink(&txn, sink, &txnLog));
    TerminalCanvas_glyph(&txnBack, 2, 0, 'z', TerminalPen());
    beginDiagnostics();
    TerminalPresentResult txnGuarded = TerminalSurface_present(&txn);
    endDiagnostics(1, "transaction is open");
    assert(txnGuarded.ok && txnGuarded.transactional && probing.swapRefused);
    REJECT(TerminalSurface_setFrame(nullptr, frameBracket, nullptr), "null bracket");
    REJECT(TerminalSurface_setFrame(&txn, nullptr, nullptr), "null bracket");
    assert(!TerminalSurface_isBracketed(nullptr));

    // --- Gap tolerance configuration has no fabricated ceiling ---
    assert(TerminalSurface_setGapTolerance(&surface, 3) && TerminalSurface_getGapTolerance(&surface) == 3);
    assert(TerminalSurface_setGapTolerance(&surface, 0) && TerminalSurface_getGapTolerance(&surface) == 0);
    assert(TerminalSurface_setGapTolerance(&surface, SIZE_MAX));
    assert(TerminalSurface_getGapTolerance(&surface) == SIZE_MAX);
    assert(TerminalSurface_setGapTolerance(&surface, TERMINAL_SURFACE_GAP_DEFAULT));
    REJECT(TerminalSurface_setGapTolerance(nullptr, 1), "gapTolerance");
    REJECT(TerminalSurface_setSink(nullptr, sink, &silent), "setSink");
    REJECT(TerminalSurface_setSink(&surface, nullptr, nullptr), "setSink");

    // --- Projections: value string, struct dump mirroring STRUCT FIELDS ---
    TERMINAL_PROJECT(TerminalSurface, &surface);
    TERMINAL_PROJECT(TerminalSurface, &unbound);
    TerminalProjection value = TerminalSurface_toString(&surface);
    assert(strstr(value.text, "TerminalSurface{presented=") != nullptr);
    TerminalProjection structured = TerminalSurface_toStringStruct(&surface);
    assert(strstr(structured.text, "gapTolerance=") != nullptr);
    assert(strstr(structured.text, "refusalCount=") != nullptr);
    assert(strstr(structured.text, "emittedCells=") != nullptr);
    assert(strstr(structured.text, "sink=bound") != nullptr);
    assert(strstr(TerminalSurface_toStringStruct(&unbound).text, "sink=none") != nullptr);

    return 0;
}
