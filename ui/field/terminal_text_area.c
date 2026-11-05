#include "ui/field/terminal_text_area.h"
#include "nio/mem.h"
#include "oop/type.h"
#include "exception/throw.h"
#include "annotation/annotation.h"
#include <string.h>
#include <stdio.h>

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
 * size_t capacity                         usable byte capacity
 * size_t length                           live byte length
 * size_t cursorByte                       active caret offset
 * size_t scrollRow, scrollCol             viewport offsets
 * bool wrap                               soft-wrap flag
 * TerminalPen pen, cursorPen              text and cursor brush pens
 * ======================================================================== */

TerminalTextArea *TerminalTextArea_0(void) {
    return TerminalTextArea_2(nullptr, 0u);
}

TerminalTextArea *TerminalTextArea_2(char *buffer, size_t capacity) {
    TerminalTextArea *self = (TerminalTextArea*) Memory_alloc(TYPE_BYTE_ARRAY, sizeof(TerminalTextArea));
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
        size_t slen = strlen(buffer);
        if (slen >= capacity)
            slen = capacity - 1u;
        (*self).length = slen;
        (*self).cursorByte = slen;
        buffer[slen] = '\0';
    }
    return self;
}

void TerminalTextArea_free(TerminalTextArea *self) {
    if (self == nullptr)
        return;
    Memory_free(self);
}

bool TerminalTextArea_bind(TerminalTextArea *self, char *buffer, size_t capacity) {
    if (self == nullptr || buffer == nullptr || capacity == 0u)
        return false;
    (*self).buffer = buffer;
    (*self).capacity = capacity;
    size_t slen = strlen(buffer);
    if (slen >= capacity)
        slen = capacity - 1u;
    (*self).length = slen;
    (*self).cursorByte = slen;
    buffer[slen] = '\0';
    return true;
}

const char *TerminalTextArea_getBuffer(const TerminalTextArea *self) {
    return (self != nullptr && (*self).buffer != nullptr) ? (*self).buffer : "";
}

size_t TerminalTextArea_getCapacity(const TerminalTextArea *self) {
    return self != nullptr ? (*self).capacity : 0u;
}

size_t TerminalTextArea_getLength(const TerminalTextArea *self) {
    return self != nullptr ? (*self).length : 0u;
}

bool TerminalTextArea_insertChar(TerminalTextArea *self, int character) {
    if (self == nullptr || (*self).buffer == nullptr)
        return false;
    if ((*self).length + 1u >= (*self).capacity)
        return false;

    char *buf = (*self).buffer;
    size_t at = (*self).cursorByte;
    size_t moveCount = (*self).length - at;
    if (moveCount > 0u)
        memmove(buf + at + 1u, buf + at, moveCount);

    buf[at] = (char) character;
    (*self).length++;
    (*self).cursorByte++;
    buf[(*self).length] = '\0';
    return true;
}

bool TerminalTextArea_insertText(TerminalTextArea *self, const char *text, size_t length) {
    if (self == nullptr || (*self).buffer == nullptr || text == nullptr || length == 0u)
        return false;
    if ((*self).length + length >= (*self).capacity)
        return false;

    char *buf = (*self).buffer;
    size_t at = (*self).cursorByte;
    size_t moveCount = (*self).length - at;
    if (moveCount > 0u)
        memmove(buf + at + length, buf + at, moveCount);

    memcpy(buf + at, text, length);
    (*self).length += length;
    (*self).cursorByte += length;
    buf[(*self).length] = '\0';
    return true;
}

bool TerminalTextArea_backspace(TerminalTextArea *self) {
    if (self == nullptr || (*self).buffer == nullptr || (*self).cursorByte == 0u)
        return false;

    char *buf = (*self).buffer;
    size_t at = (*self).cursorByte;
    size_t moveCount = (*self).length - at;
    if (moveCount > 0u)
        memmove(buf + at - 1u, buf + at, moveCount);

    (*self).length--;
    (*self).cursorByte--;
    buf[(*self).length] = '\0';
    return true;
}

bool TerminalTextArea_delete(TerminalTextArea *self) {
    if (self == nullptr || (*self).buffer == nullptr || (*self).cursorByte >= (*self).length)
        return false;

    char *buf = (*self).buffer;
    size_t at = (*self).cursorByte;
    size_t moveCount = (*self).length - at - 1u;
    if (moveCount > 0u)
        memmove(buf + at, buf + at + 1u, moveCount);

    (*self).length--;
    buf[(*self).length] = '\0';
    return true;
}

void TerminalTextArea_clear(TerminalTextArea *self) {
    if (self == nullptr)
        return;
    if ((*self).buffer != nullptr && (*self).capacity > 0u)
        (*self).buffer[0] = '\0';
    (*self).length = 0u;
    (*self).cursorByte = 0u;
    (*self).scrollRow = 0u;
    (*self).scrollCol = 0u;
}

