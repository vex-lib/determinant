#ifndef DETERMINANT_TERMINAL_COMPOSITOR_H
#define DETERMINANT_TERMINAL_COMPOSITOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ui/surface/terminal_surface.h"
#include "ui/surface/terminal_projection.h"
#include "format/terminal_color.h"
#include "format/terminal_style.h"
#include "c23/constructor.h"

// ui/compositor/terminal_compositor.h — the terminal output compositor.
//
// TerminalCompositor translates TerminalSurface delta runs into optimized ANSI/VT
// escape sequences, buffers output, and flushes it to a target descriptor (default
// STDOUT_FILENO).
//
// It encapsulates alternate screen entry/exit, cursor hide/show, absolute cursor
// addressing (CUP), SGR style/color transitions, synchronized frame transactions
// (\033[?2026h / \033[?2026l), and background alpha compositing via TerminalColor_over.
//
// By implementing TerminalRunFn and TerminalPresentPhaseFn, it serves as the direct
// rendering sink for TerminalSurface_present, eliminating ad-hoc ANSI formatters in
// client applications.

#define TERMINAL_COMPOSITOR_BUFFER_CAPACITY 16384u
#define TERMINAL_COMPOSITOR_PEN_CAPACITY 64u

typedef enum TerminalCompositorColorMode {
    TERMINAL_COMPOSITOR_COLOR_TRUECOLOR = 0,
    TERMINAL_COMPOSITOR_COLOR_256,
    TERMINAL_COMPOSITOR_COLOR_BASIC
} TerminalCompositorColorMode;

typedef struct TerminalCompositor {
    int fd;
    TerminalCompositorColorMode colorMode;
    uint32_t deviceBg;
    size_t presentWidth;
    size_t cursorColumn;
    size_t cursorRow;
    char pen[TERMINAL_COMPOSITOR_PEN_CAPACITY];
    size_t penLength;
    bool penValid;
    bool inAlternate;
    bool inTransaction;
    bool outFailed;
    size_t outLength;
    char out[TERMINAL_COMPOSITOR_BUFFER_CAPACITY];
} TerminalCompositor;

TerminalCompositor *TerminalCompositor_0(void);
TerminalCompositor *TerminalCompositor_1(int fd);
#define TerminalCompositor(...) CONSTRUCTOR_DISPATCH(TerminalCompositor, __VA_ARGS__)
#define TerminalCompositor_zero() TerminalCompositor_0()

void TerminalCompositor_free(TerminalCompositor *self);

bool TerminalCompositor_enter(TerminalCompositor *self);
bool TerminalCompositor_leave(TerminalCompositor *self);
void TerminalCompositor_flush(TerminalCompositor *self);
void TerminalCompositor_forget(TerminalCompositor *self);
void TerminalCompositor_clear(TerminalCompositor *self);

// TerminalSurface sink callbacks:
bool TerminalCompositor_blitRun(void *context, TerminalRun run);
void TerminalCompositor_phase(void *context, TerminalPresentPhase phase);

bool TerminalCompositor_setFd(TerminalCompositor *self, int fd);
int TerminalCompositor_getFd(const TerminalCompositor *self);
bool TerminalCompositor_setDeviceBackground(TerminalCompositor *self, uint32_t bg);
uint32_t TerminalCompositor_getDeviceBackground(const TerminalCompositor *self);
bool TerminalCompositor_setPresentWidth(TerminalCompositor *self, size_t width);
size_t TerminalCompositor_getPresentWidth(const TerminalCompositor *self);
bool TerminalCompositor_setColorMode(TerminalCompositor *self, TerminalCompositorColorMode mode);
TerminalCompositorColorMode TerminalCompositor_getColorMode(const TerminalCompositor *self);
bool TerminalCompositor_getFailed(const TerminalCompositor *self);
size_t TerminalCompositor_getBufferedLength(const TerminalCompositor *self);

TerminalProjection TerminalCompositor_toString(const TerminalCompositor *self);
TerminalProjection TerminalCompositor_toStringStruct(const TerminalCompositor *self);

#endif
