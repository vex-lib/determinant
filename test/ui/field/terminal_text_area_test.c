// determinant/test/ui/field/terminal_text_area_test.c
// Owner: TerminalTextArea — multiline text editing area, 2D cursor movement,
// and canvas rendering.

#include "ui/field/terminal_text_area.h"
#include "determinant/test/support.h"
#include <string.h>

static void testTextAreaEditing(void) {
    char storage[256];
    storage[0] = '\0';

    TerminalTextArea *area = TerminalTextArea(storage, sizeof(storage));
    assert(area != nullptr);
    assert(TerminalTextArea_getCapacity(area) == sizeof(storage));
    assert(TerminalTextArea_getLength(area) == 0);

    // Insert multiline text
    assert(TerminalTextArea_insertText(area, "Hello\nWorld!", 12));
    assert(TerminalTextArea_getLength(area) == 12);
    assert(strcmp(TerminalTextArea_getBuffer(area), "Hello\nWorld!") == 0);
    assert(TerminalTextArea_getCursorByte(area) == 12);

    // Backspace '!'
    assert(TerminalTextArea_backspace(area));
    assert(strcmp(TerminalTextArea_getBuffer(area), "Hello\nWorld") == 0);
    assert(TerminalTextArea_getLength(area) == 11);

    // Insert character
    assert(TerminalTextArea_insertChar(area, '?'));
    assert(strcmp(TerminalTextArea_getBuffer(area), "Hello\nWorld?") == 0);

    // Cursor navigation
    TerminalTextArea_moveCursor(area, -1, 0); // move up to first line
    assert(TerminalTextArea_getCursorByte(area) < 6);

    // Render into canvas
    u32 cells[20 * 5];
    for (bytes i = 0; i < 20 * 5; ++i)
        cells[i] = ' ';
    TerminalCanvas canvas = TerminalCanvas_4(cells, nullptr, 20, 5);
    TerminalTextArea_render(area, &canvas);

    // Wrap toggle
    assert(!TerminalTextArea_getWrap(area));
    assert(TerminalTextArea_setWrap(area, true));
    assert(TerminalTextArea_getWrap(area));

    TERMINAL_PROJECT(TerminalTextArea, area);

    TerminalTextArea_clear(area);
    assert(TerminalTextArea_getLength(area) == 0);
    assert(TerminalTextArea_getCursorByte(area) == 0);

    TerminalTextArea_free(area);
}

static fn testNegativeRejections() {
    assert(!TerminalTextArea_bind(nullptr, nullptr, 0));
    assert(!TerminalTextArea_insertChar(nullptr, 'a'));
    assert(!TerminalTextArea_insertText(nullptr, "test", 4));
    assert(!TerminalTextArea_backspace(nullptr));
    assert(!TerminalTextArea_delete(nullptr));
    assert(!TerminalTextArea_setWrap(nullptr, true));
    assert(!TerminalTextArea_setPen(nullptr, TerminalPen_0()));

    TerminalTextArea *empty = TerminalTextArea_0();
    assert(empty != nullptr);
    assert(!TerminalTextArea_insertChar(empty, 'a')); // unbounded buffer
    TerminalTextArea_free(empty);
}

int main() {
    testTextAreaEditing();
    testNegativeRejections();
    return 0;
}
