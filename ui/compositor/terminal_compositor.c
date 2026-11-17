#include "vex.h"
#include "ui/compositor/terminal_compositor.h"
#include "ui/surface/terminal_canvas.h"
#include "nio/mem.h"
#include "oop/oop.h"
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
 * i32 fd                                  target file descriptor (default STDOUT_FILENO)
 * TerminalCompositorColorMode colorMode   TRUECOLOR, 256 or BASIC
 * u32 deviceBg                       backdrop color for alpha blending
 * bytes presentWidth                     boundary for wide-glyph clipping
 * bytes cursorColumn, cursorRow          tracked device cursor position
 * char pen[64], bytes penLength          cached SGR parameter string
 * bool penValid                           whether pen cache is live
 * bool inAlternate                        alternate screen active
 * bool inTransaction                      synchronized frame transaction active
 * bool outFailed                          write error latch
 * bytes outLength                        buffered byte count
 * char out[16384]                         output buffer
 * ======================================================================== */


;;GETTER
static inline TerminalCompositorColorMode TerminalCompositor_colorMode(const TerminalCompositor *s) { return (*s).colorMode; }
;;GETTER
static inline TerminalCompositorColorMode *TerminalCompositor_colorModeMut(TerminalCompositor *s) { return &(*s).colorMode; }
;;SETTER
static inline fn TerminalCompositor_setColorMode(TerminalCompositor *s, TerminalCompositorColorMode v) { (*s).colorMode = v; }
;;GETTER
static inline bytes TerminalCompositor_cursorColumn(const TerminalCompositor *s) { return (*s).cursorColumn; }
;;GETTER
static inline bytes *TerminalCompositor_cursorColumnMut(TerminalCompositor *s) { return &(*s).cursorColumn; }
;;SETTER
static inline fn TerminalCompositor_setCursorColumn(TerminalCompositor *s, bytes v) { (*s).cursorColumn = v; }
;;GETTER
static inline bytes TerminalCompositor_cursorRow(const TerminalCompositor *s) { return (*s).cursorRow; }
;;GETTER
static inline bytes *TerminalCompositor_cursorRowMut(TerminalCompositor *s) { return &(*s).cursorRow; }
;;SETTER
static inline fn TerminalCompositor_setCursorRow(TerminalCompositor *s, bytes v) { (*s).cursorRow = v; }
;;GETTER
static inline u32 TerminalCompositor_deviceBg(const TerminalCompositor *s) { return (*s).deviceBg; }
;;GETTER
static inline u32 *TerminalCompositor_deviceBgMut(TerminalCompositor *s) { return &(*s).deviceBg; }
;;SETTER
static inline fn TerminalCompositor_setDeviceBg(TerminalCompositor *s, u32 v) { (*s).deviceBg = v; }
;;GETTER
static inline i32 TerminalCompositor_fd(const TerminalCompositor *s) { return (*s).fd; }
;;GETTER
static inline i32 *TerminalCompositor_fdMut(TerminalCompositor *s) { return &(*s).fd; }
;;SETTER
static inline fn TerminalCompositor_setFd(TerminalCompositor *s, i32 v) { (*s).fd = v; }
;;GETTER
static inline bool TerminalCompositor_inAlternate(const TerminalCompositor *s) { return (*s).inAlternate; }
;;GETTER
static inline bool *TerminalCompositor_inAlternateMut(TerminalCompositor *s) { return &(*s).inAlternate; }
;;SETTER
static inline fn TerminalCompositor_setInAlternate(TerminalCompositor *s, bool v) { (*s).inAlternate = v; }
;;GETTER
static inline bool TerminalCompositor_inTransaction(const TerminalCompositor *s) { return (*s).inTransaction; }
;;GETTER
static inline bool *TerminalCompositor_inTransactionMut(TerminalCompositor *s) { return &(*s).inTransaction; }
;;SETTER
static inline fn TerminalCompositor_setInTransaction(TerminalCompositor *s, bool v) { (*s).inTransaction = v; }
;;GETTER
static inline char *TerminalCompositor_outMut(TerminalCompositor *s) { return (*s).out; }
;;GETTER
static inline const char *TerminalCompositor_out(const TerminalCompositor *s) { return (*s).out; }
;;GETTER
static inline bool TerminalCompositor_outFailed(const TerminalCompositor *s) { return (*s).outFailed; }
;;GETTER
static inline bool *TerminalCompositor_outFailedMut(TerminalCompositor *s) { return &(*s).outFailed; }
;;SETTER
static inline fn TerminalCompositor_setOutFailed(TerminalCompositor *s, bool v) { (*s).outFailed = v; }
;;GETTER
static inline bytes TerminalCompositor_outLength(const TerminalCompositor *s) { return (*s).outLength; }
;;GETTER
static inline bytes *TerminalCompositor_outLengthMut(TerminalCompositor *s) { return &(*s).outLength; }
;;SETTER
static inline fn TerminalCompositor_setOutLength(TerminalCompositor *s, bytes v) { (*s).outLength = v; }
;;GETTER
static inline char *TerminalCompositor_penMut(TerminalCompositor *s) { return (*s).pen; }
;;GETTER
static inline const char *TerminalCompositor_pen(const TerminalCompositor *s) { return (*s).pen; }
;;GETTER
static inline bytes TerminalCompositor_penLength(const TerminalCompositor *s) { return (*s).penLength; }
;;GETTER
static inline bytes *TerminalCompositor_penLengthMut(TerminalCompositor *s) { return &(*s).penLength; }
;;SETTER
static inline fn TerminalCompositor_setPenLength(TerminalCompositor *s, bytes v) { (*s).penLength = v; }
;;GETTER
static inline bool TerminalCompositor_penValid(const TerminalCompositor *s) { return (*s).penValid; }
;;GETTER
static inline bool *TerminalCompositor_penValidMut(TerminalCompositor *s) { return &(*s).penValid; }
;;SETTER
static inline fn TerminalCompositor_setPenValid(TerminalCompositor *s, bool v) { (*s).penValid = v; }
;;GETTER
static inline bytes TerminalCompositor_presentWidth(const TerminalCompositor *s) { return (*s).presentWidth; }
;;GETTER
static inline bytes *TerminalCompositor_presentWidthMut(TerminalCompositor *s) { return &(*s).presentWidth; }
;;SETTER
static inline fn TerminalCompositor_setPresentWidth(TerminalCompositor *s, bytes v) { (*s).presentWidth = v; }

