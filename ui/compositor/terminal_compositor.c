#include "vex.h"
#include "ui/compositor/terminal_compositor.h"
#include "ui/surface/terminal_canvas.h"
#include "nio/mem.h"
#include "type/re_type.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "core/determinant_memory.h"

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
static inline TerminalCompositorColorMode Comp_colorMode(const TerminalCompositor *s) { return (*s).colorMode; }
;;SETTER
static inline fn Comp_setColorMode(TerminalCompositor *s, TerminalCompositorColorMode v) { (*s).colorMode = v; }
;;GETTER
static inline bytes Comp_cursorColumn(const TerminalCompositor *s) { return (*s).cursorColumn; }
;;SETTER
static inline fn Comp_setCursorColumn(TerminalCompositor *s, bytes v) { (*s).cursorColumn = v; }
;;GETTER
static inline bytes Comp_cursorRow(const TerminalCompositor *s) { return (*s).cursorRow; }
;;SETTER
static inline fn Comp_setCursorRow(TerminalCompositor *s, bytes v) { (*s).cursorRow = v; }
;;GETTER
static inline u32 Comp_deviceBg(const TerminalCompositor *s) { return (*s).deviceBg; }
;;SETTER
static inline fn Comp_setDeviceBg(TerminalCompositor *s, u32 v) { (*s).deviceBg = v; }
;;GETTER
static inline i32 Comp_fd(const TerminalCompositor *s) { return (*s).fd; }
;;SETTER
static inline fn Comp_setFd(TerminalCompositor *s, i32 v) { (*s).fd = v; }
;;GETTER
static inline bool Comp_inAlternate(const TerminalCompositor *s) { return (*s).inAlternate; }
;;SETTER
static inline fn Comp_setInAlternate(TerminalCompositor *s, bool v) { (*s).inAlternate = v; }
;;GETTER
static inline bool Comp_inTransaction(const TerminalCompositor *s) { return (*s).inTransaction; }
;;SETTER
static inline fn Comp_setInTransaction(TerminalCompositor *s, bool v) { (*s).inTransaction = v; }
;;GETTER
static inline char *Comp_outMut(TerminalCompositor *s) { return (*s).out; }
;;GETTER
static inline bool Comp_outFailed(const TerminalCompositor *s) { return (*s).outFailed; }
;;SETTER
static inline fn Comp_setOutFailed(TerminalCompositor *s, bool v) { (*s).outFailed = v; }
;;GETTER
static inline bytes Comp_outLength(const TerminalCompositor *s) { return (*s).outLength; }
;;SETTER
static inline fn Comp_setOutLength(TerminalCompositor *s, bytes v) { (*s).outLength = v; }
;;GETTER
static inline char *Comp_penMut(TerminalCompositor *s) { return (*s).pen; }
;;GETTER
static inline bytes Comp_penLength(const TerminalCompositor *s) { return (*s).penLength; }
;;SETTER
static inline fn Comp_setPenLength(TerminalCompositor *s, bytes v) { (*s).penLength = v; }
;;GETTER
static inline bool Comp_penValid(const TerminalCompositor *s) { return (*s).penValid; }
;;SETTER
static inline fn Comp_setPenValid(TerminalCompositor *s, bool v) { (*s).penValid = v; }
;;GETTER
static inline bytes Comp_presentWidth(const TerminalCompositor *s) { return (*s).presentWidth; }
;;SETTER
static inline fn Comp_setPresentWidth(TerminalCompositor *s, bytes v) { (*s).presentWidth = v; }

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
    if (self == nullptr || length == 0u || Comp_fd(self) < 0)
        return;
    bytes written = 0u;
    while (written < length) {
        ssize_t result = write(Comp_fd(self), data + written, length - written);
        if (result <= 0) {
            Comp_setOutFailed(self, true);
            return;
        }
        written += (bytes) result;
    }
}

