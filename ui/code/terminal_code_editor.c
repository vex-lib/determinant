#include "vex.h"
#include "ui/code/terminal_code_editor.h"
#include "nio/mem.h"
#include "oop/oop.h"
#include <string.h>
#include <stdio.h>
#include "core/determinant_memory.h"

;;DEFINITION
/** The canonical code editor area (CodeArea). Manages line-indexed row storage,
 * a dynamic line-number gutter, auto-indentation matching on Enter, line joining/splitting,
 * 2D caret placement, and canvas rendering.
 *
 * Implements the CodeArea layer distinct from single-line fields (TerminalInputLine)
 * and plain multiline note areas (TerminalTextArea). */
;;OVERVIEW
/** ========================================================================
 * CLASS: TerminalCodeEditor (ui/code/terminal_code_editor.c)
 * STRUCT FIELDS (Mirroring ui/code/terminal_code_editor.h)
 *
 * char **lines                            row string storage
 * bytes *lineLengths, *lineCapacities    per-line metrics
 * bytes lineCount, lineCapacity          line array geometry
 * bytes cursorLine, cursorCol            2D caret coordinates
 * bytes scrollTop, scrollLeft            viewport offsets
 * bool showGutter                         gutter enabled flag
 * TerminalPen gutterPen, textPen, cursorPen  styling pens
 * ======================================================================== */


