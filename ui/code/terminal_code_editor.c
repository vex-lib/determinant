#include "ui/code/terminal_code_editor.h"
#include "nio/mem.h"
#include "oop/type.h"
#include "exception/throw.h"
#include "annotation/annotation.h"
#include <string.h>
#include <stdio.h>

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
 * size_t *lineLengths, *lineCapacities    per-line metrics
 * size_t lineCount, lineCapacity          line array geometry
 * size_t cursorLine, cursorCol            2D caret coordinates
 * size_t scrollTop, scrollLeft            viewport offsets
 * bool showGutter                         gutter enabled flag
 * TerminalPen gutterPen, textPen, cursorPen  styling pens
 * ======================================================================== */

#define TERMINAL_CODE_EDITOR_INIT_CAPACITY 16u
#define TERMINAL_CODE_EDITOR_LINE_INIT_CAP 64u

static bool reserveLineCapacity(TerminalCodeEditor *self, size_t lineIndex, size_t needed) {
    if (self == nullptr || lineIndex >= (*self).lineCount)
        return false;
    if ((*self).lineCapacities[lineIndex] >= needed)
        return true;
    size_t newCap = (*self).lineCapacities[lineIndex] * 2u;
    if (newCap < needed)
        newCap = needed + 32u;
    char *newBuf = (char*) Memory_alloc(TYPE_BYTE_ARRAY, newCap);
    if (newBuf == nullptr)
        return false;
    if ((*self).lines[lineIndex] != nullptr) {
        memcpy(newBuf, (*self).lines[lineIndex], (*self).lineLengths[lineIndex] + 1u);
        Memory_free((*self).lines[lineIndex]);
    } else {
        newBuf[0] = '\0';
    }
    (*self).lines[lineIndex] = newBuf;
    (*self).lineCapacities[lineIndex] = newCap;
    return true;
}

static bool reserveArrayCapacity(TerminalCodeEditor *self, size_t needed) {
    if (self == nullptr)
        return false;
    if ((*self).lineCapacity >= needed)
        return true;
    size_t newCap = (*self).lineCapacity * 2u;
    if (newCap < needed)
        newCap = needed + 8u;

    char **newLines = (char**) Memory_alloc(TYPE_BYTE_ARRAY, newCap * sizeof(char*));
    size_t *newLengths = (size_t*) Memory_alloc(TYPE_BYTE_ARRAY, newCap * sizeof(size_t));
    size_t *newCaps = (size_t*) Memory_alloc(TYPE_BYTE_ARRAY, newCap * sizeof(size_t));

    if (newLines == nullptr || newLengths == nullptr || newCaps == nullptr) {
        if (newLines != nullptr) Memory_free(newLines);
        if (newLengths != nullptr) Memory_free(newLengths);
        if (newCaps != nullptr) Memory_free(newCaps);
        return false;
    }

    if ((*self).lines != nullptr) {
        memcpy(newLines, (*self).lines, (*self).lineCount * sizeof(char*));
        memcpy(newLengths, (*self).lineLengths, (*self).lineCount * sizeof(size_t));
        memcpy(newCaps, (*self).lineCapacities, (*self).lineCount * sizeof(size_t));
        Memory_free((*self).lines);
        Memory_free((*self).lineLengths);
        Memory_free((*self).lineCapacities);
    }

    (*self).lines = newLines;
    (*self).lineLengths = newLengths;
    (*self).lineCapacities = newCaps;
    (*self).lineCapacity = newCap;
    return true;
}

static void freeAllLines(TerminalCodeEditor *self) {
    if (self == nullptr || (*self).lines == nullptr)
        return;
    for (size_t i = 0u; i < (*self).lineCount; ++i) {
        if ((*self).lines[i] != nullptr)
            Memory_free((*self).lines[i]);
    }
    Memory_free((*self).lines);
    Memory_free((*self).lineLengths);
    Memory_free((*self).lineCapacities);
    (*self).lines = nullptr;
    (*self).lineLengths = nullptr;
    (*self).lineCapacities = nullptr;
    (*self).lineCount = 0u;
    (*self).lineCapacity = 0u;
}

TerminalCodeEditor *TerminalCodeEditor_0(void) {
    return TerminalCodeEditor_1(nullptr);
}

TerminalCodeEditor *TerminalCodeEditor_1(const char *initialText) {
    TerminalCodeEditor *self = (TerminalCodeEditor*) Memory_alloc(TYPE_BYTE_ARRAY, sizeof(TerminalCodeEditor));
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
        (*self).lineCount = 1u;
        (*self).lines[0] = (char*) Memory_alloc(TYPE_BYTE_ARRAY, TERMINAL_CODE_EDITOR_LINE_INIT_CAP);
        (*self).lines[0][0] = '\0';
        (*self).lineLengths[0] = 0u;
        (*self).lineCapacities[0] = TERMINAL_CODE_EDITOR_LINE_INIT_CAP;
    }
    return self;
}

