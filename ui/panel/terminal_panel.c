#include "vex.h"
#include "ui/panel/terminal_panel.h"
#include "ui/text/terminal_text.h"
#include "nio/mem.h"
#include "oop/oop.h"
#include <string.h>
#include <stdio.h>
#include "core/determinant_memory.h"

;;DEFINITION
/** The container. A panel holds children and arranges them, and that is all it
 * does: it has a pen, an optional background image, a layout flavour and a slot
 * array. Its children are panels and text leaves, and either may be a child of
 * either, so the interface is one tree and not a pile of special cases.
 *
 * Two flavours, one line apart. FLEX advances a cursor past each child that
 * reserves its slice, so a title band, a rail and a centre compose in insertion
 * order; LAYERED never advances it, so every child resolves against the whole
 * area and z-order is simply the order they were added. The anchor still decides
 * WHERE — a band child in a layered panel keeps its own slice of the full area,
 * which is what lets an overlay sit at a pin above a flowing layout. Neither
 * flavour owns any geometry of its own — TerminalAnchor answers where each child
 * lands.
 *
 * Resize is why the tree exists at all. Every rectangle is recomputed from the
 * canvas it is handed on every paint, so nothing stores a size and a resize is
 * just the next paint at new dimensions. A panel that cached a rectangle would
 * need invalidation, dirty flags and a second code path; this one needs none.
 *
 * Ownership transfers on add and returns on remove or free, recursively. The
 * panel never reaches into a child's state to draw it — it hands the child the
 * region it resolved and the pen it wants inherited, and the child decides. */
;;OVERVIEW
/** ========================================================================
 * CLASS: TerminalPanel (ui/panel/terminal_panel.c)
 * STRUCT FIELDS (Mirroring ui/panel/terminal_panel.h)
 * TerminalPanelLayout layout    FLEX (bands reserve) or LAYERED (stack)
 * TerminalChild *children       owned growable slot array
 * bytes capacity               admitted slots
 * bytes usedCount              live children
 * bytes paintCount             completed paints
 * bytes refusalCount           refusals in this epoch
 * TerminalPen pen               inherited by the children added directly here
 * const TerminalImage *image    borrowed background (nullable)
 * TerminalPanelImageFit fit     how the background fills the area
 * bool painting                 a paint is in flight (reentrancy guard)
 * SLOT RECORD: TerminalChild
 * bool used                     slot holds a live child
 * u32 generation           bumped on slot reuse, so a stale id is detectable
 * TerminalComponentKind kind    which payload is live
 * TerminalAnchor anchor         placement in the parent
 * TerminalPanel *panel          owned nested panel when kind == PANEL
 * TerminalText *text            owned text leaf when kind == TEXT
 * SLOT RECORD: TerminalChildId
 * bytes index                  slot index
 * u32 generation           the generation expected at that slot (0 = invalid)
 * PRIVATE HELPERS
 * TERMINAL_PANEL_NO_SLOT      sentinel index meaning "no live child here"
 * invalidId()                 the zero id every refusal returns
 * slotFor(self, id)           validate id -> slot, or NO_SLOT when unknown/stale
 * refuse(self, reason)        count a refusal, reporting once per epoch
 * growChildren(self)          cold doubling growth preserving every live child
 * freeChild(child)            release one child payload (recursively for panels)
 * paintImage(self, canvas)    borrowed background through its declared fit
 * paintChildren(self, canvas) place, then paint every child in slot order
 * FUNCTION REGISTRY (exports)
 * Constructor: TerminalPanel *TerminalPanel_0() [zero/arity chooser]
 * Lifecycle: fn TerminalPanel_free(self)
 * Layout: bool setLayout/getLayout(self)
 * Children: TerminalChildId addPanel/addText(self, child, anchor)
 *   bool remove(self, id)
 *   TerminalRect childRegion(self, canvas, id)
 *   TerminalChildId childAt(self, canvas, x, y)
 *   TerminalComponentKind childKind(self, id)
 *   TerminalPanel *childPanel(self, id), TerminalText *childText(self, id)
 * Configuration: bool setPen/getPen, setStyle/getStyle,
 *   setForegroundColor/getForegroundColor, setBackgroundColor/getBackgroundColor,
 *   setImage/getImage/getFit, setAnchor/getAnchor
 * Paint: fn TerminalPanel_paint(self, canvas)
 * Queries: bytes childCount/capacity/paintCount/exhaustionCount(self)
 *   bool isValid(self, id), isPainting(self), isChildId(id)
 *   bool TerminalChildId_equal(a, b.sh), fn clearExhaustion(self)
 * Projections: TerminalProjection TerminalPanel_toString/toStringStruct(self)
 * CONTRACT: cold operations validate the id and reject loudly without mutating
 * anything — an unknown, stale or retired id touches no child. Paint is hot and
 * total: one entry guard, no allocation, no logging, no per-cell validation, and
 * a reentrant paint is refused, counted and reported at most once per epoch.
 * Adding transfers ownership; a refused add leaves the child with the caller.
 * Freeing a panel frees its whole subtree. Template/type identity is not claimed
 * here: a panel is a panel, and its flavour is a field.
 * ======================================================================== */


