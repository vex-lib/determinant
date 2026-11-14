#include "vex.h"
#include "ui/markdown/markdown_table.h"
#include "ui/markdown/markdown_text.h"
#include "ui/surface/terminal_canvas.h"

;;DEFINITION
/** The pipe-table model: alignment from the delimiter row, escape- and
 * code-aware cell splitting, and a width policy that prefers each column's
 * natural width, falls back to its longest word, and only then truncates inside
 * a word — and when it truncates it says so, so the caller can draw an ellipsis
 * instead of pretending the cell ended there. No terminal, no pen, no owned
 * memory. */
;;OVERVIEW
/** ============================================================================
 * CLASS: TerminalMarkdownTable (ui/markdown/markdown_table.c)
 * STRUCT FIELDS: none — the cell-line value record is declared in the header.
 * FUNCTION REGISTRY (exports)
 * Delimiter: parseDelimiter(text, length, aligns, capacity)
 * Row:       split(text, length, cells, lengths, capacity)
 * Measure:   natural(text, length); longestWord(text, length)
 *            lineCount(text, length, width)
 * Wrap:      cellLine(text, length, offset, width)
 * Allocate:  allocate(needed, minimums, widths, columns, viewport)
 *            allocateUnwarped(needed, widths, columns); padWidth(width)
 * Warp:      isWarped(needed, widths, columns, threshold)
 * CONTRACT: `\|` and pipes inside a backtick run never split a cell; an optional
 * outer pipe is ignored; an empty interior cell is a real column; truncation
 * leaves one cell free for the caller's ellipsis. Column content widths are
 * raised to a multiple of 4 whenever the raised table still fits its fit
 * budget, so the borders sit on a 4-cell grid; otherwise the budget wins and
 * the exact widths stand.
 * ============================================================================ */

static bytes cellTrimStart(const char *text, bytes start, bytes end) {
    while (start < end && TerminalMarkdownText_isSpace(text[start]))
        ++start;
    return start;
}

static bytes cellTrimEnd(const char *text, bytes start, bytes end) {
    while (end > start && TerminalMarkdownText_isSpace(text[end - 1u]))
        --end;
    return end;
}

static bytes skipCodeRun(const char *text, bytes at, bytes end) {
    bytes run = 0;
    while (at + run < end && text[at + run] == '`')
        ++run;
    bytes close = TerminalMarkdownText_findRun(text, end, at + run, '`', run);
    return close == SIZE_MAX ? at + run : close + run;
}

bytes TerminalMarkdownTable_split(const char *text, bytes length, const char **cells,
                           bytes *lengths, bytes capacity) {
    if (text == nullptr || cells == nullptr || lengths == nullptr || capacity == 0)
        return 0;
    bytes start = TerminalMarkdownText_leadingSpaces(text, length);
    bytes end = TerminalMarkdownText_trimRight(text, length);
    if (start < end && text[start] == '|')
        ++start;
    if (end > start && text[end - 1u] == '|')
        --end;
    bytes count = 0;
    bytes cellStart = start;
    bytes at = start;
    while (at <= end) {
        bool boundary = false;
        if (at == end) {
            boundary = true;
        } else if (text[at] == '\\' && at + 1u < end) {
            at += 2u;
            continue;
        } else if (text[at] == '`') {
            bytes next = skipCodeRun(text, at, end);
            at = next > at ? next : at + 1u;
            continue;
        } else if (text[at] == '|') {
            boundary = true;
        }
        if (!boundary) {
            ++at;
            continue;
        }
        bytes trimmedStart = cellTrimStart(text, cellStart, at);
        bytes trimmedEnd = cellTrimEnd(text, trimmedStart, at);
        if (count < capacity) {
            cells[count] = text + trimmedStart;
            lengths[count] = trimmedEnd - trimmedStart;
        }
        ++count;
        if (at == end)
            break;
        cellStart = at + 1u;
        at = cellStart;
    }
    return count;
}

bytes TerminalMarkdownTable_parseDelimiter(const char *text, bytes length,
                                    TerminalMarkdownTableAlign *aligns, bytes capacity) {
    const char *cells[TERMINAL_MARKDOWN_TABLE_COLUMNS];
    bytes lengths[TERMINAL_MARKDOWN_TABLE_COLUMNS];
    bytes count = TerminalMarkdownTable_split(text, length, cells, lengths, TERMINAL_MARKDOWN_TABLE_COLUMNS);
    if (count < 2u)
        return 0;
    for (bytes column = 0; column < count; ++column) {
        bool leftColon = false;
        bool rightColon = false;
        bytes dashes = 0;
        for (bytes at = 0; at < lengths[column]; ++at) {
            char character = cells[column][at];
            if (character == ':') {
                if (at == 0)
                    leftColon = true;
                else if (at + 1u == lengths[column])
                    rightColon = true;
                else
                    return 0;
            } else if (character == '-') {
                ++dashes;
            } else {
                return 0;
            }
        }
        if (dashes == 0)
            return 0;
        if (aligns != nullptr && column < capacity)
            aligns[column] = leftColon && rightColon ? TERMINAL_MARKDOWN_TABLE_CENTER
                           : rightColon ? TERMINAL_MARKDOWN_TABLE_RIGHT : TERMINAL_MARKDOWN_TABLE_LEFT;
    }
    return count;
}

