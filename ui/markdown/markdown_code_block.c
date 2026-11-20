#include "vex.h"
#include "ui/markdown/markdown_code_block.h"
#include "nio/mem.h"
#include "oop/oop.h"
#include <string.h>
#include "core/determinant_memory.h"

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


;;GETTER
static inline bytes TerminalMarkdownCodeBlock_firstRow(const TerminalMarkdownCodeBlock *s) { return (*s).firstRow; }
;;GETTER
static inline bytes *TerminalMarkdownCodeBlock_firstRowMut(TerminalMarkdownCodeBlock *s) { return &(*s).firstRow; }
;;SETTER
static inline fn TerminalMarkdownCodeBlock_setFirstRow(TerminalMarkdownCodeBlock *s, bytes v) { (*s).firstRow = v; }
;;GETTER
static inline bytes TerminalMarkdownCodeBlock_rowCount(const TerminalMarkdownCodeBlock *s) { return (*s).rowCount; }
;;GETTER
static inline bytes *TerminalMarkdownCodeBlock_rowCountMut(TerminalMarkdownCodeBlock *s) { return &(*s).rowCount; }
;;SETTER
static inline fn TerminalMarkdownCodeBlock_setRowCount(TerminalMarkdownCodeBlock *s, bytes v) { (*s).rowCount = v; }
;;GETTER
static inline bytes TerminalMarkdownCodeBlock_sourceLineCount(const TerminalMarkdownCodeBlock *s) { return (*s).sourceLineCount; }
;;GETTER
static inline bytes *TerminalMarkdownCodeBlock_sourceLineCountMut(TerminalMarkdownCodeBlock *s) { return &(*s).sourceLineCount; }
;;SETTER
static inline fn TerminalMarkdownCodeBlock_setSourceLineCount(TerminalMarkdownCodeBlock *s, bytes v) { (*s).sourceLineCount = v; }

#define CODE_BLOCK_POOL_DEFAULT 8u
#define CODE_BLOCK_POOL_LIMIT 100000u

struct TerminalMarkdownCodeBlockPool {
    TerminalMarkdownCodeBlock *blocks;
    bytes count;
    bytes capacity;
    bytes refusals;
};

;;GETTER
static inline TerminalMarkdownCodeBlock * TerminalMarkdownCodeBlockPool_blocks(const TerminalMarkdownCodeBlockPool *s) { return (*s).blocks; }
;;GETTER
static inline TerminalMarkdownCodeBlock * *TerminalMarkdownCodeBlockPool_blocksMut(TerminalMarkdownCodeBlockPool *s) { return &(*s).blocks; }
;;SETTER
static inline fn TerminalMarkdownCodeBlockPool_setBlocks(TerminalMarkdownCodeBlockPool *s, TerminalMarkdownCodeBlock * v) { (*s).blocks = v; }
;;GETTER
static inline bytes TerminalMarkdownCodeBlockPool_capacity(const TerminalMarkdownCodeBlockPool *s) { return (*s).capacity; }
;;GETTER
static inline bytes *TerminalMarkdownCodeBlockPool_capacityMut(TerminalMarkdownCodeBlockPool *s) { return &(*s).capacity; }
;;SETTER
static inline fn TerminalMarkdownCodeBlockPool_setCapacity(TerminalMarkdownCodeBlockPool *s, bytes v) { (*s).capacity = v; }
;;GETTER
static inline bytes TerminalMarkdownCodeBlockPool_count(const TerminalMarkdownCodeBlockPool *s) { return (*s).count; }
;;GETTER
static inline bytes *TerminalMarkdownCodeBlockPool_countMut(TerminalMarkdownCodeBlockPool *s) { return &(*s).count; }
;;SETTER
static inline fn TerminalMarkdownCodeBlockPool_setCount(TerminalMarkdownCodeBlockPool *s, bytes v) { (*s).count = v; }
;;GETTER
static inline bytes TerminalMarkdownCodeBlockPool_refusals(const TerminalMarkdownCodeBlockPool *s) { return (*s).refusals; }
;;GETTER
static inline bytes *TerminalMarkdownCodeBlockPool_refusalsMut(TerminalMarkdownCodeBlockPool *s) { return &(*s).refusals; }
;;SETTER
static inline fn TerminalMarkdownCodeBlockPool_setRefusals(TerminalMarkdownCodeBlockPool *s, bytes v) { (*s).refusals = v; }