;;GETTER
static inline TerminalAnchor TerminalChild_anchor(const TerminalChild *s) { return (*s).anchor; }
;;GETTER
static inline TerminalAnchor *TerminalChild_anchorMut(TerminalChild *s) { return &(*s).anchor; }
;;SETTER
static inline fn TerminalChild_setAnchor(TerminalChild *s, TerminalAnchor v) { (*s).anchor = v; }
;;GETTER
static inline u32 TerminalChild_generation(const TerminalChild *s) { return (*s).generation; }
;;GETTER
static inline u32 *TerminalChild_generationMut(TerminalChild *s) { return &(*s).generation; }
;;SETTER
static inline fn TerminalChild_setGeneration(TerminalChild *s, u32 v) { (*s).generation = v; }
;;GETTER
static inline TerminalComponentKind TerminalChild_kind(const TerminalChild *s) { return (*s).kind; }
;;GETTER
static inline TerminalComponentKind *TerminalChild_kindMut(TerminalChild *s) { return &(*s).kind; }
;;SETTER
static inline fn TerminalChild_setKind(TerminalChild *s, TerminalComponentKind v) { (*s).kind = v; }
;;GETTER
static inline TerminalPanel * TerminalChild_panel(const TerminalChild *s) { return (*s).panel; }
;;GETTER
static inline TerminalPanel * *TerminalChild_panelMut(TerminalChild *s) { return &(*s).panel; }
;;SETTER
static inline fn TerminalChild_setPanel(TerminalChild *s, TerminalPanel * v) { (*s).panel = v; }
;;GETTER
static inline TerminalText * TerminalChild_text(const TerminalChild *s) { return (*s).text; }
;;GETTER
static inline TerminalText * *TerminalChild_textMut(TerminalChild *s) { return &(*s).text; }
;;SETTER
static inline fn TerminalChild_setText(TerminalChild *s, TerminalText * v) { (*s).text = v; }
;;GETTER
static inline bool TerminalChild_used(const TerminalChild *s) { return (*s).used; }
;;GETTER
static inline bool *TerminalChild_usedMut(TerminalChild *s) { return &(*s).used; }
;;SETTER
static inline fn TerminalChild_setUsed(TerminalChild *s, bool v) { (*s).used = v; }

;;GETTER
static inline u32 TerminalPen_bg(const TerminalPen *s) { return (*s).bg; }
;;GETTER
static inline u32 *TerminalPen_bgMut(TerminalPen *s) { return &(*s).bg; }
;;SETTER
static inline fn TerminalPen_setBg(TerminalPen *s, u32 v) { (*s).bg = v; }
;;GETTER
static inline u32 TerminalPen_fg(const TerminalPen *s) { return (*s).fg; }
;;GETTER
static inline u32 *TerminalPen_fgMut(TerminalPen *s) { return &(*s).fg; }
;;SETTER
static inline fn TerminalPen_setFg(TerminalPen *s, u32 v) { (*s).fg = v; }
;;GETTER
static inline u16 TerminalPen_style(const TerminalPen *s) { return (*s).style; }
;;GETTER
static inline u16 *TerminalPen_styleMut(TerminalPen *s) { return &(*s).style; }
;;SETTER
static inline fn TerminalPen_setStyle(TerminalPen *s, u16 v) { (*s).style = v; }

