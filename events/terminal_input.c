#include "vex.h"
#include "events/terminal_input.h"
#include <stdio.h>

;;DEFINITION
/** The mouse escape grammar, decoded once for the whole ecosystem.
 *
 * Every terminal application that wants hover has to do the same three things:
 * read the mouse, understand an escape sequence, and hand the rest of the program
 * a coordinate it can reason about. Primeagen did it by hand inside its terminal
 * loop; Semicolon would have had to do it again. This class is that job moved to
 * the toolkit, where it can be proved without a terminal at all — which is the
 * point, because mouse bugs are miserable to reproduce by hand and trivial to
 * reproduce as a byte string.
 *
 * It is a state machine rather than a parser over a buffer because the host
 * receives a stream one byte at a time from its input call, and a sequence can be
 * split across calls by nothing more than scheduling. Pure and total: no I/O, no
 * allocation, no clock, and no input can overflow a field or wedge the machine —
 * a bad byte returns it to idle and raises a counter.
 *
 * What it deliberately does NOT own: the terminal mode. Whether the terminal
 * sends motion at all, and whether the mouse belongs to the application or to the
 * terminal's own selection, is the host's decision; this decoder reports whatever
 * it is given. That separation is why "disable mouse reporting" and "track the
 * mouse" can each be answered honestly instead of pretending to coexist. */
;;OVERVIEW
/** ========================================================================
 * CLASS: TerminalInput (events/terminal_input.c)
 * STRUCT FIELDS (Mirroring events/terminal_input.h)
 *
 * TerminalInputState state         where the byte stream currently is
 * bytes rawCount                  X10 bytes collected so far
 * bytes field                     SGR field under construction (0 button, 1 x, 2 y)
 * bytes digits                    digits seen in the current SGR field
 * bytes values[3]                 button, column, row as received (1-based)
 * bytes malformed                 refused sequences in this epoch
 * TerminalPointerEvent event       the last completed event
 * bool ready                       set for the feed() call that completed an event
 * SLOT RECORD: TerminalPointerEvent
 * TerminalPointerKind kind     what happened
 * TerminalPointerButton button which button, when one applies
 * bytes x, y                  0-based cell column and row
 * bool shift, meta, ctrl       modifiers held at the time
 * PRIVATE HELPERS
 * beginSgr(self)              reset the SGR fields and enter the button field
 * noteMalformed(self)         count a refusal, return to idle, set no event
 * buttonFor(low)              low two bits -> TerminalPointerButton
 * complete(self, raw, x, y, release, sgr)  shared decode for both encodings
 * FUNCTION REGISTRY (exports)
 * Constructor: TerminalInput TerminalInput_0() [zero/arity chooser]
 * Feed: bool TerminalInput_feed(self, byte)
 * Queries: TerminalPointerEvent getEvent(self), getKind/getButton(self)
 *   bool isReady/isPending(self), TerminalInputState getState(self)
 *   bytes malformedCount(self)
 * Lifecycle: fn TerminalInput_reset(self), fn clearMalformed(self)
 * Projections: TerminalProjection TerminalInput_toString/toStringStruct(self)
 * CONTRACT: feed is hot and quiet — one range guard, no allocation, no logging,
 * and a malformed byte only increments a counter. Coordinates arrive 1-based and
 * leave 0-based; a zero coordinate is refused because it cannot address a cell.
 * A field longer than TERMINAL_INPUT_DIGITS_MAX is refused, so no value can
 * overflow. A completed event is reported for exactly one feed call. Null self is
 * refused loudly by the cold projections only; feed(null) is a quiet false.
 * ======================================================================== */