;;GETTER
static inline bytes TerminalCodeEditor_cursorCol(const TerminalCodeEditor *s) { return (*s).cursorCol; }
;;GETTER
static inline bytes *TerminalCodeEditor_cursorColMut(TerminalCodeEditor *s) { return &(*s).cursorCol; }
;;SETTER
static inline fn TerminalCodeEditor_setCursorCol(TerminalCodeEditor *s, bytes v) { (*s).cursorCol = v; }
;;GETTER
static inline bytes TerminalCodeEditor_cursorLine(const TerminalCodeEditor *s) { return (*s).cursorLine; }
;;GETTER
static inline bytes *TerminalCodeEditor_cursorLineMut(TerminalCodeEditor *s) { return &(*s).cursorLine; }
;;SETTER
static inline fn TerminalCodeEditor_setCursorLine(TerminalCodeEditor *s, bytes v) { (*s).cursorLine = v; }
;;GETTER
static inline TerminalPen TerminalCodeEditor_cursorPen(const TerminalCodeEditor *s) { return (*s).cursorPen; }
;;GETTER
static inline TerminalPen *TerminalCodeEditor_cursorPenMut(TerminalCodeEditor *s) { return &(*s).cursorPen; }
;;SETTER
static inline fn TerminalCodeEditor_setCursorPen(TerminalCodeEditor *s, TerminalPen v) { (*s).cursorPen = v; }
;;GETTER
static inline TerminalPen TerminalCodeEditor_gutterPen(const TerminalCodeEditor *s) { return (*s).gutterPen; }
;;GETTER
static inline TerminalPen *TerminalCodeEditor_gutterPenMut(TerminalCodeEditor *s) { return &(*s).gutterPen; }
;;SETTER
static inline fn TerminalCodeEditor_setGutterPen(TerminalCodeEditor *s, TerminalPen v) { (*s).gutterPen = v; }
;;GETTER
static inline bytes * TerminalCodeEditor_lineCapacities(const TerminalCodeEditor *s) { return (*s).lineCapacities; }
;;GETTER
static inline bytes * *TerminalCodeEditor_lineCapacitiesMut(TerminalCodeEditor *s) { return &(*s).lineCapacities; }
;;SETTER
static inline fn TerminalCodeEditor_setLineCapacities(TerminalCodeEditor *s, bytes * v) { (*s).lineCapacities = v; }
;;GETTER
static inline bytes TerminalCodeEditor_lineCapacity(const TerminalCodeEditor *s) { return (*s).lineCapacity; }
;;GETTER
static inline bytes *TerminalCodeEditor_lineCapacityMut(TerminalCodeEditor *s) { return &(*s).lineCapacity; }
;;SETTER
static inline fn TerminalCodeEditor_setLineCapacity(TerminalCodeEditor *s, bytes v) { (*s).lineCapacity = v; }
;;GETTER
static inline bytes TerminalCodeEditor_lineCount(const TerminalCodeEditor *s) { return (*s).lineCount; }
;;GETTER
static inline bytes *TerminalCodeEditor_lineCountMut(TerminalCodeEditor *s) { return &(*s).lineCount; }
;;SETTER
static inline fn TerminalCodeEditor_setLineCount(TerminalCodeEditor *s, bytes v) { (*s).lineCount = v; }
;;GETTER
static inline bytes * TerminalCodeEditor_lineLengths(const TerminalCodeEditor *s) { return (*s).lineLengths; }
;;GETTER
static inline bytes * *TerminalCodeEditor_lineLengthsMut(TerminalCodeEditor *s) { return &(*s).lineLengths; }
;;SETTER
static inline fn TerminalCodeEditor_setLineLengths(TerminalCodeEditor *s, bytes * v) { (*s).lineLengths = v; }
;;GETTER
static inline char ** TerminalCodeEditor_lines(const TerminalCodeEditor *s) { return (*s).lines; }
;;GETTER
static inline char ** *TerminalCodeEditor_linesMut(TerminalCodeEditor *s) { return &(*s).lines; }
;;SETTER
static inline fn TerminalCodeEditor_setLines(TerminalCodeEditor *s, char ** v) { (*s).lines = v; }
;;GETTER
static inline bytes TerminalCodeEditor_scrollLeft(const TerminalCodeEditor *s) { return (*s).scrollLeft; }
;;GETTER
static inline bytes *TerminalCodeEditor_scrollLeftMut(TerminalCodeEditor *s) { return &(*s).scrollLeft; }
;;SETTER
static inline fn TerminalCodeEditor_setScrollLeft(TerminalCodeEditor *s, bytes v) { (*s).scrollLeft = v; }
;;GETTER
static inline bytes TerminalCodeEditor_scrollTop(const TerminalCodeEditor *s) { return (*s).scrollTop; }
;;GETTER
static inline bytes *TerminalCodeEditor_scrollTopMut(TerminalCodeEditor *s) { return &(*s).scrollTop; }
;;SETTER
static inline fn TerminalCodeEditor_setScrollTop(TerminalCodeEditor *s, bytes v) { (*s).scrollTop = v; }
;;GETTER
static inline bool TerminalCodeEditor_showGutter(const TerminalCodeEditor *s) { return (*s).showGutter; }
;;GETTER
static inline bool *TerminalCodeEditor_showGutterMut(TerminalCodeEditor *s) { return &(*s).showGutter; }
;;SETTER
static inline fn TerminalCodeEditor_setShowGutter(TerminalCodeEditor *s, bool v) { (*s).showGutter = v; }
;;GETTER
static inline TerminalPen TerminalCodeEditor_textPen(const TerminalCodeEditor *s) { return (*s).textPen; }
;;GETTER
static inline TerminalPen *TerminalCodeEditor_textPenMut(TerminalCodeEditor *s) { return &(*s).textPen; }
;;SETTER
static inline fn TerminalCodeEditor_setTextPen(TerminalCodeEditor *s, TerminalPen v) { (*s).textPen = v; }

#define TERMINAL_CODE_EDITOR_INIT_CAPACITY 16u
#define TERMINAL_CODE_EDITOR_LINE_INIT_CAP 64u

static bool reserveLineCapacity(TerminalCodeEditor *self, bytes lineIndex, bytes needed) {
    if (self == nullptr || lineIndex >= TerminalCodeEditor_lineCount(self))
        return false;
    if (TerminalCodeEditor_lineCapacities(self)[lineIndex] >= needed)
        return true;
    bytes newCap = TerminalCodeEditor_lineCapacities(self)[lineIndex] * 2u;
    if (newCap < needed)
        newCap = needed + 32u;
    char *newBuf = (char*) Memory_alloc(TYPE_BYTE_ARRAY, Determinant_arena(), newCap);
    if (newBuf == nullptr)
        return false;
    if (TerminalCodeEditor_lines(self)[lineIndex] != nullptr) {
        memcpy(newBuf, TerminalCodeEditor_lines(self)[lineIndex], TerminalCodeEditor_lineLengths(self)[lineIndex] + 1u);
        Memory_free(TerminalCodeEditor_lines(self)[lineIndex]);
    } else {
        newBuf[0] = '\0';
    }
    TerminalCodeEditor_lines(self)[lineIndex] = newBuf;
    TerminalCodeEditor_lineCapacities(self)[lineIndex] = newCap;
    return true;
}

