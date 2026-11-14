#ifndef DETERMINANT_TERMINAL_ANCHOR_H
#define DETERMINANT_TERMINAL_ANCHOR_H

#include "vex.h"
#include "ui/surface/terminal_projection.h"

// ui/panel/terminal_anchor.h — the placement vocabulary.
//
// A TerminalAnchor says WHERE a layer (or any region) lands inside its parent.
// TerminalAnchor_place() is PURE: given the parent's running cursor it returns
// both the placed region and the cursor afterwards, so resolution never mutates
// the caller and never needs a result destination (the Value Return Law).
//
// Two families, one rule each:
//   * a BAND takes a slice off one edge and RESERVES it, so the next layer sees
//     a smaller cursor — this is how a title band, two rails and a centre compose
//     in plain insertion order;
//   * FILL and the nine PINS never reserve, so a background or an overlay can sit
//     across the whole panel without disturbing the flow.
// One rule — reserve or not — is the whole layout model. It is the band math that
// TerminalFrame used to own, generalised so exactly one component decides
// placement.
//
// Terminal coordinates are CELLS, the terminal analogue of the Native Pixel Law,
// so no scale factor appears anywhere here.

// A region in cells, in the parent's coordinate space.
//
// SLOT RECORD: TerminalRect (no behaviour of its own)
typedef struct TerminalRect {
    bytes x;
    bytes y;
    bytes width;
    bytes height;
} TerminalRect;

// The result of one placement: where the layer landed, and the cursor it leaves.
//
// SLOT RECORD: TerminalAnchorPlacement (no behaviour of its own)
typedef struct TerminalAnchorPlacement {
    TerminalRect region;
    TerminalRect cursor;
} TerminalAnchorPlacement;

typedef enum TerminalAnchorKind {
    TERMINAL_ANCHOR_FILL = 0,       // the whole cursor, reserving nothing
    TERMINAL_ANCHOR_BAND_TOP,       // `extent` rows at the cursor top, reserved
    TERMINAL_ANCHOR_BAND_BOTTOM,    // `extent` rows at the cursor bottom, reserved
    TERMINAL_ANCHOR_BAND_LEFT,      // `extent` columns at the cursor left, reserved
    TERMINAL_ANCHOR_BAND_RIGHT,     // `extent` columns at the cursor right, reserved
    TERMINAL_ANCHOR_PIN_TOP_LEFT,
    TERMINAL_ANCHOR_PIN_TOP_CENTER,
    TERMINAL_ANCHOR_PIN_TOP_RIGHT,
    TERMINAL_ANCHOR_PIN_CENTER_LEFT,
    TERMINAL_ANCHOR_PIN_CENTER,
    TERMINAL_ANCHOR_PIN_CENTER_RIGHT,
    TERMINAL_ANCHOR_PIN_BOTTOM_LEFT,
    TERMINAL_ANCHOR_PIN_BOTTOM_CENTER,
    TERMINAL_ANCHOR_PIN_BOTTOM_RIGHT
} TerminalAnchorKind;

// INTENTIONAL(vex): TERMINAL_ANCHOR_AUTO is SIZE_MAX by design — it is the
// "take whatever the parent has" sentinel, so a zero extent stays a real, empty
// band instead of overloading 0 with a second meaning.
#define TERMINAL_ANCHOR_AUTO ((bytes) -1)

typedef struct TerminalAnchor {
    TerminalAnchorKind kind;   // placement family
    bytes extent;             // band thickness in cells (AUTO = the whole cursor)
    bytes width;              // pin width in cells (AUTO = the cursor's width)
    bytes height;             // pin height in cells (AUTO = the cursor's height)
    bytes insetX;             // inset applied inside the placed region
    bytes insetY;
} TerminalAnchor;

TerminalAnchor TerminalAnchor_0();                                       // FILL
TerminalAnchor TerminalAnchor_1(TerminalAnchorKind kind);
TerminalAnchor TerminalAnchor_2(TerminalAnchorKind kind, bytes extent);
TerminalAnchor TerminalAnchor_3(TerminalAnchorKind kind, bytes width, bytes height);
#define TerminalAnchor(...) CONSTRUCTOR_DISPATCH(TerminalAnchor, __VA_ARGS__)
#define TerminalAnchor_zero() TerminalAnchor_0()

bool TerminalAnchor_isBand(const TerminalAnchor *self);
// A reserving anchor shrinks the cursor for every later layer; FILL and pins do not.
bool TerminalAnchor_reserves(const TerminalAnchor *self);

TerminalAnchorKind TerminalAnchor_getKind(const TerminalAnchor *self);
bytes TerminalAnchor_getExtent(const TerminalAnchor *self);
bytes TerminalAnchor_getWidth(const TerminalAnchor *self);
bytes TerminalAnchor_getHeight(const TerminalAnchor *self);
bytes TerminalAnchor_getInsetX(const TerminalAnchor *self);
bytes TerminalAnchor_getInsetY(const TerminalAnchor *self);

bool TerminalAnchor_setKind(TerminalAnchor *self, TerminalAnchorKind kind);
bool TerminalAnchor_setExtent(TerminalAnchor *self, bytes extent);
bool TerminalAnchor_setSize(TerminalAnchor *self, bytes width, bytes height);
bool TerminalAnchor_setInset(TerminalAnchor *self, bytes insetX, bytes insetY);

// Pure: place this anchor into `cursor`. The returned `cursor` is what the next
// layer resolves against; the caller's own value is untouched. Degenerate
// geometry is clamped rather than underflowed: an extent or inset larger than the
// cursor yields an empty region and an unchanged-as-possible cursor, never a wrap.
TerminalAnchorPlacement TerminalAnchor_place(const TerminalAnchor *self, TerminalRect cursor);

TerminalProjection TerminalAnchor_toString(const TerminalAnchor *self);
TerminalProjection TerminalAnchor_toStringStruct(const TerminalAnchor *self);

#endif
