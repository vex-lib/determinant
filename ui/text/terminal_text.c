#include "vex.h"
#include "ui/text/terminal_text.h"
#include "nio/mem.h"
#include "oop/oop.h"
#include <string.h>
#include <stdio.h>
#include "core/determinant_memory.h"

;;DEFINITION
/** The addressable leaf. A terminal interface is panels and text; this is the
 * text, and it is an object rather than a string handed to a draw call because
 * the interface must be able to find it again — to recolour it on hover, to ask
 * how wide it is, to move it. Identity is what makes hover possible, so the leaf
 * carries its own record even though its glyphs stay borrowed.
 *
 * It draws on one row of whatever region its parent resolved, with its own pen,
 * optional bracket glyphs and an alignment. Colours it does not set are inherited
 * from the parent pen, so a panel can declare "everything on me is grey" once and
 * every text on it agrees; styles combine instead, because a bitmask composes.
 * Nothing here allocates per paint or per cell, and a text never holds children. */
;;OVERVIEW
/** ========================================================================
 * CLASS: TerminalText (ui/text/terminal_text.c)
 * STRUCT FIELDS (Mirroring ui/text/terminal_text.h)
 *
 * const char *glyphs            borrowed UTF-8, must outlive the text
 * bytes length                 bytes in `glyphs`
 * TerminalPen pen               this text's own style/foreground/background
 * u32 bracketLeft          codepoint before the text (0 = none)
 * u32 bracketRight         codepoint after the text (0 = none)
 * TerminalTextAlign align       placement inside the resolved region
 * PRIVATE HELPERS
 * validGlyphs(glyphs, length)  borrowed string admission (non-null when length != 0)
 * resolvePen(own, inherited)   inherit unset colours, combine styles
 * leadingCells(body, width)    the alignment offset for a measured body
 * FUNCTION REGISTRY (exports)
 * Constructors: TerminalText_0(), _1(glyphs), _2(glyphs, length)
 * Lifecycle: fn TerminalText_free(self)
 * Configuration: bool setText/getText/getLength, setPen/getPen,
 *   setStyle/getStyle, setForegroundColor/getForegroundColor,
 *   setBackgroundColor/getBackgroundColor, setBrackets/getBracketLeft/getBracketRight,
 *   setAlign/getAlign
 * Measure: bytes TerminalText_measure(self)
 * Paint: fn TerminalText_paint(self, region, parentPen)
 * Projections: TerminalProjection TerminalText_toString/toStringStruct(self)
 * CONTRACT: a null text, a null glyph span carrying a non-zero length, or an
 * unknown alignment is rejected loudly and changes nothing; getters answer safe
 * defaults. The glyph string is borrowed and never freed here. Paint is clipped
 * to the region, quiet, and allocation-free; a text wider than its region is cut
 * at the region edge, so an over-wide label degrades rather than wrapping.
 * ======================================================================== */


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
static inline TerminalTextAlign TerminalText_align(const TerminalText *s) { return (*s).align; }
;;GETTER
static inline TerminalTextAlign *TerminalText_alignMut(TerminalText *s) { return &(*s).align; }
;;SETTER
static inline fn TerminalText_setAlign(TerminalText *s, TerminalTextAlign v) { (*s).align = v; }
;;GETTER
static inline u32 TerminalText_bracketLeft(const TerminalText *s) { return (*s).bracketLeft; }
;;GETTER
static inline u32 *TerminalText_bracketLeftMut(TerminalText *s) { return &(*s).bracketLeft; }
;;SETTER
static inline fn TerminalText_setBracketLeft(TerminalText *s, u32 v) { (*s).bracketLeft = v; }
;;GETTER
static inline u32 TerminalText_bracketRight(const TerminalText *s) { return (*s).bracketRight; }
;;GETTER
static inline u32 *TerminalText_bracketRightMut(TerminalText *s) { return &(*s).bracketRight; }
;;SETTER
static inline fn TerminalText_setBracketRight(TerminalText *s, u32 v) { (*s).bracketRight = v; }
;;GETTER
static inline const char * TerminalText_glyphs(const TerminalText *s) { return (*s).glyphs; }
;;GETTER
static inline const char * *TerminalText_glyphsMut(TerminalText *s) { return &(*s).glyphs; }
;;SETTER
static inline fn TerminalText_setGlyphs(TerminalText *s, const char * v) { (*s).glyphs = v; }
;;GETTER
static inline bytes TerminalText_length(const TerminalText *s) { return (*s).length; }
;;GETTER
static inline bytes *TerminalText_lengthMut(TerminalText *s) { return &(*s).length; }
;;SETTER
static inline fn TerminalText_setLength(TerminalText *s, bytes v) { (*s).length = v; }
;;GETTER
static inline TerminalPen TerminalText_pen(const TerminalText *s) { return (*s).pen; }
;;GETTER
static inline TerminalPen *TerminalText_penMut(TerminalText *s) { return &(*s).pen; }
;;SETTER
static inline fn TerminalText_setPen(TerminalText *s, TerminalPen v) { (*s).pen = v; }