;;GETTER
static inline bytes TerminalPanel_capacity(const TerminalPanel *s) { return (*s).capacity; }
;;GETTER
static inline bytes *TerminalPanel_capacityMut(TerminalPanel *s) { return &(*s).capacity; }
;;SETTER
static inline fn TerminalPanel_setCapacity(TerminalPanel *s, bytes v) { (*s).capacity = v; }
;;GETTER
static inline TerminalChild * TerminalPanel_children(const TerminalPanel *s) { return (*s).children; }
;;GETTER
static inline TerminalChild * *TerminalPanel_childrenMut(TerminalPanel *s) { return &(*s).children; }
;;SETTER
static inline fn TerminalPanel_setChildren(TerminalPanel *s, TerminalChild * v) { (*s).children = v; }
;;GETTER
static inline TerminalPanelImageFit TerminalPanel_fit(const TerminalPanel *s) { return (*s).fit; }
;;GETTER
static inline TerminalPanelImageFit *TerminalPanel_fitMut(TerminalPanel *s) { return &(*s).fit; }
;;SETTER
static inline fn TerminalPanel_setFit(TerminalPanel *s, TerminalPanelImageFit v) { (*s).fit = v; }
;;GETTER
static inline const TerminalImage * TerminalPanel_image(const TerminalPanel *s) { return (*s).image; }
;;GETTER
static inline const TerminalImage * *TerminalPanel_imageMut(TerminalPanel *s) { return &(*s).image; }
;;SETTER
static inline fn TerminalPanel_setImage(TerminalPanel *s, const TerminalImage * v) { (*s).image = v; }
;;GETTER
static inline TerminalPanelLayout TerminalPanel_layout(const TerminalPanel *s) { return (*s).layout; }
;;GETTER
static inline TerminalPanelLayout *TerminalPanel_layoutMut(TerminalPanel *s) { return &(*s).layout; }
;;SETTER
static inline fn TerminalPanel_setLayout(TerminalPanel *s, TerminalPanelLayout v) { (*s).layout = v; }
;;GETTER
static inline bytes TerminalPanel_paintCount(const TerminalPanel *s) { return (*s).paintCount; }
;;GETTER
static inline bytes *TerminalPanel_paintCountMut(TerminalPanel *s) { return &(*s).paintCount; }
;;SETTER
static inline fn TerminalPanel_setPaintCount(TerminalPanel *s, bytes v) { (*s).paintCount = v; }
;;GETTER
static inline bool TerminalPanel_painting(const TerminalPanel *s) { return (*s).painting; }
;;GETTER
static inline bool *TerminalPanel_paintingMut(TerminalPanel *s) { return &(*s).painting; }
;;SETTER
static inline fn TerminalPanel_setPainting(TerminalPanel *s, bool v) { (*s).painting = v; }
;;GETTER
static inline TerminalPen TerminalPanel_pen(const TerminalPanel *s) { return (*s).pen; }
;;GETTER
static inline TerminalPen *TerminalPanel_penMut(TerminalPanel *s) { return &(*s).pen; }
;;SETTER
static inline fn TerminalPanel_setPen(TerminalPanel *s, TerminalPen v) { (*s).pen = v; }
;;GETTER
static inline bytes TerminalPanel_refusalCount(const TerminalPanel *s) { return (*s).refusalCount; }
;;GETTER
static inline bytes *TerminalPanel_refusalCountMut(TerminalPanel *s) { return &(*s).refusalCount; }
;;SETTER
static inline fn TerminalPanel_setRefusalCount(TerminalPanel *s, bytes v) { (*s).refusalCount = v; }
;;GETTER
static inline bytes TerminalPanel_usedCount(const TerminalPanel *s) { return (*s).usedCount; }
;;GETTER
static inline bytes *TerminalPanel_usedCountMut(TerminalPanel *s) { return &(*s).usedCount; }
;;SETTER
static inline fn TerminalPanel_setUsedCount(TerminalPanel *s, bytes v) { (*s).usedCount = v; }

#define TERMINAL_PANEL_NO_SLOT ((bytes) -1)

static TerminalChildId invalidId() { return (TerminalChildId) { 0, 0 }; }

static fn refuse(TerminalPanel *self, const char *reason) {
    if (TerminalPanel_refusalCount(self) == 0)
        THROW("TerminalPanel exhausted: %s (capacity %zu, live %zu)",
              reason, TerminalPanel_capacity(self), TerminalPanel_usedCount(self));
    if (TerminalPanel_refusalCount(self) != SIZE_MAX)
        TerminalPanel_setRefusalCount(self, TerminalPanel_refusalCount(self) + 1u);
}

