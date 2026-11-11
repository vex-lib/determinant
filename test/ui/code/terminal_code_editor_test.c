// determinant/test/ui/code/terminal_code_editor_test.c
// Owner: TerminalCodeEditor — canonical code editor widget with line indexing,
// gutter calculation, auto-indentation, line split/join, and canvas rendering.

#include "ui/code/terminal_code_editor.h"
#include "determinant/test/support.h"
#include <string.h>

static void testCodeEditorBasicsAndText(void) {
    TerminalCodeEditor *editor = TerminalCodeEditor("int main() {\n    return 0;\n}");
    assert(editor != nullptr);
    assert(TerminalCodeEditor_getLineCount(editor) == 3u);
    assert(strcmp(TerminalCodeEditor_getLine(editor, 0u), "int main() {") == 0);
    assert(strcmp(TerminalCodeEditor_getLine(editor, 1u), "    return 0;") == 0);
    assert(strcmp(TerminalCodeEditor_getLine(editor, 2u), "}") == 0);

    // Gutter width for 3 lines: 1 digit + 3 padding = 4 cells
    assert(TerminalCodeEditor_getShowGutter(editor));
    assert(TerminalCodeEditor_getGutterWidth(editor) == 4u);

    // Toggle gutter off and on
    assert(TerminalCodeEditor_setShowGutter(editor, false));
    assert(!TerminalCodeEditor_getShowGutter(editor));
    assert(TerminalCodeEditor_getGutterWidth(editor) == 0u);

    assert(TerminalCodeEditor_setShowGutter(editor, true));
    assert(TerminalCodeEditor_getShowGutter(editor));
    assert(TerminalCodeEditor_getGutterWidth(editor) == 4u);

    TERMINAL_PROJECT(TerminalCodeEditor, editor);
    TerminalCodeEditor_free(editor);
}

static void testEditingOperations(void) {
    TerminalCodeEditor *editor = TerminalCodeEditor();
    assert(editor != nullptr);
    assert(TerminalCodeEditor_getLineCount(editor) == 1u);
    assert(strcmp(TerminalCodeEditor_getLine(editor, 0u), "") == 0);

    // Insert text
    assert(TerminalCodeEditor_insertText(editor, "    int x = 42;", 15u));
    assert(TerminalCodeEditor_getLineCount(editor) == 1u);
    assert(strcmp(TerminalCodeEditor_getLine(editor, 0u), "    int x = 42;") == 0);
    assert(TerminalCodeEditor_getCursorLine(editor) == 0u);
    assert(TerminalCodeEditor_getCursorCol(editor) == 15u);

    // Insert newline: tests auto-indentation feature (preserves 4 spaces)
    assert(TerminalCodeEditor_insertChar(editor, '\n'));
    assert(TerminalCodeEditor_getLineCount(editor) == 2u);
    assert(TerminalCodeEditor_getCursorLine(editor) == 1u);
    assert(TerminalCodeEditor_getCursorCol(editor) == 4u);
    assert(strcmp(TerminalCodeEditor_getLine(editor, 1u), "    ") == 0);

    // Insert remaining code on line 1
    assert(TerminalCodeEditor_insertText(editor, "return x;", 9u));
    assert(strcmp(TerminalCodeEditor_getLine(editor, 1u), "    return x;") == 0);

    // Backspace semicolon
    assert(TerminalCodeEditor_backspace(editor));
    assert(strcmp(TerminalCodeEditor_getLine(editor, 1u), "    return x") == 0);

    // Move cursor to start of line 1 and backspace to join lines
    assert(TerminalCodeEditor_setCursor(editor, 1u, 0u));
    assert(TerminalCodeEditor_backspace(editor));
    assert(TerminalCodeEditor_getLineCount(editor) == 1u);
    assert(TerminalCodeEditor_getCursorLine(editor) == 0u);
    assert(TerminalCodeEditor_getCursorCol(editor) == 15u);
    assert(strcmp(TerminalCodeEditor_getLine(editor, 0u), "    int x = 42;    return x") == 0);

    // Delete character at cursor position
    assert(TerminalCodeEditor_setCursor(editor, 0u, 4u));
    assert(TerminalCodeEditor_delete(editor)); // deletes 'i'
    assert(strcmp(TerminalCodeEditor_getLine(editor, 0u), "    nt x = 42;    return x") == 0);

    // Clear buffer
    TerminalCodeEditor_clear(editor);
    assert(TerminalCodeEditor_getLineCount(editor) == 1u);
    assert(TerminalCodeEditor_getCursorLine(editor) == 0u);
    assert(TerminalCodeEditor_getCursorCol(editor) == 0u);
    assert(strcmp(TerminalCodeEditor_getLine(editor, 0u), "") == 0);

    TerminalCodeEditor_free(editor);
}

