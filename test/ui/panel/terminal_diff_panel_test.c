// Owner test for determinant/ui/panel/terminal_diff_panel.c.
// Verifies responsive layout switching (wide -> side-by-side, narrow -> unified),
// inline word/letter span highlighting, metadata banner, custom header hook, and null safety.

#include "vex.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ui/panel/terminal_diff_panel.h"

static i32 g_customHeaderHits = 0;

/** Custom header callback fixture verifying consumer hook delegation. */
static fn customHeaderHook(const TerminalDiffPanel *panel,
                           const TerminalDiffMeta *meta,
                           u32 columns,
                           TerminalDiffLayoutMode layout,
                           FILE *out,
                           any userdata) {
    (discard) panel;
    (discard) meta;
    (discard) columns;
    (discard) layout;
    (discard) userdata;
    g_customHeaderHits++;
    fputs("CUSTOM_DIFF_HEADER\n", out);
}

/** Proves responsive layout switching, inline spans, custom header hook, and null safety. */
i32 main() {
    TerminalDiffPanel panel;
    TerminalDiffPanel_init(&panel);

    u32 cols = 0;
    TerminalDiffPanel_setColumns(&panel, 120u);
    assert(TerminalDiffPanel_resolveLayout(&panel, &cols) == TERMINAL_DIFF_LAYOUT_SIDE_BY_SIDE);
    assert(cols == 120u);

    TerminalDiffPanel_setColumns(&panel, 80u);
    assert(TerminalDiffPanel_resolveLayout(&panel, &cols) == TERMINAL_DIFF_LAYOUT_UNIFIED);
    assert(cols == 80u);

    TerminalDiffPanel_setSideBySideMinCols(&panel, 70u);
    assert(TerminalDiffPanel_resolveLayout(&panel, &cols) == TERMINAL_DIFF_LAYOUT_SIDE_BY_SIDE);

    TerminalDiffPanel_setLayoutMode(&panel, TERMINAL_DIFF_LAYOUT_UNIFIED);
    assert(TerminalDiffPanel_resolveLayout(&panel, &cols) == TERMINAL_DIFF_LAYOUT_UNIFIED);
    TerminalDiffPanel_setLayoutMode(&panel, TERMINAL_DIFF_LAYOUT_AUTO);
    TerminalDiffPanel_setSideBySideMinCols(&panel, 100u);

    TerminalDiffSpan delSpan = { .start = 4, .length = 3 };
    TerminalDiffSpan addSpan = { .start = 4, .length = 3 };
    TerminalDiffLine lines[] = {
        { .kind = TERMINAL_DIFF_CONTEXT, .oldLine = 1, .newLine = 1, .text = "i32 a = 1;", .length = 10 },
        { .kind = TERMINAL_DIFF_REMOVED, .oldLine = 2, .newLine = 0, .text = "i32 foo = 2;", .length = 12, .spans = &delSpan, .spanCount = 1 },
        { .kind = TERMINAL_DIFF_ADDED, .oldLine = 0, .newLine = 2, .text = "i32 bar = 2;", .length = 12, .spans = &addSpan, .spanCount = 1 },
    };
    TerminalDiffHunk hunk = { .oldStart = 1, .oldCount = 2, .newStart = 1, .newCount = 2, .lines = lines, .lineCount = 3 };
    TerminalDiffMeta meta = { .path = "src/main.c", .eventLabel = "MODIFIED", .timestamp = "12:05:00", .linesAdded = 1, .linesRemoved = 1 };

    TerminalDiffTheme th = TerminalDiffPanel_defaultTheme();
    th.useColor = false;
    th.useUnicodeBorder = false;
    TerminalDiffPanel_setTheme(&panel, th);

    char narrowBuf[4096];
    memset(narrowBuf, 0, sizeof(narrowBuf));
    FILE *narrowOut = fmemopen(narrowBuf, sizeof(narrowBuf) - 1u, "wb");
    assert(narrowOut != nullptr);
    TerminalDiffPanel_setColumns(&panel, 80u);
    assert(TerminalDiffPanel_render(&panel, &meta, &hunk, 1, narrowOut));
    fclose(narrowOut);
    assert(strstr(narrowBuf, "[MODIFIED] src/main.c") != nullptr);
    assert(strstr(narrowBuf, "+1 -1") != nullptr);
    assert(strstr(narrowBuf, "12:05:00") != nullptr);
    assert(strstr(narrowBuf, "(unified 80c)") != nullptr);
    assert(strstr(narrowBuf, "@@ -1,2 +1,2 @@") != nullptr);

    char wideBuf[4096];
    memset(wideBuf, 0, sizeof(wideBuf));
    FILE *wideOut = fmemopen(wideBuf, sizeof(wideBuf) - 1u, "wb");
    assert(wideOut != nullptr);
    TerminalDiffPanel_setColumns(&panel, 120u);
    assert(TerminalDiffPanel_render(&panel, &meta, &hunk, 1, wideOut));
    fclose(wideOut);
    assert(strstr(wideBuf, "(split 120c)") != nullptr);
    assert(strstr(wideBuf, "foo") != nullptr && strstr(wideBuf, "bar") != nullptr);
    assert(strstr(wideBuf, " | ") != nullptr);

    th.useColor = true;
    TerminalDiffPanel_setTheme(&panel, th);
    TerminalDiffPanel_setHeaderHook(&panel, customHeaderHook, nullptr);
    char colorBuf[4096];
    memset(colorBuf, 0, sizeof(colorBuf));
    FILE *colorOut = fmemopen(colorBuf, sizeof(colorBuf) - 1u, "wb");
    assert(colorOut != nullptr);
    assert(TerminalDiffPanel_render(&panel, &meta, &hunk, 1, colorOut));
    fclose(colorOut);
    assert(g_customHeaderHits == 1);
    assert(strstr(colorBuf, "CUSTOM_DIFF_HEADER") != nullptr);
    assert(strstr(colorBuf, "\x1b[48;2;38;108;60m") != nullptr);
    assert(strstr(colorBuf, "\x1b[48;2;130;40;48m") != nullptr);

    assert(!TerminalDiffPanel_render(nullptr, &meta, &hunk, 1, stdout));
    assert(!TerminalDiffPanel_render(&panel, &meta, &hunk, 1, nullptr));
    puts("terminal_diff_panel_test: PASS");
    return 0;
}