;;GETTER
static inline TerminalPointerButton TerminalPointerEvent_button(const TerminalPointerEvent *s) { return (*s).button; }
;;GETTER
static inline TerminalPointerButton *TerminalPointerEvent_buttonMut(TerminalPointerEvent *s) { return &(*s).button; }
;;SETTER
static inline fn TerminalPointerEvent_setButton(TerminalPointerEvent *s, TerminalPointerButton v) { (*s).button = v; }
;;GETTER
static inline bool TerminalPointerEvent_ctrl(const TerminalPointerEvent *s) { return (*s).ctrl; }
;;GETTER
static inline bool *TerminalPointerEvent_ctrlMut(TerminalPointerEvent *s) { return &(*s).ctrl; }
;;SETTER
static inline fn TerminalPointerEvent_setCtrl(TerminalPointerEvent *s, bool v) { (*s).ctrl = v; }
;;GETTER
static inline TerminalPointerKind TerminalPointerEvent_kind(const TerminalPointerEvent *s) { return (*s).kind; }
;;GETTER
static inline TerminalPointerKind *TerminalPointerEvent_kindMut(TerminalPointerEvent *s) { return &(*s).kind; }
;;SETTER
static inline fn TerminalPointerEvent_setKind(TerminalPointerEvent *s, TerminalPointerKind v) { (*s).kind = v; }
;;GETTER
static inline bool TerminalPointerEvent_meta(const TerminalPointerEvent *s) { return (*s).meta; }
;;GETTER
static inline bool *TerminalPointerEvent_metaMut(TerminalPointerEvent *s) { return &(*s).meta; }
;;SETTER
static inline fn TerminalPointerEvent_setMeta(TerminalPointerEvent *s, bool v) { (*s).meta = v; }
;;GETTER
static inline bool TerminalPointerEvent_shift(const TerminalPointerEvent *s) { return (*s).shift; }
;;GETTER
static inline bool *TerminalPointerEvent_shiftMut(TerminalPointerEvent *s) { return &(*s).shift; }
;;SETTER
static inline fn TerminalPointerEvent_setShift(TerminalPointerEvent *s, bool v) { (*s).shift = v; }
;;GETTER
static inline bytes TerminalPointerEvent_x(const TerminalPointerEvent *s) { return (*s).x; }
;;GETTER
static inline bytes *TerminalPointerEvent_xMut(TerminalPointerEvent *s) { return &(*s).x; }
;;SETTER
static inline fn TerminalPointerEvent_setX(TerminalPointerEvent *s, bytes v) { (*s).x = v; }
;;GETTER
static inline bytes TerminalPointerEvent_y(const TerminalPointerEvent *s) { return (*s).y; }
;;GETTER
static inline bytes *TerminalPointerEvent_yMut(TerminalPointerEvent *s) { return &(*s).y; }
;;SETTER
static inline fn TerminalPointerEvent_setY(TerminalPointerEvent *s, bytes v) { (*s).y = v; }

;;GETTER
static inline bytes TerminalInput_digits(const TerminalInput *s) { return (*s).digits; }
;;GETTER
static inline bytes *TerminalInput_digitsMut(TerminalInput *s) { return &(*s).digits; }
;;SETTER
static inline fn TerminalInput_setDigits(TerminalInput *s, bytes v) { (*s).digits = v; }
;;GETTER
static inline TerminalPointerEvent TerminalInput_event(const TerminalInput *s) { return (*s).event; }
;;GETTER
static inline TerminalPointerEvent *TerminalInput_eventMut(TerminalInput *s) { return &(*s).event; }
;;SETTER
static inline fn TerminalInput_setEvent(TerminalInput *s, TerminalPointerEvent v) { (*s).event = v; }
;;GETTER
static inline bytes TerminalInput_field(const TerminalInput *s) { return (*s).field; }
;;GETTER
static inline bytes *TerminalInput_fieldMut(TerminalInput *s) { return &(*s).field; }
;;SETTER
static inline fn TerminalInput_setField(TerminalInput *s, bytes v) { (*s).field = v; }
;;GETTER
static inline bytes TerminalInput_malformed(const TerminalInput *s) { return (*s).malformed; }
;;GETTER
static inline bytes *TerminalInput_malformedMut(TerminalInput *s) { return &(*s).malformed; }
;;SETTER
static inline fn TerminalInput_setMalformed(TerminalInput *s, bytes v) { (*s).malformed = v; }
;;GETTER
static inline bytes TerminalInput_rawCount(const TerminalInput *s) { return (*s).rawCount; }
;;GETTER
static inline bytes *TerminalInput_rawCountMut(TerminalInput *s) { return &(*s).rawCount; }
;;SETTER
static inline fn TerminalInput_setRawCount(TerminalInput *s, bytes v) { (*s).rawCount = v; }
;;GETTER
static inline bool TerminalInput_ready(const TerminalInput *s) { return (*s).ready; }
;;GETTER
static inline bool *TerminalInput_readyMut(TerminalInput *s) { return &(*s).ready; }
;;SETTER
static inline fn TerminalInput_setReady(TerminalInput *s, bool v) { (*s).ready = v; }
;;GETTER
static inline TerminalInputState TerminalInput_state(const TerminalInput *s) { return (*s).state; }
;;GETTER
static inline TerminalInputState *TerminalInput_stateMut(TerminalInput *s) { return &(*s).state; }
;;SETTER
static inline fn TerminalInput_setState(TerminalInput *s, TerminalInputState v) { (*s).state = v; }
;;GETTER
static inline bytes *TerminalInput_valuesMut(TerminalInput *s) { return (*s).values; }
;;GETTER
static inline const bytes *TerminalInput_values(const TerminalInput *s) { return (*s).values; }

