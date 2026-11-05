// Owner: TerminalInput — the mouse byte decoder. It proves the SGR and X10
// grammars, the button/modifier/motion/wheel bit layout, 1-based-in/0-based-out
// coordinates, byte-at-a-time feeding, recovery from a truncated sequence, the
// field-overflow and zero-coordinate refusals, and the once-per-event report.
//
// The whole point of this owner is that it needs no terminal: every case is a
// literal byte string, so a mouse bug that would take twenty minutes to
// reproduce by hand takes one assert.
#include "events/terminal_input.h"
#include "determinant/test/support.h"

// Feed a literal sequence, returning how many feed calls completed an event.
static size_t feedString(TerminalInput *input, const char *bytes) {
    size_t completions = 0;
    for (const char *at = bytes; *at != '\0'; ++at)
        if (TerminalInput_feed(input, (unsigned char) *at))
            ++completions;
    return completions;
}

int main(void) {
    // --- Constructor and null safety ---
    TerminalInput fresh = TerminalInput_0();
    assert(TerminalInput_getState(&fresh) == TERMINAL_INPUT_IDLE);
    assert(!TerminalInput_isReady(&fresh) && !TerminalInput_isPending(&fresh));
    assert(TerminalInput_malformedCount(&fresh) == 0);
    assert(TerminalInput_getKind(&fresh) == TERMINAL_POINTER_NONE);
    assert(TerminalInput_getButton(&fresh) == TERMINAL_BUTTON_NONE);
    TerminalInput zeroed = TerminalInput_zero();
    assert(TerminalInput_getState(&zeroed) == TERMINAL_INPUT_IDLE);
    assert(!TerminalInput_feed(nullptr, 'x'));
    assert(!TerminalInput_feed(&fresh, -1));
    assert(!TerminalInput_feed(&fresh, 256));
    assert(!TerminalInput_isReady(nullptr) && !TerminalInput_isPending(nullptr));
    assert(TerminalInput_getState(nullptr) == TERMINAL_INPUT_IDLE);
    assert(TerminalInput_malformedCount(nullptr) == 0);
    assert(TerminalInput_getKind(nullptr) == TERMINAL_POINTER_NONE);
    assert(TerminalInput_getButton(nullptr) == TERMINAL_BUTTON_NONE);
    TerminalPointerEvent nothing = TerminalInput_getEvent(nullptr);
    assert(nothing.kind == TERMINAL_POINTER_NONE && nothing.x == 0 && nothing.y == 0);
    TerminalInput_clearMalformed(nullptr);
    beginDiagnostics();
    TerminalInput_reset(nullptr);                    // loudly refused, changes nothing
    endDiagnostics(1, "null input");

    // --- SGR: press, release, and 1-based in / 0-based out ---
    TerminalInput input = TerminalInput_0();
    assert(feedString(&input, "\x1b[<0;12;4M") == 1);
    TerminalPointerEvent event = TerminalInput_getEvent(&input);
    assert(event.kind == TERMINAL_POINTER_PRESS && event.button == TERMINAL_BUTTON_LEFT);
    assert(event.x == 11 && event.y == 3);
    assert(!event.shift && !event.meta && !event.ctrl);
    // A completed event is reported for exactly one feed call.
    assert(!TerminalInput_feed(&input, 'z'));
    assert(!TerminalInput_isReady(&input));
    assert(TerminalInput_getEvent(&input).kind == TERMINAL_POINTER_PRESS);   // still readable

    assert(feedString(&input, "\x1b[<0;12;4m") == 1);
    assert(TerminalInput_getKind(&input) == TERMINAL_POINTER_RELEASE);
    assert(TerminalInput_getEvent(&input).x == 11);

    // --- Every button ---
    assert(feedString(&input, "\x1b[<1;1;1M") == 1);
    assert(TerminalInput_getButton(&input) == TERMINAL_BUTTON_MIDDLE);
    assert(feedString(&input, "\x1b[<2;1;1M") == 1);
    assert(TerminalInput_getButton(&input) == TERMINAL_BUTTON_RIGHT);
    assert(feedString(&input, "\x1b[<3;1;1m") == 1);
    assert(TerminalInput_getKind(&input) == TERMINAL_POINTER_RELEASE);
    assert(TerminalInput_getButton(&input) == TERMINAL_BUTTON_NONE);

    // --- Modifiers are bits 2, 3 and 4 ---
    assert(feedString(&input, "\x1b[<4;1;1M") == 1);          // shift
    assert(TerminalInput_getEvent(&input).shift);
    assert(feedString(&input, "\x1b[<8;1;1M") == 1);          // meta
    assert(TerminalInput_getEvent(&input).meta);
    assert(feedString(&input, "\x1b[<16;1;1M") == 1);         // ctrl
    assert(TerminalInput_getEvent(&input).ctrl);
    assert(feedString(&input, "\x1b[<28;1;1M") == 1);         // 16+8+4 = all three
    event = TerminalInput_getEvent(&input);
    assert(event.shift && event.meta && event.ctrl && event.button == TERMINAL_BUTTON_LEFT);

    // --- Wheel is bit 6, and a wheel event carries no button ---
    assert(feedString(&input, "\x1b[<64;5;9M") == 1);
    assert(TerminalInput_getKind(&input) == TERMINAL_POINTER_WHEEL_UP);
    assert(TerminalInput_getButton(&input) == TERMINAL_BUTTON_NONE);
    assert(TerminalInput_getEvent(&input).x == 4 && TerminalInput_getEvent(&input).y == 8);
    assert(feedString(&input, "\x1b[<65;5;9M") == 1);
    assert(TerminalInput_getKind(&input) == TERMINAL_POINTER_WHEEL_DOWN);
    // The horizontal pair: without it a trackpad can never pan sideways.
    assert(feedString(&input, "\x1b[<66;5;9M") == 1);
    assert(TerminalInput_getKind(&input) == TERMINAL_POINTER_WHEEL_LEFT);
    assert(TerminalInput_getButton(&input) == TERMINAL_BUTTON_NONE);
    assert(feedString(&input, "\x1b[<67;5;9M") == 1);
    assert(TerminalInput_getKind(&input) == TERMINAL_POINTER_WHEEL_RIGHT);
    assert(feedString(&input, "\x1b[<98;5;9M") == 1);   // X10 packs 66 the same way
    assert(TerminalInput_getKind(&input) == TERMINAL_POINTER_WHEEL_LEFT);

    // --- Motion is bit 5: the bit that makes hover possible at all ---
    assert(feedString(&input, "\x1b[<35;7;2M") == 1);         // 32 + 3 = motion, no button
    assert(TerminalInput_getKind(&input) == TERMINAL_POINTER_MOVE);
    assert(TerminalInput_getButton(&input) == TERMINAL_BUTTON_NONE);
    assert(feedString(&input, "\x1b[<32;7;2M") == 1);         // 32 + 0 = motion, left held
    assert(TerminalInput_getKind(&input) == TERMINAL_POINTER_DRAG);
    assert(TerminalInput_getButton(&input) == TERMINAL_BUTTON_LEFT);
    assert(feedString(&input, "\x1b[<34;7;2M") == 1);         // 32 + 2 = drag with right
    assert(TerminalInput_getKind(&input) == TERMINAL_POINTER_DRAG);
    assert(TerminalInput_getButton(&input) == TERMINAL_BUTTON_RIGHT);

    // --- Byte at a time: the same sequence, split by nothing but scheduling ---
    TerminalInput split = TerminalInput_0();
    const char *sequence = "\x1b[<32;40;11M";
    size_t completions = 0;
    for (const char *at = sequence; *at != '\0'; ++at)
        if (TerminalInput_feed(&split, (unsigned char) *at))
            ++completions;
    assert(completions == 1);
    assert(TerminalInput_isReady(&split));                    // the last byte completed it
    assert(!TerminalInput_feed(&split, 'x'));                 // the next byte does not re-report
    assert(!TerminalInput_isReady(&split));
    assert(TerminalInput_getKind(&split) == TERMINAL_POINTER_DRAG);
    assert(TerminalInput_getEvent(&split).x == 39 && TerminalInput_getEvent(&split).y == 10);
    // Mid-sequence the decoder reports itself pending, so a host can wait.
    TerminalInput pending = TerminalInput_0();
    assert(!TerminalInput_feed(&pending, 0x1B));
    assert(TerminalInput_isPending(&pending));
    assert(!TerminalInput_feed(&pending, '['));
    assert(TerminalInput_isPending(&pending));
    assert(!TerminalInput_feed(&pending, '<'));
    assert(TerminalInput_isPending(&pending));

    // --- X10 is the legacy fallback: three raw bytes after ESC [ M ---
    assert(feedString(&input, "\x1b[M\x20\x2c\x24") == 1);   // 32, 44, 36 -> press at 11,3
    event = TerminalInput_getEvent(&input);
    assert(event.kind == TERMINAL_POINTER_PRESS && event.button == TERMINAL_BUTTON_LEFT);
    assert(event.x == 11 && event.y == 3);
    assert(!TerminalInput_getEvent(&input).shift);
    assert(feedString(&input, "\x1b[M\x23\x2c\x24") == 1);   // button byte 35 -> release
    assert(TerminalInput_getKind(&input) == TERMINAL_POINTER_RELEASE);
    assert(feedString(&input, "\x1b[M\x60\x2c\x24") == 1);   // 96-32 = 64 -> wheel up
    assert(TerminalInput_getKind(&input) == TERMINAL_POINTER_WHEEL_UP);

    // --- Refusals: counted, quiet, and never wedging the decoder ---
    TerminalInput broken = TerminalInput_0();
    assert(feedString(&broken, "\x1b[<abc") == 0);
    assert(TerminalInput_malformedCount(&broken) == 1);
    assert(!TerminalInput_isPending(&broken));
    assert(feedString(&broken, "\x1b[<99999;1;1M") == 0);    // 5 digits in a 4-digit field
    assert(TerminalInput_malformedCount(&broken) == 2);
    assert(feedString(&broken, "\x1b[<0;0;4M") == 0);        // a zero column cannot address a cell
    assert(TerminalInput_malformedCount(&broken) == 3);
    assert(feedString(&broken, "\x1b[<0;1;0M") == 0);        // nor a zero row
    assert(TerminalInput_malformedCount(&broken) == 4);
    assert(feedString(&broken, "\x1b[<0;1;1;1M") == 0);      // a fourth field does not exist
    assert(TerminalInput_malformedCount(&broken) == 5);
    assert(feedString(&broken, "\x1b[]") == 0);              // not a mouse introducer
    assert(TerminalInput_malformedCount(&broken) == 6);
    // ...and after all that, a good sequence still decodes.
    assert(feedString(&broken, "\x1b[<0;3;3M") == 1);
    assert(TerminalInput_getEvent(&broken).x == 2 && TerminalInput_getEvent(&broken).y == 2);
    TerminalInput_clearMalformed(&broken);
    assert(TerminalInput_malformedCount(&broken) == 0);

    // --- A truncated sequence does not poison the next one ---
    TerminalInput recovered = TerminalInput_0();
    assert(feedString(&recovered, "\x1b[<0;1") == 0);
    assert(TerminalInput_isPending(&recovered));
    assert(feedString(&recovered, "2;4M") == 1);             // the tail completes it
    assert(TerminalInput_getEvent(&recovered).x == 11);
    assert(feedString(&recovered, "\x1b[<0;1") == 0);
    assert(feedString(&recovered, "zz") == 0);               // garbage abandons the sequence
    assert(!TerminalInput_isPending(&recovered));
    assert(feedString(&recovered, "\x1b[<0;2;2M") == 1);     // and a fresh one still works
    assert(TerminalInput_getEvent(&recovered).y == 1);

    // --- A reset reopens the decode but keeps the evidence ---
    TerminalInput restarted = TerminalInput_0();
    assert(feedString(&restarted, "\x1b[<zz") == 0);
    assert(feedString(&restarted, "\x1b[<0;1") == 0);
    assert(TerminalInput_isPending(&restarted));
    assert(TerminalInput_malformedCount(&restarted) == 1);
    TerminalInput_reset(&restarted);
    assert(TerminalInput_getState(&restarted) == TERMINAL_INPUT_IDLE);
    assert(!TerminalInput_isPending(&restarted));
    assert(TerminalInput_malformedCount(&restarted) == 1);   // the count survives a reset
    assert(feedString(&restarted, "\x1b[<0;1;1M") == 1);

    // --- Projections: value string and a struct dump mirroring STRUCT FIELDS ---
    TERMINAL_PROJECT(TerminalInput, &input);
    TerminalProjection value = TerminalInput_toString(&input);
    assert(strstr(value.text, "TerminalInput{wheel-up") != nullptr);
    TerminalProjection structured = TerminalInput_toStringStruct(&input);
    assert(strstr(structured.text, "state=") != nullptr);
    assert(strstr(structured.text, "rawCount=") != nullptr);
    assert(strstr(structured.text, "values={") != nullptr);
    assert(strstr(structured.text, "malformed=") != nullptr);
    assert(strstr(structured.text, "ready=") != nullptr);

    return 0;
}
