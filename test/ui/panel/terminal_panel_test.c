// Owner: TerminalPanel — the container. Layout flavours, child identity and
// staleness, add/remove/detach ownership, FLEX reserve flow against semicolon's
// measured IDE geometry, LAYERED stacking, total repaint with no dirty tracking,
// pen inheritance, background fill, the cold hit-test queries, cold growth, the
// reentrancy guard against a cyclic tree, and the projections.
//
// The grounding case is semicolon's IDE example (103x23, root pipes at 1/29/103,
// editor pipes at 1/29/36/103) expressed as a real child tree, with the centre as
// a nested panel resolving over the sub-canvas its parent handed it.
//
// Scope note: cycle safety is proved for PAINT only. An indirect cycle makes
// teardown undefined, so the tree is acyclic by contract; a direct self-add is
// refused outright. Layer-growth allocation failure is unreachable in-process.
#include "ui/panel/terminal_panel.h"
#include "ui/text/terminal_text.h"
#include "format/terminal_color.h"
#include "determinant/test/support.h"

enum { W = 24, H = 8 };

static bool rectEqual(TerminalRect a, TerminalRect b) {
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}
static TerminalRect rectOf(size_t x, size_t y, size_t w, size_t h) {
    return (TerminalRect) { x, y, w, h };
}