// Resolve an id to a live slot. Zero generation, out-of-range index, a retired
// slot and a generation mismatch all mean "no such child" — never a silent
// substitute for a recycled one.
static bytes slotFor(const TerminalPanel *self, TerminalChildId id) {
    if (self == nullptr || id.generation == 0 || id.index >= TerminalPanel_capacity(self))
        return TERMINAL_PANEL_NO_SLOT;
    const TerminalChild *child = &TerminalPanel_childrenMut(self)[id.index];
    if (!TerminalChild_used(child) || TerminalChild_generation(child) != id.generation)
        return TERMINAL_PANEL_NO_SLOT;
    return id.index;
}

// Cold doubling growth. Every live child survives; a failed allocation leaves the
// panel exactly as it was and is reported through the refusal channel.
static bool growChildren(TerminalPanel *self) {
    bytes next = TerminalPanel_capacity(self) == 0 ? TERMINAL_PANEL_CHILDREN_DEFAULT : TerminalPanel_capacity(self) * 2;
    if (next < TerminalPanel_capacity(self) || next > SIZE_MAX / sizeof(TerminalChild)) {
        refuse(self, "child capacity overflow");
        return false;
    }
    TerminalChild *grown = Memory_alloc(TYPE_BYTE_ARRAY, Determinant_arena(), next * sizeof(TerminalChild));
    if (grown == nullptr) {
        refuse(self, "child array allocation refused");
        return false;
    }
    if (TerminalPanel_capacity(self) != 0)
        memcpy(grown, TerminalPanel_children(self), TerminalPanel_capacity(self) * sizeof(TerminalChild));
    memset(grown + TerminalPanel_capacity(self), 0, (next - TerminalPanel_capacity(self)) * sizeof(TerminalChild));
    Memory_free(TerminalPanel_children(self));
    TerminalPanel_setChildren(self, grown);
    TerminalPanel_setCapacity(self, next);
    return true;
}

// Release one child's payload. A nested panel takes its own subtree with it.
static fn freeChild(TerminalChild *child) {
    if (child == nullptr)
        return;
    if (TerminalChild_kind(child) == TERMINAL_COMPONENT_PANEL)
        TerminalPanel_free(TerminalChild_panel(child));
    else
        TerminalText_free(TerminalChild_text(child));
    TerminalChild_setPanel(child, nullptr);
    TerminalChild_setText(child, nullptr);
    TerminalChild_setUsed(child, false);
}

static fn paintImage(const TerminalPanel *self, TerminalCanvas *canvas) {
    switch (TerminalPanel_fit(self)) {
        case TERMINAL_PANEL_IMAGE_CONTAIN:
            TerminalImage_renderContain(TerminalPanel_image(self), canvas);
            break;
        case TERMINAL_PANEL_IMAGE_NATIVE:
            TerminalImage_render(TerminalPanel_image(self), canvas, 0, 0);
            break;
        case TERMINAL_PANEL_IMAGE_STRETCH:
        default:
            TerminalImage_renderFit(TerminalPanel_image(self), canvas);
            break;
    }
}

static fn paintChildren(TerminalPanel *self, TerminalCanvas *canvas) {
    if (TerminalPanel_image(self) != nullptr)
        paintImage(self, canvas);
    TerminalPen pen = TerminalPanel_pen(self);
    if (TERMINAL_COLOR_IS_SET(pen.bg))
        TerminalCanvas_fill(canvas, ' ', pen);
    bool flowing = TerminalPanel_layout(self) == TERMINAL_PANEL_FLEX;
    TerminalRect cursor = { 0, 0, TerminalCanvas_getWidth(canvas), TerminalCanvas_getHeight(canvas) };
    for (bytes i = 0; i < TerminalPanel_capacity(self); ++i) {
        TerminalChild *child = &TerminalPanel_childrenMut(self)[i];
        if (!TerminalChild_used(child))
            continue;
        TerminalAnchorPlacement placed = TerminalAnchor_place(TerminalChild_anchorMut(child), cursor);
        if (flowing)
            cursor = placed.cursor;
        TerminalCanvas region = TerminalCanvas_sub(canvas, placed.region.x, placed.region.y,
                                                  placed.region.width, placed.region.height);
        if (!TerminalCanvas_isValid(&region))
            continue;
        if (TerminalChild_kind(child) == TERMINAL_COMPONENT_PANEL)
            TerminalPanel_paint(TerminalChild_panel(child), &region);
        else
            TerminalText_paint(TerminalChild_text(child), &region, pen);
    }
}

