#include "vex.h"
#include "ui/surface/terminal_canvas.h"
#include <string.h>
#include <stdio.h>

;;DEFINITION
/** The compositor primitive: a clipped rectangular view over a caller-owned cell
 * grid of four parallel planes (glyph codepoint, style, fg, bg). Owns no memory;
 * every widget draws through a region with a TerminalPen and cannot escape it.
 * Regions compose via TerminalCanvas_sub, so a rail, a viewport and a status bar
 * each draw in local coordinates and the layout owner alone decides where they
 * land. Glyphs are Unicode codepoints, so the toolkit is UTF-8 capable; width is
 * computed locally (no locale), wide glyphs get a continuation cell, and the host
 * blits wide. All primitives clip and never fault; a fully-outside draw is a
 * silent no-op (composition, not exhaustion). Colour is packed 0xRRGGBBAA. */
;;OVERVIEW
/** ========================================================================
 * CLASS: TerminalCanvas (ui/surface/terminal_canvas.c)
 * STRUCT FIELDS (Mirroring ui/surface/terminal_canvas.h)
 * u32 *cells               borrowed row-major glyph CODEPOINTS
 * u16 *styles              borrowed parallel style plane (nullable)
 * u32 *fg, *bg             borrowed parallel colour planes (nullable)
 * bytes gridWidth, gridHeight  full grid extent (indexing stride)
 * bytes x, y, width, height    this region, absolute, clipped to the grid
 * PRIVATE HELPERS
 * widthOf(cp)        deterministic cell width (0 combining, 1, 2 wide)
 * encodeUtf8(cp)     encode a scalar into a byte buffer
 * putCell(...)       write one cell + optional continuation marker
 * clampRail-shaped geometry is NOT here; this file is geometry-free
 * FUNCTION REGISTRY (exports)
 * Pen: TerminalPen_0/_1/_2/_3
 * Constructors: TerminalCanvas_0(), _4(cells,styles,w,h),
 *   _6(cells,styles,fg,bg,w,h)
 * Queries: bool TerminalCanvas_isValid
 *   bytes getWidth/getHeight
 *   u32 glyphAt, u8 styleAt, u32 fgAt/bgAt
 *   bool hasStylePlane/hasColorPlane
 * Decode: TerminalCanvasDecodedScalar TerminalCanvas_utf8Decode(text, length)
 * Regions: TerminalCanvas TerminalCanvas_sub(self, lx, ly, w, h)
 * Primitives: fill(self, glyph, pen), text(self, lx, ly, text, len, pen),
 *   hline(self, ly, glyph, pen), vline(self, lx, glyph, pen), box(self, pen)
 * Projections: TerminalCanvasTextRow copyRow(self, ly)
 *   TerminalProjection toString/toStringStruct(self)
 * CONTRACT: null/invalid canvases no-op; out-of-region coordinates clip; a null
 * plane drops that dimension; malformed UTF-8 becomes '?'; combining marks are
 * not composed (documented shaping gap). No allocation, no logging on any draw.
 * ======================================================================== */


;;GETTER
static inline u32 *TerminalCanvas_bg(const TerminalCanvas *s) { return (*s).bg; }
;;GETTER
static inline u32 *TerminalCanvas_cells(const TerminalCanvas *s) { return (*s).cells; }
;;GETTER
static inline u32 *TerminalCanvas_fg(const TerminalCanvas *s) { return (*s).fg; }
;;GETTER
static inline bytes TerminalCanvas_gridHeight(const TerminalCanvas *s) { return (*s).gridHeight; }
;;GETTER
static inline bytes TerminalCanvas_gridWidth(const TerminalCanvas *s) { return (*s).gridWidth; }
;;GETTER
static inline bytes TerminalCanvas_height(const TerminalCanvas *s) { return (*s).height; }
;;GETTER
static inline u16 *TerminalCanvas_styles(const TerminalCanvas *s) { return (*s).styles; }
;;GETTER
static inline bytes TerminalCanvas_width(const TerminalCanvas *s) { return (*s).width; }
;;GETTER
static inline bytes TerminalCanvas_x(const TerminalCanvas *s) { return (*s).x; }
;;GETTER
static inline bytes TerminalCanvas_y(const TerminalCanvas *s) { return (*s).y; }

