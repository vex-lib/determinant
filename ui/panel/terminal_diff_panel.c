#include "vex.h"
#include "ui/panel/terminal_diff_panel.h"

#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: TerminalDiffPanel
 * ============================================================================
 * Responsive scrollable terminal diff printer in determinant. Switches
 * automatically between side-by-side and unified (one-line) rendering based on
 * terminal column width, highlighting changed lines and inline word/letter
 * spans using packed 0xRRGGBBAA TerminalColor palettes.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: TerminalDiffPanel (ui/panel/terminal_diff_panel.c)
 * ============================================================================
 * Zero heap allocation on the render hot path: streams ANSI/VT SGR sequences
 * directly to a caller-supplied FILE* sink so live file-watch diffs append
 * cleanly to terminal scrollback.
 * ============================================================================
 */

;;GETTER
static inline u32 DiffPanel_columns(const TerminalDiffPanel *p) { return (*p).columns; }
;;SETTER
static inline fn DiffPanel_setColumnsRaw(TerminalDiffPanel *p, u32 v) { (*p).columns = v; }
;;GETTER
static inline u32 DiffPanel_minCols(const TerminalDiffPanel *p) { return (*p).sideBySideMinCols; }
;;SETTER
static inline fn DiffPanel_setMinColsRaw(TerminalDiffPanel *p, u32 v) { (*p).sideBySideMinCols = v; }
;;GETTER
static inline TerminalDiffLayoutMode DiffPanel_mode(const TerminalDiffPanel *p) { return (*p).layoutMode; }
;;SETTER
static inline fn DiffPanel_setModeRaw(TerminalDiffPanel *p, TerminalDiffLayoutMode m) { (*p).layoutMode = m; }
;;GETTER
static inline TerminalDiffTheme DiffPanel_theme(const TerminalDiffPanel *p) { return (*p).theme; }
;;SETTER
static inline fn DiffPanel_setThemeRaw(TerminalDiffPanel *p, TerminalDiffTheme t) { (*p).theme = t; }
;;GETTER
static inline TerminalDiffHeaderFn DiffPanel_headerHook(const TerminalDiffPanel *p) { return (*p).headerHook; }
;;SETTER
static inline fn DiffPanel_setHeaderHookRaw(TerminalDiffPanel *p, TerminalDiffHeaderFn h) { (*p).headerHook = h; }
;;GETTER
static inline any DiffPanel_headerUserdata(const TerminalDiffPanel *p) { return (*p).headerUserdata; }
;;SETTER
static inline fn DiffPanel_setHeaderUserdataRaw(TerminalDiffPanel *p, any ud) { (*p).headerUserdata = ud; }
;;GETTER
static inline bytes DiffPanel_scrollX(const TerminalDiffPanel *p) { return (*p).scrollX; }
;;SETTER
static inline fn DiffPanel_setScrollXRaw(TerminalDiffPanel *p, bytes v) { (*p).scrollX = v; }

;;GETTER
static inline const char *DiffMeta_path(const TerminalDiffMeta *m) { return (*m).path; }
;;GETTER
static inline const char *DiffMeta_eventLabel(const TerminalDiffMeta *m) { return (*m).eventLabel; }
;;GETTER
static inline const char *DiffMeta_timestamp(const TerminalDiffMeta *m) { return (*m).timestamp; }
;;GETTER
static inline u32 DiffMeta_added(const TerminalDiffMeta *m) { return (*m).linesAdded; }
;;GETTER
static inline u32 DiffMeta_removed(const TerminalDiffMeta *m) { return (*m).linesRemoved; }

;;GETTER
static inline u32 DiffHunk_oldStart(const TerminalDiffHunk *h) { return (*h).oldStart; }
;;GETTER
static inline u32 DiffHunk_oldCount(const TerminalDiffHunk *h) { return (*h).oldCount; }
;;GETTER
static inline u32 DiffHunk_newStart(const TerminalDiffHunk *h) { return (*h).newStart; }
;;GETTER
static inline u32 DiffHunk_newCount(const TerminalDiffHunk *h) { return (*h).newCount; }
;;GETTER
static inline const TerminalDiffLine *DiffHunk_lines(const TerminalDiffHunk *h) { return (*h).lines; }
;;GETTER
static inline bytes DiffHunk_lineCount(const TerminalDiffHunk *h) { return (*h).lineCount; }

;;GETTER
static inline TerminalDiffLineKind DiffLine_kind(const TerminalDiffLine *l) { return (*l).kind; }
;;GETTER
static inline u32 DiffLine_oldLine(const TerminalDiffLine *l) { return (*l).oldLine; }
;;GETTER
static inline u32 DiffLine_newLine(const TerminalDiffLine *l) { return (*l).newLine; }
;;GETTER
static inline const char *DiffLine_text(const TerminalDiffLine *l) { return (*l).text; }
;;GETTER
static inline bytes DiffLine_length(const TerminalDiffLine *l) { return (*l).length; }
;;GETTER
static inline const TerminalDiffSpan *DiffLine_spans(const TerminalDiffLine *l) { return (*l).spans; }
;;GETTER
static inline bytes DiffLine_spanCount(const TerminalDiffLine *l) { return (*l).spanCount; }

/** Emits a 24-bit foreground SGR escape when color is enabled and set. */
static fn emitFg(FILE *out, bool useColor, u32 color) {
    if (!useColor || !TERMINAL_COLOR_IS_SET(color))
        return;
    fprintf(out, "\x1b[38;2;%u;%u;%um",
            (unsigned) TERMINAL_COLOR_RED(color),
            (unsigned) TERMINAL_COLOR_GREEN(color),
            (unsigned) TERMINAL_COLOR_BLUE(color));
}