TerminalPanel *TerminalPanel_0() {
    TerminalPanel *self = Memory_alloc(TYPE_BYTE_ARRAY, Determinant_arena(), sizeof(TerminalPanel));
    if (self == nullptr)
        return nullptr; // R2 owner reports/counts allocation exhaustion.
    *self = (TerminalPanel) { .layout = TERMINAL_PANEL_FLEX, .pen = TerminalPen_0() };
    return self;
}

fn TerminalPanel_free(TerminalPanel *self) {
    if (self == nullptr)
        return;
    if (TerminalPanel_painting(self)) {
        THROW("TerminalPanel free: mid-paint");
        return;
    }
    for (bytes i = 0; i < TerminalPanel_capacity(self); ++i) {
        TerminalChild *child = &TerminalPanel_childrenMut(self)[i];
        if (TerminalChild_used(child))
            freeChild(child);
    }
    if (TerminalPanel_children(self) != nullptr)
        Memory_free(TerminalPanel_children(self));
    *self = (TerminalPanel) {0};
}

bool TerminalPanel_setLayout(TerminalPanel *self, TerminalPanelLayout layout) {
    if (self == nullptr
        || (layout != TERMINAL_PANEL_FLEX && layout != TERMINAL_PANEL_LAYERED)) {
        THROW("TerminalPanel layout: null panel or unknown layout");
        return false;
    }
    TerminalPanel_setLayout(self, layout);
    return true;
}

TerminalPanelLayout TerminalPanel_getLayout(const TerminalPanel *self) {
    return self ? TerminalPanel_layout(self) : TERMINAL_PANEL_FLEX;
}

// Shared tail of both adds: take a free slot, take ownership, publish the id.
static TerminalChildId adoptChild(TerminalPanel *self, TerminalComponentKind kind,
                                 any payload, TerminalAnchor anchor) {
    if (self == nullptr || payload == nullptr || TerminalPanel_painting(self)) {
        THROW("TerminalPanel add: null panel, null child or mid-paint");
        return invalidId();
    }
    bytes slot = TERMINAL_PANEL_NO_SLOT;
    for (bytes i = 0; i < TerminalPanel_capacity(self); ++i) {
        TerminalChild *candidate = &TerminalPanel_childrenMut(self)[i];
        if (!TerminalChild_used(candidate)) {
            slot = i;
            break;
        }
    }
    if (slot == TERMINAL_PANEL_NO_SLOT) {
        bytes previous = TerminalPanel_capacity(self);
        if (!growChildren(self))
            return invalidId();
        slot = previous;
    }
    TerminalChild *child = &TerminalPanel_childrenMut(self)[slot];
    if (TerminalChild_generation(child) == UINT32_MAX) {
        THROW("TerminalPanel add: child slot generation exhausted");
        return invalidId();
    }
    u32 generation = TerminalChild_generation(child) + 1;
    memset(child, 0, sizeof *child);
    TerminalChild_setGeneration(child, generation);
    TerminalChild_setUsed(child, true);
    TerminalChild_setKind(child, kind);
    TerminalChild_setAnchor(child, anchor);
    if (kind == TERMINAL_COMPONENT_PANEL)
        TerminalChild_setPanel(child, payload);
    else
        TerminalChild_setText(child, payload);
    TerminalPanel_setUsedCount(self, TerminalPanel_usedCount(self) + 1u);
    return (TerminalChildId) { slot, generation };
}

TerminalChildId TerminalPanel_addPanel(TerminalPanel *self, TerminalPanel *child, TerminalAnchor anchor) {
    if (child == nullptr) {
        THROW("TerminalPanel addPanel: null child");
        return invalidId();
    }
    if (child == self) {
        // A panel cannot be its own child: the tree would never terminate.
        THROW("TerminalPanel addPanel: a panel cannot contain itself");
        return invalidId();
    }
    return adoptChild(self, TERMINAL_COMPONENT_PANEL, child, anchor);
}

TerminalChildId TerminalPanel_addText(TerminalPanel *self, TerminalText *child, TerminalAnchor anchor) {
    if (child == nullptr) {
        THROW("TerminalPanel addText: null child");
        return invalidId();
    }
    return adoptChild(self, TERMINAL_COMPONENT_TEXT, child, anchor);
}

