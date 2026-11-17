#include "vex.h"
#include "ui/button/terminal_button.h"
#include <string.h>
#include <stdio.h>

;;DEFINITION
/** A reusable terminal-cell button borrows its immutable label and paints into
 * a resident Terminal grid. Host-normalized keys and click coordinates feed
 * interaction, never stdin. Activation is a returned event, not owned code;
 * consumer consent/tool/application policy remains outside this widget. */
;;OVERVIEW
/** ========================================================================
 * CLASS: TerminalButton (ui/button/terminal_button.c)
 * STRUCT FIELDS (Mirroring ui/button/terminal_button.h)
 * const char *label         borrowed printable ASCII label
 * bytes x                 left column
 * bytes y                 row
 * bool focused             admitted keyboard focus
 * bool enabled             interaction enabled
 * FUNCTION REGISTRY (exports)
 * Constructors: TerminalButton TerminalButton_0()
 *   TerminalButton TerminalButton_3(const char *label, bytes x, bytes y)
 * Setters: bool TerminalButton_setLabel(TerminalButton *self, const char *label)
 *   bool TerminalButton_setPosition(TerminalButton *self, bytes x, bytes y)
 *   fn TerminalButton_setFocused(TerminalButton *self, bool focused)
 *   fn TerminalButton_setEnabled(TerminalButton *self, bool enabled)
 * Getters: const char *TerminalButton_getLabel(const TerminalButton *self)
 *   fn TerminalButton_getPosition(const TerminalButton *self,
 *     bytes *outX, bytes *outY)
 *   bool TerminalButton_isFocused(const TerminalButton *self)
 *   bool TerminalButton_isEnabled(const TerminalButton *self)
 * Input: bool TerminalButton_key(TerminalButton *self, TerminalKey key)
 *   bool TerminalButton_click(TerminalButton *self, bytes x, bytes y)
 * Painting: bool TerminalButton_render(const TerminalButton *self, Terminal *terminal)
 * Projections: TerminalProjection TerminalButton_toString/toStringStruct(self)
 * PRIVATE HELPERS: validLabel(label) checks printable ASCII/size arithmetic.
 * CONTRACT: externally serialized; borrowed label must remain immutable/live.
 * Default empty label at 0,0, unfocused/enabled. Disabling clears focus; setting
 * focus on disabled has no effect. Enabled inside click focuses and activates;
 * outside click loses focus. Enter/Space activate once per fed key only when
 * focused/enabled; Escape loses focus. No repeats/timing/device decoding owned.
 * Paint clips by rejection before writing, preserving grid on failure; brackets
 * denote normal/focused/disabled. Projections redact label (no secret copying).
 * ======================================================================== */

;;GETTER
static inline bool TerminalButton_enabled(const TerminalButton *s) { return (*s).enabled; }
;;GETTER
static inline bool *TerminalButton_enabledMut(TerminalButton *s) { return &(*s).enabled; }
;;SETTER
static inline fn TerminalButton_setEnabled(TerminalButton *s, bool v) { (*s).enabled = v; }
;;GETTER
static inline bool TerminalButton_focused(const TerminalButton *s) { return (*s).focused; }
;;GETTER
static inline bool *TerminalButton_focusedMut(TerminalButton *s) { return &(*s).focused; }
;;SETTER
static inline fn TerminalButton_setFocused(TerminalButton *s, bool v) { (*s).focused = v; }
;;GETTER
static inline const char * TerminalButton_label(const TerminalButton *s) { return (*s).label; }
;;GETTER
static inline const char * *TerminalButton_labelMut(TerminalButton *s) { return &(*s).label; }
;;SETTER
static inline fn TerminalButton_setLabel(TerminalButton *s, const char * v) { (*s).label = v; }
;;GETTER
static inline bytes TerminalButton_x(const TerminalButton *s) { return (*s).x; }
;;GETTER
static inline bytes *TerminalButton_xMut(TerminalButton *s) { return &(*s).x; }
;;SETTER
static inline fn TerminalButton_setX(TerminalButton *s, bytes v) { (*s).x = v; }
;;GETTER
static inline bytes TerminalButton_y(const TerminalButton *s) { return (*s).y; }
;;GETTER
static inline bytes *TerminalButton_yMut(TerminalButton *s) { return &(*s).y; }
;;SETTER
static inline fn TerminalButton_setY(TerminalButton *s, bytes v) { (*s).y = v; }

