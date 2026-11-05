#include "ui/compositor/terminal_compositor.h"
#include "ui/surface/terminal_canvas.h"
#include "nio/mem.h"
#include "oop/type.h"
#include "exception/throw.h"
#include "annotation/annotation.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>

;;DEFINITION
/** The terminal output compositor. Consumes TerminalSurface delta runs, translates
 * them into optimized ANSI/VT100 escape sequences, and performs synchronized, bounded
 * buffer flushes to the host descriptor.
 *
 * It manages the alternate screen state, cursor addressing, style and truecolor/256 SGR
 * parameters, and source-over alpha blending against the resolved device background.
 * By fulfilling TerminalRunFn and TerminalPresentPhaseFn, it decouples the application
 * from terminal escape generation entirely. */
;;OVERVIEW
/** ========================================================================
 * CLASS: TerminalCompositor (ui/compositor/terminal_compositor.c)
 * STRUCT FIELDS (Mirroring ui/compositor/terminal_compositor.h)
 *
 * int fd                                  target file descriptor (default STDOUT_FILENO)
 * TerminalCompositorColorMode colorMode   TRUECOLOR, 256 or BASIC
 * uint32_t deviceBg                       backdrop color for alpha blending
 * size_t presentWidth                     boundary for wide-glyph clipping
 * size_t cursorColumn, cursorRow          tracked device cursor position
 * char pen[64], size_t penLength          cached SGR parameter string
 * bool penValid                           whether pen cache is live
 * bool inAlternate                        alternate screen active
 * bool inTransaction                      synchronized frame transaction active
 * bool outFailed                          write error latch
 * size_t outLength                        buffered byte count
 * char out[16384]                         output buffer
 * ======================================================================== */

static size_t utf8Encode(uint32_t glyph, char *bytes) {
    if (glyph < 0x80u) {
        bytes[0] = (char) glyph;
        return 1u;
    }
    if (glyph < 0x800u) {
        bytes[0] = (char) (0xC0u | (glyph >> 6));
        bytes[1] = (char) (0x80u | (glyph & 0x3Fu));
        return 2u;
    }
    if (glyph < 0x10000u) {
        bytes[0] = (char) (0xE0u | (glyph >> 12));
        bytes[1] = (char) (0x80u | ((glyph >> 6) & 0x3Fu));
        bytes[2] = (char) (0x80u | (glyph & 0x3Fu));
        return 3u;
    }
    bytes[0] = (char) (0xF0u | (glyph >> 18));
    bytes[1] = (char) (0x80u | ((glyph >> 12) & 0x3Fu));
    bytes[2] = (char) (0x80u | ((glyph >> 6) & 0x3Fu));
    bytes[3] = (char) (0x80u | (glyph & 0x3Fu));
    return 4u;
}

static short colorIndex256(uint32_t color) {
    if (!TERMINAL_COLOR_IS_SET(color))
        return -1;
    int r = TERMINAL_COLOR_RED(color);
    int g = TERMINAL_COLOR_GREEN(color);
    int b = TERMINAL_COLOR_BLUE(color);
    int cr = (r * 5 + 127) / 255;
    int cg = (g * 5 + 127) / 255;
    int cb = (b * 5 + 127) / 255;
    return (short) (16 + 36 * cr + 6 * cg + cb);
}

static void compositorWriteRaw(TerminalCompositor *self, const char *bytes, size_t length) {
    if (self == nullptr || length == 0u || (*self).fd < 0)
        return;
    size_t written = 0u;
    while (written < length) {
        ssize_t result = write((*self).fd, bytes + written, length - written);
        if (result <= 0) {
            (*self).outFailed = true;
            return;
        }
        written += (size_t) result;
    }
}

static void compositorAppend(TerminalCompositor *self, const char *bytes, size_t length) {
    if (self == nullptr || bytes == nullptr || length == 0u)
        return;
    while (length != 0u) {
        size_t room = sizeof((*self).out) - (*self).outLength;
        if (room == 0u) {
            TerminalCompositor_flush(self);
            room = sizeof((*self).out);
        }
        size_t count = length < room ? length : room;
        memcpy((*self).out + (*self).outLength, bytes, count);
        (*self).outLength += count;
        bytes += count;
        length -= count;
    }
}

