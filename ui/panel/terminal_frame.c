#include "vex.h"
#include "ui/panel/terminal_frame.h"
#include "format/terminal_style.h"
#include <string.h>
#include <stdio.h>

;;DEFINITION
/** A bare region layout. TerminalFrame splits a rectangle into header / left /
 * centre / right / footer canvases and draws only its own rules; every other
 * pixel belongs to the caller. It carries no notion of a chat, a list, a badge
 * or a setting — those are application content. Geometry is recomputed from the
 * root size on each call, so resize is free and rails collapse on a narrow
 * terminal to keep the centre usable. Owns no memory; may paint one borrowed
 * background image. */
;;OVERVIEW
/** ========================================================================
 * CLASS: TerminalFrame (ui/panel/terminal_frame.c)
 * STRUCT FIELDS (Mirroring ui/panel/terminal_frame.h)
 * bytes leftWidth, rightWidth   requested column widths
 * bytes headerRows, footerRows  top/bottom band rows
 * const TerminalImage *background  borrowed background image (nullable)
 * TerminalRule rule              connector glyph set (see TerminalRule)
 * PRIVATE HELPERS
 * clampRail(requested, total)  rail width clamped to a third of the root
 * frameGeometry(self, root)    computed rails + band offsets
 * FUNCTION REGISTRY (exports)
 * Constructors: TerminalFrame_0(), TerminalFrame_2(left, right)
 * Rules: TerminalRule_0/_2/_4, TerminalRule_draw(rule, canvas, ly, pen),
 *   TerminalRule_drawCut(rule, canvas, ly, inset, pen)  cut, stopped-i16 run
 *   TerminalRule_drawBox(rule, canvas, pen)             detached box + corners
 *   TerminalFrame_setRule/getRule(self, rule)
 * Config: setRails/getRails, setBands/getBands, setBackground/getBackground
 * Layout: TerminalFrame_getLayout(self, root, outLeft, outRight, outRails)
 * Regions: TerminalFrame_header/left/center/right/footer(self, root)
 * Paint: TerminalFrame_paintBackground, TerminalFrame_drawRules(self, root, pen)
 * Projections: TerminalProjection TerminalFrame_toString/toStringStruct(self)
 * CONTRACT: a null/invalid root yields an invalid region (draws no-op); regions
 * are clipped to the root and to the grid; no allocation, no logging.
 * ======================================================================== */


