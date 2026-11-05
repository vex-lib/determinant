// Owner: TerminalInputLine — generic caller-buffered input. Editing, capacity
// refusal, prefix, render, unbound safety.
#include "ui/field/terminal_input_line.h"
#include "determinant/test/support.h"

int main(void) {
    char buffer[8];
    TerminalInputLine line = TerminalInputLine(buffer, sizeof buffer);
    assert(TerminalInputLine_isBound(&line));
    assert(strcmp(TerminalInputLine_getPrefix(&line), "> ") == 0);
    assert(TerminalInputLine_length(&line) == 0);

    assert(TerminalInputLine_add(&line, 'h') && TerminalInputLine_add(&line, 'i'));
    assert(strcmp(TerminalInputLine_getBuffer(&line), "hi") == 0 && TerminalInputLine_length(&line) == 2);
    assert(TerminalInputLine_backspace(&line));
    assert(strcmp(TerminalInputLine_getBuffer(&line), "h") == 0);
    TerminalInputLine_clear(&line);
    assert(TerminalInputLine_length(&line) == 0 && TerminalInputLine_getBuffer(&line)[0] == '\0');

    // Render prefix + text into a canvas.
    enum { W = 8, H = 1 };
    static uint32_t cells[W * H];
    static uint32_t fg[W * H];
    TerminalCanvas canvas = TerminalCanvas(cells, nullptr, fg, nullptr, W, H);
    TerminalInputLine_add(&line, 'a');
    TerminalInputLine_setPens(&line, TerminalPen(0, TERMINAL_RGB(1, 2, 3)), TerminalPen(0, TERMINAL_RGB(4, 5, 6)));
    TerminalInputLinePens pens = TerminalInputLine_getPens(&line);
    assert(pens.prefix.fg == TERMINAL_RGB(1, 2, 3) && pens.text.fg == TERMINAL_RGB(4, 5, 6));
    TerminalInputLine_render(&line, &canvas);
    assert(TerminalCanvas_glyphAt(&canvas, 0, 0) == '>' && TerminalCanvas_glyphAt(&canvas, 2, 0) == 'a');
    assert(TerminalCanvas_fgAt(&canvas, 0, 0) == TERMINAL_RGB(1, 2, 3));
    assert(TerminalCanvas_fgAt(&canvas, 2, 0) == TERMINAL_RGB(4, 5, 6));

    // Capacity: an 8-byte buffer keeps a NUL, so 7 characters fit; the 8th is refused.
    TerminalInputLine_clear(&line);
    for (int i = 0; i < 7; ++i)
        assert(TerminalInputLine_add(&line, 'x'));
    REJECT(TerminalInputLine_add(&line, 'x'), "capacity");

    // Prefix override + unbound/null safety.
    assert(TerminalInputLine_setPrefix(&line, ":: "));
    assert(strcmp(TerminalInputLine_getPrefix(&line), ":: ") == 0);
    TerminalInputLine unbound = TerminalInputLine_0();
    assert(!TerminalInputLine_isBound(&unbound));
    REJECT(TerminalInputLine_add(&unbound, 'x'), "unbound");
    assert(!TerminalInputLine_backspace(&unbound));
    TerminalInputLine_render(&unbound, &canvas);
    TerminalInputLine_render(nullptr, &canvas);
    TerminalInputLine_render(&line, nullptr);
    assert(TerminalInputLine_getBuffer(nullptr) == nullptr && TerminalInputLine_length(nullptr) == 0);
    assert(!TerminalInputLine_backspace(nullptr));
    REJECT(TerminalInputLine_add(&line, 0x1b), "non-printable");
    REJECT(TerminalInputLine_setPrefix(&line, nullptr), "null");
    REJECT(TerminalInputLine_bind(&line, nullptr, 8), "too small");
    TerminalInputLine_setPens(nullptr, TerminalPen_0(), TerminalPen_0());
    pens = TerminalInputLine_getPens(nullptr);
    assert(pens.prefix.fg == TERMINAL_COLOR_DEFAULT && pens.text.fg == TERMINAL_COLOR_DEFAULT);

    TERMINAL_PROJECT(TerminalInputLine, &line);
    return 0;
}