static void compositorText(TerminalCompositor *self, const char *text) {
    if (text == nullptr)
        return;
    compositorAppend(self, text, strlen(text));
}

static void compositorByte(TerminalCompositor *self, char byte) {
    compositorAppend(self, &byte, 1u);
}

static size_t formatPenColor(TerminalCompositor *self, char *params, size_t capacity,
                             size_t at, bool foreground, uint32_t color) {
    if (!TERMINAL_COLOR_IS_SET(color))
        return at;
    int prefix = foreground ? 3 : 4;
    int written = 0;
    if ((*self).colorMode == TERMINAL_COMPOSITOR_COLOR_TRUECOLOR) {
        written = snprintf(params + at, capacity - at, ";%d8;2;%u;%u;%u",
                           prefix,
                           (unsigned) TERMINAL_COLOR_RED(color),
                           (unsigned) TERMINAL_COLOR_GREEN(color),
                           (unsigned) TERMINAL_COLOR_BLUE(color));
    } else if ((*self).colorMode == TERMINAL_COMPOSITOR_COLOR_256) {
        short index = colorIndex256(color);
        if (index >= 0)
            written = snprintf(params + at, capacity - at, ";%d8;5;%d", prefix, (int) index);
    } else {
        short index = colorIndex256(color);
        int basic = index >= 0 ? (index % 8) : 7;
        written = snprintf(params + at, capacity - at, ";%d", (foreground ? 30 : 40) + basic);
    }
    if (written <= 0 || (size_t) written >= capacity - at)
        return at;
    return at + (size_t) written;
}

static size_t formatPenParams(TerminalCompositor *self, char *params, size_t capacity,
                              uint16_t style, uint32_t fg, uint32_t bg) {
    int written = snprintf(params, capacity, "0%s%s%s%s%s",
                           (style & TERMINAL_STYLE_BOLD) ? ";1" : "",
                           (style & (TERMINAL_STYLE_DIM | TERMINAL_STYLE_MUTED)) ? ";2" : "",
                           (style & TERMINAL_STYLE_ITALIC) ? ";3" : "",
                           (style & TERMINAL_STYLE_UNDERLINE) ? ";4" : "",
                           (style & TERMINAL_STYLE_REVERSE) ? ";7" : "");
    if (written <= 0)
        return 0u;
    size_t at = (size_t) written < capacity ? (size_t) written : capacity;
    at = formatPenColor(self, params, capacity, at, true, fg);
    at = formatPenColor(self, params, capacity, at, false, bg);
    return at;
}

static void compositorPen(TerminalCompositor *self, uint16_t style, uint32_t fg, uint32_t bg) {
    char params[TERMINAL_COMPOSITOR_PEN_CAPACITY];
    size_t length = formatPenParams(self, params, sizeof(params), style, fg, bg);
    if ((*self).penValid && length == (*self).penLength
        && memcmp((*self).pen, params, length) == 0)
        return;
    memcpy((*self).pen, params, length);
    (*self).penLength = length;
    (*self).penValid = true;
    compositorText(self, "\033[");
    compositorAppend(self, params, length);
    compositorByte(self, 'm');
}

static void compositorMove(TerminalCompositor *self, size_t column, size_t row) {
    if ((*self).cursorColumn == column && (*self).cursorRow == row)
        return;
    char sequence[32];
    int written = snprintf(sequence, sizeof(sequence), "\033[%zu;%zuH", row + 1u, column + 1u);
    if (written > 0)
        compositorAppend(self, sequence, (size_t) written);
    (*self).cursorColumn = column;
    (*self).cursorRow = row;
}

TerminalCompositor *TerminalCompositor_0(void) {
    return TerminalCompositor_1(STDOUT_FILENO);
}