;;GETTER
static inline u32 TerminalRule_corner(const TerminalRule *s) { return (*s).corner; }
;;GETTER
static inline u32 *TerminalRule_cornerMut(TerminalRule *s) { return &(*s).corner; }
;;SETTER
static inline fn TerminalRule_setCorner(TerminalRule *s, u32 v) { (*s).corner = v; }
;;GETTER
static inline u32 TerminalRule_cornerBottomLeft(const TerminalRule *s) { return (*s).cornerBottomLeft; }
;;GETTER
static inline u32 *TerminalRule_cornerBottomLeftMut(TerminalRule *s) { return &(*s).cornerBottomLeft; }
;;SETTER
static inline fn TerminalRule_setCornerBottomLeft(TerminalRule *s, u32 v) { (*s).cornerBottomLeft = v; }
;;GETTER
static inline u32 TerminalRule_cornerBottomRight(const TerminalRule *s) { return (*s).cornerBottomRight; }
;;GETTER
static inline u32 *TerminalRule_cornerBottomRightMut(TerminalRule *s) { return &(*s).cornerBottomRight; }
;;SETTER
static inline fn TerminalRule_setCornerBottomRight(TerminalRule *s, u32 v) { (*s).cornerBottomRight = v; }
;;GETTER
static inline u32 TerminalRule_cornerTopLeft(const TerminalRule *s) { return (*s).cornerTopLeft; }
;;GETTER
static inline u32 *TerminalRule_cornerTopLeftMut(TerminalRule *s) { return &(*s).cornerTopLeft; }
;;SETTER
static inline fn TerminalRule_setCornerTopLeft(TerminalRule *s, u32 v) { (*s).cornerTopLeft = v; }
;;GETTER
static inline u32 TerminalRule_cornerTopRight(const TerminalRule *s) { return (*s).cornerTopRight; }
;;GETTER
static inline u32 *TerminalRule_cornerTopRightMut(TerminalRule *s) { return &(*s).cornerTopRight; }
;;SETTER
static inline fn TerminalRule_setCornerTopRight(TerminalRule *s, u32 v) { (*s).cornerTopRight = v; }
;;GETTER
static inline u32 TerminalRule_cutLeft(const TerminalRule *s) { return (*s).cutLeft; }
;;GETTER
static inline u32 *TerminalRule_cutLeftMut(TerminalRule *s) { return &(*s).cutLeft; }
;;SETTER
static inline fn TerminalRule_setCutLeft(TerminalRule *s, u32 v) { (*s).cutLeft = v; }
;;GETTER
static inline u32 TerminalRule_cutRight(const TerminalRule *s) { return (*s).cutRight; }
;;GETTER
static inline u32 *TerminalRule_cutRightMut(TerminalRule *s) { return &(*s).cutRight; }
;;SETTER
static inline fn TerminalRule_setCutRight(TerminalRule *s, u32 v) { (*s).cutRight = v; }
;;GETTER
static inline u32 TerminalRule_horizontal(const TerminalRule *s) { return (*s).horizontal; }
;;GETTER
static inline u32 *TerminalRule_horizontalMut(TerminalRule *s) { return &(*s).horizontal; }
;;SETTER
static inline fn TerminalRule_setHorizontal(TerminalRule *s, u32 v) { (*s).horizontal = v; }
;;GETTER
static inline u32 TerminalRule_junction(const TerminalRule *s) { return (*s).junction; }
;;GETTER
static inline u32 *TerminalRule_junctionMut(TerminalRule *s) { return &(*s).junction; }
;;SETTER
static inline fn TerminalRule_setJunction(TerminalRule *s, u32 v) { (*s).junction = v; }
;;GETTER
static inline u32 TerminalRule_stripe(const TerminalRule *s) { return (*s).stripe; }
;;GETTER
static inline u32 *TerminalRule_stripeMut(TerminalRule *s) { return &(*s).stripe; }
;;SETTER
static inline fn TerminalRule_setStripe(TerminalRule *s, u32 v) { (*s).stripe = v; }
;;GETTER
static inline bytes TerminalRule_stripeEvery(const TerminalRule *s) { return (*s).stripeEvery; }
;;GETTER
static inline bytes *TerminalRule_stripeEveryMut(TerminalRule *s) { return &(*s).stripeEvery; }
;;SETTER
static inline fn TerminalRule_setStripeEvery(TerminalRule *s, bytes v) { (*s).stripeEvery = v; }
;;GETTER
static inline u32 TerminalRule_vertical(const TerminalRule *s) { return (*s).vertical; }
;;GETTER
static inline u32 *TerminalRule_verticalMut(TerminalRule *s) { return &(*s).vertical; }
;;SETTER
static inline fn TerminalRule_setVertical(TerminalRule *s, u32 v) { (*s).vertical = v; }

