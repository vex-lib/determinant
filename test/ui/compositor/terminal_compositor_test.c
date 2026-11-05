// determinant/test/ui/compositor/terminal_compositor_test.c
// Owner: TerminalCompositor — output serialization, ANSI/VT escape formatting,
// alternate screen transitions, and synchronized frame emission.

#include "ui/compositor/terminal_compositor.h"
#include "ui/surface/terminal_surface.h"
#include "format/terminal_color.h"
#include "format/terminal_style.h"
#include "determinant/test/support.h"
#include <unistd.h>
#include <fcntl.h>

static void testPipeOutput(void) {
    int fds[2];
    assert(pipe(fds) == 0);
    assert(fcntl(fds[0], F_SETFL, O_NONBLOCK) == 0);

    TerminalCompositor *comp = TerminalCompositor(fds[1]);
    assert(comp != nullptr);
    assert(TerminalCompositor_getFd(comp) == fds[1]);
    assert(TerminalCompositor_getColorMode(comp) == TERMINAL_COMPOSITOR_COLOR_TRUECOLOR);

    // Test alternate screen enter
    assert(TerminalCompositor_enter(comp));
    char buf[2048];
    ssize_t n = read(fds[0], buf, sizeof(buf) - 1);
    assert(n > 0);
    buf[n] = '\0';
    assert(strstr(buf, "\033[?1049h") != nullptr);
    assert(strstr(buf, "\033[?25l") != nullptr); // cursor hidden

    // Test blitRun inside synchronized transaction
    uint32_t glyphs[3] = { 'H', 'i', '!' };
    uint16_t styles[3] = { TERMINAL_STYLE_BOLD, TERMINAL_STYLE_NONE, TERMINAL_STYLE_UNDERLINE };
    uint32_t fg[3] = { TERMINAL_RGB(255, 0, 0), TERMINAL_RGB(0, 255, 0), TERMINAL_COLOR_DEFAULT };
    uint32_t bg[3] = { TERMINAL_COLOR_DEFAULT, TERMINAL_COLOR_DEFAULT, TERMINAL_COLOR_DEFAULT };

    TerminalRun run = {
        .x = 2,
        .y = 5,
        .length = 3,
        .cells = glyphs,
        .styles = styles,
        .fg = fg,
        .bg = bg
    };

    TerminalCompositor_phase(comp, TERMINAL_PRESENT_BEGIN);
    assert(TerminalCompositor_blitRun(comp, run));
    TerminalCompositor_phase(comp, TERMINAL_PRESENT_END);

    n = read(fds[0], buf, sizeof(buf) - 1);
    assert(n > 0);
    buf[n] = '\0';
    assert(strstr(buf, "\033[?2026h") != nullptr); // sync begin
    assert(strstr(buf, "\033[6;3H") != nullptr);   // 1-indexed (row 5+1, col 2+1)
    assert(strstr(buf, ";38;2;255;0;0") != nullptr); // red truecolor
    assert(strstr(buf, "H") != nullptr);
    assert(strstr(buf, "i") != nullptr);
    assert(strstr(buf, "!") != nullptr);
    assert(strstr(buf, "\033[?2026l") != nullptr); // sync end

    // Test uniform run (same pen, contiguous text)
    uint16_t uniformStyles[3] = { TERMINAL_STYLE_BOLD, TERMINAL_STYLE_BOLD, TERMINAL_STYLE_BOLD };
    uint32_t uniformFg[3] = { TERMINAL_RGB(255, 0, 0), TERMINAL_RGB(255, 0, 0), TERMINAL_RGB(255, 0, 0) };
    TerminalRun uniformRun = {
        .x = 0,
        .y = 0,
        .length = 3,
        .cells = glyphs,
        .styles = uniformStyles,
        .fg = uniformFg,
        .bg = bg
    };
    assert(TerminalCompositor_blitRun(comp, uniformRun));
    TerminalCompositor_flush(comp);
    n = read(fds[0], buf, sizeof(buf) - 1);
    assert(n > 0);
    buf[n] = '\0';
    assert(strstr(buf, "Hi!") != nullptr); // contiguous because pen did not change mid-run

    // Test 256-color mode
    assert(TerminalCompositor_setColorMode(comp, TERMINAL_COMPOSITOR_COLOR_256));
    assert(TerminalCompositor_getColorMode(comp) == TERMINAL_COMPOSITOR_COLOR_256);
    assert(TerminalCompositor_blitRun(comp, run));
    TerminalCompositor_flush(comp);

    n = read(fds[0], buf, sizeof(buf) - 1);
    assert(n > 0);
    buf[n] = '\0';
    assert(strstr(buf, ";38;5;") != nullptr); // 256-color escape

    // Test leave
    assert(TerminalCompositor_leave(comp));
    n = read(fds[0], buf, sizeof(buf) - 1);
    assert(n > 0);
    buf[n] = '\0';
    assert(strstr(buf, "\033[?1049l") != nullptr); // leave alternate
    assert(strstr(buf, "\033[?25h") != nullptr); // show cursor

    TERMINAL_PROJECT(TerminalCompositor, comp);

    TerminalCompositor_free(comp);
    close(fds[0]);
    close(fds[1]);
}

static void testNegativeRejections(void) {
    REJECT(TerminalCompositor_setFd(nullptr, 1), "invalid fd");
    TerminalCompositor *comp = TerminalCompositor_0();
    assert(comp != nullptr);
    REJECT(TerminalCompositor_setFd(comp, -1), "invalid fd");

    assert(!TerminalCompositor_enter(nullptr));
    assert(!TerminalCompositor_leave(nullptr));
    assert(!TerminalCompositor_blitRun(nullptr, (TerminalRun){ 0 }));

    TerminalCompositor_free(comp);
}

int main(void) {
    testPipeOutput();
    testNegativeRejections();
    return 0;
}