void TerminalTextArea_moveCursor(TerminalTextArea *self, int deltaRow, int deltaCol) {
    if (self == nullptr || (*self).buffer == nullptr || (*self).length == 0u)
        return;

    if (deltaCol < 0) {
        if ((*self).cursorByte > 0u)
            (*self).cursorByte--;
    } else if (deltaCol > 0) {
        if ((*self).cursorByte < (*self).length)
            (*self).cursorByte++;
    }

    if (deltaRow < 0) {
        size_t at = (*self).cursorByte;
        while (at > 0u && (*self).buffer[at - 1u] != '\n')
            at--;
        if (at > 0u) {
            at--;
            while (at > 0u && (*self).buffer[at - 1u] != '\n')
                at--;
            (*self).cursorByte = at;
        }
    } else if (deltaRow > 0) {
        size_t at = (*self).cursorByte;
        while (at < (*self).length && (*self).buffer[at] != '\n')
            at++;
        if (at < (*self).length)
            (*self).cursorByte = at + 1u;
    }
}

size_t TerminalTextArea_getCursorByte(const TerminalTextArea *self) {
    return self != nullptr ? (*self).cursorByte : 0u;
}

bool TerminalTextArea_setCursorByte(TerminalTextArea *self, size_t byteOffset) {
    if (self == nullptr)
        return false;
    if (byteOffset > (*self).length)
        byteOffset = (*self).length;
    (*self).cursorByte = byteOffset;
    return true;
}

bool TerminalTextArea_setWrap(TerminalTextArea *self, bool wrap) {
    if (self == nullptr)
        return false;
    (*self).wrap = wrap;
    return true;
}

bool TerminalTextArea_getWrap(const TerminalTextArea *self) {
    return self != nullptr && (*self).wrap;
}

bool TerminalTextArea_setPen(TerminalTextArea *self, TerminalPen pen) {
    if (self == nullptr)
        return false;
    (*self).pen = pen;
    return true;
}

TerminalPen TerminalTextArea_getPen(const TerminalTextArea *self) {
    return self != nullptr ? (*self).pen : TerminalPen_0();
}

void TerminalTextArea_render(const TerminalTextArea *self, TerminalCanvas *canvas) {
    if (self == nullptr || canvas == nullptr || !TerminalCanvas_isValid(canvas))
        return;

    size_t width = TerminalCanvas_getWidth(canvas);
    size_t height = TerminalCanvas_getHeight(canvas);
    if (width == 0u || height == 0u)
        return;

    const char *buf = (*self).buffer;
    size_t len = (*self).length;
    size_t at = 0u;
    size_t row = 0u;
    size_t cursorRow = 0u;
    size_t cursorCol = 0u;

    while (at <= len) {
        size_t lineStart = at;
        while (at < len && buf[at] != '\n')
            at++;
        size_t lineLen = at - lineStart;

        if ((*self).cursorByte >= lineStart && (*self).cursorByte <= at) {
            cursorRow = row;
            cursorCol = (*self).cursorByte - lineStart;
        }

        if (row >= (*self).scrollRow && (row - (*self).scrollRow) < height) {
            size_t renderY = row - (*self).scrollRow;
            if (lineLen > (*self).scrollCol) {
                size_t showLen = lineLen - (*self).scrollCol;
                if (showLen > width)
                    showLen = width;
                TerminalCanvas_text(canvas, 0u, renderY, buf + lineStart + (*self).scrollCol, showLen, (*self).pen);
            }
        }
        row++;
        if (at >= len)
            break;
        at++; // skip \n
    }

    // Draw cursor if visible
    if (cursorRow >= (*self).scrollRow && (cursorRow - (*self).scrollRow) < height
        && cursorCol >= (*self).scrollCol && (cursorCol - (*self).scrollCol) < width) {
        size_t cx = cursorCol - (*self).scrollCol;
        size_t cy = cursorRow - (*self).scrollRow;
        char curChar = ' ';
        if ((*self).cursorByte < len && buf[(*self).cursorByte] != '\n')
            curChar = buf[(*self).cursorByte];
        TerminalCanvas_text(canvas, cx, cy, &curChar, 1u, (*self).cursorPen);
    }
}

TerminalProjection TerminalTextArea_toString(const TerminalTextArea *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format("TerminalTextArea(length=%zu,capacity=%zu,cursor=%zu,wrap=%d)",
        (*self).length, (*self).capacity, (*self).cursorByte, (*self).wrap);
}

TerminalProjection TerminalTextArea_toStringStruct(const TerminalTextArea *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format(
        "TerminalTextArea { buffer=%p capacity=%zu length=%zu cursorByte=%zu scrollRow=%zu scrollCol=%zu wrap=%d }",
        (const void*) (*self).buffer, (*self).capacity, (*self).length, (*self).cursorByte,
        (*self).scrollRow, (*self).scrollCol, (*self).wrap);
}
