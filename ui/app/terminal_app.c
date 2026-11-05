#include "ui/app/terminal_app.h"
#include "nio/mem.h"
#include "oop/type.h"
#include "exception/throw.h"
#include "annotation/annotation.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <signal.h>

;;DEFINITION
/** The canonical terminal application runner. Owns the double-buffered cell planes,
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
 * size_t width, height                    grid dimensions in cells
 * size_t capacity                         allocated cell capacity
 * uint32_t *backCells, *frontCells        glyph planes
 * uint16_t *backStyles, *frontStyles      attribute planes
 * uint32_t *backFg, *frontFg              foreground color planes
 * uint32_t *backBg, *frontBg              background color planes
 * bool running                            main loop flag
 * bool headless                           offline testing flag
 * size_t frameCount                       rendered frame count
 * ======================================================================== */

static volatile sig_atomic_t gSignalQuit = 0;

static void onSignal(int sig) {
    (void) sig;
    gSignalQuit = 1;
}

static void freePlanes(TerminalApp *self) {
    if (self == nullptr)
        return;
    if ((*self).backCells != nullptr)
        Memory_free((*self).backCells);
    if ((*self).backStyles != nullptr)
        Memory_free((*self).backStyles);
    if ((*self).backFg != nullptr)
        Memory_free((*self).backFg);
    if ((*self).backBg != nullptr)
        Memory_free((*self).backBg);
    if ((*self).frontCells != nullptr)
        Memory_free((*self).frontCells);
    if ((*self).frontStyles != nullptr)
        Memory_free((*self).frontStyles);
    if ((*self).frontFg != nullptr)
        Memory_free((*self).frontFg);
    if ((*self).frontBg != nullptr)
        Memory_free((*self).frontBg);
    (*self).backCells = nullptr;
    (*self).backStyles = nullptr;
    (*self).backFg = nullptr;
    (*self).backBg = nullptr;
    (*self).frontCells = nullptr;
    (*self).frontStyles = nullptr;
    (*self).frontFg = nullptr;
    (*self).frontBg = nullptr;
    (*self).capacity = 0u;
}

static bool allocatePlanes(TerminalApp *self, size_t count) {
    freePlanes(self);
    (*self).backCells = (uint32_t*) Memory_alloc(TYPE_BYTE_ARRAY, count * sizeof(uint32_t));
    (*self).backStyles = (uint16_t*) Memory_alloc(TYPE_BYTE_ARRAY, count * sizeof(uint16_t));
    (*self).backFg = (uint32_t*) Memory_alloc(TYPE_BYTE_ARRAY, count * sizeof(uint32_t));
    (*self).backBg = (uint32_t*) Memory_alloc(TYPE_BYTE_ARRAY, count * sizeof(uint32_t));

    (*self).frontCells = (uint32_t*) Memory_alloc(TYPE_BYTE_ARRAY, count * sizeof(uint32_t));
    (*self).frontStyles = (uint16_t*) Memory_alloc(TYPE_BYTE_ARRAY, count * sizeof(uint16_t));
    (*self).frontFg = (uint32_t*) Memory_alloc(TYPE_BYTE_ARRAY, count * sizeof(uint32_t));
    (*self).frontBg = (uint32_t*) Memory_alloc(TYPE_BYTE_ARRAY, count * sizeof(uint32_t));

    if ((*self).backCells == nullptr || (*self).backStyles == nullptr
        || (*self).backFg == nullptr || (*self).backBg == nullptr
        || (*self).frontCells == nullptr || (*self).frontStyles == nullptr
        || (*self).frontFg == nullptr || (*self).frontBg == nullptr) {
        freePlanes(self);
        return false;
    }

    (*self).capacity = count;
    for (size_t i = 0u; i < count; ++i) {
        (*self).backCells[i] = ' ';
        (*self).backStyles[i] = 0u;
        (*self).backFg[i] = TERMINAL_COLOR_DEFAULT;
        (*self).backBg[i] = TERMINAL_COLOR_DEFAULT;

        (*self).frontCells[i] = 0xFFFFFFFFu;
        (*self).frontStyles[i] = 0xFFFFu;
        (*self).frontFg[i] = 0xFFFFFFFFu;
        (*self).frontBg[i] = 0xFFFFFFFFu;
    }
    return true;
}

static void rebindSurface(TerminalApp *self) {
    if (self == nullptr || (*self).surface == nullptr || (*self).compositor == nullptr
        || (*self).capacity == 0u)
        return;

    TerminalCanvas back = TerminalCanvas_6((*self).backCells, (*self).backStyles,
                                          (*self).backFg, (*self).backBg,
                                          (*self).width, (*self).height);
    TerminalCanvas front = TerminalCanvas_6((*self).frontCells, (*self).frontStyles,
                                           (*self).frontFg, (*self).frontBg,
                                           (*self).width, (*self).height);

    TerminalSurface_bind((*self).surface, &back, &front,
                         TerminalCompositor_blitRun, (*self).compositor);
    TerminalSurface_setFrame((*self).surface,
                            TerminalCompositor_phase, (*self).compositor);
}

