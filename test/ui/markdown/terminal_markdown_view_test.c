// Owner: TerminalMarkdownView — the terminal Markdown preview. Cold parsing and
// cell-aware wrapping are checked separately from hot rendering.
#include "ui/markdown/terminal_markdown_view.h"
#include "format/terminal_color.h"
#include "determinant/test/support.h"

enum { W = 48, H = 12 };

static void clearCanvas(TerminalCanvas *canvas, uint32_t *cells, uint16_t *styles,
                        uint32_t *fg, uint32_t *bg) {
    *canvas = TerminalCanvas(cells, styles, fg, bg, W, H);
    TerminalCanvas_fill(canvas, ' ', TerminalPen());
}

static bool canvasContainsText(const TerminalCanvas *canvas, const char *text) {
    size_t length = strlen(text);
    for (size_t row = 0; row < TerminalCanvas_getHeight(canvas); ++row)
        for (size_t column = 0; column + length <= TerminalCanvas_getWidth(canvas); ++column) {
            bool matches = true;
            for (size_t index = 0; index < length; ++index)
                if (TerminalCanvas_glyphAt(canvas, column + index, row)
                    != (unsigned char) text[index]) {
                    matches = false;
                    break;
                }
            if (matches)
                return true;
        }
    return false;
}

static size_t rowOfText(const TerminalCanvas *canvas, const char *text) {
    size_t length = strlen(text);
    for (size_t row = 0; row < TerminalCanvas_getHeight(canvas); ++row)
        for (size_t column = 0; column + length <= TerminalCanvas_getWidth(canvas); ++column) {
            bool matches = true;
            for (size_t index = 0; index < length; ++index)
                if (TerminalCanvas_glyphAt(canvas, column + index, row)
                    != (unsigned char) text[index]) {
                    matches = false;
                    break;
                }
            if (matches)
                return row;
        }
    return SIZE_MAX;
}