void TerminalCodeEditor_free(TerminalCodeEditor *self) {
    if (self == nullptr)
        return;
    freeAllLines(self);
    Memory_free(self);
}

bool TerminalCodeEditor_setText(TerminalCodeEditor *self, const char *text) {
    if (self == nullptr || text == nullptr)
        return false;
    freeAllLines(self);

    size_t len = strlen(text);
    size_t estimatedLines = 1u;
    for (size_t i = 0u; i < len; ++i) {
        if (text[i] == '\n')
            estimatedLines++;
    }

    if (!reserveArrayCapacity(self, estimatedLines + 4u))
        return false;

    size_t at = 0u;
    while (at <= len) {
        size_t lineStart = at;
        while (at < len && text[at] != '\n')
            at++;
        size_t lineLen = at - lineStart;

        size_t idx = (*self).lineCount;
        size_t cap = lineLen + 16u;
        char *buf = (char*) Memory_alloc(TYPE_BYTE_ARRAY, cap);
        if (buf == nullptr)
            return false;
        memcpy(buf, text + lineStart, lineLen);
        buf[lineLen] = '\0';

        (*self).lines[idx] = buf;
        (*self).lineLengths[idx] = lineLen;
        (*self).lineCapacities[idx] = cap;
        (*self).lineCount++;

        if (at >= len)
            break;
        at++;
    }

    (*self).cursorLine = 0u;
    (*self).cursorCol = 0u;
    (*self).scrollTop = 0u;
    (*self).scrollLeft = 0u;
    return true;
}

size_t TerminalCodeEditor_getLineCount(const TerminalCodeEditor *self) {
    return self != nullptr ? (*self).lineCount : 0u;
}

const char *TerminalCodeEditor_getLine(const TerminalCodeEditor *self, size_t lineIndex) {
    if (self == nullptr || lineIndex >= (*self).lineCount || (*self).lines[lineIndex] == nullptr)
        return "";
    return (*self).lines[lineIndex];
}

bool TerminalCodeEditor_insertChar(TerminalCodeEditor *self, int character) {
    if (self == nullptr || (*self).lineCount == 0u)
        return false;

    if (character == '\n') {
        size_t cur = (*self).cursorLine;
        size_t col = (*self).cursorCol;
        const char *curLine = (*self).lines[cur];
        size_t curLen = (*self).lineLengths[cur];

        // Measure leading whitespace for auto-indent
        size_t indent = 0u;
        while (indent < curLen && indent < col && (curLine[indent] == ' ' || curLine[indent] == '\t'))
            indent++;

        if (!reserveArrayCapacity(self, (*self).lineCount + 1u))
            return false;

        // Shift rows down
        for (size_t i = (*self).lineCount; i > cur + 1u; --i) {
            (*self).lines[i] = (*self).lines[i - 1u];
            (*self).lineLengths[i] = (*self).lineLengths[i - 1u];
            (*self).lineCapacities[i] = (*self).lineCapacities[i - 1u];
        }

        // Split line: right piece becomes new row
        size_t rightLen = col < curLen ? (curLen - col) : 0u;
        size_t newRowLen = indent + rightLen;
        size_t newCap = newRowLen + 32u;
        char *newBuf = (char*) Memory_alloc(TYPE_BYTE_ARRAY, newCap);
        if (newBuf == nullptr)
            return false;

        if (indent > 0u)
            memcpy(newBuf, curLine, indent);
        if (rightLen > 0u)
            memcpy(newBuf + indent, curLine + col, rightLen);
        newBuf[newRowLen] = '\0';

        // Truncate left piece
        (*self).lines[cur][col] = '\0';
        (*self).lineLengths[cur] = col;

        (*self).lines[cur + 1u] = newBuf;
        (*self).lineLengths[cur + 1u] = newRowLen;
        (*self).lineCapacities[cur + 1u] = newCap;
        (*self).lineCount++;

        (*self).cursorLine++;
        (*self).cursorCol = indent;
        return true;
    }

    // Normal character insertion into current line
    size_t cur = (*self).cursorLine;
    size_t col = (*self).cursorCol;
    size_t curLen = (*self).lineLengths[cur];
    if (col > curLen)
        col = curLen;

    if (!reserveLineCapacity(self, cur, curLen + 2u))
        return false;

    char *buf = (*self).lines[cur];
    size_t moveCount = curLen - col;
    if (moveCount > 0u)
        memmove(buf + col + 1u, buf + col, moveCount);

    buf[col] = (char) character;
    (*self).lineLengths[cur]++;
    (*self).cursorCol++;
    buf[(*self).lineLengths[cur]] = '\0';
    return true;
}

