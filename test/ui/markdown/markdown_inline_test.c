// Owner: TerminalMarkdownInline — the cold inline grammar. Emphasis flags,
// code runs, link targets on every label span, images, autolinks, escapes and
// the honest overflow report.
#include "ui/markdown/markdown_inline.h"
#include "format/terminal_style.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

static size_t parse(const char *text, TerminalMarkdownRole base, MarkdownSpan *spans,
                    size_t capacity, bool *complete) {
    return TerminalMarkdownInline_parse(text, strlen(text), base, 0, spans, capacity, complete);
}

int main(void) {
    MarkdownSpan spans[MARKDOWN_INLINE_SPANS];
    bool complete = true;

    // Strong and emphasis are flags on the surrounding role, and the markers are
    // consumed rather than printed.
    size_t count = parse("a **bold** and *quiet*", TERMINAL_MARKDOWN_BODY, spans,
                         MARKDOWN_INLINE_SPANS, &complete);
    assert(complete && count >= 4u);
    assert(spans[0].role == TERMINAL_MARKDOWN_BODY);
    assert(spans[0].length == 2u && memcmp(spans[0].text, "a ", 2) == 0);
    assert(spans[1].length == 4u && memcmp(spans[1].text, "bold", 4) == 0);
    assert((spans[1].style & TERMINAL_STYLE_BOLD) != 0);
    assert((spans[1].style & TERMINAL_STYLE_ITALIC) == 0);
    bool sawItalic = false;
    for (size_t index = 0; index < count; ++index)
        if (spans[index].length == 5u && memcmp(spans[index].text, "quiet", 5) == 0) {
            assert((spans[index].style & TERMINAL_STYLE_ITALIC) != 0);
            assert((spans[index].style & TERMINAL_STYLE_BOLD) == 0);
            sawItalic = true;
        }
    assert(sawItalic);

    count = parse("***both***", TERMINAL_MARKDOWN_BODY, spans, MARKDOWN_INLINE_SPANS, &complete);
    assert(complete && count == 1u && (spans[0].style
           & (TERMINAL_STYLE_BOLD | TERMINAL_STYLE_ITALIC))
           == (TERMINAL_STYLE_BOLD | TERMINAL_STYLE_ITALIC));

    // A code run is CODE, trims one surrounding space, and keeps its pipes.
    count = parse("`a | b.sh`", TERMINAL_MARKDOWN_BODY, spans, MARKDOWN_INLINE_SPANS, &complete);
    assert(complete && count == 1u && spans[0].role == TERMINAL_MARKDOWN_CODE);
    assert(spans[0].length == 5u && memcmp(spans[0].text, "a | b.sh", 5) == 0);

    // A link carries its borrowed target on every span of the label, so a click
    // anywhere on the label resolves without a side table.
    count = parse("[two words](https://example.test \"t\")", TERMINAL_MARKDOWN_BODY, spans,
                  MARKDOWN_INLINE_SPANS, &complete);
    assert(complete && count >= 1u);
    assert(spans[0].role == TERMINAL_MARKDOWN_LINK);
    assert(spans[0].link != nullptr && spans[0].linkLength == strlen("https://example.test"));
    assert(memcmp(spans[0].link, "https://example.test", spans[0].linkLength) == 0);
    for (size_t index = 0; index < count; ++index) {
        assert(spans[index].role == TERMINAL_MARKDOWN_LINK);
        assert(spans[index].linkLength == strlen("https://example.test"));
        assert(memcmp(spans[index].link, "https://example.test", spans[index].linkLength) == 0);
    }

    // An image becomes the marker glyph plus its alt text, never a link.
    count = parse("![alt](img.png)", TERMINAL_MARKDOWN_BODY, spans, MARKDOWN_INLINE_SPANS,
                  &complete);
    assert(complete && count == 2u);
    assert(spans[0].role == TERMINAL_MARKDOWN_MARKER);
    assert(spans[0].link == nullptr);
    assert(spans[1].length == 3u && memcmp(spans[1].text, "alt", 3) == 0);

    count = parse("<https://example.test>", TERMINAL_MARKDOWN_BODY, spans,
                  MARKDOWN_INLINE_SPANS, &complete);
    assert(complete && count == 1u && spans[0].role == TERMINAL_MARKDOWN_LINK);
    assert(spans[0].linkLength == strlen("https://example.test"));

    // Escapes print the punctuation and an unmatched marker prints literally.
    count = parse("\\*not emphasis\\*", TERMINAL_MARKDOWN_BODY, spans, MARKDOWN_INLINE_SPANS,
                  &complete);
    assert(complete && count >= 3u);
    assert(spans[0].length == 1u && spans[0].text[0] == '*');
    count = parse("*dangling", TERMINAL_MARKDOWN_BODY, spans, MARKDOWN_INLINE_SPANS, &complete);
    assert(complete && count == 2u && spans[0].text[0] == '*');
    assert(spans[1].length == strlen("dangling"));

    // A buffer too small to hold the grammar reports incomplete instead of
    // pretending the document ended.
    count = parse("a *b.sh* c *d* e", TERMINAL_MARKDOWN_BODY, spans, 2u, &complete);
    assert(!complete);
    assert(count == 2u);
    assert(TerminalMarkdownInline_parse(nullptr, 4, TERMINAL_MARKDOWN_BODY, 0, spans,
                                       MARKDOWN_INLINE_SPANS, &complete) == 0u);
    assert(TerminalMarkdownInline_parse("x", 1, TERMINAL_MARKDOWN_BODY, 0, nullptr, 4,
                                       &complete) == 0u);
    return 0;
}
