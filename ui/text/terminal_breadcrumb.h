#ifndef DETERMINANT_TERMINAL_BREADCRUMB_H
#define DETERMINANT_TERMINAL_BREADCRUMB_H

#include "vex.h"
#include "ui/surface/terminal_canvas.h"
#include "ui/surface/terminal_projection.h"
#include "format/terminal_style.h"

#define TERMINAL_BREADCRUMB_SEPARATOR_DEFAULT " › "

// ui/text/terminal_breadcrumb.h — bounded hierarchical navigation breadcrumb.
//
// Formats and renders a hierarchical breadcrumb trail from an optional root directory
// and a target path. It preserves the boundary context (root and destination file/parent),
// collapsing deep intermediate directory levels into an ellipsis ("…") while
// respecting a horizontal cell budget.
//
// The default arrow separator is " › " (U+203A single right-pointing angle quotation mark).

typedef struct TerminalBreadcrumb {
    const char *root;         // borrowed root directory or project name (nullable)
    const char *path;         // borrowed target file path
    const char *separator;    // borrowed separator string (default: " › ")
    bytes maxCells;          // maximum allowed horizontal cell width (0 = unconstrained)
    TerminalPen pen;          // rendering pen
} TerminalBreadcrumb;

TerminalBreadcrumb TerminalBreadcrumb_0();
TerminalBreadcrumb TerminalBreadcrumb_1(const char *path);
TerminalBreadcrumb TerminalBreadcrumb_2(const char *root, const char *path);
TerminalBreadcrumb TerminalBreadcrumb_3(const char *root, const char *path, const char *separator);
TerminalBreadcrumb TerminalBreadcrumb_4(const char *root, const char *path, const char *separator, bytes maxCells);
#define TerminalBreadcrumb(...) CONSTRUCTOR_DISPATCH(TerminalBreadcrumb, __VA_ARGS__)
#define TerminalBreadcrumb_zero() TerminalBreadcrumb_0()

bool TerminalBreadcrumb_setPath(TerminalBreadcrumb *self, const char *root, const char *path);
bool TerminalBreadcrumb_setRoot(TerminalBreadcrumb *self, const char *root);
bool TerminalBreadcrumb_setSeparator(TerminalBreadcrumb *self, const char *separator);
bool TerminalBreadcrumb_setMaxCells(TerminalBreadcrumb *self, bytes maxCells);
fn TerminalBreadcrumb_setPen(TerminalBreadcrumb *self, TerminalPen pen);

const char *TerminalBreadcrumb_getRoot(const TerminalBreadcrumb *self);
const char *TerminalBreadcrumb_getPath(const TerminalBreadcrumb *self);
const char *TerminalBreadcrumb_getSeparator(const TerminalBreadcrumb *self);
bytes TerminalBreadcrumb_getMaxCells(const TerminalBreadcrumb *self);
TerminalPen TerminalBreadcrumb_getPen(const TerminalBreadcrumb *self);

// Formats the breadcrumb string into destinationBuffer. Returns the byte length written.
bytes TerminalBreadcrumb_format(const TerminalBreadcrumb *self, char *destinationBuffer, bytes destinationCapacity);

// Measures the display cell width of the formatted breadcrumb.
bytes TerminalBreadcrumb_measure(const TerminalBreadcrumb *self);

// Renders the breadcrumb onto the canvas at (xCoordinate, yCoordinate).
fn TerminalBreadcrumb_render(const TerminalBreadcrumb *self, TerminalCanvas *canvas,
                               bytes xCoordinate, bytes yCoordinate);

TerminalProjection TerminalBreadcrumb_toString(const TerminalBreadcrumb *self);
TerminalProjection TerminalBreadcrumb_toStringStruct(const TerminalBreadcrumb *self);

#endif