static bool validLabel(const char *label) {
    if (label == nullptr)
        return false;
    bytes n = strlen(label);
    if (n > SIZE_MAX - 2)
        return false;
    for (bytes i = 0; i < n; ++i)
        if ((u8) label[i] < 32 || (u8) label[i] >= 127)
            return false;
    return true;
}
TerminalButton TerminalButton_0() { return (TerminalButton) {.label = "", .enabled = true}; }
TerminalButton TerminalButton_3(const char *label, bytes x, bytes y) {
    TerminalButton self = TerminalButton_0();
    if (!TerminalButton_setLabel(&self, label) || !TerminalButton_setPosition(&self, x, y))
        return TerminalButton_0();
    return self;
}
bool TerminalButton_setLabel(TerminalButton *self, const char *label) {
    if (self == nullptr || !validLabel(label) || strlen(label) + 2 > SIZE_MAX - TerminalButton_x(self)) {
        THROW("TerminalButton setLabel: invalid label/extent");
        return false;
    }
    TerminalButton_setLabel(self, label);
    return true;
}
const char *TerminalButton_getLabel(const TerminalButton *self) { return self ? TerminalButton_label(self) : nullptr; }
bool TerminalButton_setPosition(TerminalButton *self, bytes x, bytes y) {
    if (self == nullptr || strlen(TerminalButton_label(self)) + 2 > SIZE_MAX - x) {
        THROW("TerminalButton setPosition: invalid extent");
        return false;
    }
    TerminalButton_setX(self, x);
    TerminalButton_setY(self, y);
    return true;
}
TerminalButtonPosition TerminalButton_getPosition(const TerminalButton *self) {
    return self ? (TerminalButtonPosition) { TerminalButton_x(self), TerminalButton_y(self) } : (TerminalButtonPosition) { 0u, 0u };
}
fn TerminalButton_setFocused(TerminalButton *self, bool focused) {
    if (self == nullptr) {
        THROW("TerminalButton setFocused: null button");
        return;
    }
    TerminalButton_setFocused(self, focused && TerminalButton_enabled(self));
}
bool TerminalButton_isFocused(const TerminalButton *self) { return self && TerminalButton_focused(self); }
fn TerminalButton_setEnabled(TerminalButton *self, bool enabled) {
    if (self == nullptr) {
        THROW("TerminalButton setEnabled: null button");
        return;
    }
    TerminalButton_setEnabled(self, enabled);
    if (!enabled)
        TerminalButton_setFocused(self, false);
}
bool TerminalButton_isEnabled(const TerminalButton *self) { return self && TerminalButton_enabled(self); }
bool TerminalButton_key(TerminalButton *self, TerminalKey key) {
    if (self == nullptr || key < TERMINAL_KEY_NONE || key > TERMINAL_KEY_DOWN) {
        THROW("TerminalButton key: invalid input");
        return false;
    }
    if (key == TERMINAL_KEY_ESCAPE)
        TerminalButton_setFocused(self, false);
    return TerminalButton_enabled(self) && TerminalButton_focused(self) && (key == TERMINAL_KEY_ENTER || key == TERMINAL_KEY_SPACE);
}
bool TerminalButton_click(TerminalButton *self, bytes x, bytes y) {
    if (self == nullptr) {
        THROW("TerminalButton click: null button");
        return false;
    }
    bool hit = TerminalButton_enabled(self) && y == TerminalButton_y(self) && x >= TerminalButton_x(self)
        && x - TerminalButton_x(self) < strlen(TerminalButton_label(self)) + 2;
    TerminalButton_setFocused(self, hit);
    return hit;
}
bool TerminalButton_render(const TerminalButton *self, Terminal *terminal) {
    if (self == nullptr || terminal == nullptr) {
        THROW("TerminalButton render: null input");
        return false;
    }
    bytes length = strlen(TerminalButton_label(self));
    bytes width = Terminal_getWidth(terminal);
    if (TerminalButton_y(self) >= Terminal_getHeight(terminal) || TerminalButton_x(self) > width || length + 2 > width - TerminalButton_x(self)) {
        THROW("TerminalButton render: outside cell grid");
        return false;
    }
    const char *left = !TerminalButton_enabled(self) ? "(" : (TerminalButton_focused(self) ? "{" : "[");
    const char *right = !TerminalButton_enabled(self) ? ")" : (TerminalButton_focused(self) ? "}" : "]");
    return Terminal_text(terminal, TerminalButton_x(self), TerminalButton_y(self), left, 1)
        && Terminal_text(terminal, TerminalButton_x(self) + 1, TerminalButton_y(self), TerminalButton_label(self), length)
        && Terminal_text(terminal, TerminalButton_x(self) + 1 + length, TerminalButton_y(self), right, 1);
}
TerminalProjection TerminalButton_toString(const TerminalButton *self) {
    return TerminalButton_toStringStruct(self);
}
TerminalProjection TerminalButton_toStringStruct(const TerminalButton *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format("TerminalButton{label=<borrowed>,x=%zu,y=%zu,focused=%d,enabled=%d}",
        TerminalButton_x(self), TerminalButton_y(self), TerminalButton_focused(self), TerminalButton_enabled(self));
}