TerminalApp *TerminalApp_0(void) {
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

void TerminalApp_free(TerminalApp *self) {
    if (self == nullptr)
        return;
    freePlanes(self);
    if ((*self).compositor != nullptr)
        TerminalCompositor_free((*self).compositor);
    if ((*self).surface != nullptr)
        Memory_free((*self).surface);
    Memory_free(self);
}

bool TerminalApp_setRoot(TerminalApp *self, TerminalPanel *root) {
    if (self == nullptr)
        return false;
    (*self).root = root;
    return true;
}

TerminalPanel *TerminalApp_getRoot(const TerminalApp *self) {
    return self != nullptr ? (*self).root : nullptr;
}

bool TerminalApp_setCompositor(TerminalApp *self, TerminalCompositor *compositor) {
    if (self == nullptr || compositor == nullptr)
        return false;
    if ((*self).compositor != nullptr)
        TerminalCompositor_free((*self).compositor);
    (*self).compositor = compositor;
    rebindSurface(self);
    return true;
}

TerminalCompositor *TerminalApp_getCompositor(const TerminalApp *self) {
    return self != nullptr ? (*self).compositor : nullptr;
}

bool TerminalApp_resize(TerminalApp *self, size_t width, size_t height) {
    if (self == nullptr || width == 0u || height == 0u)
        return false;
    if ((*self).width == width && (*self).height == height && (*self).capacity >= width * height)
        return true;

    (*self).width = width;
    (*self).height = height;
    if (!allocatePlanes(self, width * height))
        return false;

    if ((*self).compositor != nullptr) {
        TerminalCompositor_setPresentWidth((*self).compositor, width);
        TerminalCompositor_forget((*self).compositor);
    }

    rebindSurface(self);
    return true;
}

size_t TerminalApp_getWidth(const TerminalApp *self) {
    return self != nullptr ? (*self).width : 0u;
}

size_t TerminalApp_getHeight(const TerminalApp *self) {
    return self != nullptr ? (*self).height : 0u;
}

bool TerminalApp_step(TerminalApp *self) {
    if (self == nullptr || (*self).root == nullptr || (*self).capacity == 0u)
        return false;

    TerminalCanvas back = TerminalCanvas_6((*self).backCells, (*self).backStyles,
                                          (*self).backFg, (*self).backBg,
                                          (*self).width, (*self).height);

    TerminalPen emptyPen = TerminalPen_0();
    TerminalCanvas_fill(&back, ' ', emptyPen);

    TerminalPanel_paint((*self).root, &back);
    TerminalSurface_present((*self).surface);
    (*self).frameCount++;
    return true;
}

bool TerminalApp_feedInput(TerminalApp *self, const char *bytes, size_t length) {
    if (self == nullptr || bytes == nullptr || length == 0u)
        return false;
    for (size_t i = 0u; i < length; ++i)
        TerminalInput_feed(&(*self).input, (uint8_t) bytes[i]);
    return true;
}

void TerminalApp_stop(TerminalApp *self) {
    if (self == nullptr)
        return;
    (*self).running = false;
}

bool TerminalApp_isRunning(const TerminalApp *self) {
    return self != nullptr && (*self).running;
}

bool TerminalApp_setHeadless(TerminalApp *self, bool headless) {
    if (self == nullptr)
        return false;
    (*self).headless = headless;
    return true;
}

bool TerminalApp_isHeadless(const TerminalApp *self) {
    return self != nullptr && (*self).headless;
}

int TerminalApp_run(TerminalApp *self) {
    if (self == nullptr || (*self).root == nullptr)
        return 1;

    if ((*self).headless) {
        (*self).running = true;
        TerminalApp_step(self);
        (*self).running = false;
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

    if ((*self).compositor != nullptr)
        TerminalCompositor_enter((*self).compositor);

    (*self).running = true;
    gSignalQuit = 0;

    while ((*self).running && !gSignalQuit) {
        struct winsize ws;
        if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0)
            TerminalApp_resize(self, (size_t) ws.ws_col, (size_t) ws.ws_row);

        TerminalApp_step(self);

        char inputBuf[128];
        ssize_t bytesRead = read(STDIN_FILENO, inputBuf, sizeof(inputBuf));
        if (bytesRead > 0) {
            // Check for Ctrl-C (byte 3)
            for (ssize_t i = 0; i < bytesRead; ++i) {
                if (inputBuf[i] == 3)
                    (*self).running = false;
            }
            TerminalApp_feedInput(self, inputBuf, (size_t) bytesRead);
        }
    }

    if ((*self).compositor != nullptr)
        TerminalCompositor_leave((*self).compositor);

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original);
    sigaction(SIGINT, &oldInt, nullptr);
    sigaction(SIGTERM, &oldTerm, nullptr);

    return 0;
}

TerminalProjection TerminalApp_toString(const TerminalApp *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format("TerminalApp{size=%zux%zu,running=%d,headless=%d,frames=%zu}",
        (*self).width, (*self).height, (*self).running, (*self).headless, (*self).frameCount);
}

TerminalProjection TerminalApp_toStringStruct(const TerminalApp *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format(
        "TerminalApp{width=%zu,height=%zu,capacity=%zu,root=%p,running=%d,headless=%d,frames=%zu}",
        (*self).width, (*self).height, (*self).capacity, (const void*) (*self).root,
        (*self).running, (*self).headless, (*self).frameCount);
}