/** Emits a 24-bit background SGR escape when color is enabled and set. */
static fn emitBg(FILE *out, bool useColor, u32 color) {
    if (!useColor || !TERMINAL_COLOR_IS_SET(color))
        return;
    fprintf(out, "\x1b[48;2;%u;%u;%um",
            (unsigned) TERMINAL_COLOR_RED(color),
            (unsigned) TERMINAL_COLOR_GREEN(color),
            (unsigned) TERMINAL_COLOR_BLUE(color));
}

/** Emits SGR bold attribute when color is enabled. */
static fn emitBold(FILE *out, bool useColor) {
    if (useColor)
        fputs("\x1b[1m", out);
}

/** Resets active SGR attributes when color is enabled. */
static fn emitReset(FILE *out, bool useColor) {
    if (useColor)
        fputs("\x1b[0m", out);
}

/** Emits a horizontal rule repeated count times using Unicode or ASCII glyphs. */
static fn emitRule(FILE *out, u32 count, bool unicode) {
    const char *ch = unicode ? "─" : "-";
    for (u32 i = 0; i < count; i++)
        fputs(ch, out);
}

/** Returns true if byte offset pos falls inside any highlighted inline span. */
static bool posInSpans(bytes pos, const TerminalDiffSpan *spans, bytes spanCount) {
    for (bytes i = 0; i < spanCount; i++) {
        bytes start = spans[i].start;
        bytes end = start + spans[i].length;
        if (pos >= start && pos < end)
            return true;
    }
    return false;
}

/** Resolves effective text length, stripping trailing CR/LF so single rows stay clean. */
static bytes cleanLineLen(const char *text, bytes len) {
    if (!text)
        return 0;
    if (len == 0) {
        while (text[len] != '\0' && text[len] != '\n' && text[len] != '\r')
            len++;
        return len;
    }
    while (len > 0 && (text[len - 1u] == '\n' || text[len - 1u] == '\r'))
        len--;
    return len;
}

/** Renders line text with inline word/letter span highlights and optional column clamping/padding. */
static fn emitStyledText(FILE *out,
                         const TerminalDiffLine *line,
                         u32 maxCols,
                         bool padToWidth,
                         u32 baseFg,
                         u32 baseBg,
                         u32 spanFg,
                         u32 spanBg,
                         bool useColor) {
    const char *text = line ? DiffLine_text(line) : nullptr;
    bytes len = line ? cleanLineLen(text, DiffLine_length(line)) : 0;
    const TerminalDiffSpan *spans = line ? DiffLine_spans(line) : nullptr;
    bytes spanCount = line ? DiffLine_spanCount(line) : 0;

    emitBg(out, useColor, baseBg);
    emitFg(out, useColor, baseFg);

    bool inSpan = false;
    u32 written = 0;
    for (bytes i = 0; i < len; i++) {
        if (maxCols > 0 && written >= maxCols)
            break;
        bool wantSpan = spanCount > 0 && posInSpans(i, spans, spanCount);
        if (wantSpan != inSpan) {
            emitReset(out, useColor);
            if (wantSpan) {
                emitBg(out, useColor, spanBg);
                emitFg(out, useColor, spanFg);
                emitBold(out, useColor);
            } else {
                emitBg(out, useColor, baseBg);
                emitFg(out, useColor, baseFg);
            }
            inSpan = wantSpan;
        }
        unsigned char ch = (unsigned char) text[i];
        if (ch == '\t') {
            u32 tabSpaces = 4u - (written % 4u);
            for (u32 s = 0; s < tabSpaces; s++) {
                if (maxCols > 0 && written >= maxCols)
                    break;
                fputc(' ', out);
                written++;
            }
        } else if (ch < 0x20u || ch == 0x7Fu) {
            fputc(' ', out);
            written++;
        } else {
            fputc((i32) ch, out);
            written++;
        }
    }

    if (inSpan) {
        emitReset(out, useColor);
        emitBg(out, useColor, baseBg);
        emitFg(out, useColor, baseFg);
    }

    if (padToWidth && maxCols > written)
        for (u32 p = written; p < maxCols; p++)
            fputc(' ', out);

    emitReset(out, useColor);
}

/** Emits the default header banner showing event badge, path, timestamp, and +N -M stats. */
static fn emitDefaultHeader(const TerminalDiffPanel *panel,
                            const TerminalDiffMeta *meta,
                            u32 columns,
                            TerminalDiffLayoutMode layout,
                            FILE *out) {
    TerminalDiffTheme th = DiffPanel_theme(panel);
    const char *path = (meta && DiffMeta_path(meta)) ? DiffMeta_path(meta) : "(unnamed)";
    const char *ev = (meta && DiffMeta_eventLabel(meta)) ? DiffMeta_eventLabel(meta) : "DIFF";
    const char *ts = (meta && DiffMeta_timestamp(meta)) ? DiffMeta_timestamp(meta) : "";
    u32 added = meta ? DiffMeta_added(meta) : 0u;
    u32 removed = meta ? DiffMeta_removed(meta) : 0u;
    const char *modeTag = (layout == TERMINAL_DIFF_LAYOUT_SIDE_BY_SIDE) ? "split" : "unified";

    emitFg(out, th.useColor, th.borderColor);
    emitRule(out, columns, th.useUnicodeBorder);
    emitReset(out, th.useColor);
    fputc('\n', out);

    emitBg(out, th.useColor, th.headerBg);
    emitFg(out, th.useColor, th.headerFg);
    emitBold(out, th.useColor);
    fprintf(out, " [%s] %s ", ev, path);
    emitReset(out, th.useColor);

    emitFg(out, th.useColor, th.addFg);
    fprintf(out, " +%u", (unsigned) added);
    emitFg(out, th.useColor, th.delFg);
    fprintf(out, " -%u", (unsigned) removed);
    emitFg(out, th.useColor, th.metaFg);
    if (ts[0] != '\0')
        fprintf(out, "  %s", ts);
    fprintf(out, "  (%s %uc)", modeTag, (unsigned) columns);
    emitReset(out, th.useColor);
    fputc('\n', out);

    emitFg(out, th.useColor, th.borderColor);
    emitRule(out, columns, th.useUnicodeBorder);
    emitReset(out, th.useColor);
    fputc('\n', out);
}

