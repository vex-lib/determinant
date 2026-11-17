#include "vex.h"
#include "ui/code/terminal_code_view.h"
#include "format/terminal_style.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

;;DEFINITION
/** TerminalCodeView paints one borrowed source-code line into terminal cells.
 * The parser and language adapter stay outside this R3 toolkit: they hand the
 * view ordered byte ranges with pens, and optional virtual hint labels anchored
 * at source byte offsets. The renderer turns those bytes into clipped cell
 * segments, preserving UTF-8 scalar boundaries and cell-based horizontal scroll.
 * Hints are presentation only; they never modify or masquerade as source text.
 *
 * Admission is cold and validates every span, hint, and UTF-8 source boundary
 * before replacing the previous borrowed line. Rendering is hot: it traverses
 * only the admitted arrays and writes cells without allocation, parsing, or log.
 * The host keeps text and arrays alive until its next setLine or free call. */

;;OVERVIEW
/** ============================================================================
 * CLASS: TerminalCodeView (ui/code/terminal_code_view.c)
 * STRUCT FIELDS (Mirroring ui/code/terminal_code_view.h)
 *   const char *text                    borrowed UTF-8 source line
 *   bytes length                       source byte length
 *   const TerminalCodeSpan *spans       borrowed ordered style ranges
 *   bytes spanCount                    number of admitted style ranges
 *   const TerminalCodeHint *hints       borrowed sorted virtual labels
 *   bytes hintCount                    number of admitted hints
 *   bytes scroll                       horizontal viewport offset in cells
 * SLOT RECORD: TerminalCodeSpan
 *   bytes startByte                    inclusive UTF-8 source byte offset
 *   bytes endByte                      exclusive UTF-8 source byte offset
 *   TerminalPen pen                     style/foreground/background for range
 * SLOT RECORD: TerminalCodeHint
 *   bytes byteOffset                   source byte anchor before rendered label
 *   const char *text                    borrowed virtual label text
 *   bytes length                       borrowed label byte length
 *   TerminalPen pen                     style/foreground/background for label
 * PRIVATE HELPERS
 *   scalarBytes(text, length)           valid UTF-8 scalar byte count, or one
 *   cellWidth(text, length)             scalar's terminal-cell width
 *   penAt(self, byte)                   admitted span pen or default pen
 *   paintRange(self, canvas, ...)       clip and draw one source byte range
 * FUNCTION REGISTRY (exports)
 *   Constructor: TerminalCodeView *TerminalCodeView_0(), chooser, zero
 *   Lifecycle: fn TerminalCodeView_free(self)
 *   Admission: bool TerminalCodeView_setLine(self, text, length, spans, count,
 *                                            hints, count)
 *              getters return borrowed source/span/hint views and counts
 *   Scroll: bool TerminalCodeView_setScroll(self, cells)
 *           bytes TerminalCodeView_getScroll(self)
 *   Render: fn TerminalCodeView_render(self, canvas, x, y)
 *   Projections: TerminalProjection TerminalCodeView_toString/toStringStruct(self)
 * CONTRACT:
 *   - Source, spans and hints are borrowed; invalid replacement preserves them.
 *   - Spans are ordered/non-overlapping; endpoints must be UTF-8 boundaries.
 *   - Hints are ordered by byte offset, may share an offset, and are virtual.
 *   - Horizontal offsets and render clipping count display cells, not bytes.
 *   - A null/invalid canvas or null receiver makes render a quiet no-op.
 *   - The hot renderer allocates nothing and does not validate admitted input.
 * ============================================================================ */


;;GETTER
static inline bytes TerminalCodeHint_byteOffset(const TerminalCodeHint *s) { return (*s).byteOffset; }
;;GETTER
static inline bytes *TerminalCodeHint_byteOffsetMut(TerminalCodeHint *s) { return &(*s).byteOffset; }
;;SETTER
static inline fn TerminalCodeHint_setByteOffset(TerminalCodeHint *s, bytes v) { (*s).byteOffset = v; }
;;GETTER
static inline bytes TerminalCodeHint_length(const TerminalCodeHint *s) { return (*s).length; }
;;GETTER
static inline bytes *TerminalCodeHint_lengthMut(TerminalCodeHint *s) { return &(*s).length; }
;;SETTER
static inline fn TerminalCodeHint_setLength(TerminalCodeHint *s, bytes v) { (*s).length = v; }
;;GETTER
static inline TerminalPen TerminalCodeHint_pen(const TerminalCodeHint *s) { return (*s).pen; }
;;GETTER
static inline TerminalPen *TerminalCodeHint_penMut(TerminalCodeHint *s) { return &(*s).pen; }
;;SETTER
static inline fn TerminalCodeHint_setPen(TerminalCodeHint *s, TerminalPen v) { (*s).pen = v; }
;;GETTER
static inline const char * TerminalCodeHint_text(const TerminalCodeHint *s) { return (*s).text; }
;;GETTER
static inline const char * *TerminalCodeHint_textMut(TerminalCodeHint *s) { return &(*s).text; }
;;SETTER
static inline fn TerminalCodeHint_setText(TerminalCodeHint *s, const char * v) { (*s).text = v; }