bool TerminalCodeEditor_insertText(TerminalCodeEditor *self, const char *text, size_t length) {
    if (self == nullptr || text == nullptr || length == 0u)
        return false;
    for (size_t i = 0u; i < length; ++i) {
        if (!TerminalCodeEditor_insertChar(self, text[i]))
            return false;
    }
    return true;
}

bool TerminalCodeEditor_backspace(TerminalCodeEditor *self) {
    if (self == nullptr || (*self).lineCount == 0u)
        return false;

    size_t cur = (*self).cursorLine;
    size_t col = (*self).cursorCol;

    if (col > 0u) {
        char *buf = (*self).lines[cur];
        size_t len = (*self).lineLengths[cur];
        size_t moveCount = len - col;
        if (moveCount > 0u)
            memmove(buf + col - 1u, buf + col, moveCount);
        (*self).lineLengths[cur]--;
        (*self).cursorCol--;
        buf[(*self).lineLengths[cur]] = '\0';
        return true;
    }

    if (cur > 0u) {
        // Join with previous line
        size_t prev = cur - 1u;
        size_t prevLen = (*self).lineLengths[prev];
        size_t curLen = (*self).lineLengths[cur];

        if (!reserveLineCapacity(self, prev, prevLen + curLen + 1u))
            return false;

        memcpy((*self).lines[prev] + prevLen, (*self).lines[cur], curLen);
        (*self).lineLengths[prev] += curLen;
        (*self).lines[prev][(*self).lineLengths[prev]] = '\0';

        Memory_free((*self).lines[cur]);
        for (size_t i = cur; i + 1u < (*self).lineCount; ++i) {
            (*self).lines[i] = (*self).lines[i + 1u];
            (*self).lineLengths[i] = (*self).lineLengths[i + 1u];
            (*self).lineCapacities[i] = (*self).lineCapacities[i + 1u];
        }
        (*self).lineCount--;

        (*self).cursorLine = prev;
        (*self).cursorCol = prevLen;
        return true;
    }
    return false;
}

bool TerminalCodeEditor_delete(TerminalCodeEditor *self) {
    if (self == nullptr || (*self).lineCount == 0u)
        return false;

    size_t cur = (*self).cursorLine;
    size_t col = (*self).cursorCol;
    size_t len = (*self).lineLengths[cur];

    if (col < len) {
        char *buf = (*self).lines[cur];
        size_t moveCount = len - col - 1u;
        if (moveCount > 0u)
            memmove(buf + col, buf + col + 1u, moveCount);
        (*self).lineLengths[cur]--;
        buf[(*self).lineLengths[cur]] = '\0';
        return true;
    }

    if (cur + 1u < (*self).lineCount) {
        size_t next = cur + 1u;
        size_t nextLen = (*self).lineLengths[next];

        if (!reserveLineCapacity(self, cur, len + nextLen + 1u))
            return false;

        memcpy((*self).lines[cur] + len, (*self).lines[next], nextLen);
        (*self).lineLengths[cur] += nextLen;
        (*self).lines[cur][(*self).lineLengths[cur]] = '\0';

        Memory_free((*self).lines[next]);
        for (size_t i = next; i + 1u < (*self).lineCount; ++i) {
            (*self).lines[i] = (*self).lines[i + 1u];
            (*self).lineLengths[i] = (*self).lineLengths[i + 1u];
            (*self).lineCapacities[i] = (*self).lineCapacities[i + 1u];
        }
        (*self).lineCount--;
        return true;
    }
    return false;
}

void TerminalCodeEditor_clear(TerminalCodeEditor *self) {
    if (self == nullptr)
        return;
    TerminalCodeEditor_setText(self, "");
}

void TerminalCodeEditor_moveCursor(TerminalCodeEditor *self, int deltaLine, int deltaCol) {
    if (self == nullptr || (*self).lineCount == 0u)
        return;

    if (deltaLine < 0) {
        size_t d = (size_t) (-deltaLine);
        if ((*self).cursorLine >= d)
            (*self).cursorLine -= d;
        else
            (*self).cursorLine = 0u;
    } else if (deltaLine > 0) {
        (*self).cursorLine += (size_t) deltaLine;
        if ((*self).cursorLine >= (*self).lineCount)
            (*self).cursorLine = (*self).lineCount - 1u;
    }

    size_t curLen = (*self).lineLengths[(*self).cursorLine];

    if (deltaCol < 0) {
        size_t d = (size_t) (-deltaCol);
        if ((*self).cursorCol >= d)
            (*self).cursorCol -= d;
        else
            (*self).cursorCol = 0u;
    } else if (deltaCol > 0) {
        (*self).cursorCol += (size_t) deltaCol;
        if ((*self).cursorCol > curLen)
            (*self).cursorCol = curLen;
    } else {
        if ((*self).cursorCol > curLen)
            (*self).cursorCol = curLen;
    }
}