;;GETTER
static inline const TerminalImage * TerminalFrame_background(const TerminalFrame *s) { return (*s).background; }
;;GETTER
static inline const TerminalImage * *TerminalFrame_backgroundMut(TerminalFrame *s) { return &(*s).background; }
;;SETTER
static inline fn TerminalFrame_setBackground(TerminalFrame *s, const TerminalImage * v) { (*s).background = v; }
;;GETTER
static inline bytes TerminalFrame_footerRows(const TerminalFrame *s) { return (*s).footerRows; }
;;GETTER
static inline bytes *TerminalFrame_footerRowsMut(TerminalFrame *s) { return &(*s).footerRows; }
;;SETTER
static inline fn TerminalFrame_setFooterRows(TerminalFrame *s, bytes v) { (*s).footerRows = v; }
;;GETTER
static inline bytes TerminalFrame_headerRows(const TerminalFrame *s) { return (*s).headerRows; }
;;GETTER
static inline bytes *TerminalFrame_headerRowsMut(TerminalFrame *s) { return &(*s).headerRows; }
;;SETTER
static inline fn TerminalFrame_setHeaderRows(TerminalFrame *s, bytes v) { (*s).headerRows = v; }
;;GETTER
static inline bytes TerminalFrame_leftWidth(const TerminalFrame *s) { return (*s).leftWidth; }
;;GETTER
static inline bytes *TerminalFrame_leftWidthMut(TerminalFrame *s) { return &(*s).leftWidth; }
;;SETTER
static inline fn TerminalFrame_setLeftWidth(TerminalFrame *s, bytes v) { (*s).leftWidth = v; }
;;GETTER
static inline bytes TerminalFrame_rightWidth(const TerminalFrame *s) { return (*s).rightWidth; }
;;GETTER
static inline bytes *TerminalFrame_rightWidthMut(TerminalFrame *s) { return &(*s).rightWidth; }
;;SETTER
static inline fn TerminalFrame_setRightWidth(TerminalFrame *s, bytes v) { (*s).rightWidth = v; }
;;GETTER
static inline TerminalRule TerminalFrame_rule(const TerminalFrame *s) { return (*s).rule; }
;;GETTER
static inline TerminalRule *TerminalFrame_ruleMut(TerminalFrame *s) { return &(*s).rule; }
;;SETTER
static inline fn TerminalFrame_setRule(TerminalFrame *s, TerminalRule v) { (*s).rule = v; }

typedef struct FrameGeometry {
    bytes left, right;      // effective column widths (0 => no rails)
    bool rails;
    bytes headerRows, bodyY, bodyH, footerRows;
} FrameGeometry;

;;GETTER
static inline bytes FrameGeometry_left(const FrameGeometry *s) { return (*s).left; }
;;GETTER
static inline bytes *FrameGeometry_leftMut(FrameGeometry *s) { return &(*s).left; }
;;SETTER
static inline fn FrameGeometry_setLeft(FrameGeometry *s, bytes v) { (*s).left = v; }
;;GETTER
static inline bool FrameGeometry_rails(const FrameGeometry *s) { return (*s).rails; }
;;GETTER
static inline bool *FrameGeometry_railsMut(FrameGeometry *s) { return &(*s).rails; }
;;SETTER
static inline fn FrameGeometry_setRails(FrameGeometry *s, bool v) { (*s).rails = v; }
;;GETTER
static inline bytes FrameGeometry_right(const FrameGeometry *s) { return (*s).right; }
;;GETTER
static inline bytes *FrameGeometry_rightMut(FrameGeometry *s) { return &(*s).right; }
;;SETTER
static inline fn FrameGeometry_setRight(FrameGeometry *s, bytes v) { (*s).right = v; }


static bytes clampRail(bytes requested, bytes total) {
    bytes third = total / 3;
    return requested < third ? requested : third;
}

static FrameGeometry frameGeometry(const TerminalFrame *self, const TerminalCanvas *root) {
    FrameGeometry g = {0};
    bytes W = TerminalCanvas_getWidth(root);
    bytes H = TerminalCanvas_getHeight(root);
    g.left = clampRail(TerminalFrame_leftWidth(self), W);
    g.right = clampRail(TerminalFrame_rightWidth(self), W);
    if (g.left + g.right + 3 > W) {
        g.left = 0;
        g.right = 0;
    }
    g.rails = g.left > 0 && g.right > 0;
    g.headerRows = TerminalFrame_headerRows(self);
    g.footerRows = TerminalFrame_footerRows(self);
    if (g.headerRows + g.footerRows >= H) {
        g.headerRows = 0;
        g.footerRows = 0;
    }
    g.bodyY = g.headerRows;
    g.bodyH = H - g.headerRows - g.footerRows;
    return g;
}

static bytes centreX(const FrameGeometry *g) { return FrameGeometry_rails(g) ? FrameGeometry_left(g) + 1 : 0; }
static bytes centreW(const FrameGeometry *g, const TerminalCanvas *root) {
    bytes W = TerminalCanvas_getWidth(root);
    return W - FrameGeometry_left(g) - FrameGeometry_right(g) - (FrameGeometry_rails(g) ? 2 : 0);
}

TerminalFrame TerminalFrame_0() { return TerminalFrame_2(0, 0); }