TerminalPen TerminalPen_0() {
    return (TerminalPen) { TERMINAL_STYLE_NONE, TERMINAL_COLOR_DEFAULT, TERMINAL_COLOR_DEFAULT };
}
TerminalPen TerminalPen_1(u16 style) {
    return (TerminalPen) { style, TERMINAL_COLOR_DEFAULT, TERMINAL_COLOR_DEFAULT };
}
TerminalPen TerminalPen_2(u16 style, u32 fg) {
    return (TerminalPen) { style, fg, TERMINAL_COLOR_DEFAULT };
}
TerminalPen TerminalPen_3(u16 style, u32 fg, u32 bg) {
    return (TerminalPen) { style, fg, bg };
}

TerminalCanvas TerminalCanvas_0() {
    return (TerminalCanvas) {0};
}

static TerminalCanvas canvasMake(u32 *cells, u16 *styles, u32 *fg, u32 *bg,
                                 bytes width, bytes height) {
    TerminalCanvas c = {0};
    if (cells == nullptr || width == 0 || height == 0)
        return c;
    c.cells = cells;
    c.styles = styles;
    c.fg = fg;
    c.bg = bg;
    c.gridWidth = width;
    c.gridHeight = height;
    c.x = 0;
    c.y = 0;
    c.width = width;
    c.height = height;
    return c;
}

TerminalCanvas TerminalCanvas_4(u32 *cells, u16 *styles, bytes width, bytes height) {
    return canvasMake(cells, styles, nullptr, nullptr, width, height);
}

TerminalCanvas TerminalCanvas_6(u32 *cells, u16 *styles, u32 *fg, u32 *bg,
                                bytes width, bytes height) {
    return canvasMake(cells, styles, fg, bg, width, height);
}

bool TerminalCanvas_isValid(const TerminalCanvas *self) {
    return self != nullptr && TerminalCanvas_cells(self) != nullptr && TerminalCanvas_width(self) > 0 && TerminalCanvas_height(self) > 0;
}

static bytes minSize(bytes a, bytes b) { return a < b ? a : b; }

// Deterministic, locale-free cell width. 0 = combining mark; 2 = wide (CJK,
// emoji, fullwidth); 1 = everything else.
bytes TerminalCanvas_widthOf(u32 cp) {
    if ((cp >= 0x0300 && cp <= 0x036F) || (cp >= 0x1AB0 && cp <= 0x1AFF) ||
        (cp >= 0x1DC0 && cp <= 0x1DFF) || (cp >= 0x20D0 && cp <= 0x20FF) ||
        (cp >= 0xFE20 && cp <= 0xFE2F))
        return 0;
    if ((cp >= 0x1100 && cp <= 0x115F) || (cp >= 0x2E80 && cp <= 0xA4CF) ||
        (cp >= 0xAC00 && cp <= 0xD7A3) || (cp >= 0xF900 && cp <= 0xFAFF) ||
        (cp >= 0xFE30 && cp <= 0xFE6F) || (cp >= 0xFF00 && cp <= 0xFF60) ||
        (cp >= 0xFFE0 && cp <= 0xFFE6) || (cp >= 0x1F300 && cp <= 0x1FAFF) ||
        (cp >= 0x20000 && cp <= 0x3FFFD))
        return 2;
    return 1;
}

TerminalCanvasDecodedScalar TerminalCanvas_utf8Decode(const char *text, bytes length) {
    TerminalCanvasDecodedScalar decoded = {0};
    if (text == nullptr || length == 0)
        return decoded;
    const u8 *p = (const u8*) text;
    u8 b0 = p[0];
    if (b0 < 0x80u) {
        decoded.codepoint = b0;
        decoded.bytes = 1;
        return decoded;
    }
    bytes need;
    u32 cp;
    if ((b0 & 0xE0u) == 0xC0u) { need = 1; cp = b0 & 0x1Fu; }
    else if ((b0 & 0xF0u) == 0xE0u) { need = 2; cp = b0 & 0x0Fu; }
    else if ((b0 & 0xF8u) == 0xF0u) { need = 3; cp = b0 & 0x07u; }
    else return decoded;
    if (length < need + 1)
        return decoded;
    for (bytes i = 1; i <= need; ++i) {
        if ((p[i] & 0xC0u) != 0x80u)
            return decoded;
        cp = (cp << 6) | (p[i] & 0x3Fu);
    }
    decoded.codepoint = cp;
    decoded.bytes = need + 1;
    return decoded;
}