static bool grow(TerminalMarkdownCodeBlockPool *self) {
    bytes next = TerminalMarkdownCodeBlockPool_capacity(self) == 0 ? CODE_BLOCK_POOL_DEFAULT : TerminalMarkdownCodeBlockPool_capacity(self) * 2u;
    if (next < TerminalMarkdownCodeBlockPool_capacity(self) || next > CODE_BLOCK_POOL_LIMIT
        || next > SIZE_MAX / sizeof(TerminalMarkdownCodeBlock)) {
        TerminalMarkdownCodeBlockPool_setRefusals(self, TerminalMarkdownCodeBlockPool_refusals(self) + 1u);
        THROW("TerminalMarkdownCodeBlockPool: capacity overflow (%zu)", next);
        return false;
    }
    TerminalMarkdownCodeBlock *grown =
        Memory_alloc(TYPE_BYTE_ARRAY, Determinant_arena(), next * sizeof(TerminalMarkdownCodeBlock));
    if (grown == nullptr) {
        TerminalMarkdownCodeBlockPool_setRefusals(self, TerminalMarkdownCodeBlockPool_refusals(self) + 1u);
        THROW("TerminalMarkdownCodeBlockPool: allocation refused (%zu slots)", next);
        return false;
    }
    if (TerminalMarkdownCodeBlockPool_capacity(self) != 0)
        memcpy(grown, TerminalMarkdownCodeBlockPool_blocks(self),
               TerminalMarkdownCodeBlockPool_capacity(self) * sizeof(TerminalMarkdownCodeBlock));
    Memory_free(TerminalMarkdownCodeBlockPool_blocks(self));
    TerminalMarkdownCodeBlockPool_setBlocks(self, grown);
    TerminalMarkdownCodeBlockPool_setCapacity(self, next);
    return true;
}

TerminalMarkdownCodeBlockPool *TerminalMarkdownCodeBlockPool_0() {
    TerminalMarkdownCodeBlockPool *self =
        Memory_alloc(TYPE_BYTE_ARRAY, Determinant_arena(), sizeof(TerminalMarkdownCodeBlockPool));
    if (self == nullptr)
        return nullptr;
    *self = (TerminalMarkdownCodeBlockPool) {0};
    return self;
}

fn TerminalMarkdownCodeBlockPool_free(TerminalMarkdownCodeBlockPool *self) {
    if (self == nullptr)
        return;
    Memory_free(TerminalMarkdownCodeBlockPool_blocks(self));
    Memory_free(self);
}

fn TerminalMarkdownCodeBlockPool_reset(TerminalMarkdownCodeBlockPool *self) {
    if (self != nullptr)
        TerminalMarkdownCodeBlockPool_setCount(self, 0);
}

bool TerminalMarkdownCodeBlockPool_add(TerminalMarkdownCodeBlockPool *self,
                                       bytes firstRow, bytes rowCount,
                                       bytes firstSourceLine, bytes sourceLineCount,
                                       bytes quoteDepth) {
    if (self == nullptr)
        return false;
    if (rowCount == 0)
        return true;
    if (TerminalMarkdownCodeBlockPool_count(self) == TerminalMarkdownCodeBlockPool_capacity(self) && !grow(self))
        return false;
    TerminalMarkdownCodeBlockPool_blocks(self)[TerminalMarkdownCodeBlockPool_count(self)] = (TerminalMarkdownCodeBlock) {
        true, firstRow, rowCount, firstSourceLine, sourceLineCount, quoteDepth
    };
    TerminalMarkdownCodeBlockPool_setCount(self, TerminalMarkdownCodeBlockPool_count(self) + 1u);
    return true;
}

