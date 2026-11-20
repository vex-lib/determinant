#include "vex.h"
#include "ui/field/terminal_text_area.h"
#include "nio/mem.h"
#include "oop/oop.h"
#include <string.h>
#include <stdio.h>
#include "core/determinant_memory.h"

;;DEFINITION
/** Multiline text editing area. Holds a text buffer, tracks 2D cursor and viewport
 * offsets, allows arbitrary text insertions including newlines, and renders clipped rows
 * onto a TerminalCanvas.
 *
 * Distinct from TerminalInputLine (single-line prefix input) and TerminalCodeEditor
 * (syntax-colored line-numbered code editor). Designed for commit messages, descriptions,
 * and multiline inputs. */
;;OVERVIEW
/** ========================================================================
 * CLASS: TerminalTextArea (ui/field/terminal_text_area.c)
 * STRUCT FIELDS (Mirroring ui/field/terminal_text_area.h)
 *
 * char *buffer                            storage buffer (NUL-terminated at [length])
 * bytes capacity                         usable byte capacity
 * bytes length                           live byte length
 * bytes cursorByte                       active caret offset
 * bytes scrollRow, scrollCol             viewport offsets
 * bool wrap                               soft-wrap flag
 * TerminalPen pen, cursorPen              text and cursor brush pens
 * ======================================================================== */


;;GETTER
static inline char * TerminalTextArea_buffer(const TerminalTextArea *s) { return (*s).buffer; }
;;SETTER
static inline fn TerminalTextArea_setBuffer(TerminalTextArea *s, char * v) { (*s).buffer = v; }
;;GETTER
static inline bytes TerminalTextArea_capacity(const TerminalTextArea *s) { return (*s).capacity; }
;;SETTER
static inline fn TerminalTextArea_setCapacity(TerminalTextArea *s, bytes v) { (*s).capacity = v; }
;;GETTER
static inline bytes TerminalTextArea_cursorByte(const TerminalTextArea *s) { return (*s).cursorByte; }
;;SETTER
static inline fn TerminalTextArea_setCursorByteRaw(TerminalTextArea *s, bytes v) { (*s).cursorByte = v; }
;;GETTER
static inline TerminalPen TerminalTextArea_cursorPen(const TerminalTextArea *s) { return (*s).cursorPen; }
;;GETTER
static inline bytes TerminalTextArea_length(const TerminalTextArea *s) { return (*s).length; }
;;SETTER
static inline fn TerminalTextArea_setLength(TerminalTextArea *s, bytes v) { (*s).length = v; }
;;GETTER
static inline TerminalPen TerminalTextArea_pen(const TerminalTextArea *s) { return (*s).pen; }
;;SETTER
static inline fn TerminalTextArea_setPenRaw(TerminalTextArea *s, TerminalPen v) { (*s).pen = v; }
;;GETTER
static inline bytes TerminalTextArea_scrollCol(const TerminalTextArea *s) { return (*s).scrollCol; }
;;SETTER
static inline fn TerminalTextArea_setScrollCol(TerminalTextArea *s, bytes v) { (*s).scrollCol = v; }
;;GETTER
static inline bytes TerminalTextArea_scrollRow(const TerminalTextArea *s) { return (*s).scrollRow; }
;;SETTER
static inline fn TerminalTextArea_setScrollRow(TerminalTextArea *s, bytes v) { (*s).scrollRow = v; }
;;GETTER
static inline bool TerminalTextArea_wrap(const TerminalTextArea *s) { return (*s).wrap; }
;;SETTER
static inline fn TerminalTextArea_setWrapRaw(TerminalTextArea *s, bool v) { (*s).wrap = v; }

TerminalTextArea *TerminalTextArea_0() {
    return TerminalTextArea_2(nullptr, 0u);
}

