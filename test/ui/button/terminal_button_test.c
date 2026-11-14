// Purpose: caller clicks a terminal button or feeds normalized keys. The widget
// activates only enabled/focused input, preserves the grid on rejected geometry,
// loses focus outside/on escape/disable and renders distinct deterministic cells.

#include "vex.h"
#include "ui/button/terminal_button.h"
#include "determinant/test/support.h"
static bool sink(any context, const char *text, bytes length) {
    (fn) context; (fn) text; (fn) length; return true;
}
int main() {
    char cells[20];
    Terminal terminal = Terminal();
    assert(Terminal_bind(&terminal, cells, 20, 10, 2, sink, nullptr));
    TerminalButton zero = TerminalButton();
    assert(TerminalButton_isEnabled(&zero) && !TerminalButton_isFocused(&zero));
    TerminalButton b = TerminalButton("Go", 1, 1);
    beginDiagnostics();
    assert(TerminalButton_render(&b, &terminal));
    assert(memcmp(cells + 11, "[Go]", 4) == 0);
    assert(!TerminalButton_key(&b, TERMINAL_KEY_ENTER));
    assert(TerminalButton_click(&b, 1, 1));
    assert(TerminalButton_key(&b, TERMINAL_KEY_SPACE));
    assert(TerminalButton_key(&b, TERMINAL_KEY_ENTER));
    assert(TerminalButton_render(&b, &terminal));
    assert(memcmp(cells + 11, "{Go}", 4) == 0);
    assert(!TerminalButton_click(&b, 5, 1));
    assert(!TerminalButton_isFocused(&b));
    TerminalButton_setFocused(&b, true);
    assert(!TerminalButton_key(&b, TERMINAL_KEY_ESCAPE));
    assert(!TerminalButton_isFocused(&b));
    TerminalButton_setEnabled(&b, false);
    TerminalButton_setFocused(&b, true);
    assert(!TerminalButton_isFocused(&b));
    assert(!TerminalButton_click(&b, 1, 1));
    assert(TerminalButton_render(&b, &terminal));
    assert(memcmp(cells + 11, "(Go)", 4) == 0);
    TerminalButton_setEnabled(&b, true);
    assert(TerminalButton_setLabel(&b, "Yes"));
    assert(strcmp(TerminalButton_getLabel(&b), "Yes") == 0);
    assert(TerminalButton_setPosition(&b, 5, 0));
    TerminalButtonPosition position = TerminalButton_getPosition(&b);
    assert(position.x == 5 && position.y == 0);
    assert(TerminalButton_render(&b, &terminal)); // exact right edge
    endDiagnostics(0, nullptr);
    REJECT(TerminalButton_setLabel(&b, "bad\n"), "label");
    REJECT(TerminalButton_setLabel(&b, nullptr), "label");
    REJECT(TerminalButton_setPosition(&b, SIZE_MAX, 0), "extent");
    assert(TerminalButton_setPosition(&b, 6, 0));
    char before[20]; memcpy(before, cells, 20);
    REJECT(TerminalButton_render(&b, &terminal), "outside");
    assert(memcmp(before, cells, 20) == 0);
    REJECT(TerminalButton_key(&b, (TerminalKey) 999), "invalid");
    REJECT(TerminalButton_render(nullptr, &terminal), "null");
    assert(TerminalButton_getLabel(nullptr) == nullptr && !TerminalButton_isFocused(nullptr));
    assert(!TerminalButton_isEnabled(nullptr));
    position = TerminalButton_getPosition(nullptr);
    assert(position.x == 0 && position.y == 0);
    TERMINAL_PROJECT(TerminalButton, &b);
    assert(Terminal_free(&terminal));
    return 0;
}