static fn compositorAppend(TerminalCompositor *self, const char *data, bytes length) {
    if (self == nullptr || data == nullptr || length == 0u)
        return;
    while (length != 0u) {
        bytes room = sizeof ((TerminalCompositor){0}).out - Comp_outLength(self);
        if (room == 0u) {
            TerminalCompositor_flush(self);
            room = sizeof ((TerminalCompositor){0}).out;
        }
        bytes count = length < room ? length : room;
        memcpy(Comp_outMut(self) + Comp_outLength(self), data, count);
        Comp_setOutLength(self, Comp_outLength(self) + count);
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
    if (Comp_colorMode(self) == TERMINAL_COMPOSITOR_COLOR_TRUECOLOR) {
        written = snprintf(params + at, capacity - at, ";%d8;2;%u;%u;%u",
                           prefix,
                           (u32) TERMINAL_COLOR_RED(color),
                           (u32) TERMINAL_COLOR_GREEN(color),
                           (u32) TERMINAL_COLOR_BLUE(color));
    } else if (Comp_colorMode(self) == TERMINAL_COMPOSITOR_COLOR_256) {
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
    if (Comp_penValid(self) && length == Comp_penLength(self)
        && memcmp(Comp_penMut(self), params, length) == 0)
        return;
    memcpy(Comp_penMut(self), params, length);
    Comp_setPenLength(self, length);
    Comp_setPenValid(self, true);
    compositorText(self, "\033[");
    compositorAppend(self, params, length);
    compositorByte(self, 'm');
}

static fn compositorMove(TerminalCompositor *self, bytes column, bytes row) {
    if (Comp_cursorColumn(self) == column && Comp_cursorRow(self) == row)
        return;
    char sequence[32];
    i32 written = snprintf(sequence, sizeof(sequence), "\033[%zu;%zuH", row + 1u, column + 1u);
    if (written > 0)
        compositorAppend(self, sequence, (bytes) written);
    Comp_setCursorColumn(self, column);
    Comp_setCursorRow(self, row);
}

TerminalCompositor *TerminalCompositor_0() {
    return TerminalCompositor_1(STDOUT_FILENO);
}

TerminalCompositor *TerminalCompositor_1(i32 fd) {
    TerminalCompositor *self = (TerminalCompositor*) Memory_alloc(TYPE_BYTE_ARRAY, Determinant_arena(), sizeof(TerminalCompositor));
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
    if (Comp_inAlternate(self))
        TerminalCompositor_leave(self);
    else
        TerminalCompositor_flush(self);
    Memory_free(self);
}

bool TerminalCompositor_enter(TerminalCompositor *self) {
    if (self == nullptr)
        return false;
    TerminalCompositor_forget(self);
    Comp_setOutLength(self, 0u);
    Comp_setOutFailed(self, false);
    compositorText(self, "\033[?1049h\033[?7l\033[?25l\033[H\033[2J\033[r");
    TerminalCompositor_flush(self);
    Comp_setInAlternate(self, true);
    return !Comp_outFailed(self);
}

bool TerminalCompositor_leave(TerminalCompositor *self) {
    if (self == nullptr)
        return false;
    compositorText(self, "\033[0m\033[?7h\033[?25h\033[?1049l");
    TerminalCompositor_flush(self);
    TerminalCompositor_forget(self);
    Comp_setInAlternate(self, false);
    return !Comp_outFailed(self);
}

fn TerminalCompositor_flush(TerminalCompositor *self) {
    if (self == nullptr || Comp_outLength(self) == 0u)
        return;
    compositorWriteRaw(self, Comp_outMut(self), Comp_outLength(self));
    Comp_setOutLength(self, 0u);
}

fn TerminalCompositor_forget(TerminalCompositor *self) {
    if (self == nullptr)
        return;
    Comp_setCursorColumn(self, (bytes) -1);
    Comp_setCursorRow(self, (bytes) -1);
    Comp_setPenLength(self, 0u);
    Comp_setPenValid(self, false);
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
            if (Comp_cursorRow(self) != run.y || Comp_cursorColumn(self) != run.x + i + 1u)
                Comp_setCursorColumn(self, (bytes) -1);
            continue;
        }
        if (glyph > 0x10FFFFu || (glyph >= 0xD800u && glyph <= 0xDFFFu))
            glyph = '?';
        u16 style = run.styles != nullptr ? run.styles[i] : 0u;
        u32 fg = run.fg != nullptr ? run.fg[i] : TERMINAL_COLOR_DEFAULT;
        u32 bg = run.bg != nullptr ? run.bg[i] : TERMINAL_COLOR_DEFAULT;

        u32 cellBg = TERMINAL_COLOR_IS_SET(bg)
                              ? TerminalColor_over(bg, Comp_deviceBg(self))
                              : TERMINAL_COLOR_DEFAULT;
        u32 backdrop = TERMINAL_COLOR_IS_SET(cellBg) ? cellBg : Comp_deviceBg(self);
        u32 cellFg = TERMINAL_COLOR_IS_SET(fg)
                              ? TerminalColor_over(fg, backdrop)
                              : TERMINAL_COLOR_DEFAULT;

        bytes column = run.x + i;
        bytes width = TerminalCanvas_widthOf(glyph);
        if (width == 0u)
            width = 1u;
        if (Comp_presentWidth(self) != 0u && column + width > Comp_presentWidth(self)) {
            glyph = ' ';
            width = 1u;
        }

        compositorMove(self, column, run.y);
        compositorPen(self, style, cellFg, cellBg);
        char encoded[4];
        bytes length = utf8Encode(glyph, encoded);
        compositorAppend(self, encoded, length);
        Comp_setCursorColumn(self, column + width);
        Comp_setCursorRow(self, run.y);
    }
    return !Comp_outFailed(self);
}

