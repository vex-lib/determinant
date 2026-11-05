// Owner: TerminalMarkdownCallout — one severity vocabulary, two source
// spellings, and the label/pen index the theme resolves.
#include "ui/markdown/markdown_callout.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

int main(void) {
    // GFM spelling, with and without a title.
    static const char *warning = "[!WARNING] watch out";
    TerminalMarkdownCallout callout = TerminalMarkdownCallout_parse(warning, strlen(warning));
    assert(callout.valid && callout.kind == TERMINAL_MARKDOWN_CALLOUT_WARNING);
    assert(callout.contentStart == 11u);
    assert(callout.titleStart == 11u && callout.titleLength == 9u);
    assert(memcmp(warning + callout.titleStart, "watch out", 9) == 0);
    callout = TerminalMarkdownCallout_parse("[!NOTE]", strlen("[!NOTE]"));
    assert(callout.valid && callout.kind == TERMINAL_MARKDOWN_CALLOUT_NOTE);
    assert(callout.titleLength == 0u);

    // The hand-written README spelling is the same kind, case-insensitively.
    callout = TerminalMarkdownCallout_parse("!tip! do it", strlen("!tip! do it"));
    assert(callout.valid && callout.kind == TERMINAL_MARKDOWN_CALLOUT_TIP);
    assert(callout.contentStart == 6u && callout.titleLength == 5u);
    callout = TerminalMarkdownCallout_parse("!Caution! careful", strlen("!Caution! careful"));
    assert(callout.valid && callout.kind == TERMINAL_MARKDOWN_CALLOUT_CAUTION);
    callout = TerminalMarkdownCallout_parse("[!important] read", strlen("[!important] read"));
    assert(callout.valid && callout.kind == TERMINAL_MARKDOWN_CALLOUT_IMPORTANT);

    // Anything else stays literal: an unknown name, an empty marker, or markup
    // that does not start the span is not a callout.
    assert(!TerminalMarkdownCallout_parse("[!NOPE] x", strlen("[!NOPE] x")).valid);
    assert(!TerminalMarkdownCallout_parse("[!]", 3).valid);
    assert(!TerminalMarkdownCallout_parse("text [!NOTE]", strlen("text [!NOTE]")).valid);
    assert(!TerminalMarkdownCallout_parse("[!NOTE", 6).valid);
    assert(!TerminalMarkdownCallout_parse("!NOTE", 5).valid);
    assert(!TerminalMarkdownCallout_parse(nullptr, 8).valid);

    // The vocabulary is shared, so a label cannot drift from the kind it names.
    assert(strcmp(TerminalMarkdownCallout_name(TERMINAL_MARKDOWN_CALLOUT_NOTE), "NOTE") == 0);
    assert(strcmp(TerminalMarkdownCallout_name(TERMINAL_MARKDOWN_CALLOUT_WARNING), "WARNING") == 0);
    assert(strcmp(TerminalMarkdownCallout_name(TERMINAL_MARKDOWN_CALLOUT_CAUTION), "CAUTION") == 0);
    assert(strcmp(TerminalMarkdownCallout_name(TERMINAL_MARKDOWN_CALLOUT_NONE), "") == 0);
    assert(strcmp(TerminalMarkdownCallout_bar(), TERMINAL_MARKDOWN_CALLOUT_BAR) == 0);
    assert(strcmp(TerminalMarkdownCallout_bar(), "▌") == 0);

    assert(TerminalMarkdownCallout_penIndex(TERMINAL_MARKDOWN_CALLOUT_NOTE) == 0u);
    assert(TerminalMarkdownCallout_penIndex(TERMINAL_MARKDOWN_CALLOUT_TIP) == 1u);
    assert(TerminalMarkdownCallout_penIndex(TERMINAL_MARKDOWN_CALLOUT_IMPORTANT) == 2u);
    assert(TerminalMarkdownCallout_penIndex(TERMINAL_MARKDOWN_CALLOUT_WARNING) == 3u);
    assert(TerminalMarkdownCallout_penIndex(TERMINAL_MARKDOWN_CALLOUT_CAUTION) == 4u);
    assert(TerminalMarkdownCallout_penIndex(TERMINAL_MARKDOWN_CALLOUT_NONE)
           == TERMINAL_MARKDOWN_CALLOUT_KIND_COUNT);
    return 0;
}