static bool validGlyphs(const char *glyphs, bytes length) {
    return glyphs != nullptr || length == 0;
}

static bool validAlign(TerminalTextAlign align) {
    return align >= TERMINAL_TEXT_ALIGN_LEFT && align <= TERMINAL_TEXT_ALIGN_RIGHT;
}

// A colour the text did not set is taken from the parent; styles combine, since
// they are a bitmask and a parent DIM plus a child BOLD is a meaningful request.
static TerminalPen resolvePen(const TerminalPen *own, TerminalPen inherited) {
    TerminalPen pen = *own;
    if (!TERMINAL_COLOR_IS_SET(pen.fg))
        pen.fg = inherited.fg;
    if (!TERMINAL_COLOR_IS_SET(pen.bg))
        pen.bg = inherited.bg;
    pen.style |= inherited.style;
    return pen;
}

TerminalText *TerminalText_0() {
    TerminalText *self = Memory_alloc(TYPE_BYTE_ARRAY, Determinant_arena(), sizeof(TerminalText));
    if (self == nullptr)
        return nullptr; // R2 owner reports/counts allocation exhaustion.
    *self = (TerminalText) { .glyphs = "", .length = 0, .pen = TerminalPen_0(),
                             .align = TERMINAL_TEXT_ALIGN_LEFT };
    return self;
}

TerminalText *TerminalText_1(const char *glyphs) {
    return TerminalText(glyphs, glyphs != nullptr ? strlen(glyphs) : 0);
}

TerminalText *TerminalText_2(const char *glyphs, bytes length) {
    if (!validGlyphs(glyphs, length)) {
        THROW("TerminalText construct: null glyph span with a non-zero length");
        return nullptr;
    }
    TerminalText *self = TerminalText_0();
    if (self == nullptr)
        return nullptr;
    TerminalText_setGlyphs(self, length == 0 ? "" : glyphs);
    TerminalText_setLength(self, length);
    return self;
}

fn TerminalText_free(TerminalText *self) {
    if (self == nullptr)
        return;
    Memory_free(self);
}

bool TerminalText_setText(TerminalText *self, const char *glyphs, bytes length) {
    if (self == nullptr || !validGlyphs(glyphs, length)) {
        THROW("TerminalText text: null text or invalid glyph span");
        return false;
    }
    TerminalText_setGlyphs(self, length == 0 ? "" : glyphs);
    TerminalText_setLength(self, length);
    return true;
}

const char *TerminalText_getText(const TerminalText *self) { return self ? TerminalText_glyphs(self) : nullptr; }
bytes TerminalText_getLength(const TerminalText *self) { return self ? TerminalText_length(self) : 0; }

bool TerminalText_setPen(TerminalText *self, TerminalPen pen) {
    if (self == nullptr) {
        THROW("TerminalText pen: null text");
        return false;
    }
    TerminalText_setPen(self, pen);
    return true;
}
TerminalPen TerminalText_getPen(const TerminalText *self) {
    return self ? TerminalText_pen(self) : TerminalPen_0();
}
bool TerminalText_setStyle(TerminalText *self, u8 style) {
    if (self == nullptr) {
        THROW("TerminalText style: null text");
        return false;
    }
    TerminalPen *pen = TerminalText_penMut(self);
    TerminalPen_setStyle(pen, style);
    return true;
}
u8 TerminalText_getStyle(const TerminalText *self) {
    if (self == nullptr)
        return TERMINAL_STYLE_NONE;
    const TerminalPen *pen = TerminalText_penMut(self);
    return TerminalPen_style(pen);
}
bool TerminalText_setForegroundColor(TerminalText *self, u32 color) {
    if (self == nullptr) {
        THROW("TerminalText foreground: null text");
        return false;
    }
    TerminalPen *pen = TerminalText_penMut(self);
    TerminalPen_setFg(pen, color);
    return true;
}
u32 TerminalText_getForegroundColor(const TerminalText *self) {
    if (self == nullptr)
        return TERMINAL_COLOR_DEFAULT;
    const TerminalPen *pen = TerminalText_penMut(self);
    return TerminalPen_fg(pen);
}
bool TerminalText_setBackgroundColor(TerminalText *self, u32 color) {
    if (self == nullptr) {
        THROW("TerminalText background: null text");
        return false;
    }
    TerminalPen *pen = TerminalText_penMut(self);
    TerminalPen_setBg(pen, color);
    return true;
}
u32 TerminalText_getBackgroundColor(const TerminalText *self) {
    if (self == nullptr)
        return TERMINAL_COLOR_DEFAULT;
    const TerminalPen *pen = TerminalText_penMut(self);
    return TerminalPen_bg(pen);
}

