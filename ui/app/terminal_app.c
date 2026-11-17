#include "vex.h"
#include "ui/app/terminal_app.h"
#include "nio/mem.h"
#include "oop/oop.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <signal.h>

;;DEFINITION
/** The canonical terminal application runner. Owns the d64-buffered cell planes,
 * drives the TerminalSurface diffing pipeline, coordinates with TerminalCompositor
 * for synchronized atomic ANSI flushes, and translates TTY or caller-fed input.
 *
 * Implements the Uniform Terminal Composition Law so consumers like Semicolon can
 * wire their component tree directly into a managed runtime instead of re-implementing
 * ad-hoc terminal drivers. */
;;OVERVIEW
/** ========================================================================
 * CLASS: TerminalApp (ui/app/terminal_app.c)
 * STRUCT FIELDS (Mirroring ui/app/terminal_app.h)
 *
 * TerminalPanel *root                     borrowed root panel to paint
 * TerminalCompositor *compositor          owned output flusher
 * TerminalSurface *surface                owned last-paint presenter
 * TerminalInput input                     mouse input decoder
 * bytes width, height                    grid dimensions in cells
 * bytes capacity                         allocated cell capacity
 * u32 *backCells, *frontCells        glyph planes
 * u16 *backStyles, *frontStyles      attribute planes
 * u32 *backFg, *frontFg              foreground color planes
 * u32 *backBg, *frontBg              background color planes
 * bool running                            main loop flag
 * bool headless                           offline testing flag
 * bytes frameCount                       rendered frame count
 * ======================================================================== */