bytes TerminalMarkdownTable_natural(const char *text, bytes length) {
    return TerminalMarkdownText_cells(text, length);
}

bytes TerminalMarkdownTable_longestWord(const char *text, bytes length) {
    bytes longest = 0;
    bytes word = 0;
    bytes at = 0;
    while (text != nullptr && at < length) {
        TerminalCanvasDecodedScalar decoded = TerminalCanvas_utf8Decode(text + at, length - at);
        bytes bytes = decoded.bytes == 0 ? 1u : decoded.bytes;
        if (decoded.bytes == 0 || TerminalMarkdownText_isSpace(text[at])) {
            if (word > longest)
                longest = word;
            word = 0;
        } else {
            word += TerminalCanvas_widthOf(decoded.codepoint);
        }
        at += bytes;
    }
    if (word > longest)
        longest = word;
    return longest == 0 ? 1u : longest;
}

TerminalMarkdownTableCellLine TerminalMarkdownTable_cellLine(const char *text, bytes length,
                                             bytes offset, bytes width) {
    TerminalMarkdownTableCellLine line = { text + length, 0, length, 0, false, false, false };
    if (text == nullptr)
        return line;
    if (offset > length)
        offset = length;
    while (offset < length && TerminalMarkdownText_isSpace(text[offset]))
        ++offset;
    line.text = text + offset;
    line.next = offset;
    if (offset == length)
        return line;
    if (width == 0) {
        line.truncated = true;
        return line;
    }
    bool isMidWord = (offset > 0u && offset < length
                      && !TerminalMarkdownText_isSpace(text[offset - 1u])
                      && !TerminalMarkdownText_isSpace(text[offset]));
    line.leadingHyphen = isMidWord;
    bytes availableWidth = width;
    if (isMidWord && availableWidth > 1u)
        availableWidth -= 1u;
    bytes at = offset;
    bytes used = 0;
    bytes lastSpace = SIZE_MAX;
    bytes afterSpace = SIZE_MAX;
    while (at < length) {
        if (TerminalMarkdownText_isSpace(text[at]) && at > offset) {
            lastSpace = at;
            afterSpace = at + 1u;
            while (afterSpace < length && TerminalMarkdownText_isSpace(text[afterSpace]))
                ++afterSpace;
        }
        TerminalCanvasDecodedScalar decoded = TerminalCanvas_utf8Decode(text + at, length - at);
        bytes bytes = decoded.bytes == 0 ? 1u : decoded.bytes;
        bytes glyphCells = decoded.bytes == 0 ? 1u : TerminalCanvas_widthOf(decoded.codepoint);
        if (glyphCells != 0 && used + glyphCells > availableWidth) {
            if (lastSpace != SIZE_MAX) {
                line.length = lastSpace - offset;
                line.next = afterSpace;
                line.cells = TerminalMarkdownText_cells(line.text, line.length) + (isMidWord ? 1u : 0u);
                return line;
            }
            bytes budget = availableWidth > 1u ? availableWidth - 1u : 1u;
            bytes take = TerminalMarkdownText_bytesForCells(text + offset, length - offset,
                                                             budget, &used);
            if (take == 0) {
                take = TerminalMarkdownText_scalarBytes(text + offset, length - offset);
                used = TerminalMarkdownText_cells(text + offset, take);
            }
            line.length = take;
            line.next = offset + take;
            line.cells = used + (isMidWord ? 1u : 0u) + 1u;
            line.trailingHyphen = true;
            line.truncated = false;
            return line;
        }
        used += glyphCells;
        at += bytes;
    }
    line.length = at - offset;
    line.next = length;
    line.cells = used + (isMidWord ? 1u : 0u);
    return line;
}

bytes TerminalMarkdownTable_lineCount(const char *text, bytes length, bytes width) {
    bytes offset = 0;
    bytes count = 0;
    while (offset < length) {
        TerminalMarkdownTableCellLine line = TerminalMarkdownTable_cellLine(text, length, offset, width);
        ++count;
        if (line.next <= offset)
            break;
        offset = line.next;
        if (TerminalMarkdownText_blank(text + (offset < length ? offset : length),
                               length - (offset < length ? offset : length)))
            break;
    }
    return count == 0 ? 1u : count;
}