TerminalTextArea *TerminalTextArea_2(char *buffer, bytes capacity) {
    TerminalTextArea *self = (TerminalTextArea*) Memory_alloc(TYPE_BYTE_ARRAY, Determinant_arena(), sizeof(TerminalTextArea));
    if (self == nullptr)
        return nullptr;
    *self = (TerminalTextArea) {
        .buffer = buffer,
        .capacity = capacity,
        .length = 0u,
        .cursorByte = 0u,
        .scrollRow = 0u,
        .scrollCol = 0u,
        .wrap = false,
        .pen = TerminalPen_0(),
        .cursorPen = TerminalPen_1(TERMINAL_STYLE_REVERSE)
    };
    if (buffer != nullptr && capacity > 0u) {
        bytes slen = strlen(buffer);
        if (slen >= capacity)
            slen = capacity - 1u;
        TerminalTextArea_setLength(self, slen);
        TerminalTextArea_setCursorByteRaw(self, slen);
        buffer[slen] = '\0';
    }
    return self;
}

fn TerminalTextArea_free(TerminalTextArea *self) {
    if (self == nullptr)
        return;
    Memory_free(self);
}

bool TerminalTextArea_bind(TerminalTextArea *self, char *buffer, bytes capacity) {
    if (self == nullptr || buffer == nullptr || capacity == 0u)
        return false;
    TerminalTextArea_setBuffer(self, buffer);
    TerminalTextArea_setCapacity(self, capacity);
    bytes slen = strlen(buffer);
    if (slen >= capacity)
        slen = capacity - 1u;
    TerminalTextArea_setLength(self, slen);
    TerminalTextArea_setCursorByteRaw(self, slen);
    buffer[slen] = '\0';
    return true;
}

const char *TerminalTextArea_getBuffer(const TerminalTextArea *self) {
    return (self != nullptr && TerminalTextArea_buffer(self) != nullptr) ? TerminalTextArea_buffer(self) : "";
}

bytes TerminalTextArea_getCapacity(const TerminalTextArea *self) {
    return self != nullptr ? TerminalTextArea_capacity(self) : 0u;
}

bytes TerminalTextArea_getLength(const TerminalTextArea *self) {
    return self != nullptr ? TerminalTextArea_length(self) : 0u;
}

bool TerminalTextArea_insertChar(TerminalTextArea *self, i32 character) {
    if (self == nullptr || TerminalTextArea_buffer(self) == nullptr)
        return false;
    if (TerminalTextArea_length(self) + 1u >= TerminalTextArea_capacity(self))
        return false;

    char *buf = TerminalTextArea_buffer(self);
    bytes at = TerminalTextArea_cursorByte(self);
    bytes moveCount = TerminalTextArea_length(self) - at;
    if (moveCount > 0u)
        memmove(buf + at + 1u, buf + at, moveCount);

    buf[at] = (char) character;
    TerminalTextArea_setLength(self, TerminalTextArea_length(self) + 1u);
    TerminalTextArea_setCursorByteRaw(self, TerminalTextArea_cursorByte(self) + 1u);
    buf[TerminalTextArea_length(self)] = '\0';
    return true;
}

bool TerminalTextArea_insertText(TerminalTextArea *self, const char *text, bytes length) {
    if (self == nullptr || TerminalTextArea_buffer(self) == nullptr || text == nullptr || length == 0u)
        return false;
    if (TerminalTextArea_length(self) + length >= TerminalTextArea_capacity(self))
        return false;

    char *buf = TerminalTextArea_buffer(self);
    bytes at = TerminalTextArea_cursorByte(self);
    bytes moveCount = TerminalTextArea_length(self) - at;
    if (moveCount > 0u)
        memmove(buf + at + length, buf + at, moveCount);

    memcpy(buf + at, text, length);
    TerminalTextArea_setLength(self, TerminalTextArea_length(self) + (length));
    TerminalTextArea_setCursorByteRaw(self, TerminalTextArea_cursorByte(self) + (length));
    buf[TerminalTextArea_length(self)] = '\0';
    return true;
}