;;GETTER
static inline bytes TerminalCodeSpan_endByte(const TerminalCodeSpan *s) { return (*s).endByte; }
;;GETTER
static inline bytes *TerminalCodeSpan_endByteMut(TerminalCodeSpan *s) { return &(*s).endByte; }
;;SETTER
static inline fn TerminalCodeSpan_setEndByte(TerminalCodeSpan *s, bytes v) { (*s).endByte = v; }
;;GETTER
static inline TerminalPen TerminalCodeSpan_pen(const TerminalCodeSpan *s) { return (*s).pen; }
;;GETTER
static inline TerminalPen *TerminalCodeSpan_penMut(TerminalCodeSpan *s) { return &(*s).pen; }
;;SETTER
static inline fn TerminalCodeSpan_setPen(TerminalCodeSpan *s, TerminalPen v) { (*s).pen = v; }
;;GETTER
static inline bytes TerminalCodeSpan_startByte(const TerminalCodeSpan *s) { return (*s).startByte; }
;;GETTER
static inline bytes *TerminalCodeSpan_startByteMut(TerminalCodeSpan *s) { return &(*s).startByte; }
;;SETTER
static inline fn TerminalCodeSpan_setStartByte(TerminalCodeSpan *s, bytes v) { (*s).startByte = v; }

struct TerminalCodeView {
    const char *text;
    bytes length;
    const TerminalCodeSpan *spans;
    bytes spanCount;
    const TerminalCodeHint *hints;
    bytes hintCount;
    bytes scroll;
};

;;GETTER
static inline bytes TerminalCodeView_hintCount(const TerminalCodeView *s) { return (*s).hintCount; }
;;GETTER
static inline bytes *TerminalCodeView_hintCountMut(TerminalCodeView *s) { return &(*s).hintCount; }
;;SETTER
static inline fn TerminalCodeView_setHintCount(TerminalCodeView *s, bytes v) { (*s).hintCount = v; }
;;GETTER
static inline const TerminalCodeHint * TerminalCodeView_hints(const TerminalCodeView *s) { return (*s).hints; }
;;GETTER
static inline const TerminalCodeHint * *TerminalCodeView_hintsMut(TerminalCodeView *s) { return &(*s).hints; }
;;SETTER
static inline fn TerminalCodeView_setHints(TerminalCodeView *s, const TerminalCodeHint * v) { (*s).hints = v; }
;;GETTER
static inline bytes TerminalCodeView_length(const TerminalCodeView *s) { return (*s).length; }
;;GETTER
static inline bytes *TerminalCodeView_lengthMut(TerminalCodeView *s) { return &(*s).length; }
;;SETTER
static inline fn TerminalCodeView_setLength(TerminalCodeView *s, bytes v) { (*s).length = v; }
;;GETTER
static inline bytes TerminalCodeView_scroll(const TerminalCodeView *s) { return (*s).scroll; }
;;GETTER
static inline bytes *TerminalCodeView_scrollMut(TerminalCodeView *s) { return &(*s).scroll; }
;;SETTER
static inline fn TerminalCodeView_setScroll(TerminalCodeView *s, bytes v) { (*s).scroll = v; }
;;GETTER
static inline bytes TerminalCodeView_spanCount(const TerminalCodeView *s) { return (*s).spanCount; }
;;GETTER
static inline bytes *TerminalCodeView_spanCountMut(TerminalCodeView *s) { return &(*s).spanCount; }
;;SETTER
static inline fn TerminalCodeView_setSpanCount(TerminalCodeView *s, bytes v) { (*s).spanCount = v; }
;;GETTER
static inline const TerminalCodeSpan * TerminalCodeView_spans(const TerminalCodeView *s) { return (*s).spans; }
;;GETTER
static inline const TerminalCodeSpan * *TerminalCodeView_spansMut(TerminalCodeView *s) { return &(*s).spans; }
;;SETTER
static inline fn TerminalCodeView_setSpans(TerminalCodeView *s, const TerminalCodeSpan * v) { (*s).spans = v; }
;;GETTER
static inline const char * TerminalCodeView_text(const TerminalCodeView *s) { return (*s).text; }
;;GETTER
static inline const char * *TerminalCodeView_textMut(TerminalCodeView *s) { return &(*s).text; }
;;SETTER
static inline fn TerminalCodeView_setText(TerminalCodeView *s, const char * v) { (*s).text = v; }