TerminalFrame TerminalFrame_2(bytes leftWidth, bytes rightWidth) {
    TerminalFrame frame = {0};
    frame.leftWidth = leftWidth;
    frame.rightWidth = rightWidth;
    frame.headerRows = TERMINAL_FRAME_HEADER_ROWS_DEFAULT;
    frame.rule = TerminalRule_0();
    return frame;
}

TerminalRule TerminalRule_0() {
    return (TerminalRule) { '-', '|', '+', '+', 0, 0, 0, 0, 0, 0, 0, 0 };
}
TerminalRule TerminalRule_2(u32 horizontal, u32 vertical) {
    return (TerminalRule) { horizontal, vertical, '+', '+', 0, 0, 0, 0, 0, 0, 0, 0 };
}
TerminalRule TerminalRule_4(u32 horizontal, u32 vertical, u32 stripe, bytes stripeEvery) {
    return (TerminalRule) { horizontal, vertical, '+', '+', stripe, stripeEvery, 0, 0, 0, 0, 0, 0 };
}

fn TerminalRule_draw(const TerminalRule *rule, TerminalCanvas *canvas, bytes ly, TerminalPen pen) {
    if (rule == nullptr || !TerminalCanvas_isValid(canvas))
        return;
    bytes width = TerminalCanvas_getWidth(canvas);
    for (bytes x = 0; x < width; ++x) {
        u32 ch = TerminalRule_horizontal(rule);
        if (TerminalRule_stripe(rule) != 0 && TerminalRule_stripeEvery(rule) > 0 && (x % TerminalRule_stripeEvery(rule)) == (TerminalRule_stripeEvery(rule) - 1))
            ch = TerminalRule_stripe(rule);
        TerminalCanvas_glyph(canvas, x, ly, ch, pen);
    }
}

// A CUT run: it stops i16 of both edges and ends in the cut-endpoint glyphs, so
// a panel divider reads as `╶──────╴` (or a plain, clearly-ended ` ------ `)
// rather than a line that runs into the borders.
fn TerminalRule_drawCut(const TerminalRule *rule, TerminalCanvas *canvas, bytes ly,
                          bytes inset, TerminalPen pen) {
    if (rule == nullptr || !TerminalCanvas_isValid(canvas))
        return;
    bytes width = TerminalCanvas_getWidth(canvas);
    if (width <= inset * 2)
        return;
    bytes span = width - inset * 2;
    bytes start = inset;
    for (bytes i = 0; i < span; ++i) {
        u32 ch = TerminalRule_horizontal(rule);
        if (TerminalRule_stripe(rule) != 0 && TerminalRule_stripeEvery(rule) > 0 && (i % TerminalRule_stripeEvery(rule)) == (TerminalRule_stripeEvery(rule) - 1))
            ch = TerminalRule_stripe(rule);
        if (i == 0 && TerminalRule_cutLeft(rule) != 0)
            ch = TerminalRule_cutLeft(rule);
        if (i == span - 1 && TerminalRule_cutRight(rule) != 0)
            ch = TerminalRule_cutRight(rule);
        TerminalCanvas_glyph(canvas, start + i, ly, ch, pen);
    }
}

// A DETACHED box: its own four corners and its own verticals. Nothing joins it to
// a neighbour, which is what keeps panels reading as separate instruments.
fn TerminalRule_drawBox(const TerminalRule *rule, TerminalCanvas *canvas, TerminalPen pen) {
    if (rule == nullptr || !TerminalCanvas_isValid(canvas))
        return;
    bytes width = TerminalCanvas_getWidth(canvas);
    bytes height = TerminalCanvas_getHeight(canvas);
    if (width < 2 || height < 2)
        return;
    u32 horizontal = TerminalRule_horizontal(rule);
    u32 vertical = TerminalRule_vertical(rule);
    u32 topLeft = TerminalRule_cornerTopLeft(rule) ? TerminalRule_cornerTopLeft(rule) : TerminalRule_corner(rule);
    u32 topRight = TerminalRule_cornerTopRight(rule) ? TerminalRule_cornerTopRight(rule) : TerminalRule_corner(rule);
    u32 bottomLeft = TerminalRule_cornerBottomLeft(rule) ? TerminalRule_cornerBottomLeft(rule) : TerminalRule_corner(rule);
    u32 bottomRight = TerminalRule_cornerBottomRight(rule) ? TerminalRule_cornerBottomRight(rule) : TerminalRule_corner(rule);
    TerminalCanvas_hline(canvas, 0, horizontal, pen);
    TerminalCanvas_hline(canvas, height - 1, horizontal, pen);
    TerminalCanvas_vline(canvas, 0, vertical, pen);
    TerminalCanvas_vline(canvas, width - 1, vertical, pen);
    TerminalCanvas_glyph(canvas, 0, 0, topLeft, pen);
    TerminalCanvas_glyph(canvas, width - 1, 0, topRight, pen);
    TerminalCanvas_glyph(canvas, 0, height - 1, bottomLeft, pen);
    TerminalCanvas_glyph(canvas, width - 1, height - 1, bottomRight, pen);
}