static bool reserveArrayCapacity(TerminalCodeEditor *self, bytes needed) {
    if (self == nullptr)
        return false;
    if (TerminalCodeEditor_lineCapacity(self) >= needed)
        return true;
    bytes newCap = TerminalCodeEditor_lineCapacity(self) * 2u;
    if (newCap < needed)
        newCap = needed + 8u;

    char **newLines = (char**) Memory_alloc(TYPE_BYTE_ARRAY, Determinant_arena(), newCap * sizeof(char*));
    bytes *newLengths = (bytes*) Memory_alloc(TYPE_BYTE_ARRAY, Determinant_arena(), newCap * sizeof(bytes));
    bytes *newCaps = (bytes*) Memory_alloc(TYPE_BYTE_ARRAY, Determinant_arena(), newCap * sizeof(bytes));

    if (newLines == nullptr || newLengths == nullptr || newCaps == nullptr) {
        if (newLines != nullptr) Memory_free(newLines);
        if (newLengths != nullptr) Memory_free(newLengths);
        if (newCaps != nullptr) Memory_free(newCaps);
        return false;
    }

    if (TerminalCodeEditor_lines(self) != nullptr) {
        memcpy(newLines, TerminalCodeEditor_lines(self), TerminalCodeEditor_lineCount(self) * sizeof(char*));
        memcpy(newLengths, TerminalCodeEditor_lineLengths(self), TerminalCodeEditor_lineCount(self) * sizeof(bytes));
        memcpy(newCaps, TerminalCodeEditor_lineCapacities(self), TerminalCodeEditor_lineCount(self) * sizeof(bytes));
        Memory_free(TerminalCodeEditor_lines(self));
        Memory_free(TerminalCodeEditor_lineLengths(self));
        Memory_free(TerminalCodeEditor_lineCapacities(self));
    }

    TerminalCodeEditor_setLines(self, newLines);
    TerminalCodeEditor_setLineLengths(self, newLengths);
    TerminalCodeEditor_setLineCapacities(self, newCaps);
    TerminalCodeEditor_setLineCapacity(self, newCap);
    return true;
}

static fn freeAllLines(TerminalCodeEditor *self) {
    if (self == nullptr || TerminalCodeEditor_lines(self) == nullptr)
        return;
    for (bytes i = 0u; i < TerminalCodeEditor_lineCount(self); ++i) {
        if (TerminalCodeEditor_lines(self)[i] != nullptr)
            Memory_free(TerminalCodeEditor_lines(self)[i]);
    }
    Memory_free(TerminalCodeEditor_lines(self));
    Memory_free(TerminalCodeEditor_lineLengths(self));
    Memory_free(TerminalCodeEditor_lineCapacities(self));
    TerminalCodeEditor_setLines(self, nullptr);
    TerminalCodeEditor_setLineLengths(self, nullptr);
    TerminalCodeEditor_setLineCapacities(self, nullptr);
    TerminalCodeEditor_setLineCount(self, 0u);
    TerminalCodeEditor_setLineCapacity(self, 0u);
}

TerminalCodeEditor *TerminalCodeEditor_0() {
    return TerminalCodeEditor_1(nullptr);
}

