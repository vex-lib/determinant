#ifndef DETERMINANT_TERMINAL_TEXT_AREA_H
#define DETERMINANT_TERMINAL_TEXT_AREA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ui/surface/terminal_canvas.h"
#include "ui/surface/terminal_projection.h"
#include "c23/constructor.h"

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
    size_t capacity;           // buffer capacity in bytes
    size_t length;             // text length in bytes
    size_t cursorByte;         // caret byte offset in buffer
    size_t scrollRow;          // vertical viewport scroll offset
    size_t scrollCol;          // horizontal viewport scroll offset
    bool wrap;                 // soft-wrapping enabled
    TerminalPen pen;           // text rendering pen
    TerminalPen cursorPen;     // active cursor cell pen
} TerminalTextArea;

TerminalTextArea *TerminalTextArea_0(void);
TerminalTextArea *TerminalTextArea_2(char *buffer, size_t capacity);
#define TerminalTextArea(...) CONSTRUCTOR_DISPATCH(TerminalTextArea, __VA_ARGS__)
#define TerminalTextArea_zero() TerminalTextArea_0()

void TerminalTextArea_free(TerminalTextArea *self);

bool TerminalTextArea_bind(TerminalTextArea *self, char *buffer, size_t capacity);
const char *TerminalTextArea_getBuffer(const TerminalTextArea *self);
size_t TerminalTextArea_getCapacity(const TerminalTextArea *self);
size_t TerminalTextArea_getLength(const TerminalTextArea *self);

bool TerminalTextArea_insertChar(TerminalTextArea *self, int character);
bool TerminalTextArea_insertText(TerminalTextArea *self, const char *text, size_t length);
bool TerminalTextArea_backspace(TerminalTextArea *self);
bool TerminalTextArea_delete(TerminalTextArea *self);
void TerminalTextArea_clear(TerminalTextArea *self);

void TerminalTextArea_moveCursor(TerminalTextArea *self, int deltaRow, int deltaCol);
size_t TerminalTextArea_getCursorByte(const TerminalTextArea *self);
bool TerminalTextArea_setCursorByte(TerminalTextArea *self, size_t byteOffset);

bool TerminalTextArea_setWrap(TerminalTextArea *self, bool wrap);
bool TerminalTextArea_getWrap(const TerminalTextArea *self);

bool TerminalTextArea_setPen(TerminalTextArea *self, TerminalPen pen);
TerminalPen TerminalTextArea_getPen(const TerminalTextArea *self);

void TerminalTextArea_render(const TerminalTextArea *self, TerminalCanvas *canvas);

TerminalProjection TerminalTextArea_toString(const TerminalTextArea *self);
TerminalProjection TerminalTextArea_toStringStruct(const TerminalTextArea *self);

#endif
