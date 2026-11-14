#include "ui/markdown/markdown_inline.h"
#include "ui/markdown/markdown_text.h"
#include "format/terminal_style.h"
#include "annotation/annotation.h"
#include <stdint.h>
#include <string.h>
#include <strings.h>

;;DEFINITION
/** The cold inline grammar of the Markdown preview. A line becomes a flat span
 * run: escapes lose their backslash, code runs become CODE spans, `**strong**`
 * and `*emphasis*` (and their nesting) become style flags on the surrounding
 * role, a link keeps its target on every label span, and an image becomes the
 * marker glyph plus its alt text. No allocation and no terminal knowledge: the
 * output is borrowed bytes plus flags, ready for the wrap pass. */
;;OVERVIEW
/** ============================================================================
 * CLASS: TerminalMarkdownInline (ui/markdown/markdown_inline.c)
 * STRUCT FIELDS: none — the parser writes caller-owned MarkdownSpan records.
 * FUNCTION REGISTRY (exports)
 * Parse: TerminalMarkdownInline_parse(text, length, base, style, spans, capacity, complete)
 * PRIVATE HELPERS
 *   SpanSink  MarkdownSpan *spans; bytes capacity/count; bool complete
 *   emitSpan / emitPlain / parseInto
 * CONTRACT: markers are consumed; an unmatched marker prints literally; the
 * nesting cap prints the remainder literally rather than recursing; a full span
 * buffer reports incomplete so the caller can fall back without losing bytes.
 * ============================================================================ */

#define MARKDOWN_INLINE_DEPTH 8u
#define MARKDOWN_IMAGE_GLYPH "▧ "

typedef struct SpanSink {
    MarkdownSpan *spans;
    bytes capacity;
    bytes count;
    bool complete;
} SpanSink;

static fn emitSpan(SpanSink *sink, const char *text, bytes length,
                     TerminalMarkdownRole role, u16 style,
                     const char *link, bytes linkLength) {
    if (length == 0)
        return;
    if ((*sink).count >= (*sink).capacity) {
        (*sink).complete = false;
        return;
    }
    MarkdownSpan *span = &(*sink).spans[(*sink).count];
    (*span).text = text;
    (*span).length = length;
    (*span).role = role;
    (*span).style = style;
    (*span).link = link;
    (*span).linkLength = linkLength;
    ++(*sink).count;
}

static fn parseInto(SpanSink *sink, const char *text, bytes length,
                      TerminalMarkdownRole base, u16 style,
                      const char *link, bytes linkLength, bytes depth);

// Emit `body + start .. end` with the emphasis flag added, recursing so nesting
// composes (bold inside a link keeps the link; italic inside bold adds a flag).
static fn parseEmphasis(SpanSink *sink, const char *text, bytes start, bytes end,
                          TerminalMarkdownRole base, u16 style,
                          const char *link, bytes linkLength,
                          u16 flag, bytes depth) {
    parseInto(sink, text + start, end - start, base,
              (u16) (style | flag), link, linkLength, depth + 1u);
}

static bytes closingRun(const char *text, bytes length, bytes from, char glyph,
                         bytes run) {
    bytes at = from;
    while (at < length) {
        if (text[at] != glyph) {
            ++at;
            continue;
        }
        bytes count = 0;
        while (at + count < length && text[at + count] == glyph)
            ++count;
        if (count == run)
            return at;
        at += count;
    }
    return SIZE_MAX;
}

static bool isAutolink(const char *inner, bytes length) {
    if (length >= 4u && strncasecmp(inner, "http", 4u) == 0)
        return true;
    return length >= 6u && strncasecmp(inner, "mailto", 6u) == 0;
}

// A link target is the text between `(` and `)`, minus an optional quoted title.
// The target stays borrowed from the same line.
static fn targetSlice(const char *text, bytes start, bytes end,
                        bytes *targetStart, bytes *targetLength) {
    while (start < end && TerminalMarkdownText_isSpace(text[start]))
        ++start;
    while (end > start && TerminalMarkdownText_isSpace(text[end - 1u]))
        --end;
    bytes words = end;
    for (bytes at = start; at < end; ++at) {
        if (TerminalMarkdownText_isSpace(text[at])) {
            words = at;
            break;
        }
    }
    *targetStart = start;
    *targetLength = words > start ? words - start : 0u;
}

