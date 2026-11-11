#ifndef DETERMINANT_TERMINAL_CODE_EDITOR_H
#define DETERMINANT_TERMINAL_CODE_EDITOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ui/surface/terminal_canvas.h"
#include "ui/surface/terminal_projection.h"
#include "util/constructor.h"

// ui/code/terminal_code_editor.h — the canonical code editor area (CodeArea).
//
// Unlike TerminalInputLine (single-line input) and TerminalTextArea (multiline plain text),
// TerminalCodeEditor is code-specialized:
// - Line-indexed storage array (lines[], lineCount)
// - Left line-number gutter with dynamic digit width calculation
// - Auto-indentation on newline (matching leading spaces/tabs)
// - Line split on Enter, line join on Backspace at line start
// - 2D cursor (cursorLine, cursorCol) and viewport scrolling (scrollTop, scrollLeft)
// - Reverse-style cursor rendering and syntax pen support

typedef struct TerminalCodeEditor {
    char **lines;              // owned array of row strings
    bytes *lineLengths;       // byte count per line
    bytes *lineCapacities;    // allocated bytes per line
    bytes lineCount;          // active line count
    bytes lineCapacity;       // capacity of lines array
    bytes cursorLine;         // active line index (0-based)
    bytes cursorCol;          // active column offset (0-based)
    bytes scrollTop;          // first visible line in viewport
    bytes scrollLeft;         // horizontal column scroll offset
    bool showGutter;           // line-number gutter visibility
    TerminalPen gutterPen;     // line-number pen
    TerminalPen textPen;       // code text pen
    TerminalPen cursorPen;     // active caret cell pen
} TerminalCodeEditor;

TerminalCodeEditor *TerminalCodeEditor_0(void);
TerminalCodeEditor *TerminalCodeEditor_1(const char *initialText);
#define TerminalCodeEditor(...) CONSTRUCTOR_DISPATCH(TerminalCodeEditor, __VA_ARGS__)
#define TerminalCodeEditor_zero() TerminalCodeEditor_0()

void TerminalCodeEditor_free(TerminalCodeEditor *self);

bool TerminalCodeEditor_setText(TerminalCodeEditor *self, const char *text);
bytes TerminalCodeEditor_getLineCount(const TerminalCodeEditor *self);
const char *TerminalCodeEditor_getLine(const TerminalCodeEditor *self, bytes lineIndex);

bool TerminalCodeEditor_insertChar(TerminalCodeEditor *self, int character);
bool TerminalCodeEditor_insertText(TerminalCodeEditor *self, const char *text, bytes length);
bool TerminalCodeEditor_backspace(TerminalCodeEditor *self);
bool TerminalCodeEditor_delete(TerminalCodeEditor *self);
void TerminalCodeEditor_clear(TerminalCodeEditor *self);

void TerminalCodeEditor_moveCursor(TerminalCodeEditor *self, int deltaLine, int deltaCol);
bytes TerminalCodeEditor_getCursorLine(const TerminalCodeEditor *self);
bytes TerminalCodeEditor_getCursorCol(const TerminalCodeEditor *self);
bool TerminalCodeEditor_setCursor(TerminalCodeEditor *self, bytes line, bytes col);

bool TerminalCodeEditor_setShowGutter(TerminalCodeEditor *self, bool show);
bool TerminalCodeEditor_getShowGutter(const TerminalCodeEditor *self);
bytes TerminalCodeEditor_getGutterWidth(const TerminalCodeEditor *self);

void TerminalCodeEditor_render(const TerminalCodeEditor *self, TerminalCanvas *canvas);

TerminalProjection TerminalCodeEditor_toString(const TerminalCodeEditor *self);
TerminalProjection TerminalCodeEditor_toStringStruct(const TerminalCodeEditor *self);

#endif
