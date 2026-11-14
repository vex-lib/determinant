#ifndef DETERMINANT_TERMINAL_BUTTON_H
#define DETERMINANT_TERMINAL_BUTTON_H

#include "vex.h"
#include "ui/surface/terminal.h"
#include "ui/surface/terminal_projection.h"
typedef struct TerminalButton {
    const char *label;
    bytes x;
    bytes y;
    bool focused;
    bool enabled;
} TerminalButton;
typedef struct TerminalButtonPosition { bytes x; bytes y; } TerminalButtonPosition;
TerminalButton TerminalButton_0();
TerminalButton TerminalButton_3(const char *label, bytes x, bytes y);
#define TerminalButton(...) CONSTRUCTOR_DISPATCH(TerminalButton, __VA_ARGS__)
#define TerminalButton_zero() TerminalButton_0()
// Label borrowed immutable printable ASCII; setters reject without mutation.
bool TerminalButton_setLabel(TerminalButton *self, const char *label);
const char *TerminalButton_getLabel(const TerminalButton *self);
bool TerminalButton_setPosition(TerminalButton *self, bytes x, bytes y);
TerminalButtonPosition TerminalButton_getPosition(const TerminalButton *self);
fn TerminalButton_setFocused(TerminalButton *self, bool focused);
bool TerminalButton_isFocused(const TerminalButton *self);
fn TerminalButton_setEnabled(TerminalButton *self, bool enabled);
bool TerminalButton_isEnabled(const TerminalButton *self);
// Caller-fed normalized input; press returns activation, no retained callback.
// click acquires focus only for enabled button and returns true for an inside hit.
bool TerminalButton_key(TerminalButton *self, TerminalKey key);
bool TerminalButton_click(TerminalButton *self, bytes x, bytes y);
bool TerminalButton_render(const TerminalButton *self, Terminal *terminal);
TerminalProjection TerminalButton_toString(const TerminalButton *self);
TerminalProjection TerminalButton_toStringStruct(const TerminalButton *self);
#endif
