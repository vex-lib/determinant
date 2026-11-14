#include "ui/surface/terminal_canvas.h"
#include "c23/constructor.h"
#include "exception/throw.h"
#include "annotation/annotation.h"
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
    return self != nullptr && (*self).cells != nullptr && (*self).width > 0 && (*self).height > 0;
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
    const unsigned char *p = (const unsigned char*) text;
    unsigned char b0 = p[0];
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
    if (lx >= (*self).width || ly >= (*self).height)
        return out;
    bytes ax = (*self).x + lx;
    bytes ay = (*self).y + ly;
    if (ax >= (*self).gridWidth || ay >= (*self).gridHeight)
        return out;
    bytes cw = minSize(w, (*self).width - lx);
    bytes ch = minSize(h, (*self).height - ly);
    cw = minSize(cw, (*self).gridWidth - ax);
    ch = minSize(ch, (*self).gridHeight - ay);
    if (cw == 0 || ch == 0)
        return out;
    out.cells = (*self).cells;
    out.styles = (*self).styles;
    out.fg = (*self).fg;
    out.bg = (*self).bg;
    out.gridWidth = (*self).gridWidth;
    out.gridHeight = (*self).gridHeight;
    out.x = ax;
    out.y = ay;
    out.width = cw;
    out.height = ch;
    return out;
}

static fn putCell(const TerminalCanvas *self, bytes ax, bytes ay, u32 glyph, TerminalPen pen) {
    if (ax >= (*self).gridWidth || ay >= (*self).gridHeight)
        return;
    bytes index = ay * (*self).gridWidth + ax;
    (*self).cells[index] = glyph;
    if ((*self).styles != nullptr)
        (*self).styles[index] = pen.style;
    if ((*self).fg != nullptr)
        (*self).fg[index] = pen.fg;
    if ((*self).bg != nullptr)
        (*self).bg[index] = pen.bg;
}

fn TerminalCanvas_fill(TerminalCanvas *self, u32 glyph, TerminalPen pen) {
    if (!TerminalCanvas_isValid(self))
        return;
    for (bytes ly = 0; ly < (*self).height; ++ly)
        for (bytes lx = 0; lx < (*self).width; ++lx)
            putCell(self, (*self).x + lx, (*self).y + ly, glyph, pen);
}

// Write one scalar at a local column, advancing by its width and marking the
// continuation cell of a wide glyph with 0 so layout stays aligned.
static fn putScalar(const TerminalCanvas *self, bytes *lx, bytes ly, u32 cp, TerminalPen pen) {
    bytes width = TerminalCanvas_widthOf(cp);
    if (width == 0)
        return;   // combining marks are not composed (shaping gap)
    u32 glyph = (cp < 32u || cp == 127u) ? '?' : cp;
    putCell(self, (*self).x + *lx, (*self).y + ly, glyph, pen);
    if (width == 2 && *lx + 1 < (*self).width)
        putCell(self, (*self).x + *lx + 1, (*self).y + ly, 0, pen);
    *lx += width;
}

fn TerminalCanvas_text(TerminalCanvas *self, bytes lx, bytes ly,
                         const char *text, bytes length, TerminalPen pen) {
    if (!TerminalCanvas_isValid(self) || text == nullptr || ly >= (*self).height)
        return;
    bytes at = 0;
    while (at < length && lx < (*self).width) {
        TerminalCanvasDecodedScalar decoded = TerminalCanvas_utf8Decode(text + at, length - at);
        u32 cp = decoded.codepoint;
        bytes used = decoded.bytes;
        if (used == 0) { cp = '?'; used = 1; }
        putScalar(self, &lx, ly, cp, pen);
        at += used;
    }
}

fn TerminalCanvas_glyph(TerminalCanvas *self, bytes lx, bytes ly, u32 codepoint, TerminalPen pen) {
    if (!TerminalCanvas_isValid(self) || lx >= (*self).width || ly >= (*self).height)
        return;
    bytes x = lx;
    putScalar(self, &x, ly, codepoint, pen);
}

