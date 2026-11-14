// Owner: TerminalMarkdownText — the one cell-measurement arithmetic every
// Markdown owner shares. Wide scalars, malformed bytes, search and trimming.

#include "vex.h"
#include "ui/markdown/markdown_text.h"
#include <assert.h>
#include <string.h>

int main() {
    // ASCII is one cell per byte; a CJK scalar is two; a combining mark is zero.
    assert(TerminalMarkdownText_cells("abc", 3) == 3u);
    assert(TerminalMarkdownText_cells("λ", 2) == 1u);
    assert(TerminalMarkdownText_cells("日本", 6) == 4u);
    assert(TerminalMarkdownText_cells("e\xCC\x81", 3) == 1u);
    assert(TerminalMarkdownText_cells(nullptr, 9) == 0u);

    // A wide glyph never splits across the cell boundary; a combining mark rides
    // with the scalar it accents.
    bytes used = 0;
    assert(TerminalMarkdownText_bytesForCells("abc", 3, 2, &used) == 2u && used == 2u);
    assert(TerminalMarkdownText_bytesForCells("日本", 6, 1, &used) == 0u && used == 0u);
    assert(TerminalMarkdownText_bytesForCells("e\xCC\x81x", 4, 1, &used) == 3u && used == 1u);
    assert(TerminalMarkdownText_bytesForCells(nullptr, 0, 4, nullptr) == 0u);

    // Malformed UTF-8 is one honest replacement cell, one byte at a time.
    assert(TerminalMarkdownText_cells("\xFF\xFE", 2) == 2u);
    assert(TerminalMarkdownText_scalarBytes("\xFF", 1) == 1u);
    assert(TerminalMarkdownText_scalarBytes("日", 3) == 3u);
    assert(TerminalMarkdownText_scalarBytes("abc", 0) == 0u);

    // Search is byte-exact and reports SIZE_MAX for absence, including a needle
    // longer than the remaining span.
    assert(TerminalMarkdownText_find("a|b.sh|c", 5, 0, "|", 1) == 1u);
    assert(TerminalMarkdownText_find("a|b.sh|c", 5, 2, "|", 1) == 3u);
    assert(TerminalMarkdownText_find("a|b.sh|c", 5, 4, "|", 1) == SIZE_MAX);
    assert(TerminalMarkdownText_find("aaa", 3, 0, "aaaa", 4) == SIZE_MAX);
    assert(TerminalMarkdownText_findRun("a```b.sh", 5, 0, '`', 3) == 1u);
    assert(TerminalMarkdownText_findRun("````", 4, 0, '`', 3) == SIZE_MAX);
    assert(TerminalMarkdownText_findRun("````", 4, 0, '`', 0) == SIZE_MAX);

    assert(TerminalMarkdownText_isSpace(' ') && TerminalMarkdownText_isSpace('\t'));
    assert(!TerminalMarkdownText_isSpace('\n'));
    assert(TerminalMarkdownText_isPunctuation('[') && TerminalMarkdownText_isPunctuation('~'));
    assert(!TerminalMarkdownText_isPunctuation('a'));

    assert(TerminalMarkdownText_leadingSpaces("  x", 3) == 2u);
    assert(TerminalMarkdownText_trimRight("x  ", 3) == 1u);
    assert(TerminalMarkdownText_trimRight("   ", 3) == 0u);
    assert(TerminalMarkdownText_blank(" \t ", 3));
    assert(TerminalMarkdownText_blank(nullptr, 4));
    assert(!TerminalMarkdownText_blank(" x ", 3));
    return 0;
}
