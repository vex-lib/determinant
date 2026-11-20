#ifndef DETERMINANT_TERMINAL_DIFF_PANEL_H
#define DETERMINANT_TERMINAL_DIFF_PANEL_H

#include "vex.h"
#include <stdio.h>
#include "format/terminal_color.h"
#include "format/terminal_style.h"
#include "ui/surface/terminal_canvas.h"

// ui/panel/terminal_diff_panel.h — responsive scrollable terminal diff printer.
//
// Renders structured line + inline word/letter diffs as a scrollable ANSI/VT
// log stream. Automatically switches between side-by-side and unified (one-line)
// layout based on the active terminal width.

typedef enum TerminalDiffLayoutMode {
    TERMINAL_DIFF_LAYOUT_AUTO = 0,
    TERMINAL_DIFF_LAYOUT_UNIFIED = 1,
    TERMINAL_DIFF_LAYOUT_SIDE_BY_SIDE = 2
} TerminalDiffLayoutMode;

typedef enum TerminalDiffLineKind {
    TERMINAL_DIFF_CONTEXT = 0,
    TERMINAL_DIFF_ADDED = 1,
    TERMINAL_DIFF_REMOVED = 2
} TerminalDiffLineKind;

typedef struct TerminalDiffSpan {
    bytes start;
    bytes length;
} TerminalDiffSpan;

typedef struct TerminalDiffLine {
    TerminalDiffLineKind kind;
    u32 oldLine;
    u32 newLine;
    const char *text;
    bytes length;
    const TerminalDiffSpan *spans;
    bytes spanCount;
} TerminalDiffLine;

typedef struct TerminalDiffHunk {
    u32 oldStart;
    u32 oldCount;
    u32 newStart;
    u32 newCount;
    const TerminalDiffLine *lines;
    bytes lineCount;
} TerminalDiffHunk;

typedef struct TerminalDiffMeta {
    const char *path;
    const char *eventLabel;
    const char *timestamp;
    u32 linesAdded;
    u32 linesRemoved;
} TerminalDiffMeta;

typedef struct TerminalDiffTheme {
    u32 borderColor;
    u32 headerFg;
    u32 headerBg;
    u32 metaFg;
    u32 hunkFg;
    u32 gutterFg;
    u32 addFg;
    u32 addBg;
    u32 addSpanFg;
    u32 addSpanBg;
    u32 delFg;
    u32 delBg;
    u32 delSpanFg;
    u32 delSpanBg;
    u32 contextFg;
    bool useColor;
    bool useUnicodeBorder;
    bool showLineNumbers;
    bool showHeader;
    bool showDiffIcons;
} TerminalDiffTheme;

typedef struct TerminalDiffPanel TerminalDiffPanel;

typedef fn (*TerminalDiffHeaderFn)(const TerminalDiffPanel *panel,
                                   const TerminalDiffMeta *meta,
                                   u32 columns,
                                   TerminalDiffLayoutMode resolvedLayout,
                                   FILE *out,
                                   any userdata);

struct TerminalDiffPanel {
    u32 columns;
    u32 sideBySideMinCols;
    bytes scrollX;
    TerminalDiffLayoutMode layoutMode;
    TerminalDiffTheme theme;
    TerminalDiffHeaderFn headerHook;
    any headerUserdata;
};

u32 TerminalDiffPanel_detectColumns();
TerminalDiffTheme TerminalDiffPanel_defaultTheme();
fn TerminalDiffPanel_init(TerminalDiffPanel *panel);

fn TerminalDiffPanel_setColumns(TerminalDiffPanel *panel, u32 columns);
fn TerminalDiffPanel_setSideBySideMinCols(TerminalDiffPanel *panel, u32 minCols);
fn TerminalDiffPanel_setScrollX(TerminalDiffPanel *panel, bytes scrollX);
bytes TerminalDiffPanel_scrollX(const TerminalDiffPanel *panel);
fn TerminalDiffPanel_setLayoutMode(TerminalDiffPanel *panel, TerminalDiffLayoutMode mode);
TerminalDiffLayoutMode TerminalDiffPanel_layoutMode(const TerminalDiffPanel *panel);
fn TerminalDiffPanel_setTheme(TerminalDiffPanel *panel, TerminalDiffTheme theme);
TerminalDiffTheme TerminalDiffPanel_theme(const TerminalDiffPanel *panel);
fn TerminalDiffPanel_setHeaderHook(TerminalDiffPanel *panel, TerminalDiffHeaderFn hook, any userdata);

TerminalDiffLayoutMode TerminalDiffPanel_resolveLayout(const TerminalDiffPanel *panel, u32 *outColumns);

bool TerminalDiffPanel_render(const TerminalDiffPanel *panel,
                              const TerminalDiffMeta *meta,
                              const TerminalDiffHunk *hunks,
                              bytes hunkCount,
                              FILE *out);

bytes TerminalDiffPanel_measureRows(const TerminalDiffPanel *panel,
                                    const TerminalDiffHunk *hunks,
                                    bytes hunkCount,
                                    u32 columns);

bytes TerminalDiffPanel_paintCanvas(const TerminalDiffPanel *panel,
                                    const TerminalDiffMeta *meta,
                                    const TerminalDiffHunk *hunks,
                                    bytes hunkCount,
                                    TerminalCanvas *canvas,
                                    bytes scrollOffset);

#endif
