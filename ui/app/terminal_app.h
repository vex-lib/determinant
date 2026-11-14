#ifndef DETERMINANT_TERMINAL_APP_H
#define DETERMINANT_TERMINAL_APP_H

#include "vex.h"
#include "ui/panel/terminal_panel.h"
#include "ui/surface/terminal_canvas.h"
#include "ui/surface/terminal_surface.h"
#include "ui/surface/terminal_projection.h"
#include "ui/compositor/terminal_compositor.h"
#include "events/terminal_input.h"

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
    bytes width;                         // current grid width in cells
    bytes height;                        // current grid height in cells
    bytes capacity;                      // allocated cells (width * height)
    u32 *backCells;                  // back buffer glyphs
    u16 *backStyles;                 // back buffer styles
    u32 *backFg;                     // back buffer foreground
    u32 *backBg;                     // back buffer background
    u32 *frontCells;                 // front buffer mirror
    u16 *frontStyles;                // front buffer mirror
    u32 *frontFg;                    // front buffer mirror
    u32 *frontBg;                    // front buffer mirror
    bool running;                         // main loop active
    bool headless;                        // offline mode (no real TTY calls)
    bytes frameCount;                    // completed frame steps
} TerminalApp;

TerminalApp *TerminalApp_0();
TerminalApp *TerminalApp_1(TerminalPanel *root);
#define TerminalApp(...) CONSTRUCTOR_DISPATCH(TerminalApp, __VA_ARGS__)
#define TerminalApp_zero() TerminalApp_0()

fn TerminalApp_free(TerminalApp *self);

bool TerminalApp_setRoot(TerminalApp *self, TerminalPanel *root);
TerminalPanel *TerminalApp_getRoot(const TerminalApp *self);

bool TerminalApp_setCompositor(TerminalApp *self, TerminalCompositor *compositor);
TerminalCompositor *TerminalApp_getCompositor(const TerminalApp *self);

bool TerminalApp_resize(TerminalApp *self, bytes width, bytes height);
bytes TerminalApp_getWidth(const TerminalApp *self);
bytes TerminalApp_getHeight(const TerminalApp *self);

// Advances one frame: paints root panel into back canvas, diffs through surface,
// and flushes changed runs through compositor.
bool TerminalApp_step(TerminalApp *self);

// Feeds raw input bytes into the internal input decoder.
bool TerminalApp_feedInput(TerminalApp *self, const char *data, bytes length);

// Runs the interactive event loop until stopped or interrupted.
int TerminalApp_run(TerminalApp *self);
fn TerminalApp_stop(TerminalApp *self);
bool TerminalApp_isRunning(const TerminalApp *self);

bool TerminalApp_setHeadless(TerminalApp *self, bool headless);
bool TerminalApp_isHeadless(const TerminalApp *self);

TerminalProjection TerminalApp_toString(const TerminalApp *self);
TerminalProjection TerminalApp_toStringStruct(const TerminalApp *self);

#endif