TerminalCompositor *TerminalCompositor_1(int fd) {
    TerminalCompositor *self = (TerminalCompositor*) Memory_alloc(TYPE_BYTE_ARRAY, sizeof(TerminalCompositor));
    if (self == nullptr)
        return nullptr;
    *self = (TerminalCompositor) {
        .fd = fd,
        .colorMode = TERMINAL_COMPOSITOR_COLOR_TRUECOLOR,
        .deviceBg = TERMINAL_COLOR_DEFAULT,
        .presentWidth = 0u,
        .cursorColumn = (size_t) -1,
        .cursorRow = (size_t) -1,
        .penLength = 0u,
        .penValid = false,
        .inAlternate = false,
        .inTransaction = false,
        .outFailed = false,
        .outLength = 0u
    };
    return self;
}

void TerminalCompositor_free(TerminalCompositor *self) {
    if (self == nullptr)
        return;
    if ((*self).inAlternate)
        TerminalCompositor_leave(self);
    else
        TerminalCompositor_flush(self);
    Memory_free(self);
}

bool TerminalCompositor_enter(TerminalCompositor *self) {
    if (self == nullptr)
        return false;
    TerminalCompositor_forget(self);
    (*self).outLength = 0u;
    (*self).outFailed = false;
    // Enter alternate screen, disable autowrap, hide cursor, clear screen, reset margin
    compositorText(self, "\033[?1049h\033[?7l\033[?25l\033[H\033[2J\033[r");
    TerminalCompositor_flush(self);
    (*self).inAlternate = true;
    return !(*self).outFailed;
}

bool TerminalCompositor_leave(TerminalCompositor *self) {
    if (self == nullptr)
        return false;
    // Reset pen, enable autowrap, show cursor, exit alternate screen
    compositorText(self, "\033[0m\033[?7h\033[?25h\033[?1049l");
    TerminalCompositor_flush(self);
    TerminalCompositor_forget(self);
    (*self).inAlternate = false;
    return !(*self).outFailed;
}

void TerminalCompositor_flush(TerminalCompositor *self) {
    if (self == nullptr || (*self).outLength == 0u)
        return;
    compositorWriteRaw(self, (*self).out, (*self).outLength);
    (*self).outLength = 0u;
}

void TerminalCompositor_forget(TerminalCompositor *self) {
    if (self == nullptr)
        return;
    (*self).cursorColumn = (size_t) -1;
    (*self).cursorRow = (size_t) -1;
    (*self).penLength = 0u;
    (*self).penValid = false;
}

void TerminalCompositor_clear(TerminalCompositor *self) {
    if (self == nullptr)
        return;
    TerminalCompositor_forget(self);
    compositorText(self, "\033[H\033[2J");
    TerminalCompositor_flush(self);
}

bool TerminalCompositor_blitRun(void *context, TerminalRun run) {
    TerminalCompositor *self = (TerminalCompositor*) context;
    if (self == nullptr)
        return false;
    for (size_t i = 0u; i < run.length; ++i) {
        uint32_t glyph = run.cells != nullptr ? run.cells[i] : ' ';
        if (glyph == 0u) {
            if ((*self).cursorRow != run.y || (*self).cursorColumn != run.x + i + 1u)
                (*self).cursorColumn = (size_t) -1;
            continue;
        }
        if (glyph > 0x10FFFFu || (glyph >= 0xD800u && glyph <= 0xDFFFu))
            glyph = '?';
        uint16_t style = run.styles != nullptr ? run.styles[i] : 0u;
        uint32_t fg = run.fg != nullptr ? run.fg[i] : TERMINAL_COLOR_DEFAULT;
        uint32_t bg = run.bg != nullptr ? run.bg[i] : TERMINAL_COLOR_DEFAULT;

        uint32_t cellBg = TERMINAL_COLOR_IS_SET(bg)
                              ? TerminalColor_over(bg, (*self).deviceBg)
                              : TERMINAL_COLOR_DEFAULT;
        uint32_t backdrop = TERMINAL_COLOR_IS_SET(cellBg) ? cellBg : (*self).deviceBg;
        uint32_t cellFg = TERMINAL_COLOR_IS_SET(fg)
                              ? TerminalColor_over(fg, backdrop)
                              : TERMINAL_COLOR_DEFAULT;

        size_t column = run.x + i;
        size_t width = TerminalCanvas_widthOf(glyph);
        if (width == 0u)
            width = 1u;
        if ((*self).presentWidth != 0u && column + width > (*self).presentWidth) {
            glyph = ' ';
            width = 1u;
        }

        compositorMove(self, column, run.y);
        compositorPen(self, style, cellFg, cellBg);
        char bytes[4];
        size_t length = utf8Encode(glyph, bytes);
        compositorAppend(self, bytes, length);
        (*self).cursorColumn = column + width;
        (*self).cursorRow = run.y;
    }
    return !(*self).outFailed;
}

