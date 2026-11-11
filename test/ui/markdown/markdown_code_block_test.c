// Owner: TerminalMarkdownCodeBlockPool — laid-out fences, their shaded rows and
// the copy projection that strips quote gutters. Atomic copy, no owned source.
#include "ui/markdown/markdown_code_block.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

int main(void) {
    TerminalMarkdownCodeBlockPool *pool = TerminalMarkdownCodeBlockPool();
    assert(pool != nullptr);
    assert(TerminalMarkdownCodeBlockPool_count(pool) == 0u);
    assert(TerminalMarkdownCodeBlockPool_exhaustionCount(pool) == 0u);

    static const char *lines[] = { "before", "```", "a", "b.sh", "```" };
    assert(TerminalMarkdownCodeBlockPool_add(pool, 2u, 3u, 2u, 2u, 0u));
    assert(TerminalMarkdownCodeBlockPool_count(pool) == 1u);
    assert(TerminalMarkdownCodeBlockPool_add(pool, 9u, 0u, 8u, 0u, 0u));
    assert(TerminalMarkdownCodeBlockPool_count(pool) == 1u);

    bytes index = 99u;
    assert(TerminalMarkdownCodeBlockPool_indexAt(pool, 2u, &index) && index == 0u);
    assert(TerminalMarkdownCodeBlockPool_indexAt(pool, 4u, &index));
    assert(!TerminalMarkdownCodeBlockPool_indexAt(pool, 5u, nullptr));
    assert(!TerminalMarkdownCodeBlockPool_indexAt(pool, 1u, nullptr));
    TerminalMarkdownCodeBlock block = TerminalMarkdownCodeBlockPool_at(pool, 0u);
    assert(block.valid && block.firstRow == 2u && block.rowCount == 3u);
    assert(block.firstSourceLine == 2u && block.sourceLineCount == 2u);
    assert(!TerminalMarkdownCodeBlockPool_at(pool, 7u).valid);

    TerminalMarkdownCodeLine line = TerminalMarkdownCodeBlockPool_line(pool, lines, 5u, 0u, 0u);
    assert(line.valid && line.length == 1u && line.text[0] == 'a');
    line = TerminalMarkdownCodeBlockPool_line(pool, lines, 5u, 0u, 1u);
    assert(line.valid && line.length == 1u && line.text[0] == 'b');
    assert(!TerminalMarkdownCodeBlockPool_line(pool, lines, 5u, 0u, 2u).valid);
    assert(!TerminalMarkdownCodeBlockPool_line(pool, lines, 5u, 3u, 0u).valid);
    // The same line resolved from a record the caller kept, no pool search.
    line = TerminalMarkdownCodeBlock_lineOf(lines, 5u, block, 1u);
    assert(line.valid && line.text[0] == 'b');
    assert(!TerminalMarkdownCodeBlock_lineOf(nullptr, 5u, block, 0u).valid);
    assert(!TerminalMarkdownCodeBlock_lineOf(lines, 5u, block, 7u).valid);

    // Copy is newline-separated and atomic: a short buffer writes nothing.
    char buffer[8];
    bytes written = 99u;
    assert(TerminalMarkdownCodeBlockPool_copy(pool, lines, 5u, 0u, buffer, sizeof buffer,
                                              &written));
    assert(written == 3u && memcmp(buffer, "a\nb.sh", 3u) == 0 && buffer[3] == '\0');
    memset(buffer, 'x', sizeof buffer);
    assert(!TerminalMarkdownCodeBlockPool_copy(pool, lines, 5u, 0u, buffer, 3u, &written));
    assert(written == 0u && buffer[0] == 'x');
    assert(!TerminalMarkdownCodeBlockPool_copy(pool, lines, 5u, 4u, buffer, sizeof buffer,
                                               nullptr));

    // A fence inside a quote keeps the depth so the copy strips `>` gutters.
    static const char *quoted[] = { "> ```", "> q", "> ```" };
    TerminalMarkdownCodeBlockPool_reset(pool);
    assert(TerminalMarkdownCodeBlockPool_add(pool, 0u, 1u, 1u, 1u, 1u));
    block = TerminalMarkdownCodeBlockPool_at(pool, 0u);
    assert(block.quoteDepth == 1u);
    line = TerminalMarkdownCodeBlockPool_line(pool, quoted, 3u, 0u, 0u);
    assert(line.valid && line.length == 1u && line.text[0] == 'q');
    assert(TerminalMarkdownCodeBlockPool_copy(pool, quoted, 3u, 0u, buffer, sizeof buffer,
                                              &written));
    assert(written == 1u && buffer[0] == 'q' && buffer[1] == '\0');

    TerminalMarkdownCodeBlockPool_reset(pool);
    assert(TerminalMarkdownCodeBlockPool_count(pool) == 0u);
    assert(TerminalMarkdownCodeBlockPool_exhaustionCount(pool) == 0u);

    TerminalProjection projection = TerminalMarkdownCodeBlockPool_toString(pool);
    assert(projection.length != 0 && !projection.truncated);
    TerminalProjection structure = TerminalMarkdownCodeBlockPool_toStringStruct(pool);
    assert(strstr(structure.text, "capacity") != nullptr);
    assert(strcmp(TerminalMarkdownCodeBlockPool_toString(nullptr).text, "nullptr") == 0);
    assert(TerminalMarkdownCodeBlockPool_count(nullptr) == 0u);
    assert(!TerminalMarkdownCodeBlockPool_add(nullptr, 0u, 1u, 0u, 1u, 0u));
    TerminalMarkdownCodeBlockPool_free(nullptr);
    TerminalMarkdownCodeBlockPool_free(pool);
    return 0;
}