static bytes scalarBytes(const char *text, bytes length) {
    TerminalCanvasDecodedScalar decoded = TerminalCanvas_utf8Decode(text, length);
    return decoded.bytes == 0 ? 1u : decoded.bytes;
}

static bytes cellWidth(const char *text, bytes length) {
    TerminalCanvasDecodedScalar decoded = TerminalCanvas_utf8Decode(text, length);
    return decoded.bytes == 0 ? 1u : TerminalCanvas_widthOf(decoded.codepoint);
}

static bool isBoundary(const char *text, bytes length, bytes offset) {
    if (offset > length)
        return false;
    bytes at = 0;
    while (at < offset) {
        TerminalCanvasDecodedScalar decoded = TerminalCanvas_utf8Decode(text + at,
                                                                          length - at);
        if (decoded.bytes == 0)
            return false;
        bytes byteCount = scalarBytes(text + at, length - at);
        if (byteCount > offset - at)
            return false;
        at += byteCount;
    }
    return at == offset;
}

static bool validSpans(const char *text, bytes length,
                       const TerminalCodeSpan *spans, bytes count) {
    bytes previousEnd = 0;
    for (bytes index = 0; index < count; ++index) {
        const TerminalCodeSpan *span = &spans[index];
        if (TerminalCodeSpan_startByte(span) > TerminalCodeSpan_endByte(span) || TerminalCodeSpan_endByte(span) > length
            || TerminalCodeSpan_startByte(span) < previousEnd
            || !isBoundary(text, length, TerminalCodeSpan_startByte(span))
            || !isBoundary(text, length, TerminalCodeSpan_endByte(span)))
            return false;
        previousEnd = TerminalCodeSpan_endByte(span);
    }
    return true;
}

static bool validHints(const char *text, bytes length,
                       const TerminalCodeHint *hints, bytes count) {
    bytes previousOffset = 0;
    for (bytes index = 0; index < count; ++index) {
        const TerminalCodeHint *hint = &hints[index];
        if (TerminalCodeHint_byteOffset(hint) > length || TerminalCodeHint_text(hint) == nullptr
            || TerminalCodeHint_length(hint) == 0 || TerminalCodeHint_byteOffset(hint) < previousOffset
            || !isBoundary(text, length, TerminalCodeHint_byteOffset(hint)))
            return false;
        previousOffset = TerminalCodeHint_byteOffset(hint);
    }
    return true;
}

static TerminalPen penAt(const TerminalCodeView *self, bytes byte) {
    for (bytes index = 0; index < TerminalCodeView_spanCount(self); ++index) {
        const TerminalCodeSpan *span = &TerminalCodeView_spansMut(self)[index];
        if (byte >= TerminalCodeSpan_startByte(span) && byte < TerminalCodeSpan_endByte(span))
            return TerminalCodeSpan_pen(span);
        if (byte < TerminalCodeSpan_startByte(span))
            break;
    }
    return TerminalPen_0();
}

static fn paintRange(const TerminalCodeView *self, TerminalCanvas *canvas,
                       bytes x, bytes y, bytes first, bytes end,
                       bytes *cell, bytes left, bytes right) {
    bytes at = first;
    while (at < end) {
        bytes byteCount = scalarBytes(TerminalCodeView_text(self) + at, end - at);
        bytes width = cellWidth(TerminalCodeView_text(self) + at, end - at);
        if ((*cell) >= left && (*cell) + width <= right)
            TerminalCanvas_text(canvas, x + (*cell) - left, y,
                                TerminalCodeView_text(self) + at, byteCount, penAt(self, at));
        (*cell) += width;
        at += byteCount;
    }
}

TerminalCodeView *TerminalCodeView_0() {
    return calloc(1u, sizeof(TerminalCodeView));
}

fn TerminalCodeView_free(TerminalCodeView *self) {
    free(self);
}

bool TerminalCodeView_setLine(TerminalCodeView *self, const char *text, bytes length,
                              const TerminalCodeSpan *spans, bytes spanCount,
                              const TerminalCodeHint *hints, bytes hintCount) {
    const char *admittedText = text == nullptr ? "" : text;
    if (self == nullptr || (text == nullptr && length != 0)
        || (spans == nullptr && spanCount != 0) || (hints == nullptr && hintCount != 0)
        || !isBoundary(admittedText, length, length)
        || !validSpans(admittedText, length, spans, spanCount)
        || !validHints(admittedText, length, hints, hintCount))
        return false;
    TerminalCodeView_setText(self, admittedText);
    TerminalCodeView_setLength(self, length);
    TerminalCodeView_setSpans(self, spans);
    TerminalCodeView_setSpanCount(self, spanCount);
    TerminalCodeView_setHints(self, hints);
    TerminalCodeView_setHintCount(self, hintCount);
    TerminalCodeView_setScroll(self, 0);
    return true;
}