static TerminalPointerButton buttonFor(bytes low) {
    switch (low & 3u) {
        case 0: return TERMINAL_BUTTON_LEFT;
        case 1: return TERMINAL_BUTTON_MIDDLE;
        case 2: return TERMINAL_BUTTON_RIGHT;
        default: return TERMINAL_BUTTON_NONE;
    }
}

static fn noteMalformed(TerminalInput *self) {
    if (TerminalInput_malformed(self) != SIZE_MAX)
        TerminalInput_setMalformed(self, TerminalInput_malformed(self) + 1u);
    TerminalInput_setState(self, TERMINAL_INPUT_IDLE);
    TerminalInput_setRawCount(self, 0);
    TerminalInput_setField(self, 0);
    TerminalInput_setDigits(self, 0);
}

static fn beginSgr(TerminalInput *self) {
    TerminalInput_setState(self, TERMINAL_INPUT_SGR_BUTTON);
    TerminalInput_setField(self, 0);
    TerminalInput_setDigits(self, 0);
    TerminalInput_valuesMut(self)[0] = 0;
    TerminalInput_valuesMut(self)[1] = 0;
    TerminalInput_valuesMut(self)[2] = 0;
}

// Shared decode for both encodings. `motion` exists only in SGR, and `release` is
// SGR's lowercase final byte; X10 signals a release by reporting low bits 3.
static fn complete(TerminalInput *self, bytes raw, bytes x, bytes y,
                     bool release, bool sgr) {
    if (x == 0 || y == 0) {
        noteMalformed(self);
        return;
    }
    TerminalPointerEvent event = {0};
    event.shift = (raw & 4u) != 0;
    event.meta = (raw & 8u) != 0;
    event.ctrl = (raw & 16u) != 0;
    bytes low = raw & 3u;
    bool motion = sgr && (raw & 32u) != 0;
    bool wheel = (raw & 64u) != 0;
    if (wheel) {
        // 64/65 are vertical; 66/67 are the horizontal pair a trackpad or a tilt
        // wheel sends, and dropping them was why panning sideways was impossible.
        switch (low) {
            case 0: event.kind = TERMINAL_POINTER_WHEEL_UP; break;
            case 1: event.kind = TERMINAL_POINTER_WHEEL_DOWN; break;
            case 2: event.kind = TERMINAL_POINTER_WHEEL_LEFT; break;
            default: event.kind = TERMINAL_POINTER_WHEEL_RIGHT; break;
        }
    } else if (motion) {
        event.kind = low == 3 ? TERMINAL_POINTER_MOVE : TERMINAL_POINTER_DRAG;
    } else if (release || low == 3) {
        event.kind = TERMINAL_POINTER_RELEASE;
    } else {
        event.kind = TERMINAL_POINTER_PRESS;
    }
    event.button = wheel ? TERMINAL_BUTTON_NONE : buttonFor(low);
    if (event.kind == TERMINAL_POINTER_NONE) {
        noteMalformed(self);
        return;
    }
    event.x = x - 1;
    event.y = y - 1;
    TerminalInput_setEvent(self, event);
    TerminalInput_setReady(self, true);
    TerminalInput_setState(self, TERMINAL_INPUT_IDLE);
    TerminalInput_setRawCount(self, 0);
    TerminalInput_setField(self, 0);
    TerminalInput_setDigits(self, 0);
}