static bytes utf8Encode(u32 glyph, char *bytes) {
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

static i16 colorIndex256(u32 color) {
    if (!TERMINAL_COLOR_IS_SET(color))
        return -1;
    i32 r = TERMINAL_COLOR_RED(color);
    i32 g = TERMINAL_COLOR_GREEN(color);
    i32 b = TERMINAL_COLOR_BLUE(color);
    i32 cr = (r * 5 + 127) / 255;
    i32 cg = (g * 5 + 127) / 255;
    i32 cb = (b * 5 + 127) / 255;
    return (i16) (16 + 36 * cr + 6 * cg + cb);
}

static fn compositorWriteRaw(TerminalCompositor *self, const char *data, bytes length) {
    if (self == nullptr || length == 0u || TerminalCompositor_fd(self) < 0)
        return;
    bytes written = 0u;
    while (written < length) {
        ssize_t result = write(TerminalCompositor_fd(self), data + written, length - written);
        if (result <= 0) {
            TerminalCompositor_setOutFailed(self, true);
            return;
        }
        written += (bytes) result;
    }
}

static fn compositorAppend(TerminalCompositor *self, const char *data, bytes length) {
    if (self == nullptr || data == nullptr || length == 0u)
        return;
    while (length != 0u) {
        bytes room = sizeof ((TerminalCompositor){0}).out - TerminalCompositor_outLength(self);
        if (room == 0u) {
            TerminalCompositor_flush(self);
            room = sizeof ((TerminalCompositor){0}).out;
        }
        bytes count = length < room ? length : room;
        memcpy(TerminalCompositor_outMut(self) + TerminalCompositor_outLength(self), data, count);
        TerminalCompositor_setOutLength(self, TerminalCompositor_outLength(self) + (count));
        data += count;
        length -= count;
    }
}

static fn compositorText(TerminalCompositor *self, const char *text) {
    if (text == nullptr)
        return;
    compositorAppend(self, text, strlen(text));
}

static fn compositorByte(TerminalCompositor *self, char byte) {
    compositorAppend(self, &byte, 1u);
}

static bytes formatPenColor(TerminalCompositor *self, char *params, bytes capacity,
                             bytes at, bool foreground, u32 color) {
    if (!TERMINAL_COLOR_IS_SET(color))
        return at;
    i32 prefix = foreground ? 3 : 4;
    i32 written = 0;
    if (TerminalCompositor_colorMode(self) == TERMINAL_COMPOSITOR_COLOR_TRUECOLOR) {
        written = snprintf(params + at, capacity - at, ";%d8;2;%u;%u;%u",
                           prefix,
                           (u32) TERMINAL_COLOR_RED(color),
                           (u32) TERMINAL_COLOR_GREEN(color),
                           (u32) TERMINAL_COLOR_BLUE(color));
    } else if (TerminalCompositor_colorMode(self) == TERMINAL_COMPOSITOR_COLOR_256) {
        i16 index = colorIndex256(color);
        if (index >= 0)
            written = snprintf(params + at, capacity - at, ";%d8;5;%d", prefix, (i32) index);
    } else {
        i16 index = colorIndex256(color);
        i32 basic = index >= 0 ? (index % 8) : 7;
        written = snprintf(params + at, capacity - at, ";%d", (foreground ? 30 : 40) + basic);
    }
    if (written <= 0 || (bytes) written >= capacity - at)
        return at;
    return at + (bytes) written;
}

static bytes formatPenParams(TerminalCompositor *self, char *params, bytes capacity,
                              u16 style, u32 fg, u32 bg) {
    i32 written = snprintf(params, capacity, "0%s%s%s%s%s",
                           (style & TERMINAL_STYLE_BOLD) ? ";1" : "",
                           (style & (TERMINAL_STYLE_DIM | TERMINAL_STYLE_MUTED)) ? ";2" : "",
                           (style & TERMINAL_STYLE_ITALIC) ? ";3" : "",
                           (style & TERMINAL_STYLE_UNDERLINE) ? ";4" : "",
                           (style & TERMINAL_STYLE_REVERSE) ? ";7" : "");
    if (written <= 0)
        return 0u;
    bytes at = (bytes) written < capacity ? (bytes) written : capacity;
    at = formatPenColor(self, params, capacity, at, true, fg);
    at = formatPenColor(self, params, capacity, at, false, bg);
    return at;
}

static fn compositorPen(TerminalCompositor *self, u16 style, u32 fg, u32 bg) {
    char params[TERMINAL_COMPOSITOR_PEN_CAPACITY];
    bytes length = formatPenParams(self, params, sizeof(params), style, fg, bg);
    if (TerminalCompositor_penValid(self) && length == TerminalCompositor_penLength(self)
        && memcmp(TerminalCompositor_penMut(self), params, length) == 0)
        return;
    memcpy(TerminalCompositor_penMut(self), params, length);
    TerminalCompositor_setPenLength(self, length);
    TerminalCompositor_setPenValid(self, true);
    compositorText(self, "\033[");
    compositorAppend(self, params, length);
    compositorByte(self, 'm');
}

static fn compositorMove(TerminalCompositor *self, bytes column, bytes row) {
    if (TerminalCompositor_cursorColumn(self) == column && TerminalCompositor_cursorRow(self) == row)
        return;
    char sequence[32];
    i32 written = snprintf(sequence, sizeof(sequence), "\033[%zu;%zuH", row + 1u, column + 1u);
    if (written > 0)
        compositorAppend(self, sequence, (bytes) written);
    TerminalCompositor_setCursorColumn(self, column);
    TerminalCompositor_setCursorRow(self, row);
}

TerminalCompositor *TerminalCompositor_0() {
    return TerminalCompositor_1(STDOUT_FILENO);
}

TerminalCompositor *TerminalCompositor_1(i32 fd) {
    TerminalCompositor *self = (TerminalCompositor*) Memory_alloc(TYPE_BYTE_ARRAY, sizeof(TerminalCompositor));
    if (self == nullptr)
        return nullptr;
    *self = (TerminalCompositor) {
        .fd = fd,
        .colorMode = TERMINAL_COMPOSITOR_COLOR_TRUECOLOR,
        .deviceBg = TERMINAL_COLOR_DEFAULT,
        .presentWidth = 0u,
        .cursorColumn = (bytes) -1,
        .cursorRow = (bytes) -1,
        .penLength = 0u,
        .penValid = false,
        .inAlternate = false,
        .inTransaction = false,
        .outFailed = false,
        .outLength = 0u
    };
    return self;
}

fn TerminalCompositor_free(TerminalCompositor *self) {
    if (self == nullptr)
        return;
    if (TerminalCompositor_inAlternate(self))
        TerminalCompositor_leave(self);
    else
        TerminalCompositor_flush(self);
    Memory_free(self);
}

bool TerminalCompositor_enter(TerminalCompositor *self) {
    if (self == nullptr)
        return false;
    TerminalCompositor_forget(self);
    TerminalCompositor_setOutLength(self, 0u);
    TerminalCompositor_setOutFailed(self, false);
    // Enter alternate screen, disable autowrap, hide cursor, clear screen, reset margin
    compositorText(self, "\033[?1049h\033[?7l\033[?25l\033[H\033[2J\033[r");
    TerminalCompositor_flush(self);
    TerminalCompositor_setInAlternate(self, true);
    return !TerminalCompositor_outFailed(self);
}

bool TerminalCompositor_leave(TerminalCompositor *self) {
    if (self == nullptr)
        return false;
    // Reset pen, enable autowrap, show cursor, exit alternate screen
    compositorText(self, "\033[0m\033[?7h\033[?25h\033[?1049l");
    TerminalCompositor_flush(self);
    TerminalCompositor_forget(self);
    TerminalCompositor_setInAlternate(self, false);
    return !TerminalCompositor_outFailed(self);
}

fn TerminalCompositor_flush(TerminalCompositor *self) {
    if (self == nullptr || TerminalCompositor_outLength(self) == 0u)
        return;
    compositorWriteRaw(self, TerminalCompositor_outMut(self), TerminalCompositor_outLength(self));
    TerminalCompositor_setOutLength(self, 0u);
}

fn TerminalCompositor_forget(TerminalCompositor *self) {
    if (self == nullptr)
        return;
    TerminalCompositor_setCursorColumn(self, (bytes) -1);
    TerminalCompositor_setCursorRow(self, (bytes) -1);
    TerminalCompositor_setPenLength(self, 0u);
    TerminalCompositor_setPenValid(self, false);
}

fn TerminalCompositor_clear(TerminalCompositor *self) {
    if (self == nullptr)
        return;
    TerminalCompositor_forget(self);
    compositorText(self, "\033[H\033[2J");
    TerminalCompositor_flush(self);
}

bool TerminalCompositor_blitRun(any context, TerminalRun run) {
    TerminalCompositor *self = (TerminalCompositor*) context;
    if (self == nullptr)
        return false;
    for (bytes i = 0u; i < run.length; ++i) {
        u32 glyph = run.cells != nullptr ? run.cells[i] : ' ';
        if (glyph == 0u) {
            if (TerminalCompositor_cursorRow(self) != run.y || TerminalCompositor_cursorColumn(self) != run.x + i + 1u)
                TerminalCompositor_setCursorColumn(self, (bytes) -1);
            continue;
        }
        if (glyph > 0x10FFFFu || (glyph >= 0xD800u && glyph <= 0xDFFFu))
            glyph = '?';
        u16 style = run.styles != nullptr ? run.styles[i] : 0u;
        u32 fg = run.fg != nullptr ? run.fg[i] : TERMINAL_COLOR_DEFAULT;
        u32 bg = run.bg != nullptr ? run.bg[i] : TERMINAL_COLOR_DEFAULT;

        u32 cellBg = TERMINAL_COLOR_IS_SET(bg)
                              ? TerminalColor_over(bg, TerminalCompositor_deviceBg(self))
                              : TERMINAL_COLOR_DEFAULT;
        u32 backdrop = TERMINAL_COLOR_IS_SET(cellBg) ? cellBg : TerminalCompositor_deviceBg(self);
        u32 cellFg = TERMINAL_COLOR_IS_SET(fg)
                              ? TerminalColor_over(fg, backdrop)
                              : TERMINAL_COLOR_DEFAULT;

        bytes column = run.x + i;
        bytes width = TerminalCanvas_widthOf(glyph);
        if (width == 0u)
            width = 1u;
        if (TerminalCompositor_presentWidth(self) != 0u && column + width > TerminalCompositor_presentWidth(self)) {
            glyph = ' ';
            width = 1u;
        }

        compositorMove(self, column, run.y);
        compositorPen(self, style, cellFg, cellBg);
        char encoded[4];
        bytes length = utf8Encode(glyph, encoded);
        compositorAppend(self, encoded, length);
        TerminalCompositor_setCursorColumn(self, column + width);
        TerminalCompositor_setCursorRow(self, run.y);
    }
    return !TerminalCompositor_outFailed(self);
}

fn TerminalCompositor_phase(any context, TerminalPresentPhase phase) {
    TerminalCompositor *self = (TerminalCompositor*) context;
    if (self == nullptr)
        return;
    if (phase == TERMINAL_PRESENT_BEGIN) {
        compositorText(self, "\033[?2026h");
        TerminalCompositor_setInTransaction(self, true);
    } else if (phase == TERMINAL_PRESENT_END) {
        if (TerminalCompositor_inTransaction(self)) {
            compositorText(self, "\033[?2026l");
            TerminalCompositor_setInTransaction(self, false);
        }
        TerminalCompositor_flush(self);
    }
}

bool TerminalCompositor_setFd(TerminalCompositor *self, i32 fd) {
    if (self == nullptr || fd < 0) {
        THROW("TerminalCompositor: invalid fd");
        return false;
    }
    TerminalCompositor_setFd(self, fd);
    return true;
}

i32 TerminalCompositor_getFd(const TerminalCompositor *self) {
    return self != nullptr ? TerminalCompositor_fd(self) : -1;
}

bool TerminalCompositor_setDeviceBackground(TerminalCompositor *self, u32 bg) {
    if (self == nullptr)
        return false;
    TerminalCompositor_setDeviceBg(self, bg);
    return true;
}

u32 TerminalCompositor_getDeviceBackground(const TerminalCompositor *self) {
    return self != nullptr ? TerminalCompositor_deviceBg(self) : TERMINAL_COLOR_DEFAULT;
}

bool TerminalCompositor_setPresentWidth(TerminalCompositor *self, bytes width) {
    if (self == nullptr)
        return false;
    TerminalCompositor_setPresentWidth(self, width);
    return true;
}

bytes TerminalCompositor_getPresentWidth(const TerminalCompositor *self) {
    return self != nullptr ? TerminalCompositor_presentWidth(self) : 0u;
}

bool TerminalCompositor_setColorMode(TerminalCompositor *self, TerminalCompositorColorMode mode) {
    if (self == nullptr)
        return false;
    TerminalCompositor_setColorMode(self, mode);
    return true;
}

TerminalCompositorColorMode TerminalCompositor_getColorMode(const TerminalCompositor *self) {
    return self != nullptr ? TerminalCompositor_colorMode(self) : TERMINAL_COMPOSITOR_COLOR_TRUECOLOR;
}

bool TerminalCompositor_getFailed(const TerminalCompositor *self) {
    return self != nullptr && TerminalCompositor_outFailed(self);
}

bytes TerminalCompositor_getBufferedLength(const TerminalCompositor *self) {
    return self != nullptr ? TerminalCompositor_outLength(self) : 0u;
}

TerminalProjection TerminalCompositor_toString(const TerminalCompositor *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format("TerminalCompositor{fd=%d,colorMode=%d,inAlternate=%d,inTransaction=%d}",
        TerminalCompositor_fd(self), TerminalCompositor_colorMode(self), TerminalCompositor_inAlternate(self), TerminalCompositor_inTransaction(self));
}

TerminalProjection TerminalCompositor_toStringStruct(const TerminalCompositor *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format(
        "TerminalCompositor{fd=%d,colorMode=%d,deviceBg=0x%08X,presentWidth=%zu,cursorCol=%zu,cursorRow=%zu,inAlternate=%d,inTransaction=%d,outLength=%zu,outFailed=%d}",
        TerminalCompositor_fd(self), TerminalCompositor_colorMode(self), TerminalCompositor_deviceBg(self), TerminalCompositor_presentWidth(self),
        TerminalCompositor_cursorColumn(self), TerminalCompositor_cursorRow(self), TerminalCompositor_inAlternate(self), TerminalCompositor_inTransaction(self),
        TerminalCompositor_outLength(self), TerminalCompositor_outFailed(self));
}
