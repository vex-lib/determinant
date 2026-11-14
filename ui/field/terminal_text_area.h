#ifndef DETERMINANT_TERMINAL_TEXT_AREA_H
#define DETERMINANT_TERMINAL_TEXT_AREA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ui/surface/terminal_canvas.h"
#include "ui/surface/terminal_projection.h"
#include "util/constructor.h"

// ui/field/terminal_text_area.h — a multiline arbitrary text editor.
//
// Unlike TerminalInputLine (which is single-line with a prefix), TerminalTextArea
// supports multiline free-form text input (\n enters newlines), 2D cursor movement,
// vertical/horizontal scrolling, and optional soft word-wrapping.
//
// It is designed for commit messages, notes, and chat compose inputs. It does not
// have line numbers or syntax coloring (which belong to TerminalCodeEditor).

typedef struct TerminalTextArea {
    char *buffer;              // borrowed or allocated storage (NUL-terminated at [length])
    bytes capacity;           // buffer capacity in bytes
    bytes length;             // text length in bytes
    bytes cursorByte;         // caret byte offset in buffer
    bytes scrollRow;          // vertical viewport scroll offset
    bytes scrollCol;          // horizontal viewport scroll offset
    bool wrap;                 // soft-wrapping enabled
    TerminalPen pen;           // text rendering pen
    TerminalPen cursorPen;     // active cursor cell pen
} TerminalTextArea;

TerminalTextArea *TerminalTextArea_0();
TerminalTextArea *TerminalTextArea_2(char *buffer, bytes capacity);
#define TerminalTextArea(...) CONSTRUCTOR_DISPATCH(TerminalTextArea, __VA_ARGS__)
#define TerminalTextArea_zero() TerminalTextArea_0()

fn TerminalTextArea_free(TerminalTextArea *self);

bool TerminalTextArea_bind(TerminalTextArea *self, char *buffer, bytes capacity);
const char *TerminalTextArea_getBuffer(const TerminalTextArea *self);
bytes TerminalTextArea_getCapacity(const TerminalTextArea *self);
bytes TerminalTextArea_getLength(const TerminalTextArea *self);

bool TerminalTextArea_insertChar(TerminalTextArea *self, int character);
bool TerminalTextArea_insertText(TerminalTextArea *self, const char *text, bytes length);
bool TerminalTextArea_backspace(TerminalTextArea *self);
bool TerminalTextArea_delete(TerminalTextArea *self);
fn TerminalTextArea_clear(TerminalTextArea *self);

fn TerminalTextArea_moveCursor(TerminalTextArea *self, int deltaRow, int deltaCol);
bytes TerminalTextArea_getCursorByte(const TerminalTextArea *self);
bool TerminalTextArea_setCursorByte(TerminalTextArea *self, bytes byteOffset);

bool TerminalTextArea_setWrap(TerminalTextArea *self, bool wrap);
bool TerminalTextArea_getWrap(const TerminalTextArea *self);

bool TerminalTextArea_setPen(TerminalTextArea *self, TerminalPen pen);
TerminalPen TerminalTextArea_getPen(const TerminalTextArea *self);

fn TerminalTextArea_render(const TerminalTextArea *self, TerminalCanvas *canvas);

TerminalProjection TerminalTextArea_toString(const TerminalTextArea *self);
TerminalProjection TerminalTextArea_toStringStruct(const TerminalTextArea *self);

#endif