TerminalInput TerminalInput_0() { return (TerminalInput) {0}; }

;;INTENTION("feed is a hot path: one guard per byte, no logging, no allocation, and a malformed sequence only raises a queryable counter")
bool TerminalInput_feed(TerminalInput *self, i32 byte) {
    if (self == nullptr || byte < 0 || byte > 255)
        return false;
    TerminalInput_setReady(self, false);
    u8 incoming = (u8) byte;
    switch (TerminalInput_state(self)) {
        case TERMINAL_INPUT_IDLE:
            if (incoming == 0x1Bu)
                TerminalInput_setState(self, TERMINAL_INPUT_ESCAPE);
            break;
        case TERMINAL_INPUT_ESCAPE:
            TerminalInput_setState(self, incoming == '[' ? TERMINAL_INPUT_CSI : TERMINAL_INPUT_IDLE);
            break;
        case TERMINAL_INPUT_CSI:
            if (incoming == '<')
                beginSgr(self);
            else if (incoming == 'M') {
                TerminalInput_setState(self, TERMINAL_INPUT_X10);
                TerminalInput_setRawCount(self, 0);
            } else
                noteMalformed(self);
            break;
        case TERMINAL_INPUT_X10:
            if (incoming < 32u)
                noteMalformed(self);
            else {
                TerminalInput_valuesMut(self)[TerminalInput_rawCount(self)] = (bytes) incoming - 32u;
                if (++(*TerminalInput_rawCountMut(self)) == TERMINAL_INPUT_X10_BYTES)
                    complete(self, TerminalInput_valuesMut(self)[0], TerminalInput_valuesMut(self)[1], TerminalInput_valuesMut(self)[2],
                             false, false);
            }
            break;
        case TERMINAL_INPUT_SGR_BUTTON:
        case TERMINAL_INPUT_SGR_X:
        case TERMINAL_INPUT_SGR_Y:
            if (incoming >= '0' && incoming <= '9') {
                if (TerminalInput_digits(self) >= TERMINAL_INPUT_DIGITS_MAX) {
                    noteMalformed(self);
                    break;
                }
                TerminalInput_valuesMut(self)[TerminalInput_field(self)] =
                    TerminalInput_valuesMut(self)[TerminalInput_field(self)] * 10u + (bytes) (incoming - '0');
                TerminalInput_setDigits(self, TerminalInput_digits(self) + 1u);
            } else if (incoming == ';' && TerminalInput_field(self) < 2 && TerminalInput_digits(self) > 0) {
                TerminalInput_setField(self, TerminalInput_field(self) + 1u);
                TerminalInput_setDigits(self, 0);
                TerminalInput_setState(self, TerminalInput_field(self) == 1 ? TERMINAL_INPUT_SGR_X : TERMINAL_INPUT_SGR_Y);
            } else if ((incoming == 'M' || incoming == 'm') && TerminalInput_field(self) == 2
                       && TerminalInput_digits(self) > 0) {
                complete(self, TerminalInput_valuesMut(self)[0], TerminalInput_valuesMut(self)[1], TerminalInput_valuesMut(self)[2],
                         incoming == 'm', true);
            } else
                noteMalformed(self);
            break;
        default:
            noteMalformed(self);
            break;
    }
    return TerminalInput_ready(self);
}