TerminalCodeEditor *TerminalCodeEditor_1(const char *initialText) {
    TerminalCodeEditor *self = (TerminalCodeEditor*) Memory_alloc(TYPE_BYTE_ARRAY, Determinant_arena(), sizeof(TerminalCodeEditor));
    if (self == nullptr)
        return nullptr;
    *self = (TerminalCodeEditor) {
        .lines = nullptr,
        .lineLengths = nullptr,
        .lineCapacities = nullptr,
        .lineCount = 0u,
        .lineCapacity = 0u,
        .cursorLine = 0u,
        .cursorCol = 0u,
        .scrollTop = 0u,
        .scrollLeft = 0u,
        .showGutter = true,
        .gutterPen = TerminalPen_1(TERMINAL_STYLE_DIM),
        .textPen = TerminalPen_0(),
        .cursorPen = TerminalPen_1(TERMINAL_STYLE_REVERSE)
    };

    if (initialText != nullptr && *initialText != '\0') {
        TerminalCodeEditor_setText(self, initialText);
    } else {
        reserveArrayCapacity(self, TERMINAL_CODE_EDITOR_INIT_CAPACITY);
        TerminalCodeEditor_setLineCount(self, 1u);
        TerminalCodeEditor_lines(self)[0] = (char*) Memory_alloc(TYPE_BYTE_ARRAY, Determinant_arena(), TERMINAL_CODE_EDITOR_LINE_INIT_CAP);
        TerminalCodeEditor_lines(self)[0][0] = '\0';
        TerminalCodeEditor_lineLengths(self)[0] = 0u;
        TerminalCodeEditor_lineCapacities(self)[0] = TERMINAL_CODE_EDITOR_LINE_INIT_CAP;
    }
    return self;
}

fn TerminalCodeEditor_free(TerminalCodeEditor *self) {
    if (self == nullptr)
        return;
    freeAllLines(self);
    Memory_free(self);
}

bool TerminalCodeEditor_setText(TerminalCodeEditor *self, const char *text) {
    if (self == nullptr || text == nullptr)
        return false;
    freeAllLines(self);

    bytes len = strlen(text);
    bytes estimatedLines = 1u;
    for (bytes i = 0u; i < len; ++i) {
        if (text[i] == '\n')
            estimatedLines++;
    }

    if (!reserveArrayCapacity(self, estimatedLines + 4u))
        return false;

    bytes at = 0u;
    while (at <= len) {
        bytes lineStart = at;
        while (at < len && text[at] != '\n')
            at++;
        bytes lineLen = at - lineStart;

        bytes idx = TerminalCodeEditor_lineCount(self);
        bytes cap = lineLen + 16u;
        char *buf = (char*) Memory_alloc(TYPE_BYTE_ARRAY, Determinant_arena(), cap);
        if (buf == nullptr)
            return false;
        memcpy(buf, text + lineStart, lineLen);
        buf[lineLen] = '\0';

        TerminalCodeEditor_lines(self)[idx] = buf;
        TerminalCodeEditor_lineLengths(self)[idx] = lineLen;
        TerminalCodeEditor_lineCapacities(self)[idx] = cap;
        TerminalCodeEditor_setLineCount(self, TerminalCodeEditor_lineCount(self) + 1u);

        if (at >= len)
            break;
        at++;
    }

    TerminalCodeEditor_setCursorLine(self, 0u);
    TerminalCodeEditor_setCursorCol(self, 0u);
    TerminalCodeEditor_setScrollTop(self, 0u);
    TerminalCodeEditor_setScrollLeft(self, 0u);
    return true;
}

bytes TerminalCodeEditor_getLineCount(const TerminalCodeEditor *self) {
    return self != nullptr ? TerminalCodeEditor_lineCount(self) : 0u;
}

const char *TerminalCodeEditor_getLine(const TerminalCodeEditor *self, bytes lineIndex) {
    if (self == nullptr || lineIndex >= TerminalCodeEditor_lineCount(self) || TerminalCodeEditor_lines(self)[lineIndex] == nullptr)
        return "";
    return TerminalCodeEditor_lines(self)[lineIndex];
}

