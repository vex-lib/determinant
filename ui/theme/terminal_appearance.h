#ifndef DETERMINANT_TERMINAL_APPEARANCE_H
#define DETERMINANT_TERMINAL_APPEARANCE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "util/constructor.h"

#include "ui/surface/terminal_canvas.h"
#include "ui/surface/terminal_projection.h"
#include "ui/panel/terminal_frame.h"

// ui/theme/terminal_appearance.h — the toolkit's NAMED APPEARANCE presets.
//
// A look is a named, self-contained record: the connector glyph set (ASCII or
// real box drawing) plus the colour palette. Applications list the presets, let
// the user pick one in their own settings UI, persist the NAME, and apply it —
// the toolkit never stores preferences and never reads a config file.
//
// Presets (codepoint glyphs, so a preset may use '─' / '—' / '═' as well as
// '-'):
//   "ascii-solid"    `----------------------------`
//   "ascii-striped"  `---.---.---.---.---.---.---`   (default)
//   "ascii-dashed"   `- - - - - - - - - - - - - -`
//   "unicode-light"  `──────────┬───────────┬─────`
//   "unicode-emdash" `——————————┬———————————┬—————`
//   "unicode-d64" `══════════╦═══════════╦═════`
// Every preset also carries accent/muted/text/error colours.

typedef struct TerminalAppearance {
    TerminalRule rule;
    u32 colorAccent;
    u32 colorMuted;
    u32 colorText;
    u32 colorError;
} TerminalAppearance;
typedef struct TerminalAppearanceColors { u32 accent; u32 muted; u32 text; u32 error; } TerminalAppearanceColors;

// The default appearance (the "ascii-striped" preset).
TerminalAppearance TerminalAppearance_0();
#define TerminalAppearance(...) CONSTRUCTOR_DISPATCH(TerminalAppearance, __VA_ARGS__)

// --- Preset registry (for a settings UI: list names, pick by name/index) ---
bytes      TerminalAppearance_presetCount();
const char *TerminalAppearance_presetName(bytes index);      // NULL when out of range
TerminalAppearance TerminalAppearance_preset(bytes index);   // default when out of range
TerminalAppearance TerminalAppearance_named(const char *name);// default when unknown

// --- Configuration (symmetric setters & getters) ---
bool TerminalAppearance_setRule(TerminalAppearance *self, TerminalRule rule);
TerminalRule TerminalAppearance_getRule(const TerminalAppearance *self);
bool TerminalAppearance_setColors(TerminalAppearance *self, u32 accent, u32 muted,
                                  u32 text, u32 error);
TerminalAppearanceColors TerminalAppearance_getColors(const TerminalAppearance *self);

// --- Apply ---
// Push the preset's rule into a frame; draw a rule run into any canvas region.
fn TerminalAppearance_applyTo(const TerminalAppearance *self, TerminalFrame *frame);
fn TerminalAppearance_drawRule(const TerminalAppearance *self, TerminalCanvas *canvas,
                                 bytes ly, TerminalPen pen);

TerminalProjection TerminalAppearance_toString(const TerminalAppearance *self);
TerminalProjection TerminalAppearance_toStringStruct(const TerminalAppearance *self);

#endif