TerminalPointerEvent TerminalInput_getEvent(const TerminalInput *self) {
    return self != nullptr ? TerminalInput_event(self) : (TerminalPointerEvent) {0};
}
TerminalPointerKind TerminalInput_getKind(const TerminalInput *self) {
    if (self == nullptr) return TERMINAL_POINTER_NONE;
    TerminalPointerEvent event = TerminalInput_event(self);
    return event.kind;
}
TerminalPointerButton TerminalInput_getButton(const TerminalInput *self) {
    if (self == nullptr) return TERMINAL_BUTTON_NONE;
    TerminalPointerEvent event = TerminalInput_event(self);
    return event.button;
}
bool TerminalInput_isReady(const TerminalInput *self) { return self != nullptr && TerminalInput_ready(self); }
bool TerminalInput_isPending(const TerminalInput *self) {
    return self != nullptr && TerminalInput_state(self) != TERMINAL_INPUT_IDLE;
}
TerminalInputState TerminalInput_getState(const TerminalInput *self) {
    return self != nullptr ? TerminalInput_state(self) : TERMINAL_INPUT_IDLE;
}
bytes TerminalInput_malformedCount(const TerminalInput *self) {
    return self != nullptr ? TerminalInput_malformed(self) : 0;
}
fn TerminalInput_clearMalformed(TerminalInput *self) {
    if (self != nullptr)
        TerminalInput_setMalformed(self, 0);
}

fn TerminalInput_reset(TerminalInput *self) {
    if (self == nullptr) {
        THROW("TerminalInput reset: null input");
        return;
    }
    bytes malformed = TerminalInput_malformed(self);
    *self = TerminalInput_0();
    TerminalInput_setMalformed(self, malformed);   // reset reopens the decode, not the evidence
}

static const char *kindName(TerminalPointerKind kind) {
    switch (kind) {
        case TERMINAL_POINTER_MOVE: return "move";
        case TERMINAL_POINTER_DRAG: return "drag";
        case TERMINAL_POINTER_PRESS: return "press";
        case TERMINAL_POINTER_RELEASE: return "release";
        case TERMINAL_POINTER_WHEEL_UP: return "wheel-up";
        case TERMINAL_POINTER_WHEEL_DOWN: return "wheel-down";
        case TERMINAL_POINTER_WHEEL_LEFT: return "wheel-left";
        case TERMINAL_POINTER_WHEEL_RIGHT: return "wheel-right";
        default: return "none";
    }
}

TerminalProjection TerminalInput_toString(const TerminalInput *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    const TerminalPointerEvent *event = TerminalInput_eventMut(self);
    return TerminalProjection_format("TerminalInput{%s,button=%d,%zu,%zu,mods=%s%s%s,malformed=%zu}",
        kindName(TerminalPointerEvent_kind(event)), TerminalPointerEvent_button(event), TerminalPointerEvent_x(event), TerminalPointerEvent_y(event),
        TerminalPointerEvent_shift(event) ? "s" : "", TerminalPointerEvent_meta(event) ? "m" : "", TerminalPointerEvent_ctrl(event) ? "c" : "",
        TerminalInput_malformed(self));
}

TerminalProjection TerminalInput_toStringStruct(const TerminalInput *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    const TerminalPointerEvent *event = TerminalInput_eventMut(self);
    return TerminalProjection_format(
        "TerminalInput{state=%d,rawCount=%zu,field=%zu,digits=%zu,values={%zu,%zu,%zu},malformed=%zu,event={kind=%s,button=%d,x=%zu,y=%zu,shift=%d,meta=%d,ctrl=%d},ready=%d}",
        TerminalInput_state(self), TerminalInput_rawCount(self), TerminalInput_field(self), TerminalInput_digits(self),
        TerminalInput_valuesMut(self)[0], TerminalInput_valuesMut(self)[1], TerminalInput_valuesMut(self)[2], TerminalInput_malformed(self),
        kindName(TerminalPointerEvent_kind(event)), TerminalPointerEvent_button(event), TerminalPointerEvent_x(event), TerminalPointerEvent_y(event),
        TerminalPointerEvent_shift(event), TerminalPointerEvent_meta(event), TerminalPointerEvent_ctrl(event), TerminalInput_ready(self));
}
