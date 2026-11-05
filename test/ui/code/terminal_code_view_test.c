// TerminalCodeView paints code spans and virtual parameter labels into cells.
// The caller owns token meaning; this test proves C23-style color/weight output,
// hint placement without source mutation, horizontal clipping and rejection
// preservation for malformed spans and UTF-8 boundaries.
#include "ui/code/terminal_code_view.h"
#include <assert.h>
#include <string.h>

static void testColoredLineAndVirtualHint(void) {
    uint32_t cells[20] = {0};
    uint16_t styles[20] = {0};
    uint32_t foregrounds[20] = {0};
    uint32_t backgrounds[20] = {0};
    TerminalCanvas canvas = TerminalCanvas(cells, styles, foregrounds, backgrounds, 20u, 1u);
    const char *source = "int add(x)";
    const TerminalCodeSpan spans[] = {
        {0u, 3u, TerminalPen(0u, 0x20D050FFu)},
        {4u, 7u, TerminalPen(TERMINAL_STYLE_BOLD, 0x50A8FFFFu)},
        {8u, 9u, TerminalPen(TERMINAL_STYLE_BOLD, 0x70E5F0FFu)}
    };
    const TerminalCodeHint hints[] = {
        {8u, "arg:", 4u, TerminalPen(TERMINAL_STYLE_DIM, 0x808080FFu)}
    };
    TerminalCodeView *view = TerminalCodeView();
    assert(view != nullptr);
    assert(TerminalCodeView_setLine(view, source, strlen(source), spans, 3u, hints, 1u));
    assert(TerminalCodeView_getText(view) == source);
    assert(TerminalCodeView_getLength(view) == strlen(source));
    assert(TerminalCodeView_getSpans(view) == spans);
    assert(TerminalCodeView_getSpanCount(view) == 3u);
    assert(TerminalCodeView_getHints(view) == hints);
    assert(TerminalCodeView_getHintCount(view) == 1u);
    TerminalCodeView_render(view, &canvas, 0u, 0u);

    assert(TerminalCanvas_glyphAt(&canvas, 0u, 0u) == 'i');
    assert(TerminalCanvas_fgAt(&canvas, 0u, 0u) == 0x20D050FFu);
    assert(TerminalCanvas_glyphAt(&canvas, 4u, 0u) == 'a');
    assert(TerminalCanvas_fgAt(&canvas, 4u, 0u) == 0x50A8FFFFu);
    assert((TerminalCanvas_styleAt(&canvas, 4u, 0u) & TERMINAL_STYLE_BOLD) != 0u);
    assert(TerminalCanvas_glyphAt(&canvas, 8u, 0u) == 'a');
    assert(TerminalCanvas_fgAt(&canvas, 8u, 0u) == 0x808080FFu);
    assert((TerminalCanvas_styleAt(&canvas, 8u, 0u) & TERMINAL_STYLE_DIM) != 0u);
    assert(TerminalCanvas_glyphAt(&canvas, 12u, 0u) == 'x');
    assert(TerminalCanvas_fgAt(&canvas, 12u, 0u) == 0x70E5F0FFu);
    assert(TerminalCanvas_glyphAt(&canvas, 13u, 0u) == ')');
    assert(strcmp(source, "int add(x)") == 0);
    TerminalCodeView_free(view);
}

static void testRejectPreservesAdmittedLine(void) {
    uint32_t cells[8] = {0};
    TerminalCanvas canvas = TerminalCanvas(cells, nullptr, 8u, 1u);
    const char *source = "old";
    const TerminalCodeSpan good[] = {{0u, 3u, TerminalPen(0u, 0xFFFFFFFFu)}};
    const TerminalCodeSpan overlap[] = {
        {0u, 2u, TerminalPen(0u, 0xFFFFFFFFu)},
        {1u, 3u, TerminalPen(0u, 0xFF0000FFu)}
    };
    const char *utf8 = "é";
    const TerminalCodeSpan splitScalar[] = {{0u, 1u, TerminalPen(0u, 0xFFFFFFFFu)}};
    const TerminalCodeSpan absentSourceSpan[] = {{0u, 1u, TerminalPen(0u, 0xFFFFFFFFu)}};
    TerminalCodeView *view = TerminalCodeView();
    assert(view != nullptr);
    assert(TerminalCodeView_setLine(view, source, 3u, good, 1u, nullptr, 0u));
    assert(!TerminalCodeView_setLine(view, "new", 3u, overlap, 2u, nullptr, 0u));
    assert(!TerminalCodeView_setLine(view, utf8, strlen(utf8), splitScalar, 1u, nullptr, 0u));
    assert(!TerminalCodeView_setLine(view, nullptr, 0u, absentSourceSpan, 1u, nullptr, 0u));
    TerminalCodeView_render(view, &canvas, 0u, 0u);
    assert(TerminalCanvas_glyphAt(&canvas, 0u, 0u) == 'o');
    assert(TerminalCodeView_getScroll(view) == 0u);
    assert(!TerminalCodeView_setScroll(nullptr, 2u));
    assert(TerminalCodeView_getScroll(nullptr) == 0u);
    assert(TerminalCodeView_getText(nullptr) == nullptr);
    assert(TerminalCodeView_getLength(nullptr) == 0u);
    assert(TerminalCodeView_getSpans(nullptr) == nullptr);
    assert(TerminalCodeView_getSpanCount(nullptr) == 0u);
    assert(TerminalCodeView_getHints(nullptr) == nullptr);
    assert(TerminalCodeView_getHintCount(nullptr) == 0u);
    TerminalCodeView_free(view);
}

static void testCellScrollAndClipping(void) {
    uint32_t cells[4] = {0};
    TerminalCanvas canvas = TerminalCanvas(cells, nullptr, 4u, 1u);
    TerminalCodeView *view = TerminalCodeView();
    assert(view != nullptr);
    assert(TerminalCodeView_setLine(view, "abcdef", 6u, nullptr, 0u, nullptr, 0u));
    assert(TerminalCodeView_setScroll(view, 2u));
    TerminalCodeView_render(view, &canvas, 0u, 0u);
    assert(TerminalCanvas_glyphAt(&canvas, 0u, 0u) == 'c');
    assert(TerminalCanvas_glyphAt(&canvas, 3u, 0u) == 'f');
    assert(TerminalCodeView_getScroll(view) == 2u);
    TerminalCodeView_free(view);
}

int main(void) {
    testColoredLineAndVirtualHint();
    testRejectPreservesAdmittedLine();
    testCellScrollAndClipping();
    return 0;
}