/** Renders one hunk in unified single-column layout. */
static fn renderHunkUnified(const TerminalDiffPanel *panel,
                            const TerminalDiffHunk *hunk,
                            u32 columns,
                            FILE *out) {
    TerminalDiffTheme th = DiffPanel_theme(panel);
    const char *sep = th.useUnicodeBorder ? "│" : "|";

    emitFg(out, th.useColor, th.hunkFg);
    fprintf(out, "@@ -%u,%u +%u,%u @@",
            (unsigned) DiffHunk_oldStart(hunk),
            (unsigned) DiffHunk_oldCount(hunk),
            (unsigned) DiffHunk_newStart(hunk),
            (unsigned) DiffHunk_newCount(hunk));
    emitReset(out, th.useColor);
    fputc('\n', out);

    const TerminalDiffLine *lines = DiffHunk_lines(hunk);
    bytes count = DiffHunk_lineCount(hunk);
    bool showMarker = th.showDiffIcons || !th.useColor;
    u32 gutterCols = (showMarker ? 2u : 0u) + (th.showLineNumbers ? 12u : 2u);
    u32 textCols = columns > gutterCols ? columns - gutterCols : 20u;

    for (bytes i = 0; i < count; i++) {
        const TerminalDiffLine *line = &lines[i];
        TerminalDiffLineKind kind = DiffLine_kind(line);
        char marker = ' ';
        u32 fg = th.contextFg;
        u32 bg = TERMINAL_COLOR_DEFAULT;
        u32 sfg = th.contextFg;
        u32 sbg = TERMINAL_COLOR_DEFAULT;

        if (kind == TERMINAL_DIFF_ADDED) {
            marker = '+';
            fg = th.addFg;
            bg = th.addBg;
            sfg = th.addSpanFg;
            sbg = th.addSpanBg;
        } else if (kind == TERMINAL_DIFF_REMOVED) {
            marker = '-';
            fg = th.delFg;
            bg = th.delBg;
            sfg = th.delSpanFg;
            sbg = th.delSpanBg;
        }

        if (showMarker) {
            emitFg(out, th.useColor, fg);
            emitBold(out, th.useColor);
            fprintf(out, "%c ", marker);
            emitReset(out, th.useColor);
        }

        if (th.showLineNumbers) {
            emitFg(out, th.useColor, th.gutterFg);
            if (DiffLine_oldLine(line) > 0)
                fprintf(out, "%4u ", (unsigned) DiffLine_oldLine(line));
            else
                fputs("     ", out);
            if (DiffLine_newLine(line) > 0)
                fprintf(out, "%4u ", (unsigned) DiffLine_newLine(line));
            else
                fputs("     ", out);
            fputs(sep, out);
            emitReset(out, th.useColor);
        }

        emitBg(out, th.useColor, bg);
        fputc(' ', out);
        emitReset(out, th.useColor);

        emitStyledText(out, line, textCols, false, fg, bg, sfg, sbg, th.useColor);
        fputc('\n', out);
    }
}

/** Emits one half-cell (old or new side) for a side-by-side diff row. */
static fn emitSplitCell(FILE *out,
                        const TerminalDiffLine *line,
                        bool isRight,
                        u32 halfCols,
                        TerminalDiffTheme th) {
    bool showMarker = th.showDiffIcons || !th.useColor;
    const char *sep = th.useUnicodeBorder ? "│" : "|";
    u32 prefixCols = (showMarker ? 2u : 0u) + (th.showLineNumbers ? 7u : 1u);
    u32 textCols = halfCols > prefixCols ? halfCols - prefixCols : 4u;

    if (!line) {
        emitFg(out, th.useColor, th.gutterFg);
        for (u32 i = 0; i < halfCols; i++)
            fputc(' ', out);
        emitReset(out, th.useColor);
        return;
    }

    TerminalDiffLineKind kind = DiffLine_kind(line);
    u32 lineNo = isRight ? DiffLine_newLine(line) : DiffLine_oldLine(line);
    char marker = ' ';
    u32 fg = th.contextFg;
    u32 bg = TERMINAL_COLOR_DEFAULT;
    u32 sfg = th.contextFg;
    u32 sbg = TERMINAL_COLOR_DEFAULT;

    if (kind == TERMINAL_DIFF_ADDED) {
        marker = '+';
        fg = th.addFg;
        bg = th.addBg;
        sfg = th.addSpanFg;
        sbg = th.addSpanBg;
    } else if (kind == TERMINAL_DIFF_REMOVED) {
        marker = '-';
        fg = th.delFg;
        bg = th.delBg;
        sfg = th.delSpanFg;
        sbg = th.delSpanBg;
    }

    if (showMarker) {
        emitFg(out, th.useColor, fg);
        emitBold(out, th.useColor);
        fprintf(out, "%c ", marker);
        emitReset(out, th.useColor);
    }

    if (th.showLineNumbers) {
        emitFg(out, th.useColor, th.gutterFg);
        if (lineNo > 0)
            fprintf(out, "%4u %s", (unsigned) lineNo, sep);
        else
            fprintf(out, "     %s", sep);
        emitReset(out, th.useColor);
    }

    emitBg(out, th.useColor, bg);
    fputc(' ', out);
    emitReset(out, th.useColor);

    emitStyledText(out, line, textCols, true, fg, bg, sfg, sbg, th.useColor);
}