static void testCursorMovementAndClamping(void) {
    TerminalCodeEditor *editor = TerminalCodeEditor("short\nlonger_line\nx");
    assert(editor != nullptr);
    assert(TerminalCodeEditor_getLineCount(editor) == 3u);

    // Position at end of line 1 ("longer_line", length 11)
    assert(TerminalCodeEditor_setCursor(editor, 1u, 11u));
    assert(TerminalCodeEditor_getCursorLine(editor) == 1u);
    assert(TerminalCodeEditor_getCursorCol(editor) == 11u);

    // Move up: line 0 ("short", length 5) -> cursor column clamped to 5
    TerminalCodeEditor_moveCursor(editor, -1, 0);
    assert(TerminalCodeEditor_getCursorLine(editor) == 0u);
    assert(TerminalCodeEditor_getCursorCol(editor) == 5u);

    // Move down two lines: line 2 ("x", length 1) -> cursor column clamped to 1
    TerminalCodeEditor_moveCursor(editor, 2, 0);
    assert(TerminalCodeEditor_getCursorLine(editor) == 2u);
    assert(TerminalCodeEditor_getCursorCol(editor) == 1u);

    // Left and right navigation
    TerminalCodeEditor_moveCursor(editor, 0, -1);
    assert(TerminalCodeEditor_getCursorCol(editor) == 0u);
    TerminalCodeEditor_moveCursor(editor, 0, 1);
    assert(TerminalCodeEditor_getCursorCol(editor) == 1u);

    TerminalCodeEditor_free(editor);
}

static void testCanvasRendering(void) {
    TerminalCodeEditor *editor = TerminalCodeEditor("alpha\nbeta");
    assert(editor != nullptr);

    u32 cells[30 * 4];
    for (bytes i = 0u; i < 30u * 4u; ++i)
        cells[i] = ' ';
    TerminalCanvas canvas = TerminalCanvas_4(cells, nullptr, 30u, 4u);

    TerminalCodeEditor_render(editor, &canvas);

    // Line 1 gutter: ' ' at 0, ' ' at 1, '1' at 2, ' ' at 3 (gutter width 4)
    assert(cells[0] == ' ');
    assert(cells[1] == ' ');
    assert(cells[2] == '1');
    assert(cells[3] == ' ');
    // Text: "alpha" starting at col 4
    assert(cells[4] == 'a');
    assert(cells[5] == 'l');
    assert(cells[6] == 'p');
    assert(cells[7] == 'h');
    assert(cells[8] == 'a');

    // Line 2 (row 1, stride 30): gutter ' ' at 30, ' ' at 31, '2' at 32, ' ' at 33
    assert(cells[30] == ' ');
    assert(cells[31] == ' ');
    assert(cells[32] == '2');
    assert(cells[33] == ' ');
    // Text: "beta" starting at col 34
    assert(cells[34] == 'b');
    assert(cells[35] == 'e');
    assert(cells[36] == 't');
    assert(cells[37] == 'a');

    TerminalCodeEditor_free(editor);
}

static void testNegativeRejections(void) {
    assert(TerminalCodeEditor_getLineCount(nullptr) == 0u);
    assert(strcmp(TerminalCodeEditor_getLine(nullptr, 0u), "") == 0);
    assert(!TerminalCodeEditor_setText(nullptr, "test"));
    assert(!TerminalCodeEditor_insertChar(nullptr, 'a'));
    assert(!TerminalCodeEditor_insertText(nullptr, "test", 4u));
    assert(!TerminalCodeEditor_backspace(nullptr));
    assert(!TerminalCodeEditor_delete(nullptr));
    assert(!TerminalCodeEditor_setCursor(nullptr, 0u, 0u));
    assert(!TerminalCodeEditor_setShowGutter(nullptr, true));
    assert(!TerminalCodeEditor_getShowGutter(nullptr));
    assert(TerminalCodeEditor_getGutterWidth(nullptr) == 0u);
    assert(TerminalCodeEditor_getCursorLine(nullptr) == 0u);
    assert(TerminalCodeEditor_getCursorCol(nullptr) == 0u);
}

int main(void) {
    testCodeEditorBasicsAndText();
    testEditingOperations();
    testCursorMovementAndClamping();
    testCanvasRendering();
    testNegativeRejections();
    return 0;
}