fn TerminalCanvas_hline(TerminalCanvas *self, bytes ly, u32 glyph, TerminalPen pen) {
    if (!TerminalCanvas_isValid(self) || ly >= (*self).height)
        return;
    for (bytes lx = 0; lx < (*self).width; ++lx)
        putCell(self, (*self).x + lx, (*self).y + ly, glyph, pen);
}

fn TerminalCanvas_vline(TerminalCanvas *self, bytes lx, u32 glyph, TerminalPen pen) {
    if (!TerminalCanvas_isValid(self) || lx >= (*self).width)
        return;
    for (bytes ly = 0; ly < (*self).height; ++ly)
        putCell(self, (*self).x + lx, (*self).y + ly, glyph, pen);
}

fn TerminalCanvas_box(TerminalCanvas *self, TerminalPen pen) {
    if (!TerminalCanvas_isValid(self))
        return;
    TerminalCanvas_hline(self, 0, '-', pen);
    TerminalCanvas_hline(self, (*self).height - 1, '-', pen);
    TerminalCanvas_vline(self, 0, '|', pen);
    TerminalCanvas_vline(self, (*self).width - 1, '|', pen);
    putCell(self, (*self).x, (*self).y, '+', pen);
    putCell(self, (*self).x + (*self).width - 1, (*self).y, '+', pen);
    putCell(self, (*self).x, (*self).y + (*self).height - 1, '+', pen);
    putCell(self, (*self).x + (*self).width - 1, (*self).y + (*self).height - 1, '+', pen);
}

u32 TerminalCanvas_glyphAt(const TerminalCanvas *self, bytes lx, bytes ly) {
    if (!TerminalCanvas_isValid(self) || lx >= (*self).width || ly >= (*self).height)
        return 0;
    return (*self).cells[((*self).y + ly) * (*self).gridWidth + ((*self).x + lx)];
}

u16 TerminalCanvas_styleAt(const TerminalCanvas *self, bytes lx, bytes ly) {
    if (!TerminalCanvas_isValid(self) || (*self).styles == nullptr
        || lx >= (*self).width || ly >= (*self).height)
        return TERMINAL_STYLE_NONE;
    return (*self).styles[((*self).y + ly) * (*self).gridWidth + ((*self).x + lx)];
}

u32 TerminalCanvas_fgAt(const TerminalCanvas *self, bytes lx, bytes ly) {
    if (!TerminalCanvas_isValid(self) || (*self).fg == nullptr
        || lx >= (*self).width || ly >= (*self).height)
        return TERMINAL_COLOR_DEFAULT;
    return (*self).fg[((*self).y + ly) * (*self).gridWidth + ((*self).x + lx)];
}

u32 TerminalCanvas_bgAt(const TerminalCanvas *self, bytes lx, bytes ly) {
    if (!TerminalCanvas_isValid(self) || (*self).bg == nullptr
        || lx >= (*self).width || ly >= (*self).height)
        return TERMINAL_COLOR_DEFAULT;
    return (*self).bg[((*self).y + ly) * (*self).gridWidth + ((*self).x + lx)];
}

bytes TerminalCanvas_getWidth(const TerminalCanvas *self) { return self ? (*self).width : 0; }
bytes TerminalCanvas_getHeight(const TerminalCanvas *self) { return self ? (*self).height : 0; }
bool TerminalCanvas_hasStylePlane(const TerminalCanvas *self) { return self && (*self).styles != nullptr; }
bool TerminalCanvas_hasColorPlane(const TerminalCanvas *self) { return self && (*self).fg != nullptr; }

TerminalCanvasTextRow TerminalCanvas_copyRow(const TerminalCanvas *self, bytes ly) {
    TerminalCanvasTextRow row = {0};
    if (!TerminalCanvas_isValid(self) || ly >= (*self).height) {
        return row;
    }
    for (bytes lx = 0; lx < (*self).width; ++lx) {
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
        (*self).width, (*self).height, (*self).x, (*self).y,
        (*self).gridWidth, (*self).gridHeight,
        (*self).styles ? "yes" : "none", (*self).fg ? "yes" : "none",
        TerminalCanvas_isValid(self));
}
