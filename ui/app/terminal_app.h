#ifndef DETERMINANT_TERMINAL_APP_H
#define DETERMINANT_TERMINAL_APP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ui/panel/terminal_panel.h"
#include "ui/surface/terminal_canvas.h"
#include "ui/surface/terminal_surface.h"
#include "ui/surface/terminal_projection.h"
#include "ui/compositor/terminal_compositor.h"
#include "events/terminal_input.h"
#include "c23/constructor.h"

// ui/app/terminal_app.h — the canonical terminal application runner.
//
// TerminalApp manages the execution harness for terminal client applications:
// it owns the front/back canvas pair, drives the TerminalSurface diffing engine,
// coordinates with TerminalCompositor to flush atomic frames, and decodes caller-fed
// or TTY input through TerminalInput.
//
// By encapsulating the frame render-diff-flush step and raw mode terminal acquisition,
// it satisfies the Uniform Terminal Composition Law and allows superapps like Semicolon
// to wire views and run without hand-rolling terminal drivers.

typedef struct TerminalApp {
    TerminalPanel *root;                  // borrowed root panel
    TerminalCompositor *compositor;       // owned output compositor
    TerminalSurface *surface;             // owned last-paint presenter
    TerminalInput input;                  // mouse input state machine
    size_t width;                         // current grid width in cells
    size_t height;                        // current grid height in cells
    size_t capacity;                      // allocated cells (width * height)
    uint32_t *backCells;                  // back buffer glyphs
    uint16_t *backStyles;                 // back buffer styles
    uint32_t *backFg;                     // back buffer foreground
    uint32_t *backBg;                     // back buffer background
    uint32_t *frontCells;                 // front buffer mirror
    uint16_t *frontStyles;                // front buffer mirror
    uint32_t *frontFg;                    // front buffer mirror
    uint32_t *frontBg;                    // front buffer mirror
    bool running;                         // main loop active
    bool headless;                        // offline mode (no real TTY calls)
    size_t frameCount;                    // completed frame steps
} TerminalApp;

TerminalApp *TerminalApp_0(void);
TerminalApp *TerminalApp_1(TerminalPanel *root);
#define TerminalApp(...) CONSTRUCTOR_DISPATCH(TerminalApp, __VA_ARGS__)
#define TerminalApp_zero() TerminalApp_0()

void TerminalApp_free(TerminalApp *self);

bool TerminalApp_setRoot(TerminalApp *self, TerminalPanel *root);
TerminalPanel *TerminalApp_getRoot(const TerminalApp *self);

bool TerminalApp_setCompositor(TerminalApp *self, TerminalCompositor *compositor);
TerminalCompositor *TerminalApp_getCompositor(const TerminalApp *self);

bool TerminalApp_resize(TerminalApp *self, size_t width, size_t height);
size_t TerminalApp_getWidth(const TerminalApp *self);
size_t TerminalApp_getHeight(const TerminalApp *self);

// Advances one frame: paints root panel into back canvas, diffs through surface,
// and flushes changed runs through compositor.
bool TerminalApp_step(TerminalApp *self);

// Feeds raw input bytes into the internal input decoder.
bool TerminalApp_feedInput(TerminalApp *self, const char *bytes, size_t length);

// Runs the interactive event loop until stopped or interrupted.
int TerminalApp_run(TerminalApp *self);
void TerminalApp_stop(TerminalApp *self);
bool TerminalApp_isRunning(const TerminalApp *self);

bool TerminalApp_setHeadless(TerminalApp *self, bool headless);
bool TerminalApp_isHeadless(const TerminalApp *self);

TerminalProjection TerminalApp_toString(const TerminalApp *self);
TerminalProjection TerminalApp_toStringStruct(const TerminalApp *self);

#endif