static bytes encodeUtf8(u32 cp, char *out) {
    if (cp < 0x80u) { out[0] = (char) cp; return 1; }
    if (cp < 0x800u) {
        out[0] = (char) (0xC0u | (cp >> 6));
        out[1] = (char) (0x80u | (cp & 0x3Fu));
        return 2;
    }
    if (cp < 0x10000u) {
        out[0] = (char) (0xE0u | (cp >> 12));
        out[1] = (char) (0x80u | ((cp >> 6) & 0x3Fu));
        out[2] = (char) (0x80u | (cp & 0x3Fu));
        return 3;
    }
    out[0] = (char) (0xF0u | (cp >> 18));
    out[1] = (char) (0x80u | ((cp >> 12) & 0x3Fu));
    out[2] = (char) (0x80u | ((cp >> 6) & 0x3Fu));
    out[3] = (char) (0x80u | (cp & 0x3Fu));
    return 4;
}

TerminalCanvas TerminalCanvas_sub(const TerminalCanvas *self, bytes lx, bytes ly,
                                  bytes w, bytes h) {
    TerminalCanvas out = {0};
    if (!TerminalCanvas_isValid(self))
        return out;
    if (lx >= TerminalCanvas_width(self) || ly >= TerminalCanvas_height(self))
        return out;
    bytes ax = TerminalCanvas_x(self) + lx;
    bytes ay = TerminalCanvas_y(self) + ly;
    if (ax >= TerminalCanvas_gridWidth(self) || ay >= TerminalCanvas_gridHeight(self))
        return out;
    bytes cw = minSize(w, TerminalCanvas_width(self) - lx);
    bytes ch = minSize(h, TerminalCanvas_height(self) - ly);
    cw = minSize(cw, TerminalCanvas_gridWidth(self) - ax);
    ch = minSize(ch, TerminalCanvas_gridHeight(self) - ay);
    if (cw == 0 || ch == 0)
        return out;
    out.cells = TerminalCanvas_cells(self);
    out.styles = TerminalCanvas_styles(self);
    out.fg = TerminalCanvas_fg(self);
    out.bg = TerminalCanvas_bg(self);
    out.gridWidth = TerminalCanvas_gridWidth(self);
    out.gridHeight = TerminalCanvas_gridHeight(self);
    out.x = ax;
    out.y = ay;
    out.width = cw;
    out.height = ch;
    return out;
}

static fn putCell(const TerminalCanvas *self, bytes ax, bytes ay, u32 glyph, TerminalPen pen) {
    if (ax >= TerminalCanvas_gridWidth(self) || ay >= TerminalCanvas_gridHeight(self))
        return;
    bytes index = ay * TerminalCanvas_gridWidth(self) + ax;
    TerminalCanvas_cells(self)[index] = glyph;
    if (TerminalCanvas_styles(self) != nullptr)
        TerminalCanvas_styles(self)[index] = pen.style;
    if (TerminalCanvas_fg(self) != nullptr)
        TerminalCanvas_fg(self)[index] = pen.fg;
    if (TerminalCanvas_bg(self) != nullptr)
        TerminalCanvas_bg(self)[index] = pen.bg;
}

fn TerminalCanvas_fill(TerminalCanvas *self, u32 glyph, TerminalPen pen) {
    if (!TerminalCanvas_isValid(self))
        return;
    for (bytes ly = 0; ly < TerminalCanvas_height(self); ++ly)
        for (bytes lx = 0; lx < TerminalCanvas_width(self); ++lx)
            putCell(self, TerminalCanvas_x(self) + lx, TerminalCanvas_y(self) + ly, glyph, pen);
}