;;GETTER
static inline u32 * TerminalApp_backBg(const TerminalApp *s) { return (*s).backBg; }
;;GETTER
static inline u32 * *TerminalApp_backBgMut(TerminalApp *s) { return &(*s).backBg; }
;;SETTER
static inline fn TerminalApp_setBackBg(TerminalApp *s, u32 * v) { (*s).backBg = v; }
;;GETTER
static inline u32 * TerminalApp_backCells(const TerminalApp *s) { return (*s).backCells; }
;;GETTER
static inline u32 * *TerminalApp_backCellsMut(TerminalApp *s) { return &(*s).backCells; }
;;SETTER
static inline fn TerminalApp_setBackCells(TerminalApp *s, u32 * v) { (*s).backCells = v; }
;;GETTER
static inline u32 * TerminalApp_backFg(const TerminalApp *s) { return (*s).backFg; }
;;GETTER
static inline u32 * *TerminalApp_backFgMut(TerminalApp *s) { return &(*s).backFg; }
;;SETTER
static inline fn TerminalApp_setBackFg(TerminalApp *s, u32 * v) { (*s).backFg = v; }
;;GETTER
static inline u16 * TerminalApp_backStyles(const TerminalApp *s) { return (*s).backStyles; }
;;GETTER
static inline u16 * *TerminalApp_backStylesMut(TerminalApp *s) { return &(*s).backStyles; }
;;SETTER
static inline fn TerminalApp_setBackStyles(TerminalApp *s, u16 * v) { (*s).backStyles = v; }
;;GETTER
static inline bytes TerminalApp_capacity(const TerminalApp *s) { return (*s).capacity; }
;;GETTER
static inline bytes *TerminalApp_capacityMut(TerminalApp *s) { return &(*s).capacity; }
;;SETTER
static inline fn TerminalApp_setCapacity(TerminalApp *s, bytes v) { (*s).capacity = v; }
;;GETTER
static inline TerminalCompositor * TerminalApp_compositor(const TerminalApp *s) { return (*s).compositor; }
;;GETTER
static inline TerminalCompositor * *TerminalApp_compositorMut(TerminalApp *s) { return &(*s).compositor; }
;;SETTER
static inline fn TerminalApp_setCompositor(TerminalApp *s, TerminalCompositor * v) { (*s).compositor = v; }
;;GETTER
static inline bytes TerminalApp_frameCount(const TerminalApp *s) { return (*s).frameCount; }
;;GETTER
static inline bytes *TerminalApp_frameCountMut(TerminalApp *s) { return &(*s).frameCount; }
;;SETTER
static inline fn TerminalApp_setFrameCount(TerminalApp *s, bytes v) { (*s).frameCount = v; }
;;GETTER
static inline u32 * TerminalApp_frontBg(const TerminalApp *s) { return (*s).frontBg; }
;;GETTER
static inline u32 * *TerminalApp_frontBgMut(TerminalApp *s) { return &(*s).frontBg; }
;;SETTER
static inline fn TerminalApp_setFrontBg(TerminalApp *s, u32 * v) { (*s).frontBg = v; }
;;GETTER
static inline u32 * TerminalApp_frontCells(const TerminalApp *s) { return (*s).frontCells; }
;;GETTER
static inline u32 * *TerminalApp_frontCellsMut(TerminalApp *s) { return &(*s).frontCells; }
;;SETTER
static inline fn TerminalApp_setFrontCells(TerminalApp *s, u32 * v) { (*s).frontCells = v; }
;;GETTER
static inline u32 * TerminalApp_frontFg(const TerminalApp *s) { return (*s).frontFg; }
;;GETTER
static inline u32 * *TerminalApp_frontFgMut(TerminalApp *s) { return &(*s).frontFg; }
;;SETTER
static inline fn TerminalApp_setFrontFg(TerminalApp *s, u32 * v) { (*s).frontFg = v; }
;;GETTER
static inline u16 * TerminalApp_frontStyles(const TerminalApp *s) { return (*s).frontStyles; }
;;GETTER
static inline u16 * *TerminalApp_frontStylesMut(TerminalApp *s) { return &(*s).frontStyles; }
;;SETTER
static inline fn TerminalApp_setFrontStyles(TerminalApp *s, u16 * v) { (*s).frontStyles = v; }
;;GETTER
static inline bool TerminalApp_headless(const TerminalApp *s) { return (*s).headless; }
;;GETTER
static inline bool *TerminalApp_headlessMut(TerminalApp *s) { return &(*s).headless; }
;;SETTER
static inline fn TerminalApp_setHeadless(TerminalApp *s, bool v) { (*s).headless = v; }
;;GETTER
static inline bytes TerminalApp_height(const TerminalApp *s) { return (*s).height; }
;;GETTER
static inline bytes *TerminalApp_heightMut(TerminalApp *s) { return &(*s).height; }
;;SETTER
static inline fn TerminalApp_setHeight(TerminalApp *s, bytes v) { (*s).height = v; }
;;GETTER
static inline TerminalInput TerminalApp_input(const TerminalApp *s) { return (*s).input; }
;;GETTER
static inline TerminalInput *TerminalApp_inputMut(TerminalApp *s) { return &(*s).input; }
;;SETTER
static inline fn TerminalApp_setInput(TerminalApp *s, TerminalInput v) { (*s).input = v; }
;;GETTER
static inline TerminalPanel * TerminalApp_root(const TerminalApp *s) { return (*s).root; }
;;GETTER
static inline TerminalPanel * *TerminalApp_rootMut(TerminalApp *s) { return &(*s).root; }
;;SETTER
static inline fn TerminalApp_setRoot(TerminalApp *s, TerminalPanel * v) { (*s).root = v; }
;;GETTER
static inline bool TerminalApp_running(const TerminalApp *s) { return (*s).running; }
;;GETTER
static inline bool *TerminalApp_runningMut(TerminalApp *s) { return &(*s).running; }
;;SETTER
static inline fn TerminalApp_setRunning(TerminalApp *s, bool v) { (*s).running = v; }
;;GETTER
static inline TerminalSurface * TerminalApp_surface(const TerminalApp *s) { return (*s).surface; }
;;GETTER
static inline TerminalSurface * *TerminalApp_surfaceMut(TerminalApp *s) { return &(*s).surface; }
;;SETTER
static inline fn TerminalApp_setSurface(TerminalApp *s, TerminalSurface * v) { (*s).surface = v; }
;;GETTER
static inline bytes TerminalApp_width(const TerminalApp *s) { return (*s).width; }
;;GETTER
static inline bytes *TerminalApp_widthMut(TerminalApp *s) { return &(*s).width; }
;;SETTER
static inline fn TerminalApp_setWidth(TerminalApp *s, bytes v) { (*s).width = v; }

static volatile sig_atomic_t gSignalQuit = 0;

static fn onSignal(i32 sig) {
    (discard) sig;
    gSignalQuit = 1;
}