bool TerminalTextArea_backspace(TerminalTextArea *self) {
    if (self == nullptr || TerminalTextArea_buffer(self) == nullptr || TerminalTextArea_cursorByte(self) == 0u)
        return false;

    char *buf = TerminalTextArea_buffer(self);
    bytes at = TerminalTextArea_cursorByte(self);
    bytes moveCount = TerminalTextArea_length(self) - at;
    if (moveCount > 0u)
        memmove(buf + at - 1u, buf + at, moveCount);

    TerminalTextArea_setLength(self, TerminalTextArea_length(self) - 1u);
    TerminalTextArea_setCursorByteRaw(self, TerminalTextArea_cursorByte(self) - 1u);
    buf[TerminalTextArea_length(self)] = '\0';
    return true;
}

bool TerminalTextArea_delete(TerminalTextArea *self) {
    if (self == nullptr || TerminalTextArea_buffer(self) == nullptr || TerminalTextArea_cursorByte(self) >= TerminalTextArea_length(self))
        return false;

    char *buf = TerminalTextArea_buffer(self);
    bytes at = TerminalTextArea_cursorByte(self);
    bytes moveCount = TerminalTextArea_length(self) - at - 1u;
    if (moveCount > 0u)
        memmove(buf + at, buf + at + 1u, moveCount);

    TerminalTextArea_setLength(self, TerminalTextArea_length(self) - 1u);
    buf[TerminalTextArea_length(self)] = '\0';
    return true;
}

fn TerminalTextArea_clear(TerminalTextArea *self) {
    if (self == nullptr)
        return;
    if (TerminalTextArea_buffer(self) != nullptr && TerminalTextArea_capacity(self) > 0u)
        TerminalTextArea_buffer(self)[0] = '\0';
    TerminalTextArea_setLength(self, 0u);
    TerminalTextArea_setCursorByteRaw(self, 0u);
    TerminalTextArea_setScrollRow(self, 0u);
    TerminalTextArea_setScrollCol(self, 0u);
}

fn TerminalTextArea_moveCursor(TerminalTextArea *self, i32 deltaRow, i32 deltaCol) {
    if (self == nullptr || TerminalTextArea_buffer(self) == nullptr || TerminalTextArea_length(self) == 0u)
        return;

    if (deltaCol < 0) {
        if (TerminalTextArea_cursorByte(self) > 0u)
            TerminalTextArea_setCursorByteRaw(self, TerminalTextArea_cursorByte(self) - 1u);
    } else if (deltaCol > 0) {
        if (TerminalTextArea_cursorByte(self) < TerminalTextArea_length(self))
            TerminalTextArea_setCursorByteRaw(self, TerminalTextArea_cursorByte(self) + 1u);
    }

    if (deltaRow < 0) {
        bytes at = TerminalTextArea_cursorByte(self);
        while (at > 0u && TerminalTextArea_buffer(self)[at - 1u] != '\n')
            at--;
        if (at > 0u) {
            at--;
            while (at > 0u && TerminalTextArea_buffer(self)[at - 1u] != '\n')
                at--;
            TerminalTextArea_setCursorByteRaw(self, at);
        }
    } else if (deltaRow > 0) {
        bytes at = TerminalTextArea_cursorByte(self);
        while (at < TerminalTextArea_length(self) && TerminalTextArea_buffer(self)[at] != '\n')
            at++;
        if (at < TerminalTextArea_length(self))
            TerminalTextArea_setCursorByteRaw(self, at + 1u);
    }
}

bytes TerminalTextArea_getCursorByte(const TerminalTextArea *self) {
    return self != nullptr ? TerminalTextArea_cursorByte(self) : 0u;
}

bool TerminalTextArea_setCursorByte(TerminalTextArea *self, bytes byteOffset) {
    if (self == nullptr)
        return false;
    if (byteOffset > TerminalTextArea_length(self))
        byteOffset = TerminalTextArea_length(self);
    TerminalTextArea_setCursorByteRaw(self, byteOffset);
    return true;
}

bool TerminalTextArea_setWrap(TerminalTextArea *self, bool wrap) {
    if (self == nullptr)
        return false;
    TerminalTextArea_setWrapRaw(self, wrap);
    return true;
}

