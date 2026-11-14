#include "ui/markdown/markdown_code_block.h"
#include "nio/mem.h"
#include "oop/type.h"
#include "exception/throw.h"
#include "annotation/annotation.h"
#include <string.h>

;;DEFINITION
/** The laid-out fenced code blocks of one document, and the copy projection that
 * turns a block back into the text the author wrote between the fences. A record
 * holds document rows for painting and source rows for copying; the two cannot
 * drift because they are written by the same emit. The pool grows cold, is reset
 * by a re-layout, and never owns the source bytes it names. */
;;OVERVIEW
/** ============================================================================
 * CLASS: TerminalMarkdownCodeBlockPool (ui/markdown/markdown_code_block.c)
 * STRUCT FIELDS
 *   TerminalMarkdownCodeBlock *blocks  owned record pool
 *   bytes count                       live records
 *   bytes capacity                    admitted slots
 *   bytes refusals                    cold growth refusals in this epoch
 * FUNCTION REGISTRY (exports)
 * Lifecycle: TerminalMarkdownCodeBlockPool_0/free/reset
 * Add:       TerminalMarkdownCodeBlockPool_add(...)
 * Query:     count/exhaustionCount/indexAt/at
 * Project:   line(pool, lines, lineCount, index, line)
 * Copy:      copy(pool, lines, lineCount, index, dest, capacity, written)
 * String:    toString/toStringStruct
 * CONTRACT: add refuses only on cold growth failure; indexAt is a binary search
 * over shaded rows; line/copy strip `quoteDepth` gutters and refuse a line that
 * does not carry them; copy is atomic and never allocates.
 * ============================================================================ */

#define CODE_BLOCK_POOL_DEFAULT 8u
#define CODE_BLOCK_POOL_LIMIT 100000u

struct TerminalMarkdownCodeBlockPool {
    TerminalMarkdownCodeBlock *blocks;
    bytes count;
    bytes capacity;
    bytes refusals;
};

static bool grow(TerminalMarkdownCodeBlockPool *self) {
    bytes next = (*self).capacity == 0 ? CODE_BLOCK_POOL_DEFAULT : (*self).capacity * 2u;
    if (next < (*self).capacity || next > CODE_BLOCK_POOL_LIMIT
        || next > SIZE_MAX / sizeof(TerminalMarkdownCodeBlock)) {
        ++(*self).refusals;
        THROW("TerminalMarkdownCodeBlockPool: capacity overflow (%zu)", next);
        return false;
    }
    TerminalMarkdownCodeBlock *grown =
        Memory_alloc(TYPE_BYTE_ARRAY, next * sizeof(TerminalMarkdownCodeBlock));
    if (grown == nullptr) {
        ++(*self).refusals;
        THROW("TerminalMarkdownCodeBlockPool: allocation refused (%zu slots)", next);
        return false;
    }
    if ((*self).capacity != 0)
        memcpy(grown, (*self).blocks,
               (*self).capacity * sizeof(TerminalMarkdownCodeBlock));
    Memory_free((*self).blocks);
    (*self).blocks = grown;
    (*self).capacity = next;
    return true;
}

TerminalMarkdownCodeBlockPool *TerminalMarkdownCodeBlockPool_0() {
    TerminalMarkdownCodeBlockPool *self =
        Memory_alloc(TYPE_BYTE_ARRAY, sizeof(TerminalMarkdownCodeBlockPool));
    if (self == nullptr)
        return nullptr;
    *self = (TerminalMarkdownCodeBlockPool) {0};
    return self;
}

fn TerminalMarkdownCodeBlockPool_free(TerminalMarkdownCodeBlockPool *self) {
    if (self == nullptr)
        return;
    Memory_free((*self).blocks);
    Memory_free(self);
}

fn TerminalMarkdownCodeBlockPool_reset(TerminalMarkdownCodeBlockPool *self) {
    if (self != nullptr)
        (*self).count = 0;
}

bool TerminalMarkdownCodeBlockPool_add(TerminalMarkdownCodeBlockPool *self,
                                       bytes firstRow, bytes rowCount,
                                       bytes firstSourceLine, bytes sourceLineCount,
                                       bytes quoteDepth) {
    if (self == nullptr)
        return false;
    if (rowCount == 0)
        return true;
    if ((*self).count == (*self).capacity && !grow(self))
        return false;
    (*self).blocks[(*self).count] = (TerminalMarkdownCodeBlock) {
        true, firstRow, rowCount, firstSourceLine, sourceLineCount, quoteDepth
    };
    ++(*self).count;
    return true;
}

bytes TerminalMarkdownCodeBlockPool_count(const TerminalMarkdownCodeBlockPool *self) {
    return self != nullptr ? (*self).count : 0u;
}

bytes TerminalMarkdownCodeBlockPool_exhaustionCount(const TerminalMarkdownCodeBlockPool *self) {
    return self != nullptr ? (*self).refusals : 0u;
}