/** Renders one hunk in responsive two-column side-by-side layout. */
static fn renderHunkSideBySide(const TerminalDiffPanel *panel,
                               const TerminalDiffHunk *hunk,
                               u32 columns,
                               FILE *out) {
    TerminalDiffTheme th = DiffPanel_theme(panel);
    const char *div = th.useUnicodeBorder ? " │ " : " | ";
    u32 halfCols = columns > 3u ? (columns - 3u) / 2u : 40u;

    emitFg(out, th.useColor, th.hunkFg);
    fprintf(out, "@@ -%u,%u +%u,%u @@",
            (unsigned) DiffHunk_oldStart(hunk),
            (unsigned) DiffHunk_oldCount(hunk),
            (unsigned) DiffHunk_newStart(hunk),
            (unsigned) DiffHunk_newCount(hunk));
    emitReset(out, th.useColor);
    fputc('\n', out);

    const TerminalDiffLine *lines = DiffHunk_lines(hunk);
    bytes count = DiffHunk_lineCount(hunk);
    bytes i = 0;

    while (i < count) {
        if (DiffLine_kind(&lines[i]) == TERMINAL_DIFF_CONTEXT) {
            emitSplitCell(out, &lines[i], false, halfCols, th);
            emitFg(out, th.useColor, th.borderColor);
            fputs(div, out);
            emitReset(out, th.useColor);
            emitSplitCell(out, &lines[i], true, halfCols, th);
            fputc('\n', out);
            i++;
            continue;
        }

        bytes delStart = i;
        while (i < count && DiffLine_kind(&lines[i]) == TERMINAL_DIFF_REMOVED)
            i++;
        bytes delCount = i - delStart;

        bytes addStart = i;
        while (i < count && DiffLine_kind(&lines[i]) == TERMINAL_DIFF_ADDED)
            i++;
        bytes addCount = i - addStart;

        bytes rows = delCount > addCount ? delCount : addCount;
        for (bytes r = 0; r < rows; r++) {
            const TerminalDiffLine *left = r < delCount ? &lines[delStart + r] : nullptr;
            const TerminalDiffLine *right = r < addCount ? &lines[addStart + r] : nullptr;
            emitSplitCell(out, left, false, halfCols, th);
            emitFg(out, th.useColor, th.borderColor);
            fputs(div, out);
            emitReset(out, th.useColor);
            emitSplitCell(out, right, true, halfCols, th);
            fputc('\n', out);
        }
    }
}

/** Detects current terminal column width via TIOCGWINSZ, COLUMNS env, or 80 fallback. */
u32 TerminalDiffPanel_detectColumns() {
    struct winsize ws;
    memset(&ws, 0, sizeof(ws));
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
        return (u32) ws.ws_col;
    const char *env = getenv("COLUMNS");
    if (env && env[0]) {
        i32 parsed = atoi(env);
        if (parsed >= 20 && parsed <= 1000)
            return (u32) parsed;
    }
    return 80u;
}

/** Returns the default high-contrast truecolor palette for TerminalDiffPanel. */
TerminalDiffTheme TerminalDiffPanel_defaultTheme() {
    TerminalDiffTheme t;
    memset(&t, 0, sizeof(t));
    t.borderColor = TERMINAL_RGB(92, 103, 115);
    t.headerFg = TERMINAL_RGB(230, 237, 243);
    t.headerBg = TERMINAL_RGB(30, 36, 46);
    t.metaFg = TERMINAL_RGB(139, 148, 158);
    t.hunkFg = TERMINAL_RGB(121, 192, 255);
    t.gutterFg = TERMINAL_RGB(110, 118, 129);
    t.addFg = TERMINAL_RGB(175, 245, 180);
    t.addBg = TERMINAL_RGB(18, 42, 28);
    t.addSpanFg = TERMINAL_RGB(235, 255, 238);
    t.addSpanBg = TERMINAL_RGB(38, 108, 60);
    t.delFg = TERMINAL_RGB(255, 180, 178);
    t.delBg = TERMINAL_RGB(54, 22, 26);
    t.delSpanFg = TERMINAL_RGB(255, 236, 235);
    t.delSpanBg = TERMINAL_RGB(130, 40, 48);
    t.contextFg = TERMINAL_RGB(201, 209, 217);
    t.useColor = true;
    t.useUnicodeBorder = true;
    t.showLineNumbers = true;
    t.showHeader = true;
    t.showDiffIcons = false;
    return t;
}

/** Initializes a TerminalDiffPanel with auto-width detection and 100-column split threshold. */
fn TerminalDiffPanel_init(TerminalDiffPanel *panel) {
    if (!panel)
        return;
    memset(panel, 0, sizeof(*panel));
    DiffPanel_setColumnsRaw(panel, 0u);
    DiffPanel_setMinColsRaw(panel, 100u);
    DiffPanel_setModeRaw(panel, TERMINAL_DIFF_LAYOUT_AUTO);
    DiffPanel_setThemeRaw(panel, TerminalDiffPanel_defaultTheme());
    DiffPanel_setHeaderHookRaw(panel, nullptr);
    DiffPanel_setHeaderUserdataRaw(panel, nullptr);
    DiffPanel_setScrollXRaw(panel, 0u);
}

/** Overrides target column width (0 restores live terminal width detection). */
fn TerminalDiffPanel_setColumns(TerminalDiffPanel *panel, u32 columns) {
    if (panel)
        DiffPanel_setColumnsRaw(panel, columns);
}

/** Sets horizontal character scroll offset applied to code columns. */
fn TerminalDiffPanel_setScrollX(TerminalDiffPanel *panel, bytes scrollX) {
    if (panel)
        DiffPanel_setScrollXRaw(panel, scrollX);
}

/** Returns the active horizontal character scroll offset. */
bytes TerminalDiffPanel_scrollX(const TerminalDiffPanel *panel) {
    return panel ? DiffPanel_scrollX(panel) : 0u;
}

/** Sets the minimum column width required for AUTO mode to pick side-by-side layout. */
fn TerminalDiffPanel_setSideBySideMinCols(TerminalDiffPanel *panel, u32 minCols) {
    if (panel)
        DiffPanel_setMinColsRaw(panel, minCols > 0u ? minCols : 100u);
}

