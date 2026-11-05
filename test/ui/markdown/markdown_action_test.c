// Owner: TerminalMarkdownAction — what a preview click means. The view reports
// a value; the caller opens the link or writes the clipboard.
#include "ui/markdown/markdown_action.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

int main(void) {
    // The icon sits flush right with one cell of margin, and a row too narrow
    // for it reports no column rather than an underflowed one.
    assert(TerminalMarkdownAction_iconColumn(10u, 1u) == 8u);
    assert(TerminalMarkdownAction_iconColumn(10u, 2u) == 7u);
    assert(TerminalMarkdownAction_iconColumn(2u, 1u) == 0u);
    assert(TerminalMarkdownAction_iconColumn(1u, 1u) == 0u);
    assert(TerminalMarkdownAction_iconColumn(10u, 0u) == 0u);

    assert(TerminalMarkdownAction_iconHit(10u, 8u, 1u));
    assert(!TerminalMarkdownAction_iconHit(10u, 7u, 1u));
    assert(!TerminalMarkdownAction_iconHit(10u, 9u, 1u));
    assert(TerminalMarkdownAction_iconHit(10u, 7u, 2u));
    assert(!TerminalMarkdownAction_iconHit(1u, 0u, 1u));

    TerminalMarkdownAction none = {0};
    TerminalMarkdownAction link = { TERMINAL_MARKDOWN_ACTION_LINK, "https://example.test",
                                    strlen("https://example.test"), 0u };
    TerminalMarkdownAction copy = { TERMINAL_MARKDOWN_ACTION_COPY_CODE, nullptr, 0u, 3u };
    assert(strcmp(TerminalMarkdownAction_toString(none).text, "none") == 0);
    assert(strstr(TerminalMarkdownAction_toString(link).text, "https://example.test") != nullptr);
    assert(strstr(TerminalMarkdownAction_toString(copy).text, "block=3") != nullptr);
    assert(strstr(TerminalMarkdownAction_toStringStruct(link).text, "target=<borrowed>")
           != nullptr);
    assert(strstr(TerminalMarkdownAction_toStringStruct(none).text, "target=nullptr")
           != nullptr);
    assert(strstr(TerminalMarkdownAction_toStringStruct(copy).text, "block=3") != nullptr);
    return 0;
}