static fn freePlanes(TerminalApp *self) {
    if (self == nullptr)
        return;
    if (TerminalApp_backCells(self) != nullptr)
        Memory_free(TerminalApp_backCells(self));
    if (TerminalApp_backStyles(self) != nullptr)
        Memory_free(TerminalApp_backStyles(self));
    if (TerminalApp_backFg(self) != nullptr)
        Memory_free(TerminalApp_backFg(self));
    if (TerminalApp_backBg(self) != nullptr)
        Memory_free(TerminalApp_backBg(self));
    if (TerminalApp_frontCells(self) != nullptr)
        Memory_free(TerminalApp_frontCells(self));
    if (TerminalApp_frontStyles(self) != nullptr)
        Memory_free(TerminalApp_frontStyles(self));
    if (TerminalApp_frontFg(self) != nullptr)
        Memory_free(TerminalApp_frontFg(self));
    if (TerminalApp_frontBg(self) != nullptr)
        Memory_free(TerminalApp_frontBg(self));
    TerminalApp_setBackCells(self, nullptr);
    TerminalApp_setBackStyles(self, nullptr);
    TerminalApp_setBackFg(self, nullptr);
    TerminalApp_setBackBg(self, nullptr);
    TerminalApp_setFrontCells(self, nullptr);
    TerminalApp_setFrontStyles(self, nullptr);
    TerminalApp_setFrontFg(self, nullptr);
    TerminalApp_setFrontBg(self, nullptr);
    TerminalApp_setCapacity(self, 0u);
}

static bool allocatePlanes(TerminalApp *self, bytes count) {
    freePlanes(self);
    TerminalApp_setBackCells(self, (u32*) Memory_alloc(TYPE_BYTE_ARRAY, count * sizeof(u32)));
    TerminalApp_setBackStyles(self, (u16*) Memory_alloc(TYPE_BYTE_ARRAY, count * sizeof(u16)));
    TerminalApp_setBackFg(self, (u32*) Memory_alloc(TYPE_BYTE_ARRAY, count * sizeof(u32)));
    TerminalApp_setBackBg(self, (u32*) Memory_alloc(TYPE_BYTE_ARRAY, count * sizeof(u32)));

    TerminalApp_setFrontCells(self, (u32*) Memory_alloc(TYPE_BYTE_ARRAY, count * sizeof(u32)));
    TerminalApp_setFrontStyles(self, (u16*) Memory_alloc(TYPE_BYTE_ARRAY, count * sizeof(u16)));
    TerminalApp_setFrontFg(self, (u32*) Memory_alloc(TYPE_BYTE_ARRAY, count * sizeof(u32)));
    TerminalApp_setFrontBg(self, (u32*) Memory_alloc(TYPE_BYTE_ARRAY, count * sizeof(u32)));

    if (TerminalApp_backCells(self) == nullptr || TerminalApp_backStyles(self) == nullptr
        || TerminalApp_backFg(self) == nullptr || TerminalApp_backBg(self) == nullptr
        || TerminalApp_frontCells(self) == nullptr || TerminalApp_frontStyles(self) == nullptr
        || TerminalApp_frontFg(self) == nullptr || TerminalApp_frontBg(self) == nullptr) {
        freePlanes(self);
        return false;
    }

    TerminalApp_setCapacity(self, count);
    for (bytes i = 0u; i < count; ++i) {
        TerminalApp_backCells(self)[i] = ' ';
        TerminalApp_backStyles(self)[i] = 0u;
        TerminalApp_backFg(self)[i] = TERMINAL_COLOR_DEFAULT;
        TerminalApp_backBg(self)[i] = TERMINAL_COLOR_DEFAULT;

        TerminalApp_frontCells(self)[i] = 0xFFFFFFFFu;
        TerminalApp_frontStyles(self)[i] = 0xFFFFu;
        TerminalApp_frontFg(self)[i] = 0xFFFFFFFFu;
        TerminalApp_frontBg(self)[i] = 0xFFFFFFFFu;
    }
    return true;
}

static fn rebindSurface(TerminalApp *self) {
    if (self == nullptr || TerminalApp_surface(self) == nullptr || TerminalApp_compositor(self) == nullptr
        || TerminalApp_capacity(self) == 0u)
        return;

    TerminalCanvas back = TerminalCanvas_6(TerminalApp_backCells(self), TerminalApp_backStyles(self),
                                          TerminalApp_backFg(self), TerminalApp_backBg(self),
                                          TerminalApp_width(self), TerminalApp_height(self));
    TerminalCanvas front = TerminalCanvas_6(TerminalApp_frontCells(self), TerminalApp_frontStyles(self),
                                           TerminalApp_frontFg(self), TerminalApp_frontBg(self),
                                           TerminalApp_width(self), TerminalApp_height(self));

    TerminalSurface_bind(TerminalApp_surface(self), &back, &front,
                         TerminalCompositor_blitRun, TerminalApp_compositor(self));
    TerminalSurface_setFrame(TerminalApp_surface(self),
                            TerminalCompositor_phase, TerminalApp_compositor(self));
}