/** Sets explicit layout mode (AUTO, UNIFIED, or SIDE_BY_SIDE). */
fn TerminalDiffPanel_setLayoutMode(TerminalDiffPanel *panel, TerminalDiffLayoutMode mode) {
    if (panel)
        DiffPanel_setModeRaw(panel, mode);
}

/** Replaces the active color and chrome theme. */
fn TerminalDiffPanel_setTheme(TerminalDiffPanel *panel, TerminalDiffTheme theme) {
    if (panel)
        DiffPanel_setThemeRaw(panel, theme);
}

/** Returns the active color and chrome theme, or the default theme if panel is null. */
TerminalDiffTheme TerminalDiffPanel_theme(const TerminalDiffPanel *panel) {
    return panel ? DiffPanel_theme(panel) : TerminalDiffPanel_defaultTheme();
}

/** Installs an optional custom header printer hook for consumer apps such as difference. */
fn TerminalDiffPanel_setHeaderHook(TerminalDiffPanel *panel, TerminalDiffHeaderFn hook, any userdata) {
    if (!panel)
        return;
    DiffPanel_setHeaderHookRaw(panel, hook);
    DiffPanel_setHeaderUserdataRaw(panel, userdata);
}

/** Resolves effective column width and concrete layout mode (SIDE_BY_SIDE vs UNIFIED). */
TerminalDiffLayoutMode TerminalDiffPanel_resolveLayout(const TerminalDiffPanel *panel, u32 *outColumns) {
    u32 cols = (panel && DiffPanel_columns(panel) > 0u)
        ? DiffPanel_columns(panel)
        : TerminalDiffPanel_detectColumns();
    if (outColumns)
        *outColumns = cols;
    if (!panel)
        return cols >= 100u ? TERMINAL_DIFF_LAYOUT_SIDE_BY_SIDE : TERMINAL_DIFF_LAYOUT_UNIFIED;
    TerminalDiffLayoutMode mode = DiffPanel_mode(panel);
    if (mode != TERMINAL_DIFF_LAYOUT_AUTO)
        return mode;
    u32 threshold = DiffPanel_minCols(panel) > 0u ? DiffPanel_minCols(panel) : 100u;
    return cols >= threshold ? TERMINAL_DIFF_LAYOUT_SIDE_BY_SIDE : TERMINAL_DIFF_LAYOUT_UNIFIED;
}

/** Renders the diff header and all hunks to out in the resolved responsive layout. */
bool TerminalDiffPanel_render(const TerminalDiffPanel *panel,
                              const TerminalDiffMeta *meta,
                              const TerminalDiffHunk *hunks,
                              bytes hunkCount,
                              FILE *out) {
    if (!panel || !out)
        return false;
    u32 cols = 80u;
    TerminalDiffLayoutMode resolved = TerminalDiffPanel_resolveLayout(panel, &cols);
    TerminalDiffTheme th = DiffPanel_theme(panel);

    if (th.showHeader) {
        TerminalDiffHeaderFn hook = DiffPanel_headerHook(panel);
        if (hook)
            hook(panel, meta, cols, resolved, out, DiffPanel_headerUserdata(panel));
        else
            emitDefaultHeader(panel, meta, cols, resolved, out);
    }

    for (bytes i = 0; i < hunkCount; i++) {
        if (resolved == TERMINAL_DIFF_LAYOUT_SIDE_BY_SIDE)
            renderHunkSideBySide(panel, &hunks[i], cols, out);
        else
            renderHunkUnified(panel, &hunks[i], cols, out);
    }

    fflush(out);
    return true;
}

/** Returns the configured layout mode (AUTO, UNIFIED, or SIDE_BY_SIDE). */
TerminalDiffLayoutMode TerminalDiffPanel_layoutMode(const TerminalDiffPanel *panel) {
    return panel ? DiffPanel_mode(panel) : TERMINAL_DIFF_LAYOUT_AUTO;
}

/** Resolves concrete layout mode for a target column width without querying ioctl. */
static TerminalDiffLayoutMode resolveLayoutForCols(const TerminalDiffPanel *panel, u32 cols) {
    if (!panel)
        return cols >= 100u ? TERMINAL_DIFF_LAYOUT_SIDE_BY_SIDE : TERMINAL_DIFF_LAYOUT_UNIFIED;
    TerminalDiffLayoutMode mode = DiffPanel_mode(panel);
    if (mode != TERMINAL_DIFF_LAYOUT_AUTO)
        return mode;
    u32 threshold = DiffPanel_minCols(panel) > 0u ? DiffPanel_minCols(panel) : 100u;
    return cols >= threshold ? TERMINAL_DIFF_LAYOUT_SIDE_BY_SIDE : TERMINAL_DIFF_LAYOUT_UNIFIED;
}

/** Counts rendered rows for one hunk under the chosen layout. */
static bytes countHunkRows(const TerminalDiffHunk *hunk, TerminalDiffLayoutMode layout) {
    bytes rows = 1u;
    const TerminalDiffLine *lines = DiffHunk_lines(hunk);
    bytes count = DiffHunk_lineCount(hunk);
    if (layout != TERMINAL_DIFF_LAYOUT_SIDE_BY_SIDE)
        return rows + count;

    bytes i = 0;
    while (i < count) {
        if (DiffLine_kind(&lines[i]) == TERMINAL_DIFF_CONTEXT) {
            rows++;
            i++;
            continue;
        }
        bytes delStart = i;
        while (i < count && DiffLine_kind(&lines[i]) == TERMINAL_DIFF_REMOVED)
            i++;
        bytes delCount = i - delStart;
        bytes addStart = i;
        while (i < count && DiffLine_kind(&lines[i]) == TERMINAL_DIFF_ADDED)
            i++;
        bytes addCount = i - addStart;
        rows += delCount > addCount ? delCount : addCount;
    }
    return rows;
}