bool TerminalPanel_remove(TerminalPanel *self, TerminalChildId id) {
    if (self == nullptr || TerminalPanel_painting(self)) {
        THROW("TerminalPanel remove: null panel or mid-paint");
        return false;
    }
    bytes slot = slotFor(self, id);
    if (slot == TERMINAL_PANEL_NO_SLOT) {
        THROW("TerminalPanel remove: unknown or stale child id");
        return false;
    }
    TerminalChild *child = &TerminalPanel_childrenMut(self)[slot];
    freeChild(child);
    if (TerminalPanel_usedCount(self) != 0)
        TerminalPanel_setUsedCount(self, TerminalPanel_usedCount(self) - 1u);
    return true;
}

// Detach one child without freeing it. The slot is retired so the old id goes
// stale, and ownership returns to the caller for re-parenting.
static any detachChild(TerminalPanel *self, TerminalChildId id, TerminalComponentKind kind) {
    if (self == nullptr || TerminalPanel_painting(self))
        return nullptr;
    bytes slot = slotFor(self, id);
    if (slot == TERMINAL_PANEL_NO_SLOT)
        return nullptr;
    TerminalChild *child = &TerminalPanel_childrenMut(self)[slot];
    if (TerminalChild_kind(child) != kind)
        return nullptr;
    any payload = kind == TERMINAL_COMPONENT_PANEL ? (any) TerminalChild_panel(child) : (any) TerminalChild_text(child);
    TerminalChild_setPanel(child, nullptr);
    TerminalChild_setText(child, nullptr);
    TerminalChild_setUsed(child, false);
    if (TerminalPanel_usedCount(self) != 0)
        TerminalPanel_setUsedCount(self, TerminalPanel_usedCount(self) - 1u);
    return payload;
}

TerminalPanel *TerminalPanel_detachPanel(TerminalPanel *self, TerminalChildId id) {
    return detachChild(self, id, TERMINAL_COMPONENT_PANEL);
}

TerminalText *TerminalPanel_detachText(TerminalPanel *self, TerminalChildId id) {
    return detachChild(self, id, TERMINAL_COMPONENT_TEXT);
}

bool TerminalPanel_setPen(TerminalPanel *self, TerminalPen pen) {
    if (self == nullptr) {
        THROW("TerminalPanel pen: null panel");
        return false;
    }
    TerminalPanel_setPen(self, pen);
    return true;
}
TerminalPen TerminalPanel_getPen(const TerminalPanel *self) {
    return self ? TerminalPanel_pen(self) : TerminalPen_0();
}
bool TerminalPanel_setStyle(TerminalPanel *self, u8 style) {
    if (self == nullptr) {
        THROW("TerminalPanel style: null panel");
        return false;
    }
    TerminalPen *pen = TerminalPanel_penMut(self);
    TerminalPen_setStyle(pen, style);
    return true;
}
u8 TerminalPanel_getStyle(const TerminalPanel *self) {
    if (self == nullptr)
        return TERMINAL_STYLE_NONE;
    const TerminalPen *pen = TerminalPanel_penMut(self);
    return TerminalPen_style(pen);
}
bool TerminalPanel_setForegroundColor(TerminalPanel *self, u32 color) {
    if (self == nullptr) {
        THROW("TerminalPanel foreground: null panel");
        return false;
    }
    TerminalPen *pen = TerminalPanel_penMut(self);
    TerminalPen_setFg(pen, color);
    return true;
}
u32 TerminalPanel_getForegroundColor(const TerminalPanel *self) {
    if (self == nullptr)
        return TERMINAL_COLOR_DEFAULT;
    const TerminalPen *pen = TerminalPanel_penMut(self);
    return TerminalPen_fg(pen);
}
bool TerminalPanel_setBackgroundColor(TerminalPanel *self, u32 color) {
    if (self == nullptr) {
        THROW("TerminalPanel background: null panel");
        return false;
    }
    TerminalPen *pen = TerminalPanel_penMut(self);
    TerminalPen_setBg(pen, color);
    return true;
}
u32 TerminalPanel_getBackgroundColor(const TerminalPanel *self) {
    if (self == nullptr)
        return TERMINAL_COLOR_DEFAULT;
    const TerminalPen *pen = TerminalPanel_penMut(self);
    return TerminalPen_bg(pen);
}

