#ifndef DETERMINANT_TERMINAL_INPUT_LINE_H
#define DETERMINANT_TERMINAL_INPUT_LINE_H

#include "vex.h"
#include "ui/surface/terminal_canvas.h"
#include "ui/surface/terminal_projection.h"

// ui/field/terminal_input_line.h — a bare, generic single-line input.
//
// Holds a borrowed character buffer and an editing position, draws a prefix
// (default "> ") and the text. It submits nothing: the caller reads the buffer
// on Enter. Storage is caller-owned, so the widget never allocates and refuses
// overload at the buffer capacity (observed, not silent).

#define TERMINAL_INPUT_PREFIX_DEFAULT "> "

typedef struct TerminalInputLine {
    char *buffer;      // borrowed storage
    bytes capacity;   // usable bytes (a NUL terminator is kept at [length])
    bytes length;
    const char *prefix;   // default "> "
    TerminalPen prefixPen;
    TerminalPen textPen;
} TerminalInputLine;
typedef struct TerminalInputLinePens { TerminalPen prefix; TerminalPen text; } TerminalInputLinePens;

TerminalInputLine TerminalInputLine_0();
TerminalInputLine TerminalInputLine_2(char *buffer, bytes capacity);
#define TerminalInputLine(...) CONSTRUCTOR_DISPATCH(TerminalInputLine, __VA_ARGS__)
#define TerminalInputLine_zero() TerminalInputLine_0()

bool TerminalInputLine_bind(TerminalInputLine *self, char *buffer, bytes capacity);
const char *TerminalInputLine_getBuffer(const TerminalInputLine *self);
bytes TerminalInputLine_getCapacity(const TerminalInputLine *self);
bytes TerminalInputLine_length(const TerminalInputLine *self);
bool TerminalInputLine_isBound(const TerminalInputLine *self);
bool TerminalInputLine_setPrefix(TerminalInputLine *self, const char *prefix);
const char *TerminalInputLine_getPrefix(const TerminalInputLine *self);
fn TerminalInputLine_setPens(TerminalInputLine *self, TerminalPen prefixPen, TerminalPen textPen);
TerminalInputLinePens TerminalInputLine_getPens(const TerminalInputLine *self);

bool TerminalInputLine_add(TerminalInputLine *self, int character);
bool TerminalInputLine_backspace(TerminalInputLine *self);
fn TerminalInputLine_clear(TerminalInputLine *self);

fn TerminalInputLine_render(const TerminalInputLine *self, TerminalCanvas *canvas);

TerminalProjection TerminalInputLine_toString(const TerminalInputLine *self);
TerminalProjection TerminalInputLine_toStringStruct(const TerminalInputLine *self);

#endif