/** Computes total virtual row count for a diff at the given column width. */
bytes TerminalDiffPanel_measureRows(const TerminalDiffPanel *panel,
                                    const TerminalDiffHunk *hunks,
                                    bytes hunkCount,
                                    u32 columns) {
    if (!panel)
        return 0u;
    u32 cols = columns > 0u ? columns : 80u;
    TerminalDiffLayoutMode layout = resolveLayoutForCols(panel, cols);
    TerminalDiffTheme th = DiffPanel_theme(panel);
    bytes total = th.showHeader ? 2u : 0u;
    for (bytes i = 0; i < hunkCount; i++)
        total += countHunkRows(&hunks[i], layout);
    return total;
}

/** Paints styled diff text with inline word/letter spans onto a canvas row. */
static fn paintStyledTextCanvas(TerminalCanvas *canvas,
                                bytes startX,
                                bytes rowY,
                                bytes maxCols,
                                bytes scrollX,
                                const TerminalDiffLine *line,
                                u32 baseFg,
                                u32 baseBg,
                                u32 spanFg,
                                u32 spanBg,
                                bool useColor) {
    TerminalPen basePen = TerminalPen_3(0u,
                                        useColor ? baseFg : TERMINAL_COLOR_DEFAULT,
                                        useColor ? baseBg : TERMINAL_COLOR_DEFAULT);
    TerminalPen spanPen = TerminalPen_3(TERMINAL_STYLE_BOLD,
                                        useColor ? spanFg : TERMINAL_COLOR_DEFAULT,
                                        useColor ? spanBg : TERMINAL_COLOR_DEFAULT);

    for (bytes c = 0; c < maxCols; c++)
        TerminalCanvas_glyph(canvas, startX + c, rowY, (u32) ' ', basePen);

    if (!line)
        return;
    const char *text = DiffLine_text(line);
    bytes len = cleanLineLen(text, DiffLine_length(line));
    const TerminalDiffSpan *spans = DiffLine_spans(line);
    bytes spanCount = DiffLine_spanCount(line);

    bytes col = 0u;
    bytes written = 0u;
    for (bytes i = 0; i < len && written < maxCols; i++) {
        bool inSpan = spanCount > 0 && posInSpans(i, spans, spanCount);
        TerminalPen pen = inSpan ? spanPen : basePen;
        unsigned char ch = (unsigned char) text[i];
        if (ch == '\t') {
            bytes tabSpaces = 4u - (col % 4u);
            for (bytes s = 0; s < tabSpaces && written < maxCols; s++) {
                if (col >= scrollX) {
                    TerminalCanvas_glyph(canvas, startX + written, rowY, (u32) ' ', pen);
                    written++;
                }
                col++;
            }
        } else {
            if (col >= scrollX) {
                u32 glyph = (ch < 0x20u || ch == 0x7Fu) ? (u32) ' ' : (u32) ch;
                TerminalCanvas_glyph(canvas, startX + written, rowY, glyph, pen);
                written++;
            }
            col++;
        }
    }
}

/** Paints a single unified diff line onto canvas rowY: [+/-] old new | code. */
static fn paintUnifiedLineCanvas(TerminalCanvas *canvas,
                                 bytes rowY,
                                 bytes cols,
                                 bytes scrollX,
                                 const TerminalDiffLine *line,
                                 TerminalDiffTheme th) {
    TerminalDiffLineKind kind = DiffLine_kind(line);
    char marker = ' ';
    u32 fg = th.contextFg;
    u32 bg = TERMINAL_COLOR_DEFAULT;
    u32 sfg = th.contextFg;
    u32 sbg = TERMINAL_COLOR_DEFAULT;

    if (kind == TERMINAL_DIFF_ADDED) {
        marker = '+';
        fg = th.addFg;
        bg = th.addBg;
        sfg = th.addSpanFg;
        sbg = th.addSpanBg;
    } else if (kind == TERMINAL_DIFF_REMOVED) {
        marker = '-';
        fg = th.delFg;
        bg = th.delBg;
        sfg = th.delSpanFg;
        sbg = th.delSpanBg;
    }

    bool showMarker = th.showDiffIcons || !th.useColor;
    bytes x = 0u;
    if (showMarker && x + 2u < cols) {
        TerminalPen mPen = TerminalPen_3(TERMINAL_STYLE_BOLD,
                                         th.useColor ? fg : TERMINAL_COLOR_DEFAULT,
                                         TERMINAL_COLOR_DEFAULT);
        TerminalCanvas_glyph(canvas, x, rowY, (u32) marker, mPen);
        TerminalCanvas_glyph(canvas, x + 1u, rowY, (u32) ' ', mPen);
        x += 2u;
    }

    if (th.showLineNumbers && cols > x + 13u) {
        TerminalPen gPen = TerminalPen_3(0u, th.useColor ? th.gutterFg : TERMINAL_COLOR_DEFAULT, TERMINAL_COLOR_DEFAULT);
        char numBuf[16];
        u32 ol = DiffLine_oldLine(line);
        u32 nl = DiffLine_newLine(line);
        if (ol > 0u && nl > 0u)
            snprintf(numBuf, sizeof(numBuf), "%4u %4u ", (unsigned) ol, (unsigned) nl);
        else if (ol > 0u)
            snprintf(numBuf, sizeof(numBuf), "%4u      ", (unsigned) ol);
        else if (nl > 0u)
            snprintf(numBuf, sizeof(numBuf), "     %4u ", (unsigned) nl);
        else
            snprintf(numBuf, sizeof(numBuf), "          ");
        TerminalCanvas_text(canvas, x, rowY, numBuf, 10u, gPen);
        TerminalCanvas_glyph(canvas, x + 10u, rowY, th.useUnicodeBorder ? 0x2502u : (u32) '|', gPen);
        x += 11u;
    }

    if (x < cols) {
        TerminalPen padPen = TerminalPen_3(0u,
                                           th.useColor ? fg : TERMINAL_COLOR_DEFAULT,
                                           th.useColor ? bg : TERMINAL_COLOR_DEFAULT);
        TerminalCanvas_glyph(canvas, x, rowY, (u32) ' ', padPen);
        x++;
    }
    if (x < cols)
        paintStyledTextCanvas(canvas, x, rowY, cols - x, scrollX, line, fg, bg, sfg, sbg, th.useColor);
}