int main(void) {
    static const char *lines[] = {
        "# Semicolon",
        "",
        "A **bold** and *quiet* `code` paragraph with [docs](https://example.test).",
        "",
        "- [x] done",
        "- [ ] next",
        "",
        "> [!WARNING] Important section",
        "> Callout details stay grouped.",
        "",
        "```c",
        "int main(void) { return 0; }",
        "```",
        "",
        "| Name | Value |",
        "| ---- | ----- |",
        "| one | two |",
    };
    uint32_t cells[W * H];
    uint16_t styles[W * H];
    uint32_t fg[W * H];
    uint32_t bg[W * H];
    TerminalCanvas canvas;
    clearCanvas(&canvas, cells, styles, fg, bg);

    TerminalMarkdownView *view = TerminalMarkdownView();
    assert(view != nullptr);
    assert(TerminalMarkdownView_rowCount(view) == 0u);
    assert(TerminalMarkdownView_getWidth(view) == 0u);
    assert(TerminalMarkdownView_visibleRows(view) == 0u);
    assert(TerminalMarkdownView_getScroll(view) == 0u);
    assert(TerminalMarkdownView_maxScroll(view) == 0u);
    assert(!TerminalMarkdownView_isTruncated(view));
    assert(TerminalMarkdownView_exhaustionCount(view) == 0u);
    TerminalMarkdownStyle style = TerminalMarkdownView_getStyle(view);
    assert(style.heading.style == TERMINAL_STYLE_BOLD);
    style.body = TerminalPen(TERMINAL_STYLE_DIM, TERMINAL_RGB(9, 8, 7));
    assert(TerminalMarkdownView_setStyle(view, style));
    assert(TerminalMarkdownView_getStyle(view).body.fg == TERMINAL_RGB(9, 8, 7));
    assert(TerminalMarkdownView_setSource(view, lines, sizeof lines / sizeof lines[0]));
    assert(!TerminalMarkdownView_isReady(view));
    assert(TerminalMarkdownView_setViewport(view, W));
    assert(TerminalMarkdownView_isReady(view));
    assert(!TerminalMarkdownView_isTruncated(view));
    assert(TerminalMarkdownView_getWidth(view) == W);
    assert(TerminalMarkdownView_rowCount(view) >= 10u);
    TerminalMarkdownStyle renderedStyle = TerminalMarkdownView_getStyle(view);
    assert(renderedStyle.code.fg == TERMINAL_RGB(112, 190, 130));
    assert((renderedStyle.code.style & TERMINAL_STYLE_REVERSE) == 0);

    TerminalMarkdownView_setVisibleRows(view, H);
    assert(TerminalMarkdownView_maxScroll(view) + H >= TerminalMarkdownView_rowCount(view));
    TerminalMarkdownView_render(view, &canvas);
    assert(TerminalCanvas_glyphAt(&canvas, 0, 0) == '#'
           || TerminalCanvas_glyphAt(&canvas, 0, 0) == 'S');
    bool sawStyledCell = false;
    bool sawCalloutBar = false;
    bool sawColoredCallout = false;
    bool sawTableBorder = false;
    for (size_t i = 0; i < W * H; ++i)
        if ((styles[i] & TERMINAL_STYLE_BOLD) != 0)
            sawStyledCell = true;
    for (size_t row = 0; row < H; ++row)
        for (size_t column = 0; column < W; ++column) {
            uint32_t glyph = TerminalCanvas_glyphAt(&canvas, column, row);
            if (glyph == 0x258Cu) {
                sawCalloutBar = true;
                sawColoredCallout = (styles[row * W + column] & TERMINAL_STYLE_BOLD) != 0
                    && fg[row * W + column] == TERMINAL_RGB(235, 192, 95);
            }
        }
    assert(sawStyledCell);
    assert(sawCalloutBar);
    assert(sawColoredCallout);

    static const char *warningCallout[] = {
        "> [!WARNING]",
        "> text goes here and text goes here",
        "> and text goes here",
    };
    assert(TerminalMarkdownView_setSource(view, warningCallout,
                                          sizeof warningCallout / sizeof warningCallout[0]));
    assert(TerminalMarkdownView_setViewport(view, W));
    clearCanvas(&canvas, cells, styles, fg, bg);
    TerminalMarkdownView_render(view, &canvas);
    // A callout is a bar plus an uppercase label, never the literal `[! ...]`
    // punctuation, and its body is indented by the bar on every row.
    assert(TerminalCanvas_glyphAt(&canvas, 0, 0) == 0x258Cu);
    assert(TerminalCanvas_glyphAt(&canvas, 1, 0) == ' ');
    assert(TerminalCanvas_glyphAt(&canvas, 2, 0) == 'W');
    for (size_t column = 2u; column <= 8u; ++column) {
        assert((styles[column] & TERMINAL_STYLE_BOLD) != 0u);
        assert(fg[column] == TERMINAL_RGB(235, 192, 95));
    }
    assert(TerminalCanvas_glyphAt(&canvas, 0, 1) == 0x258Cu);
    assert(TerminalCanvas_glyphAt(&canvas, 1, 1) == ' ');
    assert(TerminalCanvas_glyphAt(&canvas, 2, 1) == 't');
    assert((styles[1u * W + 2u] & TERMINAL_STYLE_BOLD) == 0u);
    assert(fg[1u * W + 2u] == TERMINAL_RGB(9, 8, 7));

    // The README spelling of the same aside: `!WARNING!` needs no quote gutter,
    // and the marker is still consumed rather than printed.
    static const char *bangCallout[] = {
        "!WARNING!",
        "Mind the gap",
    };
    assert(TerminalMarkdownView_setSource(view, bangCallout,
                                          sizeof bangCallout / sizeof bangCallout[0]));
    assert(TerminalMarkdownView_setViewport(view, W));
    clearCanvas(&canvas, cells, styles, fg, bg);
    TerminalMarkdownView_render(view, &canvas);
    assert(TerminalCanvas_glyphAt(&canvas, 0, 0) == 0x258Cu);
    assert(TerminalCanvas_glyphAt(&canvas, 2, 0) == 'W');
    assert(TerminalCanvas_glyphAt(&canvas, 2, 1) == 'M');
    assert(!canvasContainsText(&canvas, "[!"));
    assert(!canvasContainsText(&canvas, "!WARNING!"));

    // A fenced block is a shaded band, not a box. It gets one blank row above
    // and below, every cell of its rows carries the fence background, and the
    // copy projection exposes the content lines without the fences.
    static const char *shaded[] = {
        "before",
        "",
        "```c",
        "int x = 1;",
        "",
        "int y = 2;",
        "```",
        "",
        "after",
    };
    assert(TerminalMarkdownView_setSource(view, shaded, sizeof shaded / sizeof shaded[0]));
    assert(TerminalMarkdownView_setViewport(view, W));
    TerminalMarkdownView_setVisibleRows(view, H);
    assert(TerminalMarkdownView_codeBlockCount(view) == 1u);
    TerminalMarkdownCodeBlock block = TerminalMarkdownView_codeBlockAt(view, 0);
    assert(!block.valid);
    block = TerminalMarkdownView_codeBlockAt(view, 2);
    assert(block.valid);
    assert(block.firstRow == 2u);
    assert(block.rowCount == 3u);
    assert(block.firstSourceLine == 3u);
    assert(block.sourceLineCount == 3u);
    assert(block.quoteDepth == 0u);
    assert(TerminalMarkdownView_codeBlockAt(view, 4).valid);
    assert(!TerminalMarkdownView_codeBlockAt(view, 5).valid);
    TerminalMarkdownCodeLine line = TerminalMarkdownView_codeBlockLine(view, block, 0);
    assert(line.valid && line.length == strlen("int x = 1;"));
    assert(memcmp(line.text, "int x = 1;", line.length) == 0);
    line = TerminalMarkdownView_codeBlockLine(view, block, 1);
    assert(line.valid && line.length == 0u);
    line = TerminalMarkdownView_codeBlockLine(view, block, 3);
    assert(!line.valid);

    TerminalMarkdownStyle codeStyle = TerminalMarkdownView_getStyle(view);
    assert(codeStyle.fence.bg != TERMINAL_COLOR_DEFAULT);
    clearCanvas(&canvas, cells, styles, fg, bg);
    TerminalMarkdownView_render(view, &canvas);
    assert(bg[1u * W] != codeStyle.fence.bg);
    for (size_t row = 2u; row < 5u; ++row) {
        assert(bg[row * W] == codeStyle.fence.bg);
        assert(bg[row * W + W - 1u] == codeStyle.fence.bg);
        for (size_t column = 0; column < W; ++column) {
            uint32_t glyph = TerminalCanvas_glyphAt(&canvas, column, row);
            assert(glyph != 0x250Cu && glyph != 0x2502u && glyph != 0x2518u);
        }
    }
    assert(bg[5u * W] != codeStyle.fence.bg);
    assert(canvasContainsText(&canvas, "int x = 1;"));
    assert(canvasContainsText(&canvas, "int y = 2;"));

    // A fence inside a callout keeps the quote gutter but gets the same band, and
    // its copy projection strips the quote markers.
    static const char *quotedFence[] = {
        "> [!WARNING]",
        "> Watch this block",
        "> ```c",
        "> int q = 3;",
        "> ```",
    };
    assert(TerminalMarkdownView_setSource(view, quotedFence,
                                          sizeof quotedFence / sizeof quotedFence[0]));
    assert(TerminalMarkdownView_setViewport(view, W));
    TerminalMarkdownView_setVisibleRows(view, H);
    assert(TerminalMarkdownView_codeBlockCount(view) == 1u);
    TerminalMarkdownCodeBlock quotedBlock = {0};
    for (size_t row = 0; row < TerminalMarkdownView_rowCount(view); ++row) {
        TerminalMarkdownCodeBlock candidate = TerminalMarkdownView_codeBlockAt(view, row);
        if (candidate.valid) {
            quotedBlock = candidate;
            break;
        }
    }
    assert(quotedBlock.valid);
    assert(quotedBlock.quoteDepth == 1u);
    assert(quotedBlock.sourceLineCount == 1u);
    line = TerminalMarkdownView_codeBlockLine(view, quotedBlock, 0);
    assert(line.valid && line.length == strlen("int q = 3;"));
    assert(memcmp(line.text, "int q = 3;", line.length) == 0);
    clearCanvas(&canvas, cells, styles, fg, bg);
    TerminalMarkdownView_render(view, &canvas);
    assert(TerminalCanvas_glyphAt(&canvas, 0, quotedBlock.firstRow) == 0x258Cu);
    assert(bg[quotedBlock.firstRow * W + 2u] == codeStyle.fence.bg);

    assert(TerminalMarkdownView_setSource(view, lines, sizeof lines / sizeof lines[0]));
    assert(TerminalMarkdownView_setViewport(view, W));
    TerminalMarkdownView_setVisibleRows(view, H);
    assert(TerminalMarkdownView_setScroll(view, TerminalMarkdownView_maxScroll(view)));
    clearCanvas(&canvas, cells, styles, fg, bg);
    TerminalMarkdownView_render(view, &canvas);
    assert(TerminalMarkdownView_codeBlockCount(view) == 1u);
    for (size_t row = 0; row < H; ++row)
        for (size_t column = 0; column < W; ++column) {
            uint32_t glyph = TerminalCanvas_glyphAt(&canvas, column, row);
            if (glyph == 0x2502u)
                sawTableBorder = true;
        }
    assert(sawTableBorder);

    TerminalMarkdownView_setVisibleRows(view, 2);
    size_t oldScroll = TerminalMarkdownView_getScroll(view);
    assert(TerminalMarkdownView_scrollBy(view, 3));
    assert(TerminalMarkdownView_getScroll(view) == oldScroll + 3u);
    assert(TerminalMarkdownView_setScroll(view, SIZE_MAX));
    assert(TerminalMarkdownView_getScroll(view) == TerminalMarkdownView_maxScroll(view));
    assert(!TerminalMarkdownView_scrollBy(view, 1));

    static const char *longQuote[] = {
        "> A blockquote long enough to wrap across several terminal rows must keep its "
        "colored vertical marker at the start of every continuation row."
    };
    assert(TerminalMarkdownView_setSource(view, longQuote, 1));
    assert(TerminalMarkdownView_setViewport(view, W));
    assert(TerminalMarkdownView_rowCount(view) >= 2u);
    clearCanvas(&canvas, cells, styles, fg, bg);
    TerminalMarkdownView_render(view, &canvas);
    size_t quoteRows = 0;
    for (size_t row = 0; row < H && row < TerminalMarkdownView_rowCount(view); ++row)
        if (TerminalCanvas_glyphAt(&canvas, 0, row) == '|') {
            ++quoteRows;
            assert(TerminalCanvas_glyphAt(&canvas, 1, row) == ' ');
        }
    assert(quoteRows >= 2u);

    static const char *ordered[] = {
        "1. first ordered item", "2. second ordered item", "3) third ordered item"
    };
    assert(TerminalMarkdownView_setSource(view, ordered, 3));
    assert(TerminalMarkdownView_setViewport(view, W));
    clearCanvas(&canvas, cells, styles, fg, bg);
    TerminalMarkdownView_render(view, &canvas);
    bool sawFirstNumber = false;
    bool sawSecondNumber = false;
    bool sawThirdNumber = false;
    bool sawFirstText = false;
    bool sawSecondText = false;
    for (size_t row = 0; row < H; ++row)
        for (size_t column = 0; column < W; ++column) {
            uint32_t glyph = TerminalCanvas_glyphAt(&canvas, column, row);
            if (column + 1u < W && glyph == '1'
                && TerminalCanvas_glyphAt(&canvas, column + 1u, row) == '.')
                sawFirstNumber = true;
            if (column + 1u < W && glyph == '2'
                && TerminalCanvas_glyphAt(&canvas, column + 1u, row) == '.')
                sawSecondNumber = true;
            if (column + 1u < W && glyph == '3'
                && TerminalCanvas_glyphAt(&canvas, column + 1u, row) == ')')
                sawThirdNumber = true;
            if (column + 1u < W && glyph == 'f'
                && TerminalCanvas_glyphAt(&canvas, column + 1u, row) == 'i')
                sawFirstText = true;
            if (column + 1u < W && glyph == 's'
                && TerminalCanvas_glyphAt(&canvas, column + 1u, row) == 'e')
                sawSecondText = true;
        }
    assert(sawFirstNumber && sawSecondNumber && sawThirdNumber);
    assert(sawFirstText && sawSecondText);

    static const char *unordered[] = { "- dash item", "* star item", "+ plus item" };
    assert(TerminalMarkdownView_setSource(view, unordered, 3));
    assert(TerminalMarkdownView_setViewport(view, W));
    clearCanvas(&canvas, cells, styles, fg, bg);
    TerminalMarkdownView_render(view, &canvas);
    for (size_t row = 0; row < 3u; ++row) {
        assert(TerminalCanvas_glyphAt(&canvas, 0, row) == 0x2022u);
        assert(TerminalCanvas_glyphAt(&canvas, 1, row) == ' ');
    }
    assert(canvasContainsText(&canvas, "dash item"));
    assert(canvasContainsText(&canvas, "star item"));
    assert(canvasContainsText(&canvas, "plus item"));

    static const char *pipeTable[] = {
        "| Law Title | Scope | Enforcement |",
        "| :--- | :--- | :--- |",
        "| Table Law | Terminal | Required |",
    };
    assert(TerminalMarkdownView_setSource(view, pipeTable, 3));
    assert(TerminalMarkdownView_setViewport(view, W));
    clearCanvas(&canvas, cells, styles, fg, bg);
    TerminalMarkdownView_render(view, &canvas);
    assert(canvasContainsText(&canvas, "Law Title"));
    assert(canvasContainsText(&canvas, "Scope"));
    assert(canvasContainsText(&canvas, "Enforcement"));
    assert(canvasContainsText(&canvas, "Table Law"));
    assert(canvasContainsText(&canvas, "Terminal"));
    assert(canvasContainsText(&canvas, "Required"));
    assert(!canvasContainsText(&canvas, ":---"));
    size_t tableVerticals = 0;
    for (size_t row = 0; row < H; ++row)
        for (size_t column = 0; column < W; ++column)
            if (TerminalCanvas_glyphAt(&canvas, column, row) == 0x2502u)
                ++tableVerticals;
    assert(tableVerticals >= 6u);

    static const char *lawIndexTable[] = {
        "| # | Law Title |",
        "| :-- | :--- |",
        "| 1 | Semantic Consistency Law |",
        "| 2 | Single Class Per File Law (Java Law) |",
        "| 3 | Two-Semicolon Annotation Style Law |",
        "| 4 | Build & Naming Conventions Law |",
    };
    assert(TerminalMarkdownView_setSource(view, lawIndexTable,
                                          sizeof lawIndexTable / sizeof lawIndexTable[0]));
    assert(TerminalMarkdownView_setViewport(view, W));
    clearCanvas(&canvas, cells, styles, fg, bg);
    TerminalMarkdownView_render(view, &canvas);
    bool sawLawTitle = false;
    bool sawOrdinal = false;
    bool sawUnicodeTableEdge = false;
    bool sawPaddedTableCell = false;
    bool sawSourcePipe = false;
    for (size_t row = 0; row < H; ++row)
        for (size_t column = 0; column < W; ++column) {
            uint32_t glyph = TerminalCanvas_glyphAt(&canvas, column, row);
            if (glyph == 'L' && column + 3u < W
                && TerminalCanvas_glyphAt(&canvas, column + 1u, row) == 'a'
                && TerminalCanvas_glyphAt(&canvas, column + 2u, row) == 'w'
                && TerminalCanvas_glyphAt(&canvas, column + 3u, row) == ' ')
                sawLawTitle = true;
            if (glyph == '1' && column + 1u < W
                && TerminalCanvas_glyphAt(&canvas, column + 1u, row) == ' ')
                sawOrdinal = true;
            if (glyph == 0x2502u)
                sawUnicodeTableEdge = true;
            if (row > 0u && glyph == 0x2502u && column + 2u < W
                && TerminalCanvas_glyphAt(&canvas, column + 1u, row) == ' '
                && TerminalCanvas_glyphAt(&canvas, column + 2u, row) == '#')
                sawPaddedTableCell = true;
            if (glyph == '|')
                sawSourcePipe = true;
        }
    assert(sawLawTitle && sawOrdinal && sawUnicodeTableEdge && sawPaddedTableCell);
    assert(!sawSourcePipe);

    static const char *wrappedTable[] = {
        "| Name | Scope Summary | Status |",
        "| ---- | ------------- | ------ |",
        "| Runner | Long cell wraps cleanly and keeps all of its content COMPLETE. | 🟥 ready |",
        "| Editor | Short row stays aligned below the wrapped row. | 🟩 ready |",
    };
    assert(TerminalCanvas_widthOf(0x1F7E5u) == 2u);
    static const char *naturalWidthTable[] = {
        "| ID | Description |",
        "| -- | ----------- |",
        "| 1 | This description should keep its natural unwrapped width. |",
    };
    assert(TerminalMarkdownView_setSource(view, naturalWidthTable,
                                          sizeof naturalWidthTable / sizeof naturalWidthTable[0]));
    assert(TerminalMarkdownView_setViewport(view, 100u));
    assert(TerminalMarkdownView_rowCount(view) == 5u);

    static const char *weightedTable[] = {
        "| ID | Description | State |",
        "| -- | ----------- | ----- |",
        "| 1 | This longer description needs substantially more column room than the short state. | OK |",
    };
    assert(TerminalMarkdownView_setSource(view, weightedTable,
                                          sizeof weightedTable / sizeof weightedTable[0]));
    assert(TerminalMarkdownView_setViewport(view, W));
    clearCanvas(&canvas, cells, styles, fg, bg);
    TerminalMarkdownView_render(view, &canvas);
    assert(TerminalCanvas_glyphAt(&canvas, 5u, 3u) == 0x2502u);
    assert(TerminalCanvas_glyphAt(&canvas, 39u, 3u) == 0x2502u);

    assert(TerminalMarkdownView_setSource(view, wrappedTable,
                                          sizeof wrappedTable / sizeof wrappedTable[0]));
    assert(TerminalMarkdownView_setViewport(view, W));
    assert(TerminalMarkdownView_setScroll(view, 0u));
    assert(TerminalMarkdownView_contentWidth(view) > W);
    assert(TerminalMarkdownView_maxScrollX(view) == TerminalMarkdownView_contentWidth(view) - W);
    clearCanvas(&canvas, cells, styles, fg, bg);
    TerminalMarkdownView_render(view, &canvas);
    assert(canvasContainsText(&canvas, "Runner"));

    assert(TerminalMarkdownView_setScrollX(view, TerminalMarkdownView_maxScrollX(view)));
    assert(TerminalMarkdownView_getScrollX(view) == TerminalMarkdownView_maxScrollX(view));
    clearCanvas(&canvas, cells, styles, fg, bg);
    TerminalMarkdownView_render(view, &canvas);
    assert(canvasContainsText(&canvas, "COMPLETE"));
    bool sawRedEmoji = false;
    size_t tableJoins = 0;
    for (size_t row = 0; row < H; ++row) {
        size_t lastContentColumn = W;
        for (size_t column = 0; column < W; ++column) {
            uint32_t glyph = TerminalCanvas_glyphAt(&canvas, column, row);
            if (glyph != ' ' && glyph != 0u)
                lastContentColumn = column;
            if (glyph == 0x1F7E5u) {
                sawRedEmoji = true;
                assert(column + 1u < W);
                assert(TerminalCanvas_glyphAt(&canvas, column + 1u, row) == 0u);
            }
            if (glyph == 0x253Cu)
                ++tableJoins;
        }
        if (lastContentColumn != W) {
            uint32_t rightEdge = TerminalCanvas_glyphAt(&canvas, lastContentColumn, row);
            assert(rightEdge == 0x2502u || rightEdge == 0x2524u
                   || rightEdge == 0x2510u || rightEdge == 0x2518u);
        }
    }
    assert(sawRedEmoji);
    assert(tableJoins >= 2u);
    assert(TerminalMarkdownView_scrollXBy(view, -100));
    assert(TerminalMarkdownView_getScrollX(view) == 0u);

    // The horizontal pan belongs to the table alone. Prose keeps its document
    // origin while the warped table slides under it, and the table's own right
    // edge is reachable at the document-wide maximum.
    static const char *tablePanLines[] = {
        "Prose stays put.",
        "",
        "| Repository | Workspace path |",
        "| :--- | :--- |",
        "| vexspoke | host-behavior/vexspoke/vexspoke-preferences.md |",
    };
    assert(TerminalMarkdownView_setSource(view, tablePanLines,
                                          sizeof tablePanLines / sizeof tablePanLines[0]));
    assert(TerminalMarkdownView_setViewport(view, W));
    assert(TerminalMarkdownView_setScroll(view, 0u));
    assert(TerminalMarkdownView_setScrollX(view, 0u));
    clearCanvas(&canvas, cells, styles, fg, bg);
    TerminalMarkdownView_render(view, &canvas);
    size_t proseRow = rowOfText(&canvas, "Prose stays put.");
    size_t wideHeaderRow = rowOfText(&canvas, "Repository");
    assert(proseRow != SIZE_MAX && wideHeaderRow != SIZE_MAX);
    assert(TerminalMarkdownView_rowScrollX(view, proseRow) == 0u);
    assert(TerminalMarkdownView_rowScrollX(view, wideHeaderRow) == 0u);
    assert(TerminalMarkdownView_contentWidth(view) > W);
    size_t maximumX = TerminalMarkdownView_maxScrollX(view);
    assert(maximumX > 0u);
    // Column content widths are a whole number of 4-cell steps: one left border, one
    // space and one right border per column (3*columns) plus the closing border (1).
    assert((TerminalMarkdownView_contentWidth(view) - 7u) % 4u == 0u);
    assert(TerminalMarkdownView_setScrollX(view, maximumX));
    clearCanvas(&canvas, cells, styles, fg, bg);
    TerminalMarkdownView_render(view, &canvas);
    assert(TerminalCanvas_glyphAt(&canvas, 0, proseRow) == 'P');
    assert(canvasContainsText(&canvas, "Prose stays put."));
    assert(TerminalMarkdownView_rowScrollX(view, proseRow) == 0u);
    assert(TerminalMarkdownView_rowScrollX(view, wideHeaderRow) == maximumX);
    assert(TerminalCanvas_glyphAt(&canvas, 0, wideHeaderRow) != 0x2502u);
    assert(TerminalCanvas_glyphAt(&canvas, W - 1u, wideHeaderRow) == 0x2502u);

    // A table that fits never moves, even while a wider table in the same
    // document pans by its full overflow.
    static const char *mixedTableLines[] = {
        "| ID | Ok |",
        "| -- | -- |",
        "| 1 | 2 |",
        "",
        "| Repository | Workspace path |",
        "| :--- | :--- |",
        "| vexspoke | host-behavior/vexspoke/vexspoke-preferences.md |",
    };
    assert(TerminalMarkdownView_setSource(view, mixedTableLines,
                                          sizeof mixedTableLines / sizeof mixedTableLines[0]));
    assert(TerminalMarkdownView_setViewport(view, W));
    assert(TerminalMarkdownView_setScroll(view, 0u));
    assert(TerminalMarkdownView_setScrollX(view, 0u));
    clearCanvas(&canvas, cells, styles, fg, bg);
    TerminalMarkdownView_render(view, &canvas);
    size_t smallHeaderRow = rowOfText(&canvas, "ID");
    size_t mixedHeaderRow = rowOfText(&canvas, "Repository");
    assert(smallHeaderRow != SIZE_MAX && mixedHeaderRow != SIZE_MAX);
    assert(TerminalMarkdownView_setScrollX(view, TerminalMarkdownView_maxScrollX(view)));
    clearCanvas(&canvas, cells, styles, fg, bg);
    TerminalMarkdownView_render(view, &canvas);
    assert(TerminalMarkdownView_rowScrollX(view, smallHeaderRow) == 0u);
    assert(TerminalCanvas_glyphAt(&canvas, 0, smallHeaderRow - 1u) == 0x250Cu);
    assert(TerminalCanvas_glyphAt(&canvas, 0, smallHeaderRow) == 0x2502u);
    assert(TerminalMarkdownView_rowScrollX(view, mixedHeaderRow)
           == TerminalMarkdownView_maxScrollX(view));
    assert(TerminalMarkdownView_setScrollX(view, 0u));

    static const char *numberWidthList[] = {
        "997. item 997", "1000. item 1000", "12345. item 12345",
        "1234567. item 1234567", "123456789012. item 123456789012",
    };
    assert(TerminalMarkdownView_setSource(view, numberWidthList,
                                          sizeof numberWidthList / sizeof numberWidthList[0]));
    assert(TerminalMarkdownView_setViewport(view, W));
    clearCanvas(&canvas, cells, styles, fg, bg);
    TerminalMarkdownView_render(view, &canvas);
    size_t itemColumn = W;
    size_t itemRows = 0;
    for (size_t row = 0; row < H; ++row)
        for (size_t column = 0; column + 3u < W; ++column)
            if (TerminalCanvas_glyphAt(&canvas, column, row) == 'i'
                && TerminalCanvas_glyphAt(&canvas, column + 1u, row) == 't'
                && TerminalCanvas_glyphAt(&canvas, column + 2u, row) == 'e'
                && TerminalCanvas_glyphAt(&canvas, column + 3u, row) == 'm') {
                if (itemColumn == W)
                    itemColumn = column;
                assert(column == itemColumn);
                ++itemRows;
            }
    assert(itemRows == 5u);

    static const char *narrow[] = { "A wide λ marker and a paragraph that must wrap." };
    assert(TerminalMarkdownView_setSource(view, narrow, 1));
    assert(TerminalMarkdownView_setViewport(view, 12));
    assert(TerminalMarkdownView_rowCount(view) > 1u);
    assert(TerminalMarkdownView_setVisibleRows(view, 2));
    assert(TerminalMarkdownView_setScroll(view, 0));
    clearCanvas(&canvas, cells, styles, fg, bg);
    TerminalMarkdownView_render(view, &canvas);
    assert(TerminalCanvas_glyphAt(&canvas, 0, 0) == 'A');

    // Emphasis is a style span, not printed punctuation: `*quiet*` is italic and
    // `**loud**` is bold, on the body pen.
    static const char *emphasis[] = { "plain *quiet* and **loud** text" };
    assert(TerminalMarkdownView_setSource(view, emphasis, 1));
    assert(TerminalMarkdownView_setViewport(view, W));
    clearCanvas(&canvas, cells, styles, fg, bg);
    TerminalMarkdownView_render(view, &canvas);
    bool sawItalic = false;
    bool sawBold = false;
    for (size_t row = 0; row < H; ++row)
        for (size_t column = 0; column + 5u < W; ++column) {
            if (TerminalCanvas_glyphAt(&canvas, column, row) == 'q'
                && TerminalCanvas_glyphAt(&canvas, column + 1u, row) == 'u') {
                for (size_t index = 0; index < 5u; ++index)
                    assert((styles[row * W + column + index] & TERMINAL_STYLE_ITALIC) != 0u);
                sawItalic = true;
            }
            if (TerminalCanvas_glyphAt(&canvas, column, row) == 'l'
                && TerminalCanvas_glyphAt(&canvas, column + 1u, row) == 'o'
                && TerminalCanvas_glyphAt(&canvas, column + 2u, row) == 'u') {
                for (size_t index = 0; index < 4u; ++index)
                    assert((styles[row * W + column + index] & TERMINAL_STYLE_BOLD) != 0u);
                sawBold = true;
            }
        }
    assert(sawItalic && sawBold);
    assert(!canvasContainsText(&canvas, "*quiet*"));

    // The heading ladder: four levels, four different pens. This is the whole
    // point of keeping six heading entries instead of one bold heading pen.
    static const char *headings[] = { "# one", "", "## two", "", "### three", "", "#### four" };
    // A caller that sets the legacy eight-pen style asks for one heading colour by
    // definition; this test wants the default ladder back.
    assert(TerminalMarkdownView_setTheme(view, TerminalMarkdownTheme_0()));
    assert(TerminalMarkdownView_setSource(view, headings,
                                          sizeof headings / sizeof headings[0]));
    assert(TerminalMarkdownView_setViewport(view, W));
    clearCanvas(&canvas, cells, styles, fg, bg);
    TerminalMarkdownView_render(view, &canvas);
    uint32_t headingFg[4];
    for (size_t level = 0; level < 4u; ++level) {
        size_t row = level * 2u + (level == 0 ? 0u : 1u);
        if (level == 0)
            row = 0;
        else
            row = level * 2u + 1u;
        headingFg[level] = fg[row * W];
        assert((styles[row * W] & TERMINAL_STYLE_BOLD) != 0u
               || (styles[row * W] & TERMINAL_STYLE_MUTED) != 0u);
    }
    assert(headingFg[0] != headingFg[1] && headingFg[1] != headingFg[2]
           && headingFg[2] != headingFg[3] && headingFg[0] != headingFg[3]);
    assert(TerminalMarkdownView_getTheme(view).heading[0].fg
           == TerminalMarkdownTheme_0().heading[0].fg);

    // A link is a hit target on the DOCUMENT row, and the target is the source
    // slice — the view reports it and touches no clipboard.
    static const char *linkLine[] = { "See [docs](https://example.test) now." };
    assert(TerminalMarkdownView_setSource(view, linkLine, 1));
    assert(TerminalMarkdownView_setViewport(view, W));
    TerminalMarkdownAction linkAction = {0};
    size_t linkRow = 0;
    size_t linkColumn = 0;
    for (size_t row = 0; row < TerminalMarkdownView_rowCount(view) && linkAction.kind == 0; ++row)
        for (size_t column = 0; column < W; ++column) {
            TerminalMarkdownAction candidate = TerminalMarkdownView_actionAt(view, row, column);
            if (candidate.kind == TERMINAL_MARKDOWN_ACTION_LINK) {
                linkAction = candidate;
                linkRow = row;
                linkColumn = column;
                break;
            }
        }
    assert(linkAction.kind == TERMINAL_MARKDOWN_ACTION_LINK);
    assert(linkAction.targetLength == strlen("https://example.test"));
    assert(memcmp(linkAction.target, "https://example.test", linkAction.targetLength) == 0);
    assert(TerminalMarkdownView_actionAt(view, linkRow, 0).kind
           == TERMINAL_MARKDOWN_ACTION_NONE);
    assert(TerminalMarkdownView_setHover(view, linkRow, linkColumn));
    assert(TerminalMarkdownView_actionAt(view, linkRow, linkColumn).kind
           == TERMINAL_MARKDOWN_ACTION_LINK);
    clearCanvas(&canvas, cells, styles, fg, bg);
    TerminalMarkdownView_render(view, &canvas);
    assert(fg[linkRow * W + linkColumn]
           == TerminalMarkdownView_getTheme(view).linkHover.fg);
    TerminalMarkdownView_clearHover(view);
    clearCanvas(&canvas, cells, styles, fg, bg);
    TerminalMarkdownView_render(view, &canvas);
    assert(fg[linkRow * W + linkColumn] == TerminalMarkdownView_getTheme(view).link.fg);
    assert(strstr(TerminalMarkdownView_toStringStruct(view).text, "hoverRow") != nullptr);

    // The copy affordance: a real icon on the fence's first row, a COPY_CODE
    // action under it, the source lines on demand, and a check once copied.
    assert(TerminalMarkdownView_setSource(view, shaded, sizeof shaded / sizeof shaded[0]));
    assert(TerminalMarkdownView_setViewport(view, W));
    TerminalMarkdownCodeBlock copyBlock = TerminalMarkdownView_codeBlockAt(view, 2);
    assert(copyBlock.valid && copyBlock.firstRow == 2u);
    TerminalMarkdownAction copyAction =
        TerminalMarkdownView_actionAt(view, copyBlock.firstRow, W - 2u);
    assert(copyAction.kind == TERMINAL_MARKDOWN_ACTION_COPY_CODE);
    assert(TerminalMarkdownView_actionAt(view, copyBlock.firstRow + 1u, W - 2u).kind
           == TERMINAL_MARKDOWN_ACTION_NONE);
    char copiedText[64];
    size_t copiedLength = 0;
    assert(TerminalMarkdownView_copyBlock(view, copyAction.block, copiedText,
                                          sizeof copiedText, &copiedLength));
    assert(copiedLength == strlen("int x = 1;\n\nint y = 2;"));
    assert(memcmp(copiedText, "int x = 1;\n\nint y = 2;", copiedLength) == 0);
    assert(!TerminalMarkdownView_copyBlock(view, 99, copiedText, sizeof copiedText, nullptr));
    clearCanvas(&canvas, cells, styles, fg, bg);
    TerminalMarkdownView_render(view, &canvas);
    assert(TerminalCanvas_glyphAt(&canvas, W - 2u, copyBlock.firstRow) == 0x29C9u);
    assert(TerminalMarkdownView_setCopied(view, copyAction.block, true));
    assert(TerminalMarkdownView_isCopied(view, copyAction.block));
    clearCanvas(&canvas, cells, styles, fg, bg);
    TerminalMarkdownView_render(view, &canvas);
    assert(TerminalCanvas_glyphAt(&canvas, W - 2u, copyBlock.firstRow) == 0x2713u);
    assert(TerminalMarkdownView_setCopied(view, copyAction.block, false));
    assert(!TerminalMarkdownView_isCopied(view, copyAction.block));

    // Table columns honour the delimiter row's alignment: the header fills the
    // column, the right-aligned body value ends on the column's right edge.
    static const char *alignedTable[] = {
        "| Left | Right |",
        "| :--- | ---: |",
        "| abc | xyz |",
        "| a \\| b.sh | c |",
    };
    assert(TerminalMarkdownView_setSource(view, alignedTable,
                                          sizeof alignedTable / sizeof alignedTable[0]));
    assert(TerminalMarkdownView_setViewport(view, W));
    clearCanvas(&canvas, cells, styles, fg, bg);
    TerminalMarkdownView_render(view, &canvas);
    assert(canvasContainsText(&canvas, "a | b.sh"));
    size_t rightStart = SIZE_MAX;
    size_t rightEnd = SIZE_MAX;
    size_t rightHeaderRow = SIZE_MAX;
    for (size_t row = 0; row < H && rightStart == SIZE_MAX; ++row)
        for (size_t column = 0; column + 5u <= W; ++column)
            if (TerminalCanvas_glyphAt(&canvas, column, row) == 'R'
                && TerminalCanvas_glyphAt(&canvas, column + 1u, row) == 'i'
                && TerminalCanvas_glyphAt(&canvas, column + 2u, row) == 'g'
                && TerminalCanvas_glyphAt(&canvas, column + 3u, row) == 'h'
                && TerminalCanvas_glyphAt(&canvas, column + 4u, row) == 't') {
                rightStart = column;
                rightEnd = column + 4u;
                rightHeaderRow = row;
                break;
            }
    assert(rightStart != SIZE_MAX);
    assert((styles[rightHeaderRow * W + rightStart] & TERMINAL_STYLE_BOLD) != 0u);
    bool sawAlignedValue = false;
    for (size_t row = rightHeaderRow + 1u; row < H; ++row)
        if (TerminalCanvas_glyphAt(&canvas, rightEnd, row) == 'z'
            && TerminalCanvas_glyphAt(&canvas, rightStart, row) == ' ') {
            sawAlignedValue = true;
            break;
        }
    assert(sawAlignedValue);

    beginDiagnostics();
    assert(!TerminalMarkdownView_setViewport(view, 0));
    endDiagnostics(1, "zero width");
    beginDiagnostics();
    assert(!TerminalMarkdownView_setSource(view, nullptr, 1));
    endDiagnostics(1, "null list");
    static const char *nullLine[] = { nullptr };
    beginDiagnostics();
    assert(!TerminalMarkdownView_setSource(view, nullLine, 1));
    endDiagnostics(1, "null line");
    assert(TerminalMarkdownView_isReady(view));
    assert(TerminalMarkdownView_rowCount(view) > 1u);
    assert(TerminalMarkdownView_exhaustionCount(view) == 0u);

    TerminalProjection projection = TerminalMarkdownView_toString(view);
    assert(projection.length != 0 && !projection.truncated);
    TerminalProjection structure = TerminalMarkdownView_toStringStruct(view);
    assert(strstr(structure.text, "rowCount") != nullptr);
    assert(strcmp(TerminalMarkdownView_toString(nullptr).text, "nullptr") == 0);
    TerminalMarkdownView_free(view);
    TerminalMarkdownView_free(nullptr);
    TerminalMarkdownView_clearExhaustion(nullptr);
    assert(TerminalMarkdownView_rowCount(nullptr) == 0u);
    assert(TerminalMarkdownView_getWidth(nullptr) == 0u);
    assert(TerminalMarkdownView_visibleRows(nullptr) == 0u);
    assert(TerminalMarkdownView_getScroll(nullptr) == 0u);
    assert(TerminalMarkdownView_maxScroll(nullptr) == 0u);
    assert(TerminalMarkdownView_exhaustionCount(nullptr) == 0u);
    assert(!TerminalMarkdownView_isReady(nullptr));
    assert(!TerminalMarkdownView_isTruncated(nullptr));
    TerminalMarkdownView_render(nullptr, &canvas);
    return 0;
}