bool TerminalCodeEditor_insertChar(TerminalCodeEditor *self, i32 character) {
    if (self == nullptr || TerminalCodeEditor_lineCount(self) == 0u)
        return false;

    if (character == '\n') {
        bytes cur = TerminalCodeEditor_cursorLine(self);
        bytes col = TerminalCodeEditor_cursorCol(self);
        const char *curLine = TerminalCodeEditor_lines(self)[cur];
        bytes curLen = TerminalCodeEditor_lineLengths(self)[cur];

        // Measure leading whitespace for auto-indent
        bytes indent = 0u;
        while (indent < curLen && indent < col && (curLine[indent] == ' ' || curLine[indent] == '\t'))
            indent++;

        if (!reserveArrayCapacity(self, TerminalCodeEditor_lineCount(self) + 1u))
            return false;

        // Shift rows down
        for (bytes i = TerminalCodeEditor_lineCount(self); i > cur + 1u; --i) {
            TerminalCodeEditor_lines(self)[i] = TerminalCodeEditor_lines(self)[i - 1u];
            TerminalCodeEditor_lineLengths(self)[i] = TerminalCodeEditor_lineLengths(self)[i - 1u];
            TerminalCodeEditor_lineCapacities(self)[i] = TerminalCodeEditor_lineCapacities(self)[i - 1u];
        }

        // Split line: right piece becomes new row
        bytes rightLen = col < curLen ? (curLen - col) : 0u;
        bytes newRowLen = indent + rightLen;
        bytes newCap = newRowLen + 32u;
        char *newBuf = (char*) Memory_alloc(TYPE_BYTE_ARRAY, Determinant_arena(), newCap);
        if (newBuf == nullptr)
            return false;

        if (indent > 0u)
            memcpy(newBuf, curLine, indent);
        if (rightLen > 0u)
            memcpy(newBuf + indent, curLine + col, rightLen);
        newBuf[newRowLen] = '\0';

        // Truncate left piece
        TerminalCodeEditor_lines(self)[cur][col] = '\0';
        TerminalCodeEditor_lineLengths(self)[cur] = col;

        TerminalCodeEditor_lines(self)[cur + 1u] = newBuf;
        TerminalCodeEditor_lineLengths(self)[cur + 1u] = newRowLen;
        TerminalCodeEditor_lineCapacities(self)[cur + 1u] = newCap;
        TerminalCodeEditor_setLineCount(self, TerminalCodeEditor_lineCount(self) + 1u);

        TerminalCodeEditor_setCursorLine(self, TerminalCodeEditor_cursorLine(self) + 1u);
        TerminalCodeEditor_setCursorCol(self, indent);
        return true;
    }

    // Normal character insertion into current line
    bytes cur = TerminalCodeEditor_cursorLine(self);
    bytes col = TerminalCodeEditor_cursorCol(self);
    bytes curLen = TerminalCodeEditor_lineLengths(self)[cur];
    if (col > curLen)
        col = curLen;

    if (!reserveLineCapacity(self, cur, curLen + 2u))
        return false;

    char *buf = TerminalCodeEditor_lines(self)[cur];
    bytes moveCount = curLen - col;
    if (moveCount > 0u)
        memmove(buf + col + 1u, buf + col, moveCount);

    buf[col] = (char) character;
    TerminalCodeEditor_lineLengths(self)[cur]++;
    TerminalCodeEditor_setCursorCol(self, TerminalCodeEditor_cursorCol(self) + 1u);
    buf[TerminalCodeEditor_lineLengths(self)[cur]] = '\0';
    return true;
}

bool TerminalCodeEditor_insertText(TerminalCodeEditor *self, const char *text, bytes length) {
    if (self == nullptr || text == nullptr || length == 0u)
        return false;
    for (bytes i = 0u; i < length; ++i) {
        if (!TerminalCodeEditor_insertChar(self, text[i]))
            return false;
    }
    return true;
}