bool TerminalPanel_setImage(TerminalPanel *self, const TerminalImage *image, TerminalPanelImageFit fit) {
    if (self == nullptr) {
        THROW("TerminalPanel image: null panel");
        return false;
    }
    if (fit != TERMINAL_PANEL_IMAGE_STRETCH && fit != TERMINAL_PANEL_IMAGE_CONTAIN
        && fit != TERMINAL_PANEL_IMAGE_NATIVE) {
        THROW("TerminalPanel image: unknown fit");
        return false;
    }
    TerminalPanel_setImage(self, image);
    TerminalPanel_setFit(self, fit);
    return true;
}
const TerminalImage *TerminalPanel_getImage(const TerminalPanel *self) { return self ? TerminalPanel_image(self) : nullptr; }
TerminalPanelImageFit TerminalPanel_getFit(const TerminalPanel *self) {
    return self ? TerminalPanel_fit(self) : TERMINAL_PANEL_IMAGE_STRETCH;
}

bool TerminalPanel_setAnchor(TerminalPanel *self, TerminalChildId id, TerminalAnchor anchor) {
    if (self == nullptr) {
        THROW("TerminalPanel anchor: null panel");
        return false;
    }
    bytes slot = slotFor(self, id);
    if (slot == TERMINAL_PANEL_NO_SLOT) {
        THROW("TerminalPanel anchor: unknown or stale child id");
        return false;
    }
    TerminalChild *child = &TerminalPanel_childrenMut(self)[slot];
    TerminalChild_setAnchor(child, anchor);
    return true;
}
TerminalAnchor TerminalPanel_getAnchor(const TerminalPanel *self, TerminalChildId id) {
    bytes slot = slotFor(self, id);
    if (slot == TERMINAL_PANEL_NO_SLOT)
        return TerminalAnchor_0();
    const TerminalChild *child = &TerminalPanel_childrenMut(self)[slot];
    return TerminalChild_anchor(child);
}

;;INTENTION("paint is total by design: every child repaints on every call, so a resize needs no invalidation and the presentation delta stays TerminalSurface's job; this path stays entry-guarded, allocation-free and log-free")
;;HOTCODE
fn TerminalPanel_paint(TerminalPanel *self, TerminalCanvas *canvas) {
    if (self == nullptr || !TerminalCanvas_isValid(canvas))
        return;
    if (TerminalPanel_painting(self)) {
        refuse(self, "reentrant paint");
        return;
    }
    TerminalPanel_setPainting(self, true);
    paintChildren(self, canvas);
    TerminalPanel_setPainting(self, false);
    TerminalPanel_setPaintCount(self, TerminalPanel_paintCount(self) + 1u);
}

TerminalRect TerminalPanel_childRegion(const TerminalPanel *self, const TerminalCanvas *canvas,
                                       TerminalChildId id) {
    TerminalRect none = { 0, 0, 0, 0 };
    if (self == nullptr || canvas == nullptr || !TerminalCanvas_isValid(canvas))
        return none;
    bool flowing = TerminalPanel_layout(self) == TERMINAL_PANEL_FLEX;
    TerminalRect cursor = { 0, 0, TerminalCanvas_getWidth(canvas), TerminalCanvas_getHeight(canvas) };
    for (bytes i = 0; i < TerminalPanel_capacity(self); ++i) {
        const TerminalChild *child = &TerminalPanel_childrenMut(self)[i];
        if (!TerminalChild_used(child))
            continue;
        TerminalAnchorPlacement placed = TerminalAnchor_place(TerminalChild_anchorMut(child), cursor);
        if (flowing)
            cursor = placed.cursor;
        if (i == id.index && TerminalChild_generation(child) == id.generation)
            return placed.region;
    }
    return none;
}

TerminalChildId TerminalPanel_childAt(const TerminalPanel *self, const TerminalCanvas *canvas,
                                      bytes x, bytes y) {
    TerminalChildId hit = invalidId();
    if (self == nullptr || canvas == nullptr || !TerminalCanvas_isValid(canvas))
        return hit;
    bool flowing = TerminalPanel_layout(self) == TERMINAL_PANEL_FLEX;
    TerminalRect cursor = { 0, 0, TerminalCanvas_getWidth(canvas), TerminalCanvas_getHeight(canvas) };
    for (bytes i = 0; i < TerminalPanel_capacity(self); ++i) {
        const TerminalChild *child = &TerminalPanel_childrenMut(self)[i];
        if (!TerminalChild_used(child))
            continue;
        TerminalAnchorPlacement placed = TerminalAnchor_place(TerminalChild_anchorMut(child), cursor);
        if (flowing)
            cursor = placed.cursor;
        TerminalRect region = placed.region;
        if (region.width == 0 || region.height == 0)
            continue;
        if (x < region.x || x >= region.x + region.width || y < region.y || y >= region.y + region.height)
            continue;
        hit = (TerminalChildId) { i, TerminalChild_generation(child) };   // later children are on top
    }
    return hit;
}