TerminalApp *TerminalApp_0() {
    return TerminalApp_1(nullptr);
}

TerminalApp *TerminalApp_1(TerminalPanel *root) {
    TerminalApp *self = (TerminalApp*) Memory_alloc(TYPE_BYTE_ARRAY, sizeof(TerminalApp));
    if (self == nullptr)
        return nullptr;

    TerminalCompositor *comp = TerminalCompositor_0();
    TerminalSurface *surf = (TerminalSurface*) Memory_alloc(TYPE_BYTE_ARRAY, sizeof(TerminalSurface));
    if (comp == nullptr || surf == nullptr) {
        if (comp != nullptr)
            TerminalCompositor_free(comp);
        if (surf != nullptr)
            Memory_free(surf);
        Memory_free(self);
        return nullptr;
    }

    *surf = TerminalSurface_0();

    *self = (TerminalApp) {
        .root = root,
        .compositor = comp,
        .surface = surf,
        .input = TerminalInput_0(),
        .width = 80u,
        .height = 24u,
        .capacity = 0u,
        .backCells = nullptr,
        .backStyles = nullptr,
        .backFg = nullptr,
        .backBg = nullptr,
        .frontCells = nullptr,
        .frontStyles = nullptr,
        .frontFg = nullptr,
        .frontBg = nullptr,
        .running = false,
        .headless = false,
        .frameCount = 0u
    };

    if (!allocatePlanes(self, 80u * 24u)) {
        TerminalCompositor_free(comp);
        Memory_free(surf);
        Memory_free(self);
        return nullptr;
    }

    rebindSurface(self);
    return self;
}

fn TerminalApp_free(TerminalApp *self) {
    if (self == nullptr)
        return;
    freePlanes(self);
    if (TerminalApp_compositor(self) != nullptr)
        TerminalCompositor_free(TerminalApp_compositor(self));
    if (TerminalApp_surface(self) != nullptr)
        Memory_free(TerminalApp_surface(self));
    Memory_free(self);
}

bool TerminalApp_setRoot(TerminalApp *self, TerminalPanel *root) {
    if (self == nullptr)
        return false;
    TerminalApp_setRoot(self, root);
    return true;
}

TerminalPanel *TerminalApp_getRoot(const TerminalApp *self) {
    return self != nullptr ? TerminalApp_root(self) : nullptr;
}

bool TerminalApp_setCompositor(TerminalApp *self, TerminalCompositor *compositor) {
    if (self == nullptr || compositor == nullptr)
        return false;
    if (TerminalApp_compositor(self) != nullptr)
        TerminalCompositor_free(TerminalApp_compositor(self));
    TerminalApp_setCompositor(self, compositor);
    rebindSurface(self);
    return true;
}

TerminalCompositor *TerminalApp_getCompositor(const TerminalApp *self) {
    return self != nullptr ? TerminalApp_compositor(self) : nullptr;
}

bool TerminalApp_resize(TerminalApp *self, bytes width, bytes height) {
    if (self == nullptr || width == 0u || height == 0u)
        return false;
    if (TerminalApp_width(self) == width && TerminalApp_height(self) == height && TerminalApp_capacity(self) >= width * height)
        return true;

    TerminalApp_setWidth(self, width);
    TerminalApp_setHeight(self, height);
    if (!allocatePlanes(self, width * height))
        return false;

    if (TerminalApp_compositor(self) != nullptr) {
        TerminalCompositor_setPresentWidth(TerminalApp_compositor(self), width);
        TerminalCompositor_forget(TerminalApp_compositor(self));
    }

    rebindSurface(self);
    return true;
}

bytes TerminalApp_getWidth(const TerminalApp *self) {
    return self != nullptr ? TerminalApp_width(self) : 0u;
}

bytes TerminalApp_getHeight(const TerminalApp *self) {
    return self != nullptr ? TerminalApp_height(self) : 0u;
}

