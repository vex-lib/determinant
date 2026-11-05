#ifndef DETERMINANT_TERMINAL_CODE_EDITOR_H
#define DETERMINANT_TERMINAL_CODE_EDITOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ui/surface/terminal_canvas.h"
#include "ui/surface/terminal_projection.h"
#include "c23/constructor.h"

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
    size_t *lineLengths;       // byte count per line
    size_t *lineCapacities;    // allocated bytes per line
    size_t lineCount;          // active line count
    size_t lineCapacity;       // capacity of lines array
    size_t cursorLine;         // active line index (0-based)
    size_t cursorCol;          // active column offset (0-based)
    size_t scrollTop;          // first visible line in viewport
    size_t scrollLeft;         // horizontal column scroll offset
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
size_t TerminalCodeEditor_getLineCount(const TerminalCodeEditor *self);
const char *TerminalCodeEditor_getLine(const TerminalCodeEditor *self, size_t lineIndex);

bool TerminalCodeEditor_insertChar(TerminalCodeEditor *self, int character);
bool TerminalCodeEditor_insertText(TerminalCodeEditor *self, const char *text, size_t length);
bool TerminalCodeEditor_backspace(TerminalCodeEditor *self);
bool TerminalCodeEditor_delete(TerminalCodeEditor *self);
void TerminalCodeEditor_clear(TerminalCodeEditor *self);

void TerminalCodeEditor_moveCursor(TerminalCodeEditor *self, int deltaLine, int deltaCol);
size_t TerminalCodeEditor_getCursorLine(const TerminalCodeEditor *self);
size_t TerminalCodeEditor_getCursorCol(const TerminalCodeEditor *self);
bool TerminalCodeEditor_setCursor(TerminalCodeEditor *self, size_t line, size_t col);

bool TerminalCodeEditor_setShowGutter(TerminalCodeEditor *self, bool show);
bool TerminalCodeEditor_getShowGutter(const TerminalCodeEditor *self);
size_t TerminalCodeEditor_getGutterWidth(const TerminalCodeEditor *self);

void TerminalCodeEditor_render(const TerminalCodeEditor *self, TerminalCanvas *canvas);

TerminalProjection TerminalCodeEditor_toString(const TerminalCodeEditor *self);
TerminalProjection TerminalCodeEditor_toStringStruct(const TerminalCodeEditor *self);

#endif