bool TerminalCodeEditor_backspace(TerminalCodeEditor *self) {
    if (self == nullptr || TerminalCodeEditor_lineCount(self) == 0u)
        return false;

    bytes cur = TerminalCodeEditor_cursorLine(self);
    bytes col = TerminalCodeEditor_cursorCol(self);

    if (col > 0u) {
        char *buf = TerminalCodeEditor_lines(self)[cur];
        bytes len = TerminalCodeEditor_lineLengths(self)[cur];
        bytes moveCount = len - col;
        if (moveCount > 0u)
            memmove(buf + col - 1u, buf + col, moveCount);
        TerminalCodeEditor_lineLengths(self)[cur]--;
        TerminalCodeEditor_setCursorCol(self, TerminalCodeEditor_cursorCol(self) - 1u);
        buf[TerminalCodeEditor_lineLengths(self)[cur]] = '\0';
        return true;
    }

    if (cur > 0u) {
        // Join with previous line
        bytes prev = cur - 1u;
        bytes prevLen = TerminalCodeEditor_lineLengths(self)[prev];
        bytes curLen = TerminalCodeEditor_lineLengths(self)[cur];

        if (!reserveLineCapacity(self, prev, prevLen + curLen + 1u))
            return false;

        memcpy(TerminalCodeEditor_lines(self)[prev] + prevLen, TerminalCodeEditor_lines(self)[cur], curLen);
        TerminalCodeEditor_lineLengths(self)[prev] += curLen;
        TerminalCodeEditor_lines(self)[prev][TerminalCodeEditor_lineLengths(self)[prev]] = '\0';

        Memory_free(TerminalCodeEditor_lines(self)[cur]);
        for (bytes i = cur; i + 1u < TerminalCodeEditor_lineCount(self); ++i) {
            TerminalCodeEditor_lines(self)[i] = TerminalCodeEditor_lines(self)[i + 1u];
            TerminalCodeEditor_lineLengths(self)[i] = TerminalCodeEditor_lineLengths(self)[i + 1u];
            TerminalCodeEditor_lineCapacities(self)[i] = TerminalCodeEditor_lineCapacities(self)[i + 1u];
        }
        TerminalCodeEditor_setLineCount(self, TerminalCodeEditor_lineCount(self) - 1u);

        TerminalCodeEditor_setCursorLine(self, prev);
        TerminalCodeEditor_setCursorCol(self, prevLen);
        return true;
    }
    return false;
}

bool TerminalCodeEditor_delete(TerminalCodeEditor *self) {
    if (self == nullptr || TerminalCodeEditor_lineCount(self) == 0u)
        return false;

    bytes cur = TerminalCodeEditor_cursorLine(self);
    bytes col = TerminalCodeEditor_cursorCol(self);
    bytes len = TerminalCodeEditor_lineLengths(self)[cur];

    if (col < len) {
        char *buf = TerminalCodeEditor_lines(self)[cur];
        bytes moveCount = len - col - 1u;
        if (moveCount > 0u)
            memmove(buf + col, buf + col + 1u, moveCount);
        TerminalCodeEditor_lineLengths(self)[cur]--;
        buf[TerminalCodeEditor_lineLengths(self)[cur]] = '\0';
        return true;
    }

    if (cur + 1u < TerminalCodeEditor_lineCount(self)) {
        bytes next = cur + 1u;
        bytes nextLen = TerminalCodeEditor_lineLengths(self)[next];

        if (!reserveLineCapacity(self, cur, len + nextLen + 1u))
            return false;

        memcpy(TerminalCodeEditor_lines(self)[cur] + len, TerminalCodeEditor_lines(self)[next], nextLen);
        TerminalCodeEditor_lineLengths(self)[cur] += nextLen;
        TerminalCodeEditor_lines(self)[cur][TerminalCodeEditor_lineLengths(self)[cur]] = '\0';

        Memory_free(TerminalCodeEditor_lines(self)[next]);
        for (bytes i = next; i + 1u < TerminalCodeEditor_lineCount(self); ++i) {
            TerminalCodeEditor_lines(self)[i] = TerminalCodeEditor_lines(self)[i + 1u];
            TerminalCodeEditor_lineLengths(self)[i] = TerminalCodeEditor_lineLengths(self)[i + 1u];
            TerminalCodeEditor_lineCapacities(self)[i] = TerminalCodeEditor_lineCapacities(self)[i + 1u];
        }
        TerminalCodeEditor_setLineCount(self, TerminalCodeEditor_lineCount(self) - 1u);
        return true;
    }
    return false;
}