// Write one scalar at a local column, advancing by its width and marking the
// continuation cell of a wide glyph with 0 so layout stays aligned.
static fn putScalar(const TerminalCanvas *self, bytes *lx, bytes ly, u32 cp, TerminalPen pen) {
    bytes width = TerminalCanvas_widthOf(cp);
    if (width == 0)
        return;   // combining marks are not composed (shaping gap)
    u32 glyph = (cp < 32u || cp == 127u) ? '?' : cp;
    putCell(self, TerminalCanvas_x(self) + *lx, TerminalCanvas_y(self) + ly, glyph, pen);
    if (width == 2 && *lx + 1 < TerminalCanvas_width(self))
        putCell(self, TerminalCanvas_x(self) + *lx + 1, TerminalCanvas_y(self) + ly, 0, pen);
    *lx += width;
}

fn TerminalCanvas_text(TerminalCanvas *self, bytes lx, bytes ly,
                         const char *text, bytes length, TerminalPen pen) {
    if (!TerminalCanvas_isValid(self) || text == nullptr || ly >= TerminalCanvas_height(self))
        return;
    bytes at = 0;
    while (at < length && lx < TerminalCanvas_width(self)) {
        TerminalCanvasDecodedScalar decoded = TerminalCanvas_utf8Decode(text + at, length - at);
        u32 cp = decoded.codepoint;
        bytes used = decoded.bytes;
        if (used == 0) { cp = '?'; used = 1; }
        putScalar(self, &lx, ly, cp, pen);
        at += used;
    }
}

fn TerminalCanvas_glyph(TerminalCanvas *self, bytes lx, bytes ly, u32 codepoint, TerminalPen pen) {
    if (!TerminalCanvas_isValid(self) || lx >= TerminalCanvas_width(self) || ly >= TerminalCanvas_height(self))
        return;
    bytes x = lx;
    putScalar(self, &x, ly, codepoint, pen);
}

fn TerminalCanvas_hline(TerminalCanvas *self, bytes ly, u32 glyph, TerminalPen pen) {
    if (!TerminalCanvas_isValid(self) || ly >= TerminalCanvas_height(self))
        return;
    for (bytes lx = 0; lx < TerminalCanvas_width(self); ++lx)
        putCell(self, TerminalCanvas_x(self) + lx, TerminalCanvas_y(self) + ly, glyph, pen);
}

fn TerminalCanvas_vline(TerminalCanvas *self, bytes lx, u32 glyph, TerminalPen pen) {
    if (!TerminalCanvas_isValid(self) || lx >= TerminalCanvas_width(self))
        return;
    for (bytes ly = 0; ly < TerminalCanvas_height(self); ++ly)
        putCell(self, TerminalCanvas_x(self) + lx, TerminalCanvas_y(self) + ly, glyph, pen);
}

fn TerminalCanvas_box(TerminalCanvas *self, TerminalPen pen) {
    if (!TerminalCanvas_isValid(self))
        return;
    TerminalCanvas_hline(self, 0, '-', pen);
    TerminalCanvas_hline(self, TerminalCanvas_height(self) - 1, '-', pen);
    TerminalCanvas_vline(self, 0, '|', pen);
    TerminalCanvas_vline(self, TerminalCanvas_width(self) - 1, '|', pen);
    putCell(self, TerminalCanvas_x(self), TerminalCanvas_y(self), '+', pen);
    putCell(self, TerminalCanvas_x(self) + TerminalCanvas_width(self) - 1, TerminalCanvas_y(self), '+', pen);
    putCell(self, TerminalCanvas_x(self), TerminalCanvas_y(self) + TerminalCanvas_height(self) - 1, '+', pen);
    putCell(self, TerminalCanvas_x(self) + TerminalCanvas_width(self) - 1, TerminalCanvas_y(self) + TerminalCanvas_height(self) - 1, '+', pen);
}