int main(void) {
    uint32_t cells[W * H];
    uint16_t styles[W * H];
    uint32_t fg[W * H];
    uint32_t bg[W * H];
    TerminalCanvas canvas = TerminalCanvas(cells, styles, fg, bg, W, H);

    // --- Constructor and null safety ---
    TerminalPanel *panel = TerminalPanel();
    assert(panel != nullptr);
    assert(TerminalPanel_childCount(panel) == 0 && TerminalPanel_capacity(panel) == 0);
    assert(TerminalPanel_paintCount(panel) == 0 && TerminalPanel_exhaustionCount(panel) == 0);
    assert(!TerminalPanel_isPainting(panel));
    assert(TerminalPanel_getLayout(panel) == TERMINAL_PANEL_FLEX);
    assert(TerminalPanel_getStyle(panel) == TERMINAL_STYLE_NONE);
    assert(TerminalPanel_getForegroundColor(panel) == TERMINAL_COLOR_DEFAULT);
    assert(TerminalPanel_getBackgroundColor(panel) == TERMINAL_COLOR_DEFAULT);
    assert(TerminalPanel_getImage(panel) == nullptr);
    assert(TerminalPanel_getFit(panel) == TERMINAL_PANEL_IMAGE_STRETCH);
    TerminalPanel *zeroed = TerminalPanel_zero();
    assert(zeroed != nullptr && TerminalPanel_childCount(zeroed) == 0);
    TerminalPanel_free(zeroed);
    TerminalPanel_free(nullptr);
    TerminalPanel_clearExhaustion(nullptr);

    assert(TerminalPanel_childCount(nullptr) == 0 && TerminalPanel_capacity(nullptr) == 0);
    assert(TerminalPanel_paintCount(nullptr) == 0 && TerminalPanel_exhaustionCount(nullptr) == 0);
    assert(!TerminalPanel_isPainting(nullptr));
    assert(!TerminalPanel_isValid(nullptr, (TerminalChildId) { 0, 1 }));
    assert(!TerminalPanel_isChildId((TerminalChildId) { 0, 0 }));
    assert(!TerminalPanel_isChildId((TerminalChildId) { 4, 0 }));
    assert(TerminalPanel_isChildId((TerminalChildId) { 0, 1 }));
    assert(TerminalPanel_getLayout(nullptr) == TERMINAL_PANEL_FLEX);
    assert(TerminalPanel_getStyle(nullptr) == TERMINAL_STYLE_NONE);
    assert(TerminalPanel_getImage(nullptr) == nullptr);
    assert(TerminalPanel_getPen(nullptr).style == TERMINAL_STYLE_NONE);
    assert(TerminalPanel_getAnchor(nullptr, (TerminalChildId) { 0, 1 }).kind == TERMINAL_ANCHOR_FILL);
    assert(TerminalPanel_childPanel(nullptr, (TerminalChildId) { 0, 1 }) == nullptr);
    assert(TerminalPanel_childText(nullptr, (TerminalChildId) { 0, 1 }) == nullptr);
    assert(TerminalPanel_detachPanel(nullptr, (TerminalChildId) { 0, 1 }) == nullptr);
    assert(TerminalPanel_detachText(nullptr, (TerminalChildId) { 0, 1 }) == nullptr);
    assert(TerminalPanel_childRegion(nullptr, &canvas, (TerminalChildId) { 0, 1 }).width == 0);
    assert(!TerminalPanel_isChildId(TerminalPanel_childAt(nullptr, &canvas, 0, 0)));
    // The hot entry guard is quiet and counts nothing.
    beginDiagnostics();
    TerminalPanel_paint(nullptr, &canvas);
    TerminalPanel_paint(panel, nullptr);
    TerminalCanvas invalid = TerminalCanvas_0();
    TerminalPanel_paint(panel, &invalid);
    endDiagnostics(0, nullptr);
    assert(TerminalPanel_paintCount(panel) == 0);

    // --- Layout: one field, two flavours, unknown rejected ---
    assert(TerminalPanel_setLayout(panel, TERMINAL_PANEL_LAYERED));
    assert(TerminalPanel_getLayout(panel) == TERMINAL_PANEL_LAYERED);
    assert(TerminalPanel_setLayout(panel, TERMINAL_PANEL_FLEX));
    REJECT(TerminalPanel_setLayout(panel, (TerminalPanelLayout) 9), "layout");
    assert(TerminalPanel_getLayout(panel) == TERMINAL_PANEL_FLEX);   // preserved
    REJECT(TerminalPanel_setLayout(nullptr, TERMINAL_PANEL_FLEX), "layout");

    // --- Children: identity, kind, ownership transfer, and the FLEX flow ---
    TerminalText *title = TerminalText("title");
    TerminalPanel *rail = TerminalPanel();
    TerminalText *centre = TerminalText("centre");
    TerminalChildId titleId = TerminalPanel_addText(panel, title, TerminalAnchor(TERMINAL_ANCHOR_BAND_TOP, 1));
    TerminalChildId railId = TerminalPanel_addPanel(panel, rail, TerminalAnchor(TERMINAL_ANCHOR_BAND_LEFT, 8));
    TerminalChildId centreId = TerminalPanel_addText(panel, centre, TerminalAnchor());
    assert(TerminalPanel_isChildId(titleId) && TerminalPanel_isValid(panel, titleId));
    assert(TerminalPanel_isValid(panel, railId) && TerminalPanel_isValid(panel, centreId));
    assert(TerminalPanel_childCount(panel) == 3);
    assert(TerminalPanel_capacity(panel) == TERMINAL_PANEL_CHILDREN_DEFAULT);
    assert(TerminalPanel_childKind(panel, railId) == TERMINAL_COMPONENT_PANEL);
    assert(TerminalPanel_childKind(panel, titleId) == TERMINAL_COMPONENT_TEXT);
    assert(TerminalPanel_childPanel(panel, railId) == rail);
    assert(TerminalPanel_childText(panel, titleId) == title);
    assert(TerminalPanel_childPanel(panel, titleId) == nullptr);
    assert(TerminalPanel_childText(panel, railId) == nullptr);
    assert(TerminalPanel_getAnchor(panel, titleId).kind == TERMINAL_ANCHOR_BAND_TOP);
    assert(TerminalPanel_getAnchor(panel, titleId).extent == 1);
    assert(!TerminalPanel_isValid(panel, (TerminalChildId) { 0, 0 }));
    assert(!TerminalPanel_isValid(panel, (TerminalChildId) { 99, 1 }));
    assert(!TerminalPanel_isValid(panel, (TerminalChildId) { 0, 2 }));   // wrong generation
    REJECT(TerminalPanel_isChildId(TerminalPanel_addText(panel, nullptr, TerminalAnchor())), "null child");
    REJECT(TerminalPanel_isChildId(TerminalPanel_addPanel(panel, nullptr, TerminalAnchor())), "null child");
    REJECT(TerminalPanel_isChildId(TerminalPanel_addPanel(panel, panel, TerminalAnchor())), "itself");
    assert(TerminalPanel_childCount(panel) == 3);   // every rejection left the tree alone

    // FLEX: the title takes row 0, the rail the next 8 columns, the centre the rest.
    assert(rectEqual(TerminalPanel_childRegion(panel, &canvas, titleId), rectOf(0, 0, W, 1)));
    assert(rectEqual(TerminalPanel_childRegion(panel, &canvas, railId), rectOf(0, 1, 8, H - 1)));
    assert(rectEqual(TerminalPanel_childRegion(panel, &canvas, centreId), rectOf(8, 1, W - 8, H - 1)));

    // LAYERED: the cursor never shrinks, so every child resolves against the whole
    // area — a FILL child covers it, a BAND child keeps its own slice OF it. This
    // is where the two flavours visibly diverge.
    assert(TerminalPanel_setLayout(panel, TERMINAL_PANEL_LAYERED));
    assert(rectEqual(TerminalPanel_childRegion(panel, &canvas, titleId), rectOf(0, 0, W, 1)));
    assert(rectEqual(TerminalPanel_childRegion(panel, &canvas, railId), rectOf(0, 0, 8, H)));
    assert(rectEqual(TerminalPanel_childRegion(panel, &canvas, centreId), rectOf(0, 0, W, H)));
    assert(TerminalPanel_setLayout(panel, TERMINAL_PANEL_FLEX));

    // --- Cold hit-testing, top-most first ---
    assert(TerminalChildId_equal(TerminalPanel_childAt(panel, &canvas, 0, 0), titleId));
    assert(TerminalChildId_equal(TerminalPanel_childAt(panel, &canvas, 0, 4), railId));
    assert(TerminalChildId_equal(TerminalPanel_childAt(panel, &canvas, 12, 4), centreId));
    assert(!TerminalPanel_isChildId(TerminalPanel_childAt(panel, &canvas, 0, 20)));
    assert(!TerminalPanel_isChildId(TerminalPanel_childAt(panel, &invalid, 0, 0)));

    // --- Paint: total, ordered, and it really does repaint everything ---
    TerminalPanel *stack = TerminalPanel();
    assert(TerminalPanel_setLayout(stack, TERMINAL_PANEL_LAYERED));
    TerminalChildId underId = TerminalPanel_addText(stack, TerminalText("under"), TerminalAnchor());
    TerminalChildId overId = TerminalPanel_addText(stack, TerminalText("over!"), TerminalAnchor());
    assert(TerminalChildId_equal(TerminalPanel_childAt(stack, &canvas, 0, 0), overId));
    TerminalCanvas_fill(&canvas, ' ', TerminalPen());
    beginDiagnostics();
    TerminalPanel_paint(stack, &canvas);
    endDiagnostics(0, nullptr);
    assert(TerminalCanvas_glyphAt(&canvas, 0, 0) == 'o');      // the later child is on top
    assert(TerminalPanel_paintCount(stack) == 1);
    // A stale glyph in the buffer is overwritten: there is no dirty tracking, so
    // every paint rewrites every child.
    TerminalCanvas_glyph(&canvas, 0, 0, 'Z', TerminalPen());
    assert(TerminalCanvas_glyphAt(&canvas, 0, 0) == 'Z');
    TerminalPanel_paint(stack, &canvas);
    assert(TerminalCanvas_glyphAt(&canvas, 0, 0) == 'o');
    assert(TerminalPanel_paintCount(stack) == 2);
    assert(TerminalChildId_equal(TerminalPanel_childAt(stack, &canvas, 0, 0), overId));

    // --- The panel's pen reaches its children; its background fills the area ---
    TerminalPanel *styled = TerminalPanel();
    assert(TerminalPanel_setBackgroundColor(styled, TERMINAL_RGB(0x10, 0x10, 0x10)));
    assert(TerminalPanel_setForegroundColor(styled, TERMINAL_RGB(0xD0, 0xD0, 0xD0)));
    assert(TerminalPanel_setStyle(styled, TERMINAL_STYLE_MUTED));
    assert(TerminalPanel_getBackgroundColor(styled) == TERMINAL_RGB(0x10, 0x10, 0x10));
    assert(TerminalPanel_getStyle(styled) == TERMINAL_STYLE_MUTED);
    TerminalChildId inheritId = TerminalPanel_addText(styled, TerminalText("hi"), TerminalAnchor());
    assert(TerminalPanel_isValid(styled, inheritId));
    TerminalCanvas_fill(&canvas, ' ', TerminalPen());
    TerminalPanel_paint(styled, &canvas);
    assert(TerminalCanvas_bgAt(&canvas, 20, 5) == TERMINAL_RGB(0x10, 0x10, 0x10));   // background filled
    assert(TerminalCanvas_glyphAt(&canvas, 0, 0) == 'h');
    assert(TerminalCanvas_fgAt(&canvas, 0, 0) == TERMINAL_RGB(0xD0, 0xD0, 0xD0));    // inherited foreground
    assert(TerminalCanvas_styleAt(&canvas, 0, 0) == TERMINAL_STYLE_MUTED);
    TerminalPen panelPen = TerminalPanel_getPen(styled);
    assert(panelPen.bg == TERMINAL_RGB(0x10, 0x10, 0x10) && panelPen.fg == TERMINAL_RGB(0xD0, 0xD0, 0xD0));
    REJECT(TerminalPanel_setStyle(nullptr, 0), "style");
    REJECT(TerminalPanel_setForegroundColor(nullptr, 0), "foreground");
    REJECT(TerminalPanel_setBackgroundColor(nullptr, 0), "background");
    REJECT(TerminalPanel_setPen(nullptr, TerminalPen()), "pen");
    REJECT(TerminalPanel_setImage(styled, nullptr, (TerminalPanelImageFit) 9), "fit");
    REJECT(TerminalPanel_setImage(nullptr, nullptr, TERMINAL_PANEL_IMAGE_STRETCH), "image");
    assert(TerminalPanel_setImage(styled, nullptr, TERMINAL_PANEL_IMAGE_CONTAIN));
    assert(TerminalPanel_getFit(styled) == TERMINAL_PANEL_IMAGE_CONTAIN);
    assert(TerminalPanel_setImage(styled, nullptr, TERMINAL_PANEL_IMAGE_STRETCH));

    // --- Removing frees the child; the slot is reused with a new generation ---
    size_t before = TerminalPanel_childCount(panel);
    assert(TerminalPanel_remove(panel, centreId));
    assert(TerminalPanel_childCount(panel) == before - 1);
    assert(!TerminalPanel_isValid(panel, centreId));
    assert(TerminalPanel_childText(panel, centreId) == nullptr);
    REJECT(TerminalPanel_remove(panel, centreId), "stale");
    REJECT(TerminalPanel_setAnchor(panel, centreId, TerminalAnchor()), "stale");
    TerminalChildId reused = TerminalPanel_addText(panel, TerminalText("again"), TerminalAnchor());
    assert(TerminalPanel_isValid(panel, reused));
    assert(reused.index == centreId.index && reused.generation != centreId.generation);
    assert(!TerminalPanel_isValid(panel, centreId));
    assert(!TerminalChildId_equal(centreId, reused));
    assert(TerminalChildId_equal(reused, reused));

    // --- Detach returns ownership for re-parenting, without freeing ---
    TerminalText *moved = TerminalPanel_detachText(panel, reused);
    assert(moved != nullptr);
    assert(TerminalPanel_childText(panel, reused) == nullptr);
    assert(!TerminalPanel_isValid(panel, reused));
    TerminalChildId reparented = TerminalPanel_addText(stack, moved, TerminalAnchor(TERMINAL_ANCHOR_BAND_BOTTOM, 1));
    assert(TerminalPanel_isChildId(reparented));
    assert(TerminalPanel_childText(stack, reparented) == moved);
    assert(TerminalPanel_detachPanel(panel, titleId) == nullptr);      // wrong kind
    assert(TerminalPanel_detachText(panel, railId) == nullptr);        // wrong kind
    TerminalText *orphan = TerminalText("orphan");
    REJECT(TerminalPanel_isChildId(TerminalPanel_addText(nullptr, orphan, TerminalAnchor())), "add");
    TerminalText_free(orphan);            // the refused add left it with the caller
    (void) underId;

    // --- Cold growth past the named default; every child survives ---
    TerminalPanel *grown = TerminalPanel();
    for (size_t i = 0; i < TERMINAL_PANEL_CHILDREN_DEFAULT * 3; ++i)
        assert(TerminalPanel_isChildId(TerminalPanel_addText(grown, TerminalText("x"), TerminalAnchor())));
    assert(TerminalPanel_childCount(grown) == TERMINAL_PANEL_CHILDREN_DEFAULT * 3);
    assert(TerminalPanel_capacity(grown) >= TERMINAL_PANEL_CHILDREN_DEFAULT * 3);
    assert(TerminalPanel_exhaustionCount(grown) == 0);
    beginDiagnostics();
    TerminalPanel_paint(grown, &canvas);
    endDiagnostics(0, nullptr);
    assert(TerminalPanel_paintCount(grown) == 1);
    TerminalPanel_free(grown);            // frees every text child recursively

    // --- A cyclic tree is refused by paint; breaking it makes teardown defined ---
    TerminalPanel *a = TerminalPanel();
    TerminalPanel *b = TerminalPanel();
    TerminalChildId bInA = TerminalPanel_addPanel(a, b, TerminalAnchor());
    TerminalChildId aInB = TerminalPanel_addPanel(b, a, TerminalAnchor());
    assert(TerminalPanel_isChildId(bInA) && TerminalPanel_isChildId(aInB));
    beginDiagnostics();
    TerminalPanel_paint(a, &canvas);      // a -> b.sh -> a: the inner paint refuses
    endDiagnostics(1, "reentrant");
    assert(TerminalPanel_exhaustionCount(a) == 1);
    assert(!TerminalPanel_isPainting(a) && !TerminalPanel_isPainting(b));
    beginDiagnostics();
    TerminalPanel_paint(a, &canvas);      // same epoch: counted, not re-reported
    endDiagnostics(0, nullptr);
    assert(TerminalPanel_exhaustionCount(a) == 2);
    assert(TerminalPanel_detachPanel(b, aInB) == a);   // break the cycle
    TerminalPanel_free(a);                             // frees b.sh, which no longer points back

    // --- GROUNDING: semicolon's IDE example as a real child tree, 103x23 ---
    static uint32_t ideCells[103 * 23];
    static uint16_t ideStyles[103 * 23];
    TerminalCanvas ide = TerminalCanvas(ideCells, ideStyles, 103, 23);
    TerminalPanel *ideRoot = TerminalPanel();
    TerminalChildId ideTitle = TerminalPanel_addText(ideRoot, TerminalText("vexgraph"),
                                                    TerminalAnchor(TERMINAL_ANCHOR_BAND_TOP, 1));
    TerminalChildId ideStatus = TerminalPanel_addText(ideRoot, TerminalText("[c23/java]"),
                                                     TerminalAnchor(TERMINAL_ANCHOR_BAND_BOTTOM, 1));
    TerminalPanel *ideCentre = TerminalPanel();
    TerminalChildId ideCentreId = TerminalPanel_addPanel(ideRoot, ideCentre,
                                                         TerminalAnchor(TERMINAL_ANCHOR_BAND_LEFT, 29));
    assert(rectEqual(TerminalPanel_childRegion(ideRoot, &ide, ideTitle), rectOf(0, 0, 103, 1)));
    assert(rectEqual(TerminalPanel_childRegion(ideRoot, &ide, ideStatus), rectOf(0, 22, 103, 1)));
    assert(rectEqual(TerminalPanel_childRegion(ideRoot, &ide, ideCentreId), rectOf(0, 1, 29, 21)));
    beginDiagnostics();
    TerminalPanel_paint(ideRoot, &ide);
    endDiagnostics(0, nullptr);
    assert(TerminalCanvas_glyphAt(&ide, 0, 0) == 'v');           // the title bar drew
    assert(TerminalCanvas_glyphAt(&ide, 0, 22) == '[');          // the status bar drew

    // The rail's own children resolve over the sub-canvas their parent handed them.
    TerminalRect railRegion = TerminalPanel_childRegion(ideRoot, &ide, ideCentreId);
    TerminalCanvas railCanvas = TerminalCanvas_sub(&ide, railRegion.x, railRegion.y,
                                                  railRegion.width, railRegion.height);
    assert(TerminalCanvas_getWidth(&railCanvas) == 29 && TerminalCanvas_getHeight(&railCanvas) == 21);
    TerminalChildId utils = TerminalPanel_addText(ideCentre, TerminalText("utils"),
                                                 TerminalAnchor(TERMINAL_ANCHOR_BAND_TOP, 1));
    TerminalChildId files = TerminalPanel_addText(ideCentre, TerminalText("files"), TerminalAnchor());
    assert(rectEqual(TerminalPanel_childRegion(ideCentre, &railCanvas, utils), rectOf(0, 0, 29, 1)));
    assert(rectEqual(TerminalPanel_childRegion(ideCentre, &railCanvas, files), rectOf(0, 1, 29, 20)));
    // ...and the nested panel really paints through the chain, into the root's grid.
    beginDiagnostics();
    TerminalPanel_paint(ideRoot, &ide);
    endDiagnostics(0, nullptr);
    assert(TerminalCanvas_glyphAt(&ide, 0, 1) == 'u');     // utils, one row down
    assert(TerminalCanvas_glyphAt(&ide, 0, 2) == 'f');     // files, below it
    TerminalPanel_free(ideRoot);          // frees the nested centre panel too

    // --- Projections: value string and a struct dump mirroring STRUCT FIELDS ---
    TERMINAL_PROJECT(TerminalPanel, panel);
    TerminalProjection structured = TerminalPanel_toStringStruct(panel);
    assert(strstr(structured.text, "layout=") != nullptr);
    assert(strstr(structured.text, "children=<owned>") != nullptr);
    assert(strstr(structured.text, "capacity=") != nullptr);
    assert(strstr(structured.text, "usedCount=") != nullptr);
    assert(strstr(structured.text, "refusalCount=") != nullptr);
    assert(strstr(structured.text, "painting=") != nullptr);
    assert(strstr(TerminalPanel_toString(panel).text, "TerminalPanel{layout=") != nullptr);

    TerminalPanel_free(panel);
    TerminalPanel_free(stack);
    TerminalPanel_free(styled);
    return 0;
}