TerminalComponentKind TerminalPanel_childKind(const TerminalPanel *self, TerminalChildId id) {
    bytes slot = slotFor(self, id);
    if (slot == TERMINAL_PANEL_NO_SLOT)
        return TERMINAL_COMPONENT_TEXT;
    const TerminalChild *child = &TerminalPanel_childrenMut(self)[slot];
    return TerminalChild_kind(child);
}

TerminalPanel *TerminalPanel_childPanel(const TerminalPanel *self, TerminalChildId id) {
    bytes slot = slotFor(self, id);
    if (slot == TERMINAL_PANEL_NO_SLOT)
        return nullptr;
    const TerminalChild *child = &TerminalPanel_childrenMut(self)[slot];
    return TerminalChild_kind(child) == TERMINAL_COMPONENT_PANEL ? TerminalChild_panel(child) : nullptr;
}

TerminalText *TerminalPanel_childText(const TerminalPanel *self, TerminalChildId id) {
    bytes slot = slotFor(self, id);
    if (slot == TERMINAL_PANEL_NO_SLOT)
        return nullptr;
    const TerminalChild *child = &TerminalPanel_childrenMut(self)[slot];
    return TerminalChild_kind(child) == TERMINAL_COMPONENT_TEXT ? TerminalChild_text(child) : nullptr;
}

bytes TerminalPanel_childCount(const TerminalPanel *self) { return self ? TerminalPanel_usedCount(self) : 0; }
bytes TerminalPanel_capacity(const TerminalPanel *self) { return self ? TerminalPanel_capacity(self) : 0; }
bytes TerminalPanel_paintCount(const TerminalPanel *self) { return self ? TerminalPanel_paintCount(self) : 0; }
bytes TerminalPanel_exhaustionCount(const TerminalPanel *self) { return self ? TerminalPanel_refusalCount(self) : 0; }
bool TerminalPanel_isPainting(const TerminalPanel *self) { return self != nullptr && TerminalPanel_painting(self); }
bool TerminalPanel_isValid(const TerminalPanel *self, TerminalChildId id) {
    return slotFor(self, id) != TERMINAL_PANEL_NO_SLOT;
}
bool TerminalPanel_isChildId(TerminalChildId id) { return id.generation != 0; }
bool TerminalChildId_equal(TerminalChildId a, TerminalChildId b) {
    return a.index == b.index && a.generation == b.generation;
}
fn TerminalPanel_clearExhaustion(TerminalPanel *self) {
    if (self != nullptr)
        TerminalPanel_setRefusalCount(self, 0);
}

TerminalProjection TerminalPanel_toString(const TerminalPanel *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format("TerminalPanel{layout=%d,children=%zu,capacity=%zu,paints=%zu}",
        TerminalPanel_layout(self), TerminalPanel_usedCount(self), TerminalPanel_capacity(self), TerminalPanel_paintCount(self));
}

TerminalProjection TerminalPanel_toStringStruct(const TerminalPanel *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    const TerminalPen *pen = TerminalPanel_penMut(self);
    return TerminalProjection_format(
        "TerminalPanel{layout=%d,children=<owned>,capacity=%zu,usedCount=%zu,paintCount=%zu,refusalCount=%zu,pen={style=0x%02X,fg=0x%08X,bg=0x%08X},image=%s,fit=%d,painting=%d}",
        TerminalPanel_layout(self), TerminalPanel_capacity(self), TerminalPanel_usedCount(self), TerminalPanel_paintCount(self),
        TerminalPanel_refusalCount(self), TerminalPen_style(pen), TerminalPen_fg(pen), TerminalPen_bg(pen),
        TerminalPanel_image(self) ? "bound" : "none", TerminalPanel_fit(self), TerminalPanel_painting(self));
}