static fn parseInto(SpanSink *sink, const char *text, bytes length,
                      TerminalMarkdownRole base, u16 style,
                      const char *link, bytes linkLength, bytes depth) {
    if (text == nullptr || length == 0)
        return;
    if (depth > MARKDOWN_INLINE_DEPTH) {
        emitSpan(sink, text, length, base, style, link, linkLength);
        return;
    }
    bytes at = 0;
    bool intraword = false;
    while (at < length) {
        char character = text[at];
        if (character == '\\' && at + 1u < length
            && TerminalMarkdownText_isPunctuation(text[at + 1u])) {
            emitSpan(sink, text + at + 1u, 1u, base, style, link, linkLength);
            at += 2u;
            continue;
        }
        if (character == '`') {
            bytes run = 0;
            while (at + run < length && text[at + run] == '`')
                ++run;
            bytes close = TerminalMarkdownText_findRun(text, length, at + run, '`', run);
            if (close != SIZE_MAX) {
                bytes contentStart = at + run;
                bytes contentEnd = close;
                if (contentEnd > contentStart && text[contentStart] == ' ')
                    ++contentStart;
                if (contentEnd > contentStart && text[contentEnd - 1u] == ' ')
                    --contentEnd;
                emitSpan(sink, text + contentStart, contentEnd - contentStart,
                         TERMINAL_MARKDOWN_CODE, 0, link, linkLength);
                at = close + run;
                continue;
            }
            emitSpan(sink, text + at, run, base, style, link, linkLength);
            at += run;
            continue;
        }
        if (character == '*' || character == '_') {
            bytes run = 0;
            while (at + run < length && text[at + run] == character)
                ++run;
            bytes wanted = run >= 3u ? 3u : run >= 2u ? 2u : 1u;
            u16 flag = wanted == 3u
                          ? (u16) (TERMINAL_STYLE_BOLD | TERMINAL_STYLE_ITALIC)
                          : wanted == 2u ? TERMINAL_STYLE_BOLD : TERMINAL_STYLE_ITALIC;
            bytes close = closingRun(text, length, at + wanted, character, wanted);
            if (close != SIZE_MAX && close > at + wanted) {
                parseEmphasis(sink, text, at + wanted, close, base, style, link, linkLength,
                              flag, depth);
                at = close + wanted;
                continue;
            }
            emitSpan(sink, text + at, wanted, base, style, link, linkLength);
            at += wanted;
            continue;
        }
        if (character == '!' && at + 1u < length && text[at + 1u] == '[') {
            bytes close = TerminalMarkdownText_find(text, length, at + 2u, "]", 1u);
            if (close != SIZE_MAX) {
                emitSpan(sink, MARKDOWN_IMAGE_GLYPH, strlen(MARKDOWN_IMAGE_GLYPH),
                         TERMINAL_MARKDOWN_MARKER, 0, link, linkLength);
                parseInto(sink, text + at + 2u, close - at - 2u,
                          TERMINAL_MARKDOWN_MARKER, 0, link, linkLength, depth + 1u);
                bytes after = TerminalMarkdownText_find(text, length, close, "](", 2u);
                bytes end = after == SIZE_MAX
                           ? SIZE_MAX : TerminalMarkdownText_find(text, length, after + 2u, ")", 1u);
                at = end == SIZE_MAX ? close + 1u : end + 1u;
                continue;
            }
        }
        if (character == '[' && link == nullptr) {
            bytes close = TerminalMarkdownText_find(text, length, at + 1u, "]", 1u);
            if (close != SIZE_MAX) {
                bytes paren = TerminalMarkdownText_find(text, length, close, "](", 2u);
                bytes end = paren == SIZE_MAX
                           ? SIZE_MAX : TerminalMarkdownText_find(text, length, paren + 2u, ")", 1u);
                if (paren != SIZE_MAX && end != SIZE_MAX) {
                    bytes targetStart = 0;
                    bytes targetLength = 0;
                    targetSlice(text, paren + 2u, end, &targetStart, &targetLength);
                    const char *target = targetLength == 0 ? nullptr : text + targetStart;
                    bytes labelStart = at + 1u;
                    bytes labelLength = close - labelStart;
                    if (labelLength == 0 && targetLength != 0) {
                        emitSpan(sink, target, targetLength, TERMINAL_MARKDOWN_LINK, style,
                                 target, targetLength);
                    } else {
                        parseInto(sink, text + labelStart, labelLength,
                                  TERMINAL_MARKDOWN_LINK, style,
                                  target, targetLength, depth + 1u);
                    }
                    at = end + 1u;
                    continue;
                }
            }
        }
        if (character == '<' && link == nullptr) {
            bytes close = TerminalMarkdownText_find(text, length, at + 1u, ">", 1u);
            if (close != SIZE_MAX) {
                const char *inner = text + at + 1u;
                bytes innerLength = close - at - 1u;
                if (isAutolink(inner, innerLength)) {
                    emitSpan(sink, inner, innerLength, TERMINAL_MARKDOWN_LINK, style,
                             inner, innerLength);
                    at = close + 1u;
                    continue;
                }
            }
        }
        bytes start = at;
        intraword = false;
        while (at < length) {
            char current = text[at];
            if (current == '\\' || current == '`' || current == '*' || current == '_'
                || current == '[' || current == '!' || current == '<')
                break;
            ++at;
        }
        if (at == start) {
            emitSpan(sink, text + at, 1u, base, style, link, linkLength);
            ++at;
        } else {
            emitSpan(sink, text + start, at - start, base, style, link, linkLength);
        }
        (fn) intraword;
    }
}

bytes TerminalMarkdownInline_parse(const char *text, bytes length, TerminalMarkdownRole base,
                            u16 style, MarkdownSpan *spans, bytes capacity,
                            bool *complete) {
    if (complete != nullptr)
        *complete = true;
    if (spans == nullptr || capacity == 0)
        return 0;
    SpanSink sink = { spans, capacity, 0, true };
    parseInto(&sink, text, length, base, style, nullptr, 0, 0);
    if (complete != nullptr)
        *complete = sink.complete;
    return sink.count;
}