static fn applySpacing(bytes *widths, bytes columns, bytes available) {
    // Raise every column to the 4-cell spacing floor, but only if the raised table
    // still fits the cells this viewport gave it. Otherwise the fit budget wins and
    // the exact widths stand: a table must not gain a scrollbar to satisfy rounding.
    bytes total = 0;
    for (bytes column = 0; column < columns; ++column) {
        bytes padded = TerminalMarkdownTable_padWidth(widths[column]);
        if (padded > available - total)
            return;
        total += padded;
    }
    for (bytes column = 0; column < columns; ++column)
        widths[column] = TerminalMarkdownTable_padWidth(widths[column]);
}

bool TerminalMarkdownTable_allocate(const bytes *needed, const bytes *minimums, bytes *widths,
                            bytes columns, bytes viewport) {
    if (needed == nullptr || minimums == nullptr || widths == nullptr || columns == 0)
        return false;
    bytes overhead = columns * 3u + 1u;
    if (viewport <= overhead)
        return false;
    bytes available = viewport - overhead;
    bytes neededTotal = 0;
    bytes minimumTotal = 0;
    bool neededFits = true;
    bool minimumsFit = true;
    for (bytes column = 0; column < columns; ++column) {
        if (needed[column] > available - neededTotal) {
            neededFits = false;
            neededTotal = available;
        } else if (neededFits) {
            neededTotal += needed[column];
        }
        if (minimums[column] > available - minimumTotal) {
            minimumsFit = false;
            minimumTotal = available;
        } else if (minimumsFit) {
            minimumTotal += minimums[column];
        }
        widths[column] = minimums[column];
    }
    if (!minimumsFit)
        return false;
    if (neededFits) {
        for (bytes column = 0; column < columns; ++column)
            widths[column] = needed[column];
        applySpacing(widths, columns, available);
        return true;
    }
    bytes extraBudget = available - minimumTotal;
    bytes remainingNeed = 0;
    for (bytes column = 0; column < columns; ++column) {
        bytes extra = needed[column] > minimums[column] ? needed[column] - minimums[column] : 0u;
        remainingNeed = extra > SIZE_MAX - remainingNeed ? SIZE_MAX : remainingNeed + extra;
    }
    if (remainingNeed == 0u) {
        applySpacing(widths, columns, available);
        return true;
    }
    bytes used = 0;
    long d64 fractions[TERMINAL_MARKDOWN_TABLE_COLUMNS] = {0};
    for (bytes column = 0; column < columns; ++column) {
        bytes extra = needed[column] > minimums[column] ? needed[column] - minimums[column] : 0u;
        long d64 share = (long d64) extraBudget * (long d64) extra
                          / (long d64) remainingNeed;
        bytes whole = (bytes) share;
        if (whole > extra)
            whole = extra;
        widths[column] += whole;
        used += whole;
        fractions[column] = whole == extra ? -1.0L : share - (long d64) whole;
    }
    while (used < extraBudget) {
        bytes chosen = SIZE_MAX;
        for (bytes column = 0; column < columns; ++column)
            if (fractions[column] >= 0.0L
                && (chosen == SIZE_MAX || fractions[column] > fractions[chosen]))
                chosen = column;
        if (chosen == SIZE_MAX)
            break;
        ++widths[chosen];
        ++used;
        fractions[chosen] = -1.0L;
    }
    applySpacing(widths, columns, available);
    return true;
}

bool TerminalMarkdownTable_isWarped(const bytes *needed, const bytes *widths,
                                    bytes columns, bytes threshold) {
    if (needed == nullptr || widths == nullptr || columns == 0)
        return false;
    bytes limit = threshold != 0u ? threshold : TERMINAL_MARKDOWN_TABLE_WARP_THRESHOLD;
    for (bytes column = 0; column < columns; ++column) {
        if (widths[column] < needed[column] && widths[column] < limit)
            return true;
    }
    return false;
}

bytes TerminalMarkdownTable_allocateUnwarped(const bytes *needed, bytes *widths,
                                              bytes columns) {
    if (widths == nullptr || columns == 0)
        return 0;
    bytes total = columns * 3u + 1u;
    for (bytes column = 0; column < columns; ++column) {
        bytes width = (needed != nullptr && needed[column] != 0u) ? needed[column] : 1u;
        widths[column] = TerminalMarkdownTable_padWidth(width);
        total += widths[column];
    }
    return total;
}

bytes TerminalMarkdownTable_padWidth(bytes width) {
    if (width == 0u)
        return 4u;
    if (width > SIZE_MAX - 3u)
        return SIZE_MAX & ~(bytes) 3u;
    return (width + 3u) & ~(bytes) 3u;
}