bool TerminalMarkdownCodeBlockPool_indexAt(const TerminalMarkdownCodeBlockPool *self,
                                           bytes row, bytes *index) {
    if (self == nullptr)
        return false;
    bytes low = 0;
    bytes high = (*self).count;
    while (low < high) {
        bytes mid = low + (high - low) / 2u;
        const TerminalMarkdownCodeBlock *block = &(*self).blocks[mid];
        if (row < (*block).firstRow) {
            high = mid;
            continue;
        }
        if (row - (*block).firstRow >= (*block).rowCount) {
            low = mid + 1u;
            continue;
        }
        if (index != nullptr)
            *index = mid;
        return true;
    }
    return false;
}

TerminalMarkdownCodeBlock TerminalMarkdownCodeBlockPool_at(const TerminalMarkdownCodeBlockPool *self,
                                                           bytes index) {
    TerminalMarkdownCodeBlock none = {0};
    if (self == nullptr || index >= (*self).count)
        return none;
    return (*self).blocks[index];
}

// Byte offset just past `depth` `>` gutters, or SIZE_MAX when the line does not
// carry them. One optional space after each `>` belongs to the gutter.
static bytes quoteContentStart(const char *line, bytes length, bytes depth) {
    bytes at = 0;
    for (bytes level = 0; level < depth; ++level) {
        if (at >= length || line[at] != '>')
            return SIZE_MAX;
        ++at;
        if (at < length && line[at] == ' ')
            ++at;
    }
    return at;
}

static bool contentLine(const char *const *lines, bytes lineCount,
                        TerminalMarkdownCodeBlock block, bytes line,
                        const char **text, bytes *length) {
    if (lines == nullptr || !block.valid || line >= block.sourceLineCount)
        return false;
    if (block.firstSourceLine > SIZE_MAX - line)
        return false;
    bytes sourceLine = block.firstSourceLine + line;
    if (sourceLine >= lineCount)
        return false;
    const char *lineText = lines[sourceLine];
    bytes total = strlen(lineText);
    bytes start = 0;
    if (block.quoteDepth != 0) {
        start = quoteContentStart(lineText, total, block.quoteDepth);
        if (start == SIZE_MAX)
            return false;
    }
    if (start > total)
        start = total;
    *text = lineText + start;
    *length = total - start;
    return true;
}

TerminalMarkdownCodeLine TerminalMarkdownCodeBlock_lineOf(const char *const *lines,
                                                          bytes lineCount,
                                                          TerminalMarkdownCodeBlock block,
                                                          bytes line) {
    TerminalMarkdownCodeLine none = {0};
    const char *text = nullptr;
    bytes length = 0;
    if (!contentLine(lines, lineCount, block, line, &text, &length))
        return none;
    return (TerminalMarkdownCodeLine) { true, text, length };
}

TerminalMarkdownCodeLine TerminalMarkdownCodeBlockPool_line(const TerminalMarkdownCodeBlockPool *self,
                                                            const char *const *lines,
                                                            bytes lineCount, bytes index,
                                                            bytes line) {
    if (self == nullptr || index >= (*self).count)
        return (TerminalMarkdownCodeLine) {0};
    return TerminalMarkdownCodeBlock_lineOf(lines, lineCount, (*self).blocks[index], line);
}

bool TerminalMarkdownCodeBlockPool_copy(const TerminalMarkdownCodeBlockPool *self,
                                        const char *const *lines, bytes lineCount,
                                        bytes index, char *dest, bytes capacity,
                                        bytes *written) {
    if (written != nullptr)
        *written = 0;
    if (self == nullptr || dest == nullptr || capacity == 0 || index >= (*self).count)
        return false;
    const TerminalMarkdownCodeBlock *block = &(*self).blocks[index];
    if ((*block).sourceLineCount == 0) {
        if (capacity < 1u)
            return false;
        dest[0] = '\0';
        return true;
    }
    bytes total = 0;
    for (bytes line = 0; line < (*block).sourceLineCount; ++line) {
        const char *text = nullptr;
        bytes length = 0;
        if (!contentLine(lines, lineCount, *block, line, &text, &length))
            return false;
        if (length > SIZE_MAX - total - 1u)
            return false;
        total += length + 1u;   // content plus the newline that follows it
    }
    if (total > capacity)       // total includes the last newline, which the
        return false;           // terminator below replaces
    bytes at = 0;
    for (bytes line = 0; line < (*block).sourceLineCount; ++line) {
        const char *text = nullptr;
        bytes length = 0;
        if (!contentLine(lines, lineCount, *block, line, &text, &length))
            return false;
        if (length != 0)
            memcpy(dest + at, text, length);
        at += length;
        if (line + 1u < (*block).sourceLineCount)
            dest[at++] = '\n';
    }
    dest[at] = '\0';
    if (written != nullptr)
        *written = at;
    return true;
}

TerminalProjection TerminalMarkdownCodeBlockPool_toString(const TerminalMarkdownCodeBlockPool *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format("TerminalMarkdownCodeBlockPool{blocks=%zu}", (*self).count);
}

TerminalProjection TerminalMarkdownCodeBlockPool_toStringStruct(const TerminalMarkdownCodeBlockPool *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format(
        "TerminalMarkdownCodeBlockPool{blocks=<owned:%zu>,count=%zu,capacity=%zu,refusals=%zu}",
        (*self).capacity, (*self).count, (*self).capacity, (*self).refusals);
}