bytes TerminalMarkdownCodeBlockPool_count(const TerminalMarkdownCodeBlockPool *self) {
    return self != nullptr ? TerminalMarkdownCodeBlockPool_count(self) : 0u;
}

bytes TerminalMarkdownCodeBlockPool_exhaustionCount(const TerminalMarkdownCodeBlockPool *self) {
    return self != nullptr ? TerminalMarkdownCodeBlockPool_refusals(self) : 0u;
}

bool TerminalMarkdownCodeBlockPool_indexAt(const TerminalMarkdownCodeBlockPool *self,
                                           bytes row, bytes *index) {
    if (self == nullptr)
        return false;
    bytes low = 0;
    bytes high = TerminalMarkdownCodeBlockPool_count(self);
    while (low < high) {
        bytes mid = low + (high - low) / 2u;
        const TerminalMarkdownCodeBlock *block = &TerminalMarkdownCodeBlockPool_blocksMut(self)[mid];
        if (row < TerminalMarkdownCodeBlock_firstRow(block)) {
            high = mid;
            continue;
        }
        if (row - TerminalMarkdownCodeBlock_firstRow(block) >= TerminalMarkdownCodeBlock_rowCount(block)) {
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
    if (self == nullptr || index >= TerminalMarkdownCodeBlockPool_count(self))
        return none;
    return TerminalMarkdownCodeBlockPool_blocks(self)[index];
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
    if (self == nullptr || index >= TerminalMarkdownCodeBlockPool_count(self))
        return (TerminalMarkdownCodeLine) {0};
    return TerminalMarkdownCodeBlock_lineOf(lines, lineCount, TerminalMarkdownCodeBlockPool_blocks(self)[index], line);
}

bool TerminalMarkdownCodeBlockPool_copy(const TerminalMarkdownCodeBlockPool *self,
                                        const char *const *lines, bytes lineCount,
                                        bytes index, char *dest, bytes capacity,
                                        bytes *written) {
    if (written != nullptr)
        *written = 0;
    if (self == nullptr || dest == nullptr || capacity == 0 || index >= TerminalMarkdownCodeBlockPool_count(self))
        return false;
    const TerminalMarkdownCodeBlock *block = &TerminalMarkdownCodeBlockPool_blocksMut(self)[index];
    if (TerminalMarkdownCodeBlock_sourceLineCount(block) == 0) {
        if (capacity < 1u)
            return false;
        dest[0] = '\0';
        return true;
    }
    bytes total = 0;
    for (bytes line = 0; line < TerminalMarkdownCodeBlock_sourceLineCount(block); ++line) {
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
    for (bytes line = 0; line < TerminalMarkdownCodeBlock_sourceLineCount(block); ++line) {
        const char *text = nullptr;
        bytes length = 0;
        if (!contentLine(lines, lineCount, *block, line, &text, &length))
            return false;
        if (length != 0)
            memcpy(dest + at, text, length);
        at += length;
        if (line + 1u < TerminalMarkdownCodeBlock_sourceLineCount(block))
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
    return TerminalProjection_format("TerminalMarkdownCodeBlockPool{blocks=%zu}", TerminalMarkdownCodeBlockPool_count(self));
}

TerminalProjection TerminalMarkdownCodeBlockPool_toStringStruct(const TerminalMarkdownCodeBlockPool *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format(
        "TerminalMarkdownCodeBlockPool{blocks=<owned:%zu>,count=%zu,capacity=%zu,refusals=%zu}",
        TerminalMarkdownCodeBlockPool_capacity(self), TerminalMarkdownCodeBlockPool_count(self), TerminalMarkdownCodeBlockPool_capacity(self), TerminalMarkdownCodeBlockPool_refusals(self));
}
