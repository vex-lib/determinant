#include "vex.h"
#include "ui/text/terminal_status_bar.h"
#include "format/terminal_style.h"
#include <string.h>
#include <stdio.h>

;;DEFINITION
/** A generic one-row status line: a borrowed left string and an optional right
 * string, drawn so the right only appears when it cannot collide with the left.
 * No meaning is attached to either string. Owns no memory. */
;;OVERVIEW
/** ========================================================================
 * CLASS: TerminalStatusBar (ui/text/terminal_status_bar.c)
 * STRUCT FIELDS (Mirroring ui/text/terminal_status_bar.h)
 * const char *left, *right  borrowed status strings (right nullable)
 * TerminalPen leftPen, rightPen  pens
 * FUNCTION REGISTRY (exports)
 * Constructors: TerminalStatusBar_0(), TerminalStatusBar_1(left)
 * Config: set/getLeft, set/getRight, set/getPens
 * Render: TerminalStatusBar_render(self, canvas)
 * Projections: TerminalProjection TerminalStatusBar_toString/toStringStruct(self)
 * CONTRACT: null/invalid canvas no-ops; the left string clips at the region
 * width; the right string is dropped when it would touch the left; no
 * allocation, no logging.
 * ======================================================================== */


;;GETTER
static inline const char * TerminalStatusBar_left(const TerminalStatusBar *s) { return (*s).left; }
;;GETTER
static inline const char * *TerminalStatusBar_leftMut(TerminalStatusBar *s) { return &(*s).left; }
;;SETTER
static inline fn TerminalStatusBar_setLeft(TerminalStatusBar *s, const char * v) { (*s).left = v; }
;;GETTER
static inline TerminalPen TerminalStatusBar_leftPen(const TerminalStatusBar *s) { return (*s).leftPen; }
;;GETTER
static inline TerminalPen *TerminalStatusBar_leftPenMut(TerminalStatusBar *s) { return &(*s).leftPen; }
;;SETTER
static inline fn TerminalStatusBar_setLeftPen(TerminalStatusBar *s, TerminalPen v) { (*s).leftPen = v; }
;;GETTER
static inline const char * TerminalStatusBar_right(const TerminalStatusBar *s) { return (*s).right; }
;;GETTER
static inline const char * *TerminalStatusBar_rightMut(TerminalStatusBar *s) { return &(*s).right; }
;;SETTER
static inline fn TerminalStatusBar_setRight(TerminalStatusBar *s, const char * v) { (*s).right = v; }
;;GETTER
static inline TerminalPen TerminalStatusBar_rightPen(const TerminalStatusBar *s) { return (*s).rightPen; }
;;GETTER
static inline TerminalPen *TerminalStatusBar_rightPenMut(TerminalStatusBar *s) { return &(*s).rightPen; }
;;SETTER
static inline fn TerminalStatusBar_setRightPen(TerminalStatusBar *s, TerminalPen v) { (*s).rightPen = v; }

TerminalStatusBar TerminalStatusBar_0() {
    TerminalStatusBar bar = {0};
    bar.leftPen = TerminalPen_1(TERMINAL_STYLE_MUTED);
    bar.rightPen = TerminalPen_1(TERMINAL_STYLE_MUTED);
    return bar;
}

TerminalStatusBar TerminalStatusBar_1(const char *left) {
    TerminalStatusBar bar = TerminalStatusBar_0();
    bar.left = left;
    return bar;
}

bool TerminalStatusBar_setLeft(TerminalStatusBar *self, const char *left) {
    if (self == nullptr) { THROW("TerminalStatusBar left: null self"); return false; }
    TerminalStatusBar_setLeft(self, left);
    return true;
}
const char *TerminalStatusBar_getLeft(const TerminalStatusBar *self) { return self ? TerminalStatusBar_left(self) : nullptr; }
bool TerminalStatusBar_setRight(TerminalStatusBar *self, const char *right) {
    if (self == nullptr) { THROW("TerminalStatusBar right: null self"); return false; }
    TerminalStatusBar_setRight(self, right);
    return true;
}
const char *TerminalStatusBar_getRight(const TerminalStatusBar *self) { return self ? TerminalStatusBar_right(self) : nullptr; }
fn TerminalStatusBar_setPens(TerminalStatusBar *self, TerminalPen leftPen, TerminalPen rightPen) {
    if (self != nullptr) { (*TerminalStatusBar_leftPenMut(self)) = leftPen; (*TerminalStatusBar_rightPenMut(self)) = rightPen; }
}
TerminalStatusBarPens TerminalStatusBar_getPens(const TerminalStatusBar *self) {
    return self ? (TerminalStatusBarPens) { TerminalStatusBar_leftPen(self), TerminalStatusBar_rightPen(self) }
                : (TerminalStatusBarPens) { TerminalPen_0(), TerminalPen_0() };
}

fn TerminalStatusBar_render(const TerminalStatusBar *self, TerminalCanvas *canvas) {
    if (self == nullptr || !TerminalCanvas_isValid(canvas))
        return;
    bytes width = TerminalCanvas_getWidth(canvas);
    bytes leftLen = TerminalStatusBar_left(self) ? strlen(TerminalStatusBar_left(self)) : 0;
    bytes rightLen = TerminalStatusBar_right(self) ? strlen(TerminalStatusBar_right(self)) : 0;
    bool rightFits = rightLen > 0 && leftLen + rightLen + 1 <= width;
    bytes leftCap = rightFits ? width - rightLen - 1 : width;
    if (TerminalStatusBar_left(self) != nullptr && leftCap > 0) {
        TerminalCanvas region = TerminalCanvas_sub(canvas, 0, 0, leftCap, 1);
        TerminalCanvas_text(&region, 0, 0, TerminalStatusBar_left(self), leftLen, TerminalStatusBar_leftPen(self));
    }
    if (rightFits)
        TerminalCanvas_text(canvas, width - rightLen, 0, TerminalStatusBar_right(self), rightLen, TerminalStatusBar_rightPen(self));
}

TerminalProjection TerminalStatusBar_toString(const TerminalStatusBar *self) {
    return TerminalStatusBar_toStringStruct(self);
}

TerminalProjection TerminalStatusBar_toStringStruct(const TerminalStatusBar *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format("TerminalStatusBar{left=\"%s\",right=\"%s\"}",
                                     TerminalStatusBar_left(self) ? TerminalStatusBar_left(self) : "",
                                     TerminalStatusBar_right(self) ? TerminalStatusBar_right(self) : "");
}
