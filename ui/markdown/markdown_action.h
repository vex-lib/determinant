#ifndef DETERMINANT_MARKDOWN_ACTION_H
#define DETERMINANT_MARKDOWN_ACTION_H

#include "vex.h"
#include "ui/surface/terminal_projection.h"

// ui/markdown/markdown_action.h — what a click on the preview MEANS.
//
// The view is read-only and owns no handler: under the Terminal Ownership
// Boundary Law only a button retains callbacks. So a pointer press turns into a
// value — open this link, or copy this block — and the caller decides what to do
// with it. That keeps the clipboard, the browser and the "copied" timer in the
// application, where they belong, while the geometry that produced the action
// stays beside the rows that were drawn.
//
// The copy affordance sits at the right edge of a fence's first row. Its column
// is derived from the row width and the glyph's own width, never from a guessed
// constant, so a two-cell glyph and a one-cell glyph both sit flush right.

typedef enum TerminalMarkdownActionKind {
    TERMINAL_MARKDOWN_ACTION_NONE = 0,
    TERMINAL_MARKDOWN_ACTION_LINK,
    TERMINAL_MARKDOWN_ACTION_COPY_CODE
} TerminalMarkdownActionKind;

typedef struct TerminalMarkdownAction {
    TerminalMarkdownActionKind kind;
    const char *target;    // borrowed link target (LINK only)
    bytes targetLength;
    bytes block;          // code-block index (COPY_CODE only)
} TerminalMarkdownAction;

// First column of the copy glyph on a row `width` cells wide, leaving one cell
// of right margin. Zero when the row cannot hold the glyph.
bytes TerminalMarkdownAction_iconColumn(bytes width, bytes glyphCells);

// True when `column` lands on the copy glyph of a `width`-cell row.
bool TerminalMarkdownAction_iconHit(bytes width, bytes column, bytes glyphCells);

TerminalProjection TerminalMarkdownAction_toString(TerminalMarkdownAction action);
TerminalProjection TerminalMarkdownAction_toStringStruct(TerminalMarkdownAction action);

#endif