bool TerminalFrame_setRule(TerminalFrame *self, TerminalRule rule) {
    if (self == nullptr) { THROW("TerminalFrame rule: null self"); return false; }
    TerminalFrame_setRule(self, rule);
    return true;
}
TerminalRule TerminalFrame_getRule(const TerminalFrame *self) {
    return self ? TerminalFrame_rule(self) : TerminalRule_0();
}

bool TerminalFrame_setRails(TerminalFrame *self, bytes leftWidth, bytes rightWidth) {
    if (self == nullptr) {
        THROW("TerminalFrame rails: null self");
        return false;
    }
    TerminalFrame_setLeftWidth(self, leftWidth);
    TerminalFrame_setRightWidth(self, rightWidth);
    return true;
}
TerminalFramePair TerminalFrame_getRails(const TerminalFrame *self) {
    return self ? (TerminalFramePair) { TerminalFrame_leftWidth(self), TerminalFrame_rightWidth(self) } : (TerminalFramePair) { 0u, 0u };
}
bool TerminalFrame_setBands(TerminalFrame *self, bytes headerRows, bytes footerRows) {
    if (self == nullptr) {
        THROW("TerminalFrame bands: null self");
        return false;
    }
    TerminalFrame_setHeaderRows(self, headerRows);
    TerminalFrame_setFooterRows(self, footerRows);
    return true;
}
TerminalFramePair TerminalFrame_getBands(const TerminalFrame *self) {
    return self ? (TerminalFramePair) { TerminalFrame_headerRows(self), TerminalFrame_footerRows(self) } : (TerminalFramePair) { 0u, 0u };
}
bool TerminalFrame_setBackground(TerminalFrame *self, const TerminalImage *image) {
    if (self == nullptr) {
        THROW("TerminalFrame background: null self");
        return false;
    }
    TerminalFrame_setBackground(self, image);
    return true;
}
const TerminalImage *TerminalFrame_getBackground(const TerminalFrame *self) {
    return self ? TerminalFrame_background(self) : nullptr;
}

TerminalFrameLayout TerminalFrame_getLayout(const TerminalFrame *self, const TerminalCanvas *root) {
    FrameGeometry g = {0};
    if (self != nullptr && TerminalCanvas_isValid(root))
        g = frameGeometry(self, root);
    return (TerminalFrameLayout) { g.left, g.right, g.rails };
}

fn TerminalFrame_paintBackground(const TerminalFrame *self, TerminalCanvas *root) {
    if (self == nullptr || TerminalFrame_background(self) == nullptr || !TerminalCanvas_isValid(root))
        return;
    TerminalImage_renderFit(TerminalFrame_background(self), root);
}

TerminalCanvas TerminalFrame_header(const TerminalFrame *self, const TerminalCanvas *root) {
    if (self == nullptr || !TerminalCanvas_isValid(root))
        return TerminalCanvas_0();
    FrameGeometry g = frameGeometry(self, root);
    if (g.headerRows == 0)
        return TerminalCanvas_0();
    return TerminalCanvas_sub(root, 0, 0, TerminalCanvas_getWidth(root), g.headerRows);
}