fn TerminalCompositor_phase(any context, TerminalPresentPhase phase) {
    TerminalCompositor *self = (TerminalCompositor*) context;
    if (self == nullptr)
        return;
    if (phase == TERMINAL_PRESENT_BEGIN) {
        compositorText(self, "\033[?2026h");
        Comp_setInTransaction(self, true);
    } else if (phase == TERMINAL_PRESENT_END) {
        if (Comp_inTransaction(self)) {
            compositorText(self, "\033[?2026l");
            Comp_setInTransaction(self, false);
        }
        TerminalCompositor_flush(self);
    }
}

bool TerminalCompositor_setFd(TerminalCompositor *self, i32 fd) {
    if (self == nullptr || fd < 0) {
        THROW("TerminalCompositor: invalid fd");
        return false;
    }
    Comp_setFd(self, fd);
    return true;
}

i32 TerminalCompositor_getFd(const TerminalCompositor *self) {
    return self != nullptr ? Comp_fd(self) : -1;
}

bool TerminalCompositor_setDeviceBackground(TerminalCompositor *self, u32 bg) {
    if (self == nullptr)
        return false;
    Comp_setDeviceBg(self, bg);
    return true;
}

u32 TerminalCompositor_getDeviceBackground(const TerminalCompositor *self) {
    return self != nullptr ? Comp_deviceBg(self) : TERMINAL_COLOR_DEFAULT;
}

bool TerminalCompositor_setPresentWidth(TerminalCompositor *self, bytes width) {
    if (self == nullptr)
        return false;
    Comp_setPresentWidth(self, width);
    return true;
}

bytes TerminalCompositor_getPresentWidth(const TerminalCompositor *self) {
    return self != nullptr ? Comp_presentWidth(self) : 0u;
}

bool TerminalCompositor_setColorMode(TerminalCompositor *self, TerminalCompositorColorMode mode) {
    if (self == nullptr)
        return false;
    Comp_setColorMode(self, mode);
    return true;
}

TerminalCompositorColorMode TerminalCompositor_getColorMode(const TerminalCompositor *self) {
    return self != nullptr ? Comp_colorMode(self) : TERMINAL_COMPOSITOR_COLOR_TRUECOLOR;
}

bool TerminalCompositor_getFailed(const TerminalCompositor *self) {
    return self != nullptr && Comp_outFailed(self);
}

bytes TerminalCompositor_getBufferedLength(const TerminalCompositor *self) {
    return self != nullptr ? Comp_outLength(self) : 0u;
}

TerminalProjection TerminalCompositor_toString(const TerminalCompositor *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format("TerminalCompositor{fd=%d,colorMode=%d,inAlternate=%d,inTransaction=%d}",
        Comp_fd(self), Comp_colorMode(self), Comp_inAlternate(self), Comp_inTransaction(self));
}

TerminalProjection TerminalCompositor_toStringStruct(const TerminalCompositor *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format(
        "TerminalCompositor{fd=%d,colorMode=%d,deviceBg=0x%08X,presentWidth=%zu,cursorCol=%zu,cursorRow=%zu,inAlternate=%d,inTransaction=%d,outLength=%zu,outFailed=%d}",
        Comp_fd(self), Comp_colorMode(self), Comp_deviceBg(self), Comp_presentWidth(self),
        Comp_cursorColumn(self), Comp_cursorRow(self), Comp_inAlternate(self), Comp_inTransaction(self),
        Comp_outLength(self), Comp_outFailed(self));
}
