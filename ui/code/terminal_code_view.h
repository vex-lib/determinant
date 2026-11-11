#ifndef DETERMINANT_TERMINAL_CODE_VIEW_H
#define DETERMINANT_TERMINAL_CODE_VIEW_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ui/surface/terminal_canvas.h"
#include "ui/surface/terminal_projection.h"
#include "util/constructor.h"

// A code line is borrowed text plus caller-produced byte spans. Determinant
// owns cell clipping and painting only: it does not tokenize or infer meaning.
// Byte offsets always refer to the original UTF-8 source, never to virtual hints.

typedef struct TerminalCodeSpan {
    bytes startByte;
    bytes endByte;
    TerminalPen pen;
} TerminalCodeSpan;

typedef struct TerminalCodeHint {
    bytes byteOffset;
    const char *text;
    bytes length;
    TerminalPen pen;
} TerminalCodeHint;

typedef struct TerminalCodeView TerminalCodeView;

TerminalCodeView *TerminalCodeView_0(void);
#define TerminalCodeView(...) CONSTRUCTOR_DISPATCH(TerminalCodeView, __VA_ARGS__)
#define TerminalCodeView_zero() TerminalCodeView_0()

void TerminalCodeView_free(TerminalCodeView *self);

// Borrow one UTF-8 line and its ordered, non-overlapping byte spans and sorted
// virtual hints. A hint is painted immediately before its source byte offset;
// it never enters the source, editing model, or span coordinates. Invalid input
// rejects without replacing the current line. Null arrays are valid only at
// count zero. The caller keeps all borrowed values alive until the next update.
bool TerminalCodeView_setLine(TerminalCodeView *self, const char *text, bytes length,
                              const TerminalCodeSpan *spans, bytes spanCount,
                              const TerminalCodeHint *hints, bytes hintCount);
const char *TerminalCodeView_getText(const TerminalCodeView *self);
bytes TerminalCodeView_getLength(const TerminalCodeView *self);
const TerminalCodeSpan *TerminalCodeView_getSpans(const TerminalCodeView *self);
bytes TerminalCodeView_getSpanCount(const TerminalCodeView *self);
const TerminalCodeHint *TerminalCodeView_getHints(const TerminalCodeView *self);
bytes TerminalCodeView_getHintCount(const TerminalCodeView *self);

// Horizontal offset and clipping use terminal cells. Rendering is hot,
// allocation-free and quiet. x/y are local to the canvas.
bool TerminalCodeView_setScroll(TerminalCodeView *self, bytes cells);
bytes TerminalCodeView_getScroll(const TerminalCodeView *self);
void TerminalCodeView_render(const TerminalCodeView *self, TerminalCanvas *canvas,
                             bytes x, bytes y);

TerminalProjection TerminalCodeView_toString(const TerminalCodeView *self);
TerminalProjection TerminalCodeView_toStringStruct(const TerminalCodeView *self);

#endif
