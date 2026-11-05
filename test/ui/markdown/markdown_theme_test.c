// Owner: TerminalMarkdownTheme — structure to pen. Six distinct heading levels,
// five callout severities, link hover, and the legacy eight-pen projection.
#include "ui/markdown/markdown_theme.h"
#include "format/terminal_style.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

int main(void) {
    TerminalMarkdownTheme theme = TerminalMarkdownTheme();
    // Every heading level is a different pen, not one pen with a flag: that is
    // what makes `#`, `##` and `####` distinguishable in one document.
    for (size_t level = 0; level < 6u; ++level)
        for (size_t other = level + 1u; other < 6u; ++other)
            assert(theme.heading[level].fg != theme.heading[other].fg);
    assert((theme.heading[0].style & TERMINAL_STYLE_BOLD) != 0);
    assert(theme.heading[4].fg != theme.heading[5].fg);

    assert(TerminalMarkdownTheme_pen(&theme, TERMINAL_MARKDOWN_HEADING, 1, false).fg
           == theme.heading[0].fg);
    assert(TerminalMarkdownTheme_pen(&theme, TERMINAL_MARKDOWN_HEADING, 4, false).fg
           == theme.heading[3].fg);
    // A level outside the ladder clamps instead of reading past the array.
    assert(TerminalMarkdownTheme_pen(&theme, TERMINAL_MARKDOWN_HEADING, 9, false).fg
           == theme.heading[5].fg);
    assert(TerminalMarkdownTheme_pen(&theme, TERMINAL_MARKDOWN_HEADING, 0, false).fg
           == theme.heading[0].fg);

    assert(TerminalMarkdownTheme_pen(&theme, TERMINAL_MARKDOWN_LINK, 0, false).fg
           == theme.link.fg);
    assert(TerminalMarkdownTheme_pen(&theme, TERMINAL_MARKDOWN_LINK, 0, true).fg
           == theme.linkHover.fg);
    assert(TerminalMarkdownTheme_pen(&theme, TERMINAL_MARKDOWN_LINK, 0, true).style
           != TerminalMarkdownTheme_pen(&theme, TERMINAL_MARKDOWN_LINK, 0, false).style);

    // Five severities, five pens; an unknown kind is body text rather than a
    // neighbourhood pen chosen by an out-of-range index.
    size_t calloutPens = TERMINAL_MARKDOWN_CALLOUT_KIND_COUNT - 1u;
    for (size_t kind = 0; kind < calloutPens; ++kind)
        for (size_t other = kind + 1u; other < calloutPens; ++other)
            assert(theme.callout[kind].fg != theme.callout[other].fg);
    assert(TerminalMarkdownTheme_pen(&theme, TERMINAL_MARKDOWN_CALLOUT,
                                     (uint8_t) TERMINAL_MARKDOWN_CALLOUT_WARNING, false).fg
           == theme.callout[3].fg);
    assert(TerminalMarkdownTheme_pen(&theme, TERMINAL_MARKDOWN_HEADING, 1, false).fg
           != TerminalMarkdownTheme_pen(&theme, TERMINAL_MARKDOWN_BODY, 0, false).fg);

    assert(TerminalMarkdownTheme_pen(&theme, TERMINAL_MARKDOWN_TABLE_HEADER, 0, false).fg
           == theme.tableHeader.fg);
    assert(TerminalMarkdownTheme_pen(&theme, TERMINAL_MARKDOWN_TABLE_BORDER, 0, false).fg
           == theme.tableBorder.fg);
    assert(TerminalMarkdownTheme_pen(&theme, TERMINAL_MARKDOWN_CODE, 0, false).fg
           == theme.code.fg);
    assert(TerminalMarkdownTheme_pen(&theme, TERMINAL_MARKDOWN_FENCE, 0, false).bg
           == theme.fence.bg);
    assert(TerminalMarkdownTheme_pen(&theme, TERMINAL_MARKDOWN_CALLOUT, 99, false).fg
           == theme.body.fg);
    assert(TerminalMarkdownTheme_pen(nullptr, TERMINAL_MARKDOWN_BODY, 0, false).fg
           == TerminalPen_0().fg);

    // The affordance glyphs are borrowed constants, not per-render allocations.
    assert(theme.copyGlyph != nullptr && theme.copiedGlyph != nullptr);
    assert(theme.bar != nullptr && strcmp(theme.bar, "▌") == 0);
    assert(strcmp(theme.copyGlyph, "⧉") == 0 && strcmp(theme.copiedGlyph, "✓") == 0);

    // The legacy eight-pen record is a projection: asking for one heading colour
    // collapses all six levels, and reading it back returns h1.
    TerminalMarkdownStyle legacy = TerminalMarkdownTheme_style(&theme);
    assert(legacy.body.fg == theme.body.fg && legacy.link.fg == theme.link.fg);
    assert(legacy.heading.fg == theme.heading[0].fg);
    legacy.heading = TerminalPen_2(TERMINAL_STYLE_BOLD, TERMINAL_RGB(1, 2, 3));
    TerminalMarkdownTheme projected = TerminalMarkdownTheme_fromStyle(legacy);
    for (size_t level = 0; level < 6u; ++level) {
        assert(projected.heading[level].fg == TERMINAL_RGB(1, 2, 3));
        assert(projected.heading[level].style == TERMINAL_STYLE_BOLD);
    }
    assert(projected.body.fg == theme.body.fg);
    assert(projected.callout[0].fg == theme.callout[0].fg);
    assert(TerminalMarkdownTheme_style(nullptr).body.fg == TERMINAL_COLOR_DEFAULT);
    return 0;
}