size_t TerminalCodeEditor_getCursorLine(const TerminalCodeEditor *self) {
    return self != nullptr ? (*self).cursorLine : 0u;
}

size_t TerminalCodeEditor_getCursorCol(const TerminalCodeEditor *self) {
    return self != nullptr ? (*self).cursorCol : 0u;
}

bool TerminalCodeEditor_setCursor(TerminalCodeEditor *self, size_t line, size_t col) {
    if (self == nullptr)
        return false;
    if (line >= (*self).lineCount)
        line = (*self).lineCount > 0u ? (*self).lineCount - 1u : 0u;
    size_t curLen = (*self).lineCount > 0u ? (*self).lineLengths[line] : 0u;
    if (col > curLen)
        col = curLen;
    (*self).cursorLine = line;
    (*self).cursorCol = col;
    return true;
}

bool TerminalCodeEditor_setShowGutter(TerminalCodeEditor *self, bool show) {
    if (self == nullptr)
        return false;
    (*self).showGutter = show;
    return true;
}

bool TerminalCodeEditor_getShowGutter(const TerminalCodeEditor *self) {
    return self != nullptr && (*self).showGutter;
}

size_t TerminalCodeEditor_getGutterWidth(const TerminalCodeEditor *self) {
    if (self == nullptr || !(*self).showGutter)
        return 0u;
    size_t count = (*self).lineCount;
    size_t digits = 1u;
    while (count >= 10u) {
        digits++;
        count /= 10u;
    }
    return digits + 3u; // line digits + trailing padding
}

void TerminalCodeEditor_render(const TerminalCodeEditor *self, TerminalCanvas *canvas) {
    if (self == nullptr || canvas == nullptr || !TerminalCanvas_isValid(canvas))
        return;

    size_t width = TerminalCanvas_getWidth(canvas);
    size_t height = TerminalCanvas_getHeight(canvas);
    if (width == 0u || height == 0u)
        return;

    size_t gutterW = TerminalCodeEditor_getGutterWidth(self);
    size_t codeW = width > gutterW ? (width - gutterW) : 0u;

    for (size_t y = 0u; y < height; ++y) {
        size_t lineIdx = (*self).scrollTop + y;
        if (lineIdx >= (*self).lineCount)
            break;

        // Render gutter
        if ((*self).showGutter && gutterW > 0u) {
            char numBuf[16];
            int written = snprintf(numBuf, sizeof(numBuf), " %*zu ", (int) (gutterW - 2u), lineIdx + 1u);
            if (written > 0 && (size_t) written <= gutterW)
                TerminalCanvas_text(canvas, 0u, y, numBuf, (size_t) written, (*self).gutterPen);
        }

        // Render code text
        if (codeW > 0u) {
            const char *text = (*self).lines[lineIdx];
            size_t len = (*self).lineLengths[lineIdx];
            if (len > (*self).scrollLeft) {
                size_t showLen = len - (*self).scrollLeft;
                if (showLen > codeW)
                    showLen = codeW;
                TerminalCanvas_text(canvas, gutterW, y, text + (*self).scrollLeft, showLen, (*self).textPen);
            }
        }
    }

    // Render active cursor
    if ((*self).cursorLine >= (*self).scrollTop && ((*self).cursorLine - (*self).scrollTop) < height) {
        size_t cy = (*self).cursorLine - (*self).scrollTop;
        if ((*self).cursorCol >= (*self).scrollLeft && ((*self).cursorCol - (*self).scrollLeft) < codeW) {
            size_t cx = gutterW + ((*self).cursorCol - (*self).scrollLeft);
            char curChar = ' ';
            const char *line = (*self).lines[(*self).cursorLine];
            if ((*self).cursorCol < (*self).lineLengths[(*self).cursorLine])
                curChar = line[(*self).cursorCol];
            TerminalCanvas_text(canvas, cx, cy, &curChar, 1u, (*self).cursorPen);
        }
    }
}

TerminalProjection TerminalCodeEditor_toString(const TerminalCodeEditor *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format("TerminalCodeEditor(lines=%zu,cursor={%zu,%zu},gutter=%d)",
        (*self).lineCount, (*self).cursorLine, (*self).cursorCol, (*self).showGutter);
}

TerminalProjection TerminalCodeEditor_toStringStruct(const TerminalCodeEditor *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format(
        "TerminalCodeEditor { lines=%p lineCount=%zu lineCap=%zu cursorLine=%zu cursorCol=%zu scrollTop=%zu scrollLeft=%zu showGutter=%d }",
        (const void*) (*self).lines, (*self).lineCount, (*self).lineCapacity,
        (*self).cursorLine, (*self).cursorCol, (*self).scrollTop, (*self).scrollLeft,
        (*self).showGutter);
}