bool TerminalTextArea_getWrap(const TerminalTextArea *self) {
    return self != nullptr && TerminalTextArea_wrap(self);
}

bool TerminalTextArea_setPen(TerminalTextArea *self, TerminalPen pen) {
    if (self == nullptr)
        return false;
    TerminalTextArea_setPenRaw(self, pen);
    return true;
}

TerminalPen TerminalTextArea_getPen(const TerminalTextArea *self) {
    return self != nullptr ? TerminalTextArea_pen(self) : TerminalPen_0();
}

fn TerminalTextArea_render(const TerminalTextArea *self, TerminalCanvas *canvas) {
    if (self == nullptr || canvas == nullptr || !TerminalCanvas_isValid(canvas))
        return;

    bytes width = TerminalCanvas_getWidth(canvas);
    bytes height = TerminalCanvas_getHeight(canvas);
    if (width == 0u || height == 0u)
        return;

    const char *buf = TerminalTextArea_buffer(self);
    bytes len = TerminalTextArea_length(self);
    bytes at = 0u;
    bytes row = 0u;
    bytes cursorRow = 0u;
    bytes cursorCol = 0u;

    while (at <= len) {
        bytes lineStart = at;
        while (at < len && buf[at] != '\n')
            at++;
        bytes lineLen = at - lineStart;

        if (TerminalTextArea_cursorByte(self) >= lineStart && TerminalTextArea_cursorByte(self) <= at) {
            cursorRow = row;
            cursorCol = TerminalTextArea_cursorByte(self) - lineStart;
        }

        if (row >= TerminalTextArea_scrollRow(self) && (row - TerminalTextArea_scrollRow(self)) < height) {
            bytes renderY = row - TerminalTextArea_scrollRow(self);
            if (lineLen > TerminalTextArea_scrollCol(self)) {
                bytes showLen = lineLen - TerminalTextArea_scrollCol(self);
                if (showLen > width)
                    showLen = width;
                TerminalCanvas_text(canvas, 0u, renderY, buf + lineStart + TerminalTextArea_scrollCol(self), showLen, TerminalTextArea_pen(self));
            }
        }
        row++;
        if (at >= len)
            break;
        at++; // skip \n
    }

    // Draw cursor if visible
    if (cursorRow >= TerminalTextArea_scrollRow(self) && (cursorRow - TerminalTextArea_scrollRow(self)) < height
        && cursorCol >= TerminalTextArea_scrollCol(self) && (cursorCol - TerminalTextArea_scrollCol(self)) < width) {
        bytes cx = cursorCol - TerminalTextArea_scrollCol(self);
        bytes cy = cursorRow - TerminalTextArea_scrollRow(self);
        char curChar = ' ';
        if (TerminalTextArea_cursorByte(self) < len && buf[TerminalTextArea_cursorByte(self)] != '\n')
            curChar = buf[TerminalTextArea_cursorByte(self)];
        TerminalCanvas_text(canvas, cx, cy, &curChar, 1u, TerminalTextArea_cursorPen(self));
    }
}

TerminalProjection TerminalTextArea_toString(const TerminalTextArea *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format("TerminalTextArea(length=%zu,capacity=%zu,cursor=%zu,wrap=%d)",
        TerminalTextArea_length(self), TerminalTextArea_capacity(self), TerminalTextArea_cursorByte(self), TerminalTextArea_wrap(self));
}

TerminalProjection TerminalTextArea_toStringStruct(const TerminalTextArea *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format(
        "TerminalTextArea { buffer=%p capacity=%zu length=%zu cursorByte=%zu scrollRow=%zu scrollCol=%zu wrap=%d }",
        (const_any) TerminalTextArea_buffer(self), TerminalTextArea_capacity(self), TerminalTextArea_length(self), TerminalTextArea_cursorByte(self),
        TerminalTextArea_scrollRow(self), TerminalTextArea_scrollCol(self), TerminalTextArea_wrap(self));
}