bool TerminalCodeView_setScroll(TerminalCodeView *self, bytes cells) {
    if (self == nullptr)
        return false;
    TerminalCodeView_setScroll(self, cells);
    return true;
}

bytes TerminalCodeView_getScroll(const TerminalCodeView *self) {
    return self == nullptr ? 0u : TerminalCodeView_scroll(self);
}

const char *TerminalCodeView_getText(const TerminalCodeView *self) {
    return self == nullptr ? nullptr : TerminalCodeView_text(self);
}

bytes TerminalCodeView_getLength(const TerminalCodeView *self) {
    return self == nullptr ? 0u : TerminalCodeView_length(self);
}

const TerminalCodeSpan *TerminalCodeView_getSpans(const TerminalCodeView *self) {
    return self == nullptr ? nullptr : TerminalCodeView_spans(self);
}

bytes TerminalCodeView_getSpanCount(const TerminalCodeView *self) {
    return self == nullptr ? 0u : TerminalCodeView_spanCount(self);
}

const TerminalCodeHint *TerminalCodeView_getHints(const TerminalCodeView *self) {
    return self == nullptr ? nullptr : TerminalCodeView_hints(self);
}

bytes TerminalCodeView_getHintCount(const TerminalCodeView *self) {
    return self == nullptr ? 0u : TerminalCodeView_hintCount(self);
}

fn TerminalCodeView_render(const TerminalCodeView *self, TerminalCanvas *canvas,
                             bytes x, bytes y) {
    if (self == nullptr || !TerminalCanvas_isValid(canvas)
        || x >= TerminalCanvas_getWidth(canvas) || y >= TerminalCanvas_getHeight(canvas))
        return;
    bytes width = TerminalCanvas_getWidth(canvas) - x;
    bytes left = TerminalCodeView_scroll(self);
    bytes right = left > SIZE_MAX - width ? SIZE_MAX : left + width;
    bytes cell = 0;
    bytes at = 0;
    bytes hintIndex = 0;
    while (at <= TerminalCodeView_length(self)) {
        while (hintIndex < TerminalCodeView_hintCount(self)
               && TerminalCodeView_hints(self)[hintIndex].byteOffset == at) {
            const TerminalCodeHint *hint = &TerminalCodeView_hintsMut(self)[hintIndex++];
            bytes hintAt = 0;
            while (hintAt < TerminalCodeHint_length(hint)) {
                bytes byteCount = scalarBytes(TerminalCodeHint_text(hint) + hintAt, TerminalCodeHint_length(hint) - hintAt);
                bytes glyphWidth = cellWidth(TerminalCodeHint_text(hint) + hintAt, TerminalCodeHint_length(hint) - hintAt);
                if (cell >= left && cell + glyphWidth <= right)
                    TerminalCanvas_text(canvas, x + cell - left, y,
                                        TerminalCodeHint_text(hint) + hintAt, byteCount, TerminalCodeHint_pen(hint));
                cell += glyphWidth;
                hintAt += byteCount;
            }
        }
        if (at == TerminalCodeView_length(self))
            break;
        bytes next = at + scalarBytes(TerminalCodeView_text(self) + at, TerminalCodeView_length(self) - at);
        paintRange(self, canvas, x, y, at, next, &cell, left, right);
        at = next;
    }
}

TerminalProjection TerminalCodeView_toString(const TerminalCodeView *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format("TerminalCodeView(bytes=%zu, spans=%zu, hints=%zu, scroll=%zu)",
                                     TerminalCodeView_length(self), TerminalCodeView_spanCount(self),
                                     TerminalCodeView_hintCount(self), TerminalCodeView_scroll(self));
}

TerminalProjection TerminalCodeView_toStringStruct(const TerminalCodeView *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format("TerminalCodeView { text=%p, length=%zu, spans=%p, spanCount=%zu, hints=%p, hintCount=%zu, scroll=%zu }",
                                     (any) TerminalCodeView_text(self), TerminalCodeView_length(self),
                                     (any) TerminalCodeView_spans(self), TerminalCodeView_spanCount(self),
                                     (any) TerminalCodeView_hints(self), TerminalCodeView_hintCount(self),
                                     TerminalCodeView_scroll(self));
}
