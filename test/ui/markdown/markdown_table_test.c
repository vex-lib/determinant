// Owner: TerminalMarkdownTable — delimiter alignment, escape- and code-aware
// splitting, natural/longest-word measurement, honest truncation and width
// allocation.
#include "ui/markdown/markdown_table.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

int main(void) {
    TerminalMarkdownTableAlign aligns[TERMINAL_MARKDOWN_TABLE_COLUMNS] = {0};
    size_t columns = TerminalMarkdownTable_parseDelimiter("| :--- | :---: | ---: |",
                                                          strlen("| :--- | :---: | ---: |"),
                                                          aligns, 8u);
    assert(columns == 3u);
    assert(aligns[0] == TERMINAL_MARKDOWN_TABLE_LEFT);
    assert(aligns[1] == TERMINAL_MARKDOWN_TABLE_CENTER);
    assert(aligns[2] == TERMINAL_MARKDOWN_TABLE_RIGHT);
    assert(TerminalMarkdownTable_parseDelimiter("--- | ---", strlen("--- | ---"), aligns, 8u) == 2u);
    assert(TerminalMarkdownTable_parseDelimiter("| --- |", strlen("| --- |"), aligns, 8u) == 0u);
    assert(TerminalMarkdownTable_parseDelimiter("| x | --- |", strlen("| x | --- |"), aligns, 8u) == 0u);
    assert(TerminalMarkdownTable_parseDelimiter("| --- | :: |", strlen("| --- | :: |"), aligns, 8u) == 0u);
    assert(TerminalMarkdownTable_parseDelimiter("| --- | -- |", strlen("| --- | -- |"),
                                                nullptr, 0u) == 2u);

    // A literal pipe is escaped or inside a code run; neither may invent a
    // column. The optional outer pipes are not cells.
    const char *cells[TERMINAL_MARKDOWN_TABLE_COLUMNS];
    size_t lengths[TERMINAL_MARKDOWN_TABLE_COLUMNS];
    const char *row = "| a \\| b.sh | `x|y` |";
    size_t count = TerminalMarkdownTable_split(row, strlen(row), cells, lengths, 8u);
    assert(count == 2u);
    assert(lengths[0] == strlen("a \\| b.sh") && memcmp(cells[0], "a \\| b.sh", lengths[0]) == 0);
    assert(lengths[1] == strlen("`x|y`") && memcmp(cells[1], "`x|y`", lengths[1]) == 0);
    row = "a | b.sh || c";
    count = TerminalMarkdownTable_split(row, strlen(row), cells, lengths, 8u);
    assert(count == 4u);
    assert(lengths[1] == 1u && cells[1][0] == 'b');
    assert(lengths[2] == 0u);
    assert(lengths[3] == 1u && cells[3][0] == 'c');
    assert(TerminalMarkdownTable_split("| a | b.sh |", strlen("| a | b.sh |"), cells, lengths,
                                       1u) == 2u);
    assert(TerminalMarkdownTable_split(nullptr, 0, cells, lengths, 8u) == 0u);

    assert(TerminalMarkdownTable_natural("日本", 6) == 4u);
    assert(TerminalMarkdownTable_longestWord("ab cdef g", 9) == 4u);
    assert(TerminalMarkdownTable_longestWord("", 0) == 1u);
    assert(TerminalMarkdownTable_lineCount("ab cd", 5, 8u) == 1u);
    assert(TerminalMarkdownTable_lineCount("ab cd ef", 8, 3u) == 3u);

    // A word break leaves the following word whole; a word too wide for the
    // column is cut with the ellipsis cell handed back to the caller.
    TerminalMarkdownTableCellLine line = TerminalMarkdownTable_cellLine("ab cd", 5, 0, 3u);
    assert(!line.truncated && line.cells == 2u && line.length == 2u && line.next == 3u);
    line = TerminalMarkdownTable_cellLine("abcdefgh", 8, 0, 4u);
    assert(!line.truncated && line.trailingHyphen);
    assert(line.cells == 4u && line.length == 3u && line.next == 3u);
    line = TerminalMarkdownTable_cellLine("ab cd", 5, 3, 3u);
    assert(!line.truncated && line.cells == 2u && line.length == 2u && line.next == 5u);
    line = TerminalMarkdownTable_cellLine("", 0, 0, 3u);
    assert(!line.truncated && line.cells == 0u && line.length == 0u);

    // Natural widths when they fit, minimums plus slack when they do not, and a
    // refusal — never a zero-width column — when even the minimums overflow.
    size_t needed[4] = { 10u, 10u, 1u, 1u };
    size_t minimums[4] = { 3u, 3u, 1u, 1u };
    size_t widths[4] = {0};
    assert(TerminalMarkdownTable_allocate(needed, minimums, widths, 2u, 30u));
    assert(widths[0] == 10u && widths[1] == 10u);
    // The 4-cell spacing floor lands when the padded table still fits, and the exact
    // natural widths are kept when the padding would overflow the viewport.
    assert(TerminalMarkdownTable_allocate(needed, minimums, widths, 2u, 31u));
    assert(widths[0] == 12u && widths[1] == 12u);
    assert(TerminalMarkdownTable_allocate(needed, minimums, widths, 2u, 30u));
    assert(widths[0] == 10u && widths[1] == 10u);
    assert(TerminalMarkdownTable_allocate(needed, minimums, widths, 2u, 15u));
    assert(widths[0] == 4u && widths[1] == 4u);
    assert(!TerminalMarkdownTable_allocate(needed, minimums, widths, 2u, 10u));
    assert(!TerminalMarkdownTable_allocate(nullptr, minimums, widths, 2u, 30u));
    assert(!TerminalMarkdownTable_allocate(needed, minimums, widths, 0u, 30u));

    // Warping policy: a table is warped when any column needing room is squeezed below
    // the threshold (default 30). Unwarped allocation preserves natural needed widths.
    assert(TerminalMarkdownTable_isWarped(needed, widths, 2u, 30u));
    widths[0] = 10u;
    widths[1] = 10u;
    assert(!TerminalMarkdownTable_isWarped(needed, widths, 2u, 30u));
    assert(!TerminalMarkdownTable_isWarped(nullptr, widths, 2u, 30u));
    assert(!TerminalMarkdownTable_isWarped(needed, widths, 0u, 30u));

    size_t unwarpedWidths[4] = {0};
    assert(TerminalMarkdownTable_allocateUnwarped(needed, unwarpedWidths, 2u) == 31u);
    assert(unwarpedWidths[0] == 12u && unwarpedWidths[1] == 12u);
    assert(TerminalMarkdownTable_allocateUnwarped(nullptr, unwarpedWidths, 0u) == 0u);

    // Spacing policy: one column's content width is a whole number of 4-cell steps,
    // so the borders sit on a stable grid (21 -> 24, an empty column still gets 4).
    assert(TerminalMarkdownTable_padWidth(0u) == 4u);
    assert(TerminalMarkdownTable_padWidth(1u) == 4u);
    assert(TerminalMarkdownTable_padWidth(4u) == 4u);
    assert(TerminalMarkdownTable_padWidth(5u) == 8u);
    assert(TerminalMarkdownTable_padWidth(21u) == 24u);
    assert(TerminalMarkdownTable_padWidth(SIZE_MAX) % 4u == 0u);
    return 0;
}