TerminalCanvas TerminalFrame_left(const TerminalFrame *self, const TerminalCanvas *root) {
    if (self == nullptr || !TerminalCanvas_isValid(root))
        return TerminalCanvas_0();
    FrameGeometry g = frameGeometry(self, root);
    if (!g.rails)
        return TerminalCanvas_0();
    return TerminalCanvas_sub(root, 0, g.bodyY, g.left, g.bodyH);
}

TerminalCanvas TerminalFrame_center(const TerminalFrame *self, const TerminalCanvas *root) {
    if (self == nullptr || !TerminalCanvas_isValid(root))
        return TerminalCanvas_0();
    FrameGeometry g = frameGeometry(self, root);
    bytes w = centreW(&g, root);
    if (w == 0 || g.bodyH == 0)
        return TerminalCanvas_0();
    return TerminalCanvas_sub(root, centreX(&g), g.bodyY, w, g.bodyH);
}

TerminalCanvas TerminalFrame_right(const TerminalFrame *self, const TerminalCanvas *root) {
    if (self == nullptr || !TerminalCanvas_isValid(root))
        return TerminalCanvas_0();
    FrameGeometry g = frameGeometry(self, root);
    if (!g.rails)
        return TerminalCanvas_0();
    return TerminalCanvas_sub(root, TerminalCanvas_getWidth(root) - g.right, g.bodyY, g.right, g.bodyH);
}

TerminalCanvas TerminalFrame_footer(const TerminalFrame *self, const TerminalCanvas *root) {
    if (self == nullptr || !TerminalCanvas_isValid(root))
        return TerminalCanvas_0();
    FrameGeometry g = frameGeometry(self, root);
    bytes w = centreW(&g, root);
    if (g.footerRows == 0 || w == 0)
        return TerminalCanvas_0();
    bytes y = TerminalCanvas_getHeight(root) - g.footerRows;
    return TerminalCanvas_sub(root, centreX(&g), y, w, g.footerRows);
}

fn TerminalFrame_drawRules(const TerminalFrame *self, TerminalCanvas *root, TerminalPen pen) {
    if (self == nullptr || !TerminalCanvas_isValid(root))
        return;
    FrameGeometry g = frameGeometry(self, root);
    const TerminalRule *rule = TerminalFrame_ruleMut(self);
    bytes W = TerminalCanvas_getWidth(root);
    if (g.headerRows > 0) {
        TerminalCanvas header = TerminalCanvas_sub(root, 0, 0, W, g.headerRows);
        TerminalRule_draw(rule, &header, g.headerRows - 1, pen);
        if (g.rails) {
            // Join the run to each rail with a junction glyph (the connector).
            u32 j = TerminalRule_junction(rule);
            TerminalCanvas_glyph(root, g.left, g.headerRows - 1, j, pen);
            TerminalCanvas_glyph(root, W - g.right - 1, g.headerRows - 1, j, pen);
        }
    }
    if (g.rails) {
        TerminalCanvas leftRule = TerminalCanvas_sub(root, g.left, g.bodyY, 1, g.bodyH);
        TerminalCanvas_vline(&leftRule, 0, TerminalRule_vertical(rule), pen);
        TerminalCanvas rightRule = TerminalCanvas_sub(root, W - g.right - 1, g.bodyY, 1, g.bodyH);
        TerminalCanvas_vline(&rightRule, 0, TerminalRule_vertical(rule), pen);
        if (g.headerRows == 0) {
            // No header run: cap each rail with the corner glyph.
            u32 c = TerminalRule_corner(rule);
            TerminalCanvas_glyph(root, g.left, g.bodyY, c, pen);
            TerminalCanvas_glyph(root, W - g.right - 1, g.bodyY, c, pen);
        }
    }
}

TerminalProjection TerminalFrame_toString(const TerminalFrame *self) {
    return TerminalFrame_toStringStruct(self);
}

TerminalProjection TerminalFrame_toStringStruct(const TerminalFrame *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format(
        "TerminalFrame{left=%zu,right=%zu,header=%zu,footer=%zu,background=%d}",
        TerminalFrame_leftWidth(self), TerminalFrame_rightWidth(self), TerminalFrame_headerRows(self), TerminalFrame_footerRows(self),
        TerminalFrame_background(self) != nullptr);
}