/** Paints one half-cell (left or right) of a side-by-side row onto canvas: [+/-] line | code. */
static fn paintSplitCellCanvas(TerminalCanvas *canvas,
                               bytes startX,
                               bytes rowY,
                               bytes halfCols,
                               bytes scrollX,
                               const TerminalDiffLine *line,
                               bool isRight,
                               TerminalDiffTheme th) {
    if (!line) {
        TerminalPen emptyPen = TerminalPen_0();
        for (bytes i = 0; i < halfCols; i++)
            TerminalCanvas_glyph(canvas, startX + i, rowY, (u32) ' ', emptyPen);
        return;
    }

    TerminalDiffLineKind kind = DiffLine_kind(line);
    u32 lineNo = isRight ? DiffLine_newLine(line) : DiffLine_oldLine(line);
    char marker = ' ';
    u32 fg = th.contextFg;
    u32 bg = TERMINAL_COLOR_DEFAULT;
    u32 sfg = th.contextFg;
    u32 sbg = TERMINAL_COLOR_DEFAULT;

    if (kind == TERMINAL_DIFF_ADDED) {
        marker = '+';
        fg = th.addFg;
        bg = th.addBg;
        sfg = th.addSpanFg;
        sbg = th.addSpanBg;
    } else if (kind == TERMINAL_DIFF_REMOVED) {
        marker = '-';
        fg = th.delFg;
        bg = th.delBg;
        sfg = th.delSpanFg;
        sbg = th.delSpanBg;
    }

    bool showMarker = th.showDiffIcons || !th.useColor;
    bytes x = 0u;
    if (showMarker && x + 2u < halfCols) {
        TerminalPen mPen = TerminalPen_3(TERMINAL_STYLE_BOLD,
                                         th.useColor ? fg : TERMINAL_COLOR_DEFAULT,
                                         TERMINAL_COLOR_DEFAULT);
        TerminalCanvas_glyph(canvas, startX + x, rowY, (u32) marker, mPen);
        TerminalCanvas_glyph(canvas, startX + x + 1u, rowY, (u32) ' ', mPen);
        x += 2u;
    }

    if (th.showLineNumbers && halfCols > x + 8u) {
        TerminalPen gPen = TerminalPen_3(0u, th.useColor ? th.gutterFg : TERMINAL_COLOR_DEFAULT, TERMINAL_COLOR_DEFAULT);
        char numBuf[12];
        if (lineNo > 0u)
            snprintf(numBuf, sizeof(numBuf), "%4u ", (unsigned) lineNo);
        else
            snprintf(numBuf, sizeof(numBuf), "     ");
        TerminalCanvas_text(canvas, startX + x, rowY, numBuf, 5u, gPen);
        TerminalCanvas_glyph(canvas, startX + x + 5u, rowY, th.useUnicodeBorder ? 0x2502u : (u32) '|', gPen);
        x += 6u;
    }

    if (x < halfCols) {
        TerminalPen padPen = TerminalPen_3(0u,
                                           th.useColor ? fg : TERMINAL_COLOR_DEFAULT,
                                           th.useColor ? bg : TERMINAL_COLOR_DEFAULT);
        TerminalCanvas_glyph(canvas, startX + x, rowY, (u32) ' ', padPen);
        x++;
    }
    if (x < halfCols)
        paintStyledTextCanvas(canvas, startX + x, rowY, halfCols - x, scrollX, line, fg, bg, sfg, sbg, th.useColor);
}

