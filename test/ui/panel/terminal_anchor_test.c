// Owner: TerminalAnchor — the placement vocabulary and the single owner of band
// math. Constructors and arity, the reserve rule, all nine pins, AUTO extents,
// pure placement with clamping instead of underflow, insets, setter rejection
// with preserved state, and the projections.
//
// The grounding case is semicolon's IDE example, measured from
// suite/projects/semicolon/src/tui/tui_example.txt: 103x23 cells, root pipes at
// columns 1/29/103, editor pipes at 1/29/36/103. If the resolver cannot
// reproduce that layout exactly, it is the wrong layout model.
//
// Scope note: this owner proves placement arithmetic. It does not compile a
// negative-arity case (no determinant owner does) and renders no pixels.
#include "ui/panel/terminal_anchor.h"
#include "determinant/test/support.h"

static bool rectEqual(TerminalRect a, TerminalRect b) {
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}
static TerminalRect rectOf(bytes x, bytes y, bytes w, bytes h) {
    return (TerminalRect) { x, y, w, h };
}

int main(void) {
    // --- Constructors: zero / one / two / three, and the chooser ---
    TerminalAnchor fill = TerminalAnchor();
    assert(TerminalAnchor_getKind(&fill) == TERMINAL_ANCHOR_FILL);
    assert(!TerminalAnchor_isBand(&fill) && !TerminalAnchor_reserves(&fill));
    assert(TerminalAnchor_getExtent(&fill) == TERMINAL_ANCHOR_AUTO);
    assert(TerminalAnchor_getWidth(&fill) == TERMINAL_ANCHOR_AUTO);
    TerminalAnchor zeroed = TerminalAnchor_zero();
    assert(TerminalAnchor_getKind(&zeroed) == TERMINAL_ANCHOR_FILL);
    TerminalAnchor one = TerminalAnchor(TERMINAL_ANCHOR_BAND_TOP);
    assert(TerminalAnchor_getKind(&one) == TERMINAL_ANCHOR_BAND_TOP);
    assert(TerminalAnchor_isBand(&one) && TerminalAnchor_reserves(&one));
    TerminalAnchor two = TerminalAnchor(TERMINAL_ANCHOR_BAND_TOP, 1);
    assert(TerminalAnchor_getExtent(&two) == 1);
    TerminalAnchor three = TerminalAnchor(TERMINAL_ANCHOR_PIN_CENTER, 10, 3);
    assert(TerminalAnchor_getKind(&three) == TERMINAL_ANCHOR_PIN_CENTER);
    assert(TerminalAnchor_getWidth(&three) == 10 && TerminalAnchor_getHeight(&three) == 3);

    beginDiagnostics();
    TerminalAnchor badKind = TerminalAnchor_1((TerminalAnchorKind) 99);
    endDiagnostics(1, "kind");
    assert(TerminalAnchor_getKind(&badKind) == TERMINAL_ANCHOR_FILL);   // rejected -> default

    // Exactly the four band kinds reserve; every pin and FILL does not.
    for (int kind = TERMINAL_ANCHOR_FILL; kind <= TERMINAL_ANCHOR_PIN_BOTTOM_RIGHT; ++kind) {
        TerminalAnchor probe = TerminalAnchor_1((TerminalAnchorKind) kind);
        bool expected = kind >= TERMINAL_ANCHOR_BAND_TOP && kind <= TERMINAL_ANCHOR_BAND_RIGHT;
        assert(TerminalAnchor_isBand(&probe) == expected);
        assert(TerminalAnchor_reserves(&probe) == expected);
    }

    // --- FILL takes the cursor and reserves nothing ---
    TerminalRect cursor = rectOf(0, 0, 10, 4);
    TerminalAnchorPlacement placed = TerminalAnchor_place(&fill, cursor);
    assert(rectEqual(placed.region, cursor) && rectEqual(placed.cursor, cursor));

    // --- Each band takes its slice from the right edge and reserves it ---
    TerminalAnchor top = TerminalAnchor(TERMINAL_ANCHOR_BAND_TOP, 1);
    placed = TerminalAnchor_place(&top, cursor);
    assert(rectEqual(placed.region, rectOf(0, 0, 10, 1)));
    assert(rectEqual(placed.cursor, rectOf(0, 1, 10, 3)));

    TerminalAnchor bottom = TerminalAnchor(TERMINAL_ANCHOR_BAND_BOTTOM, 2);
    placed = TerminalAnchor_place(&bottom, cursor);
    assert(rectEqual(placed.region, rectOf(0, 2, 10, 2)));
    assert(rectEqual(placed.cursor, rectOf(0, 0, 10, 2)));

    TerminalAnchor left = TerminalAnchor(TERMINAL_ANCHOR_BAND_LEFT, 3);
    placed = TerminalAnchor_place(&left, cursor);
    assert(rectEqual(placed.region, rectOf(0, 0, 3, 4)));
    assert(rectEqual(placed.cursor, rectOf(3, 0, 7, 4)));

    TerminalAnchor right = TerminalAnchor(TERMINAL_ANCHOR_BAND_RIGHT, 3);
    placed = TerminalAnchor_place(&right, cursor);
    assert(rectEqual(placed.region, rectOf(7, 0, 3, 4)));
    assert(rectEqual(placed.cursor, rectOf(0, 0, 7, 4)));

    // --- All nine pins, and none of them reserve ---
    TerminalAnchor pinTL = TerminalAnchor(TERMINAL_ANCHOR_PIN_TOP_LEFT, 3, 2);
    TerminalAnchor pinTC = TerminalAnchor(TERMINAL_ANCHOR_PIN_TOP_CENTER, 4, 2);
    TerminalAnchor pinTR = TerminalAnchor(TERMINAL_ANCHOR_PIN_TOP_RIGHT, 3, 2);
    TerminalAnchor pinCL = TerminalAnchor(TERMINAL_ANCHOR_PIN_CENTER_LEFT, 3, 2);
    TerminalAnchor pinCC = TerminalAnchor(TERMINAL_ANCHOR_PIN_CENTER, 4, 2);
    TerminalAnchor pinCR = TerminalAnchor(TERMINAL_ANCHOR_PIN_CENTER_RIGHT, 3, 2);
    TerminalAnchor pinBL = TerminalAnchor(TERMINAL_ANCHOR_PIN_BOTTOM_LEFT, 3, 2);
    TerminalAnchor pinBC = TerminalAnchor(TERMINAL_ANCHOR_PIN_BOTTOM_CENTER, 4, 2);
    TerminalAnchor pinBR = TerminalAnchor(TERMINAL_ANCHOR_PIN_BOTTOM_RIGHT, 3, 2);
    assert(rectEqual(TerminalAnchor_place(&pinTL, cursor).region, rectOf(0, 0, 3, 2)));
    assert(rectEqual(TerminalAnchor_place(&pinTC, cursor).region, rectOf(3, 0, 4, 2)));
    assert(rectEqual(TerminalAnchor_place(&pinTR, cursor).region, rectOf(7, 0, 3, 2)));
    assert(rectEqual(TerminalAnchor_place(&pinCL, cursor).region, rectOf(0, 1, 3, 2)));
    assert(rectEqual(TerminalAnchor_place(&pinCC, cursor).region, rectOf(3, 1, 4, 2)));
    assert(rectEqual(TerminalAnchor_place(&pinCR, cursor).region, rectOf(7, 1, 3, 2)));
    assert(rectEqual(TerminalAnchor_place(&pinBL, cursor).region, rectOf(0, 2, 3, 2)));
    assert(rectEqual(TerminalAnchor_place(&pinBC, cursor).region, rectOf(3, 2, 4, 2)));
    assert(rectEqual(TerminalAnchor_place(&pinBR, cursor).region, rectOf(7, 2, 3, 2)));
    assert(rectEqual(TerminalAnchor_place(&pinCC, cursor).cursor, cursor));   // a pin never reserves

    // --- AUTO means "whatever the parent has" ---
    TerminalAnchor autoPin = TerminalAnchor(TERMINAL_ANCHOR_PIN_CENTER);
    assert(rectEqual(TerminalAnchor_place(&autoPin, cursor).region, cursor));
    TerminalAnchor autoTop = TerminalAnchor(TERMINAL_ANCHOR_BAND_TOP, TERMINAL_ANCHOR_AUTO);
    TerminalAnchorPlacement whole = TerminalAnchor_place(&autoTop, cursor);
    assert(rectEqual(whole.region, cursor));
    assert(whole.cursor.height == 0 && whole.cursor.y == 4);

    // --- Clamping, never underflow ---
    TerminalAnchor huge = TerminalAnchor(TERMINAL_ANCHOR_BAND_LEFT, SIZE_MAX);
    TerminalAnchorPlacement clamped = TerminalAnchor_place(&huge, cursor);
    assert(rectEqual(clamped.region, cursor));
    assert(clamped.cursor.width == 0 && clamped.cursor.x == 10);
    TerminalAnchor oversize = TerminalAnchor(TERMINAL_ANCHOR_PIN_CENTER, 99, 99);
    assert(rectEqual(TerminalAnchor_place(&oversize, cursor).region, cursor));
    TerminalRect nothing = rectOf(0, 0, 0, 0);
    TerminalAnchorPlacement emptyBand = TerminalAnchor_place(&huge, nothing);
    assert(emptyBand.region.width == 0 && emptyBand.region.height == 0);
    assert(emptyBand.cursor.width == 0 && emptyBand.cursor.height == 0);
    TerminalAnchorPlacement emptyPin = TerminalAnchor_place(&oversize, nothing);
    assert(emptyPin.region.width == 0 && emptyPin.region.height == 0);

    // --- Insets sit inside the placed region and collapse rather than wrap ---
    TerminalAnchor insetFill = TerminalAnchor();
    assert(TerminalAnchor_setInset(&insetFill, 1, 1));
    assert(TerminalAnchor_getInsetX(&insetFill) == 1 && TerminalAnchor_getInsetY(&insetFill) == 1);
    assert(rectEqual(TerminalAnchor_place(&insetFill, cursor).region, rectOf(1, 1, 8, 2)));
    TerminalAnchor_setInset(&insetFill, 5, 9);
    TerminalAnchorPlacement collapsed = TerminalAnchor_place(&insetFill, cursor);
    assert(collapsed.region.width == 0 && collapsed.region.height == 0);
    assert(collapsed.region.x == 5 && collapsed.region.y == 2);   // cut clamps to half
    TerminalAnchor_setInset(&insetFill, 0, 0);
    assert(rectEqual(TerminalAnchor_place(&insetFill, cursor).region, cursor));

    // --- Pure and deterministic: same cursor in, same placement out ---
    TerminalAnchorPlacement first = TerminalAnchor_place(&left, cursor);
    TerminalAnchorPlacement second = TerminalAnchor_place(&left, cursor);
    assert(rectEqual(first.region, second.region) && rectEqual(first.cursor, second.cursor));
    assert(rectEqual(cursor, rectOf(0, 0, 10, 4)));   // the caller's cursor is untouched

    // --- GROUNDING: semicolon's measured IDE layout, three levels deep ---
    TerminalAnchor titleAnchor = TerminalAnchor(TERMINAL_ANCHOR_BAND_TOP, 1);
    TerminalAnchor statusAnchor = TerminalAnchor(TERMINAL_ANCHOR_BAND_BOTTOM, 1);
    TerminalAnchor railAnchor = TerminalAnchor(TERMINAL_ANCHOR_BAND_LEFT, 29);
    TerminalRect panel = rectOf(0, 0, 103, 23);
    TerminalAnchorPlacement titlePos = TerminalAnchor_place(&titleAnchor, panel);
    TerminalAnchorPlacement statusPos = TerminalAnchor_place(&statusAnchor, titlePos.cursor);
    TerminalAnchorPlacement railPos = TerminalAnchor_place(&railAnchor, statusPos.cursor);
    TerminalAnchorPlacement centrePos = TerminalAnchor_place(&fill, railPos.cursor);
    assert(rectEqual(titlePos.region, rectOf(0, 0, 103, 1)));     // example row 1
    assert(rectEqual(statusPos.region, rectOf(0, 22, 103, 1)));   // example row 23
    assert(rectEqual(railPos.region, rectOf(0, 1, 29, 21)));      // example cols 1..29
    assert(rectEqual(centrePos.region, rectOf(29, 1, 74, 21)));   // example cols 30..103

    TerminalAnchor sepAnchor = TerminalAnchor(TERMINAL_ANCHOR_BAND_TOP, 1);
    TerminalRect centre = rectOf(0, 0, centrePos.region.width, centrePos.region.height);
    TerminalAnchorPlacement tabsPos = TerminalAnchor_place(&titleAnchor, centre);
    TerminalAnchorPlacement sepPos = TerminalAnchor_place(&sepAnchor, tabsPos.cursor);
    TerminalAnchorPlacement editorPos = TerminalAnchor_place(&fill, sepPos.cursor);
    assert(rectEqual(tabsPos.region, rectOf(0, 0, 74, 1)));       // example row 2 tabs
    assert(rectEqual(sepPos.region, rectOf(0, 1, 74, 1)));        // example row 3 separator
    assert(rectEqual(editorPos.region, rectOf(0, 2, 74, 19)));    // example rows 4..22

    TerminalAnchor gutterAnchor = TerminalAnchor(TERMINAL_ANCHOR_BAND_LEFT, 7);
    TerminalRect editor = rectOf(0, 0, editorPos.region.width, editorPos.region.height);
    TerminalAnchorPlacement gutterPos = TerminalAnchor_place(&gutterAnchor, editor);
    TerminalAnchorPlacement codePos = TerminalAnchor_place(&fill, gutterPos.cursor);
    assert(rectEqual(gutterPos.region, rectOf(0, 0, 7, 19)));     // example cols 30..36
    assert(rectEqual(codePos.region, rectOf(7, 0, 67, 19)));      // example cols 37..103

    // --- Setters validate and preserve on rejection ---
    TerminalAnchor mutable = TerminalAnchor();
    assert(TerminalAnchor_setKind(&mutable, TERMINAL_ANCHOR_BAND_RIGHT));
    assert(TerminalAnchor_getKind(&mutable) == TERMINAL_ANCHOR_BAND_RIGHT);
    assert(TerminalAnchor_setExtent(&mutable, 12) && TerminalAnchor_getExtent(&mutable) == 12);
    assert(TerminalAnchor_setSize(&mutable, 7, 5));
    assert(TerminalAnchor_getWidth(&mutable) == 7 && TerminalAnchor_getHeight(&mutable) == 5);
    REJECT(TerminalAnchor_setKind(&mutable, (TerminalAnchorKind) -1), "kind");
    assert(TerminalAnchor_getKind(&mutable) == TERMINAL_ANCHOR_BAND_RIGHT);   // preserved
    REJECT(TerminalAnchor_setKind(nullptr, TERMINAL_ANCHOR_FILL), "kind");
    REJECT(TerminalAnchor_setExtent(nullptr, 1), "extent");
    REJECT(TerminalAnchor_setSize(nullptr, 1, 1), "size");
    REJECT(TerminalAnchor_setInset(nullptr, 1, 1), "inset");

    // --- Null safety across the public surface ---
    assert(!TerminalAnchor_isBand(nullptr) && !TerminalAnchor_reserves(nullptr));
    assert(TerminalAnchor_getKind(nullptr) == TERMINAL_ANCHOR_FILL);
    assert(TerminalAnchor_getExtent(nullptr) == TERMINAL_ANCHOR_AUTO);
    assert(TerminalAnchor_getWidth(nullptr) == TERMINAL_ANCHOR_AUTO);
    assert(TerminalAnchor_getHeight(nullptr) == TERMINAL_ANCHOR_AUTO);
    assert(TerminalAnchor_getInsetX(nullptr) == 0 && TerminalAnchor_getInsetY(nullptr) == 0);
    TerminalAnchorPlacement nullPlace = TerminalAnchor_place(nullptr, cursor);
    assert(rectEqual(nullPlace.region, cursor) && rectEqual(nullPlace.cursor, cursor));

    // --- Projections: value string and a struct dump mirroring STRUCT FIELDS ---
    TERMINAL_PROJECT(TerminalAnchor, &mutable);
    TerminalProjection value = TerminalAnchor_toString(&mutable);
    assert(strstr(value.text, "TerminalAnchor{band-right,reserves=1") != nullptr);
    TerminalProjection structured = TerminalAnchor_toStringStruct(&mutable);
    assert(strstr(structured.text, "kind=band-right") != nullptr);
    assert(strstr(structured.text, "extent=12") != nullptr);
    assert(strstr(structured.text, "insetX=") != nullptr && strstr(structured.text, "insetY=") != nullptr);

    return 0;
}