fn TerminalCodeEditor_clear(TerminalCodeEditor *self) {
    if (self == nullptr)
        return;
    TerminalCodeEditor_setText(self, "");
}

fn TerminalCodeEditor_moveCursor(TerminalCodeEditor *self, i32 deltaLine, i32 deltaCol) {
    if (self == nullptr || TerminalCodeEditor_lineCount(self) == 0u)
        return;

    if (deltaLine < 0) {
        bytes d = (bytes) (-deltaLine);
        if (TerminalCodeEditor_cursorLine(self) >= d)
            TerminalCodeEditor_setCursorLine(self, TerminalCodeEditor_cursorLine(self) - (d));
        else
            TerminalCodeEditor_setCursorLine(self, 0u);
    } else if (deltaLine > 0) {
        TerminalCodeEditor_setCursorLine(self, TerminalCodeEditor_cursorLine(self) + ((bytes) deltaLine));
        if (TerminalCodeEditor_cursorLine(self) >= TerminalCodeEditor_lineCount(self))
            TerminalCodeEditor_setCursorLine(self, TerminalCodeEditor_lineCount(self) - 1u);
    }

    bytes curLen = TerminalCodeEditor_lineLengths(self)[TerminalCodeEditor_cursorLine(self)];

    if (deltaCol < 0) {
        bytes d = (bytes) (-deltaCol);
        if (TerminalCodeEditor_cursorCol(self) >= d)
            TerminalCodeEditor_setCursorCol(self, TerminalCodeEditor_cursorCol(self) - (d));
        else
            TerminalCodeEditor_setCursorCol(self, 0u);
    } else if (deltaCol > 0) {
        TerminalCodeEditor_setCursorCol(self, TerminalCodeEditor_cursorCol(self) + ((bytes) deltaCol));
        if (TerminalCodeEditor_cursorCol(self) > curLen)
            TerminalCodeEditor_setCursorCol(self, curLen);
    } else {
        if (TerminalCodeEditor_cursorCol(self) > curLen)
            TerminalCodeEditor_setCursorCol(self, curLen);
    }
}

bytes TerminalCodeEditor_getCursorLine(const TerminalCodeEditor *self) {
    return self != nullptr ? TerminalCodeEditor_cursorLine(self) : 0u;
}

bytes TerminalCodeEditor_getCursorCol(const TerminalCodeEditor *self) {
    return self != nullptr ? TerminalCodeEditor_cursorCol(self) : 0u;
}

bool TerminalCodeEditor_setCursor(TerminalCodeEditor *self, bytes line, bytes col) {
    if (self == nullptr)
        return false;
    if (line >= TerminalCodeEditor_lineCount(self))
        line = TerminalCodeEditor_lineCount(self) > 0u ? TerminalCodeEditor_lineCount(self) - 1u : 0u;
    bytes curLen = TerminalCodeEditor_lineCount(self) > 0u ? TerminalCodeEditor_lineLengths(self)[line] : 0u;
    if (col > curLen)
        col = curLen;
    TerminalCodeEditor_setCursorLine(self, line);
    TerminalCodeEditor_setCursorCol(self, col);
    return true;
}

bool TerminalCodeEditor_setShowGutter(TerminalCodeEditor *self, bool show) {
    if (self == nullptr)
        return false;
    TerminalCodeEditor_setShowGutter(self, show);
    return true;
}

bool TerminalCodeEditor_getShowGutter(const TerminalCodeEditor *self) {
    return self != nullptr && TerminalCodeEditor_showGutter(self);
}

bytes TerminalCodeEditor_getGutterWidth(const TerminalCodeEditor *self) {
    if (self == nullptr || !TerminalCodeEditor_showGutter(self))
        return 0u;
    bytes count = TerminalCodeEditor_lineCount(self);
    bytes digits = 1u;
    while (count >= 10u) {
        digits++;
        count /= 10u;
    }
    return digits + 3u; // line digits + trailing padding
}