void TerminalCompositor_phase(void *context, TerminalPresentPhase phase) {
    TerminalCompositor *self = (TerminalCompositor*) context;
    if (self == nullptr)
        return;
    if (phase == TERMINAL_PRESENT_BEGIN) {
        compositorText(self, "\033[?2026h");
        (*self).inTransaction = true;
    } else if (phase == TERMINAL_PRESENT_END) {
        if ((*self).inTransaction) {
            compositorText(self, "\033[?2026l");
            (*self).inTransaction = false;
        }
        TerminalCompositor_flush(self);
    }
}

bool TerminalCompositor_setFd(TerminalCompositor *self, int fd) {
    if (self == nullptr || fd < 0) {
        THROW("TerminalCompositor: invalid fd");
        return false;
    }
    (*self).fd = fd;
    return true;
}

int TerminalCompositor_getFd(const TerminalCompositor *self) {
    return self != nullptr ? (*self).fd : -1;
}

bool TerminalCompositor_setDeviceBackground(TerminalCompositor *self, uint32_t bg) {
    if (self == nullptr)
        return false;
    (*self).deviceBg = bg;
    return true;
}

uint32_t TerminalCompositor_getDeviceBackground(const TerminalCompositor *self) {
    return self != nullptr ? (*self).deviceBg : TERMINAL_COLOR_DEFAULT;
}

bool TerminalCompositor_setPresentWidth(TerminalCompositor *self, size_t width) {
    if (self == nullptr)
        return false;
    (*self).presentWidth = width;
    return true;
}

size_t TerminalCompositor_getPresentWidth(const TerminalCompositor *self) {
    return self != nullptr ? (*self).presentWidth : 0u;
}

bool TerminalCompositor_setColorMode(TerminalCompositor *self, TerminalCompositorColorMode mode) {
    if (self == nullptr)
        return false;
    (*self).colorMode = mode;
    return true;
}

TerminalCompositorColorMode TerminalCompositor_getColorMode(const TerminalCompositor *self) {
    return self != nullptr ? (*self).colorMode : TERMINAL_COMPOSITOR_COLOR_TRUECOLOR;
}

bool TerminalCompositor_getFailed(const TerminalCompositor *self) {
    return self != nullptr && (*self).outFailed;
}

size_t TerminalCompositor_getBufferedLength(const TerminalCompositor *self) {
    return self != nullptr ? (*self).outLength : 0u;
}

TerminalProjection TerminalCompositor_toString(const TerminalCompositor *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format("TerminalCompositor{fd=%d,colorMode=%d,inAlternate=%d,inTransaction=%d}",
        (*self).fd, (*self).colorMode, (*self).inAlternate, (*self).inTransaction);
}

TerminalProjection TerminalCompositor_toStringStruct(const TerminalCompositor *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format(
        "TerminalCompositor{fd=%d,colorMode=%d,deviceBg=0x%08X,presentWidth=%zu,cursorCol=%zu,cursorRow=%zu,inAlternate=%d,inTransaction=%d,outLength=%zu,outFailed=%d}",
        (*self).fd, (*self).colorMode, (*self).deviceBg, (*self).presentWidth,
        (*self).cursorColumn, (*self).cursorRow, (*self).inAlternate, (*self).inTransaction,
        (*self).outLength, (*self).outFailed);
}