u32 TerminalCanvas_glyphAt(const TerminalCanvas *self, bytes lx, bytes ly) {
    if (!TerminalCanvas_isValid(self) || lx >= TerminalCanvas_width(self) || ly >= TerminalCanvas_height(self))
        return 0;
    return TerminalCanvas_cells(self)[(TerminalCanvas_y(self) + ly) * TerminalCanvas_gridWidth(self) + (TerminalCanvas_x(self) + lx)];
}

u16 TerminalCanvas_styleAt(const TerminalCanvas *self, bytes lx, bytes ly) {
    if (!TerminalCanvas_isValid(self) || TerminalCanvas_styles(self) == nullptr
        || lx >= TerminalCanvas_width(self) || ly >= TerminalCanvas_height(self))
        return TERMINAL_STYLE_NONE;
    return TerminalCanvas_styles(self)[(TerminalCanvas_y(self) + ly) * TerminalCanvas_gridWidth(self) + (TerminalCanvas_x(self) + lx)];
}

u32 TerminalCanvas_fgAt(const TerminalCanvas *self, bytes lx, bytes ly) {
    if (!TerminalCanvas_isValid(self) || TerminalCanvas_fg(self) == nullptr
        || lx >= TerminalCanvas_width(self) || ly >= TerminalCanvas_height(self))
        return TERMINAL_COLOR_DEFAULT;
    return TerminalCanvas_fg(self)[(TerminalCanvas_y(self) + ly) * TerminalCanvas_gridWidth(self) + (TerminalCanvas_x(self) + lx)];
}

u32 TerminalCanvas_bgAt(const TerminalCanvas *self, bytes lx, bytes ly) {
    if (!TerminalCanvas_isValid(self) || TerminalCanvas_bg(self) == nullptr
        || lx >= TerminalCanvas_width(self) || ly >= TerminalCanvas_height(self))
        return TERMINAL_COLOR_DEFAULT;
    return TerminalCanvas_bg(self)[(TerminalCanvas_y(self) + ly) * TerminalCanvas_gridWidth(self) + (TerminalCanvas_x(self) + lx)];
}

bytes TerminalCanvas_getWidth(const TerminalCanvas *self) { return self ? TerminalCanvas_width(self) : 0; }
bytes TerminalCanvas_getHeight(const TerminalCanvas *self) { return self ? TerminalCanvas_height(self) : 0; }
bool TerminalCanvas_hasStylePlane(const TerminalCanvas *self) { return self && TerminalCanvas_styles(self) != nullptr; }
bool TerminalCanvas_hasColorPlane(const TerminalCanvas *self) { return self && TerminalCanvas_fg(self) != nullptr; }

TerminalCanvasTextRow TerminalCanvas_copyRow(const TerminalCanvas *self, bytes ly) {
    TerminalCanvasTextRow row = {0};
    if (!TerminalCanvas_isValid(self) || ly >= TerminalCanvas_height(self))
        return row;
    for (bytes lx = 0; lx < TerminalCanvas_width(self); ++lx) {
        u32 cp = TerminalCanvas_glyphAt(self, lx, ly);
        if (cp == 0)                    // wide-glyph continuation cell
            cp = ' ';
        char encoded[4];
        bytes n = encodeUtf8(cp, encoded);
        if (row.bytes + n >= sizeof row.data) {
            row.truncated = true;
            break;
        }
        memcpy(row.data + row.bytes, encoded, n);
        row.bytes += n;
    }
    row.data[row.bytes] = '\0';
    return row;
}

TerminalProjection TerminalCanvas_toString(const TerminalCanvas *self) {
    return TerminalCanvas_toStringStruct(self);
}

TerminalProjection TerminalCanvas_toStringStruct(const TerminalCanvas *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format(
        "TerminalCanvas{region=%zux%zu@%zu,%zu,grid=%zux%zu,styles=%s,color=%s,valid=%d}",
        TerminalCanvas_width(self), TerminalCanvas_height(self), TerminalCanvas_x(self), TerminalCanvas_y(self),
        TerminalCanvas_gridWidth(self), TerminalCanvas_gridHeight(self),
        TerminalCanvas_styles(self) ? "yes" : "none", TerminalCanvas_fg(self) ? "yes" : "none",
        TerminalCanvas_isValid(self));
}