fn TerminalCodeEditor_render(const TerminalCodeEditor *self, TerminalCanvas *canvas) {
    if (self == nullptr || canvas == nullptr || !TerminalCanvas_isValid(canvas))
        return;

    bytes width = TerminalCanvas_getWidth(canvas);
    bytes height = TerminalCanvas_getHeight(canvas);
    if (width == 0u || height == 0u)
        return;

    bytes gutterW = TerminalCodeEditor_getGutterWidth(self);
    bytes codeW = width > gutterW ? (width - gutterW) : 0u;

    for (bytes y = 0u; y < height; ++y) {
        bytes lineIdx = TerminalCodeEditor_scrollTop(self) + y;
        if (lineIdx >= TerminalCodeEditor_lineCount(self))
            break;

        // Render gutter
        if (TerminalCodeEditor_showGutter(self) && gutterW > 0u) {
            char numBuf[16];
            i32 written = snprintf(numBuf, sizeof(numBuf), " %*zu ", (i32) (gutterW - 2u), lineIdx + 1u);
            if (written > 0 && (bytes) written <= gutterW)
                TerminalCanvas_text(canvas, 0u, y, numBuf, (bytes) written, TerminalCodeEditor_gutterPen(self));
        }

        // Render code text
        if (codeW > 0u) {
            const char *text = TerminalCodeEditor_lines(self)[lineIdx];
            bytes len = TerminalCodeEditor_lineLengths(self)[lineIdx];
            if (len > TerminalCodeEditor_scrollLeft(self)) {
                bytes showLen = len - TerminalCodeEditor_scrollLeft(self);
                if (showLen > codeW)
                    showLen = codeW;
                TerminalCanvas_text(canvas, gutterW, y, text + TerminalCodeEditor_scrollLeft(self), showLen, TerminalCodeEditor_textPen(self));
            }
        }
    }

    // Render active cursor
    if (TerminalCodeEditor_cursorLine(self) >= TerminalCodeEditor_scrollTop(self) && (TerminalCodeEditor_cursorLine(self) - TerminalCodeEditor_scrollTop(self)) < height) {
        bytes cy = TerminalCodeEditor_cursorLine(self) - TerminalCodeEditor_scrollTop(self);
        if (TerminalCodeEditor_cursorCol(self) >= TerminalCodeEditor_scrollLeft(self) && (TerminalCodeEditor_cursorCol(self) - TerminalCodeEditor_scrollLeft(self)) < codeW) {
            bytes cx = gutterW + (TerminalCodeEditor_cursorCol(self) - TerminalCodeEditor_scrollLeft(self));
            char curChar = ' ';
            const char *line = TerminalCodeEditor_lines(self)[TerminalCodeEditor_cursorLine(self)];
            if (TerminalCodeEditor_cursorCol(self) < TerminalCodeEditor_lineLengths(self)[TerminalCodeEditor_cursorLine(self)])
                curChar = line[TerminalCodeEditor_cursorCol(self)];
            TerminalCanvas_text(canvas, cx, cy, &curChar, 1u, TerminalCodeEditor_cursorPen(self));
        }
    }
}

TerminalProjection TerminalCodeEditor_toString(const TerminalCodeEditor *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format("TerminalCodeEditor(lines=%zu,cursor={%zu,%zu},gutter=%d)",
        TerminalCodeEditor_lineCount(self), TerminalCodeEditor_cursorLine(self), TerminalCodeEditor_cursorCol(self), TerminalCodeEditor_showGutter(self));
}

TerminalProjection TerminalCodeEditor_toStringStruct(const TerminalCodeEditor *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format(
        "TerminalCodeEditor { lines=%p lineCount=%zu lineCap=%zu cursorLine=%zu cursorCol=%zu scrollTop=%zu scrollLeft=%zu showGutter=%d }",
        (const_any) TerminalCodeEditor_lines(self), TerminalCodeEditor_lineCount(self), TerminalCodeEditor_lineCapacity(self),
        TerminalCodeEditor_cursorLine(self), TerminalCodeEditor_cursorCol(self), TerminalCodeEditor_scrollTop(self), TerminalCodeEditor_scrollLeft(self),
        TerminalCodeEditor_showGutter(self));
}