bool TerminalApp_step(TerminalApp *self) {
    if (self == nullptr || TerminalApp_root(self) == nullptr || TerminalApp_capacity(self) == 0u)
        return false;

    TerminalCanvas back = TerminalCanvas_6(TerminalApp_backCells(self), TerminalApp_backStyles(self),
                                          TerminalApp_backFg(self), TerminalApp_backBg(self),
                                          TerminalApp_width(self), TerminalApp_height(self));

    TerminalPen emptyPen = TerminalPen_0();
    TerminalCanvas_fill(&back, ' ', emptyPen);

    TerminalPanel_paint(TerminalApp_root(self), &back);
    TerminalSurface_present(TerminalApp_surface(self));
    TerminalApp_setFrameCount(self, TerminalApp_frameCount(self) + 1u);
    return true;
}

bool TerminalApp_feedInput(TerminalApp *self, const char *data, bytes length) {
    if (self == nullptr || data == nullptr || length == 0u)
        return false;
    for (bytes i = 0u; i < length; ++i)
        TerminalInput_feed(TerminalApp_inputMut(self), (u8) data[i]);
    return true;
}

fn TerminalApp_stop(TerminalApp *self) {
    if (self == nullptr)
        return;
    TerminalApp_setRunning(self, false);
}

bool TerminalApp_isRunning(const TerminalApp *self) {
    return self != nullptr && TerminalApp_running(self);
}

bool TerminalApp_setHeadless(TerminalApp *self, bool headless) {
    if (self == nullptr)
        return false;
    TerminalApp_setHeadless(self, headless);
    return true;
}

bool TerminalApp_isHeadless(const TerminalApp *self) {
    return self != nullptr && TerminalApp_headless(self);
}

i32 TerminalApp_run(TerminalApp *self) {
    if (self == nullptr || TerminalApp_root(self) == nullptr)
        return 1;

    if (TerminalApp_headless(self)) {
        TerminalApp_setRunning(self, true);
        TerminalApp_step(self);
        TerminalApp_setRunning(self, false);
        return 0;
    }

    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        THROW("TerminalApp requires interactive terminal stdin and stdout");
        return 1;
    }

    struct termios original, raw;
    if (tcgetattr(STDIN_FILENO, &original) != 0)
        return 1;

    raw = original;
    cfmakeraw(&raw);
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0)
        return 1;

    struct sigaction sa, oldInt, oldTerm;
    sa.sa_handler = onSignal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, &oldInt);
    sigaction(SIGTERM, &sa, &oldTerm);

    if (TerminalApp_compositor(self) != nullptr)
        TerminalCompositor_enter(TerminalApp_compositor(self));

    TerminalApp_setRunning(self, true);
    gSignalQuit = 0;

    while (TerminalApp_running(self) && !gSignalQuit) {
        struct winsize ws;
        if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0)
            TerminalApp_resize(self, (bytes) ws.ws_col, (bytes) ws.ws_row);

        TerminalApp_step(self);

        char inputBuf[128];
        ssize_t bytesRead = read(STDIN_FILENO, inputBuf, sizeof(inputBuf));
        if (bytesRead > 0) {
            // Check for Ctrl-C (byte 3)
            for (ssize_t i = 0; i < bytesRead; ++i) {
                if (inputBuf[i] == 3)
                    TerminalApp_setRunning(self, false);
            }
            TerminalApp_feedInput(self, inputBuf, (bytes) bytesRead);
        }
    }

    if (TerminalApp_compositor(self) != nullptr)
        TerminalCompositor_leave(TerminalApp_compositor(self));

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original);
    sigaction(SIGINT, &oldInt, nullptr);
    sigaction(SIGTERM, &oldTerm, nullptr);

    return 0;
}

TerminalProjection TerminalApp_toString(const TerminalApp *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format("TerminalApp{size=%zux%zu,running=%d,headless=%d,frames=%zu}",
        TerminalApp_width(self), TerminalApp_height(self), TerminalApp_running(self), TerminalApp_headless(self), TerminalApp_frameCount(self));
}

TerminalProjection TerminalApp_toStringStruct(const TerminalApp *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format(
        "TerminalApp{width=%zu,height=%zu,capacity=%zu,root=%p,running=%d,headless=%d,frames=%zu}",
        TerminalApp_width(self), TerminalApp_height(self), TerminalApp_capacity(self), (const_any) TerminalApp_root(self),
        TerminalApp_running(self), TerminalApp_headless(self), TerminalApp_frameCount(self));
}