/** Paints the diff onto a TerminalCanvas region starting at virtual row scrollOffset. */
bytes TerminalDiffPanel_paintCanvas(const TerminalDiffPanel *panel,
                                    const TerminalDiffMeta *meta,
                                    const TerminalDiffHunk *hunks,
                                    bytes hunkCount,
                                    TerminalCanvas *canvas,
                                    bytes scrollOffset) {
    if (!panel || !canvas || !TerminalCanvas_isValid(canvas))
        return 0u;

    bytes width = TerminalCanvas_getWidth(canvas);
    bytes height = TerminalCanvas_getHeight(canvas);
    if (width == 0u || height == 0u)
        return 0u;

    u32 cols = DiffPanel_columns(panel) > 0u ? DiffPanel_columns(panel) : (u32) width;
    if (cols > (u32) width)
        cols = (u32) width;
    TerminalDiffLayoutMode layout = resolveLayoutForCols(panel, cols);
    TerminalDiffTheme th = DiffPanel_theme(panel);
    bytes scrollX = DiffPanel_scrollX(panel);
    bytes vrow = 0u;

    if (th.showHeader) {
        if (vrow >= scrollOffset && vrow - scrollOffset < height) {
            bytes sy = vrow - scrollOffset;
            TerminalPen hdrPen = TerminalPen_3(TERMINAL_STYLE_BOLD,
                                               th.useColor ? th.headerFg : TERMINAL_COLOR_DEFAULT,
                                               th.useColor ? th.headerBg : TERMINAL_COLOR_DEFAULT);
            for (bytes c = 0; c < width; c++)
                TerminalCanvas_glyph(canvas, c, sy, (u32) ' ', hdrPen);
            char hdrBuf[512];
            const char *path = (meta && DiffMeta_path(meta)) ? DiffMeta_path(meta) : "(unnamed)";
            const char *ev = (meta && DiffMeta_eventLabel(meta)) ? DiffMeta_eventLabel(meta) : "DIFF";
            const char *ts = (meta && DiffMeta_timestamp(meta)) ? DiffMeta_timestamp(meta) : "";
            u32 added = meta ? DiffMeta_added(meta) : 0u;
            u32 removed = meta ? DiffMeta_removed(meta) : 0u;
            const char *modeTag = (layout == TERMINAL_DIFF_LAYOUT_SIDE_BY_SIDE) ? "split" : "unified";
            i32 n = snprintf(hdrBuf, sizeof(hdrBuf), " [%s] %s  +%u -%u  %s (%s)",
                             ev, path, (unsigned) added, (unsigned) removed, ts, modeTag);
            if (n > 0)
                TerminalCanvas_text(canvas, 0u, sy, hdrBuf, (bytes) n, hdrPen);
        }
        vrow++;

        if (vrow >= scrollOffset && vrow - scrollOffset < height) {
            TerminalPen rulePen = TerminalPen_3(0u, th.useColor ? th.borderColor : TERMINAL_COLOR_DEFAULT, TERMINAL_COLOR_DEFAULT);
            TerminalCanvas_hline(canvas, vrow - scrollOffset, th.useUnicodeBorder ? 0x2500u : (u32) '-', rulePen);
        }
        vrow++;
    }

    bytes halfCols = cols > 3u ? (cols - 3u) / 2u : (cols / 2u);
    TerminalPen hunkRowPen = TerminalPen_3(0u,
                                           th.useColor ? th.gutterFg : TERMINAL_COLOR_DEFAULT,
                                           th.useColor ? TERMINAL_RGB(20, 25, 34) : TERMINAL_COLOR_DEFAULT);
    TerminalPen hunkPen = TerminalPen_3(TERMINAL_STYLE_BOLD,
                                        th.useColor ? th.hunkFg : TERMINAL_COLOR_DEFAULT,
                                        th.useColor ? TERMINAL_RGB(34, 44, 62) : TERMINAL_COLOR_DEFAULT);
    TerminalPen borderPen = TerminalPen_3(0u, th.useColor ? th.borderColor : TERMINAL_COLOR_DEFAULT, TERMINAL_COLOR_DEFAULT);

    for (bytes h = 0; h < hunkCount; h++) {
        const TerminalDiffHunk *hunk = &hunks[h];
        if (vrow >= scrollOffset && vrow - scrollOffset < height) {
            bytes sy = vrow - scrollOffset;
            for (bytes c = 0u; c < width; c++)
                TerminalCanvas_glyph(canvas, c, sy, (u32) ' ', hunkRowPen);
            char hbuf[96];
            i32 hn = snprintf(hbuf, sizeof(hbuf), " @@ -%u,%u +%u,%u @@ ",
                              (unsigned) DiffHunk_oldStart(hunk),
                              (unsigned) DiffHunk_oldCount(hunk),
                              (unsigned) DiffHunk_newStart(hunk),
                              (unsigned) DiffHunk_newCount(hunk));
            if (hn > 0) {
                bytes hlen = (bytes) hn;
                if (hlen > width)
                    hlen = width;
                bytes hx = (width - hlen) / 2u;
                TerminalCanvas_text(canvas, hx, sy, hbuf, hlen, hunkPen);
            }
        }
        vrow++;

        const TerminalDiffLine *lines = DiffHunk_lines(hunk);
        bytes count = DiffHunk_lineCount(hunk);

        if (layout != TERMINAL_DIFF_LAYOUT_SIDE_BY_SIDE) {
            for (bytes i = 0; i < count; i++) {
                if (vrow >= scrollOffset && vrow - scrollOffset < height)
                    paintUnifiedLineCanvas(canvas, vrow - scrollOffset, cols, scrollX, &lines[i], th);
                vrow++;
            }
            continue;
        }

        bytes i = 0u;
        while (i < count) {
            if (DiffLine_kind(&lines[i]) == TERMINAL_DIFF_CONTEXT) {
                if (vrow >= scrollOffset && vrow - scrollOffset < height) {
                    bytes sy = vrow - scrollOffset;
                    paintSplitCellCanvas(canvas, 0u, sy, halfCols, scrollX, &lines[i], false, th);
                    TerminalCanvas_glyph(canvas, halfCols + 1u, sy, th.useUnicodeBorder ? 0x2502u : (u32) '|', borderPen);
                    paintSplitCellCanvas(canvas, halfCols + 3u, sy, cols - (halfCols + 3u), scrollX, &lines[i], true, th);
                }
                vrow++;
                i++;
                continue;
            }

            bytes delStart = i;
            while (i < count && DiffLine_kind(&lines[i]) == TERMINAL_DIFF_REMOVED)
                i++;
            bytes delCount = i - delStart;

            bytes addStart = i;
            while (i < count && DiffLine_kind(&lines[i]) == TERMINAL_DIFF_ADDED)
                i++;
            bytes addCount = i - addStart;

            bytes rows = delCount > addCount ? delCount : addCount;
            for (bytes r = 0; r < rows; r++) {
                if (vrow >= scrollOffset && vrow - scrollOffset < height) {
                    bytes sy = vrow - scrollOffset;
                    const TerminalDiffLine *left = r < delCount ? &lines[delStart + r] : nullptr;
                    const TerminalDiffLine *right = r < addCount ? &lines[addStart + r] : nullptr;
                    paintSplitCellCanvas(canvas, 0u, sy, halfCols, scrollX, left, false, th);
                    TerminalCanvas_glyph(canvas, halfCols + 1u, sy, th.useUnicodeBorder ? 0x2502u : (u32) '|', borderPen);
                    paintSplitCellCanvas(canvas, halfCols + 3u, sy, cols - (halfCols + 3u), scrollX, right, true, th);
                }
                vrow++;
            }
        }
    }

    return vrow;
}