bool TerminalText_setBrackets(TerminalText *self, u32 left, u32 right) {
    if (self == nullptr) {
        THROW("TerminalText brackets: null text");
        return false;
    }
    TerminalText_setBracketLeft(self, left);
    TerminalText_setBracketRight(self, right);
    return true;
}
u32 TerminalText_getBracketLeft(const TerminalText *self) {
    return self ? TerminalText_bracketLeft(self) : 0;
}
u32 TerminalText_getBracketRight(const TerminalText *self) {
    return self ? TerminalText_bracketRight(self) : 0;
}

bool TerminalText_setAlign(TerminalText *self, TerminalTextAlign align) {
    if (self == nullptr || !validAlign(align)) {
        THROW("TerminalText align: null text or unknown alignment");
        return false;
    }
    TerminalText_setAlign(self, align);
    return true;
}
TerminalTextAlign TerminalText_getAlign(const TerminalText *self) {
    return self ? TerminalText_align(self) : TERMINAL_TEXT_ALIGN_LEFT;
}

// Cells the body occupies, using the canvas's locale-free width rule so a wide
// glyph counts as two and the brackets line up with what paint actually writes.
static bytes bodyCells(const char *glyphs, bytes length) {
    bytes cells = 0;
    bytes at = 0;
    while (at < length) {
        TerminalCanvasDecodedScalar decoded = TerminalCanvas_utf8Decode(glyphs + at, length - at);
        if (decoded.bytes == 0) {
            ++cells;   // malformed byte: paint writes '?', one cell
            ++at;
            continue;
        }
        cells += TerminalCanvas_widthOf(decoded.codepoint);
        at += decoded.bytes;
    }
    return cells;
}

bytes TerminalText_measure(const TerminalText *self) {
    if (self == nullptr)
        return 0;
    bytes cells = bodyCells(TerminalText_glyphs(self), TerminalText_length(self));
    if (TerminalText_bracketLeft(self) != 0)
        ++cells;
    if (TerminalText_bracketRight(self) != 0)
        ++cells;
    return cells;
}

fn TerminalText_paint(const TerminalText *self, TerminalCanvas *region, TerminalPen parentPen) {
    if (self == nullptr || !TerminalCanvas_isValid(region))
        return;
    bytes regionWidth = TerminalCanvas_getWidth(region);
    bytes cells = TerminalText_measure(self);
    bytes x = 0;
    if (cells < regionWidth) {
        if (TerminalText_align(self) == TERMINAL_TEXT_ALIGN_CENTER)
            x = (regionWidth - cells) / 2;
        else if (TerminalText_align(self) == TERMINAL_TEXT_ALIGN_RIGHT)
            x = regionWidth - cells;
    }
    TerminalPen pen = resolvePen(TerminalText_penMut(self), parentPen);
    if (TerminalText_bracketLeft(self) != 0) {
        TerminalCanvas_glyph(region, x, 0, TerminalText_bracketLeft(self), pen);
        ++x;
    }
    TerminalCanvas_text(region, x, 0, TerminalText_glyphs(self), TerminalText_length(self), pen);
    x += bodyCells(TerminalText_glyphs(self), TerminalText_length(self));
    if (TerminalText_bracketRight(self) != 0)
        TerminalCanvas_glyph(region, x, 0, TerminalText_bracketRight(self), pen);
}

TerminalProjection TerminalText_toString(const TerminalText *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    const TerminalPen *pen = TerminalText_penMut(self);
    return TerminalProjection_format("TerminalText{cells=%zu,brackets=%d,align=%d,style=0x%02X}",
        TerminalText_measure(self), TerminalText_bracketLeft(self) != 0 || TerminalText_bracketRight(self) != 0,
        TerminalText_align(self), TerminalPen_style(pen));
}

TerminalProjection TerminalText_toStringStruct(const TerminalText *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    const TerminalPen *pen = TerminalText_penMut(self);
    return TerminalProjection_format(
        "TerminalText{glyphs=<borrowed:%zu bytes>,length=%zu,pen={style=0x%02X,fg=0x%08X,bg=0x%08X},bracketLeft=U+%04X,bracketRight=U+%04X,align=%d}",
        TerminalText_length(self), TerminalText_length(self), TerminalPen_style(pen), TerminalPen_fg(pen), TerminalPen_bg(pen),
        TerminalText_bracketLeft(self), TerminalText_bracketRight(self), TerminalText_align(self));
}
