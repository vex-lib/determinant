// Owner: TerminalScrollGlide — per-axis viewport momentum. Defaults, configured
// easing, impulse/target/position/halt/clamp, one-frame step and snap, safe
// defaults, projections, and the hot step guard.
#include "ui/scroll/terminal_scroll_glide.h"
#include "determinant/test/support.h"
#include <math.h>
#include <stdint.h>

static void assertNear(d64 actual, d64 expected) {
    assert(fabs(actual - expected) < 1e-9);
}

int main(void) {
    TerminalScrollGlide glide = TerminalScrollGlide();
    assertNear(TerminalScrollGlide_getEase(&glide), TERMINAL_SCROLL_EASE_DEFAULT);
    assertNear(TerminalScrollGlide_getEpsilon(&glide), TERMINAL_SCROLL_EPSILON_DEFAULT);
    assertNear(TerminalScrollGlide_getRowShown(&glide), 0.0);
    assertNear(TerminalScrollGlide_getRowTarget(&glide), 0.0);
    assertNear(TerminalScrollGlide_getColumnShown(&glide), 0.0);
    assertNear(TerminalScrollGlide_getColumnTarget(&glide), 0.0);
    assert(!TerminalScrollGlide_isActive(&glide));
    assert(TerminalScrollGlide_getPosition(&glide).row == 0u);
    assert(TerminalScrollGlide_getPosition(&glide).column == 0u);

    // Configured construction, and validated setters.
    glide = TerminalScrollGlide(0.5, 0.25);
    assertNear(TerminalScrollGlide_getEase(&glide), 0.5);
    assertNear(TerminalScrollGlide_getEpsilon(&glide), 0.25);
    assert(TerminalScrollGlide_setEase(&glide, 0.2));
    assert(TerminalScrollGlide_setEpsilon(&glide, 0.1));
    assertNear(TerminalScrollGlide_getEase(&glide), 0.2);
    assertNear(TerminalScrollGlide_getEpsilon(&glide), 0.1);

    // Invalid configuration rejects loudly and preserves the previous values.
    REJECT(TerminalScrollGlide_setEase(&glide, 0.0), "outside (0,1)");
    REJECT(TerminalScrollGlide_setEase(&glide, 1.0), "outside (0,1)");
    REJECT(TerminalScrollGlide_setEase(&glide, -0.5), "outside (0,1)");
    REJECT(TerminalScrollGlide_setEpsilon(&glide, 0.0), "not positive");
    REJECT(TerminalScrollGlide_setEpsilon(&glide, INFINITY), "not positive");
    REJECT(TerminalScrollGlide_setEase(nullptr, 0.5), "null");
    REJECT(TerminalScrollGlide_setEpsilon(nullptr, 0.5), "null");
    assertNear(TerminalScrollGlide_getEase(&glide), 0.2);
    assertNear(TerminalScrollGlide_getEpsilon(&glide), 0.1);
    beginDiagnostics();
    TerminalScrollGlide badEase = TerminalScrollGlide(0.0, 0.4);
    endDiagnostics(1, "ease");
    assertNear(TerminalScrollGlide_getEase(&badEase), TERMINAL_SCROLL_EASE_DEFAULT);
    assertNear(TerminalScrollGlide_getEpsilon(&badEase), TERMINAL_SCROLL_EPSILON_DEFAULT);
    beginDiagnostics();
    TerminalScrollGlide badEpsilon = TerminalScrollGlide(0.5, 0.0);
    endDiagnostics(1, "epsilon");
    assertNear(TerminalScrollGlide_getEase(&badEpsilon), TERMINAL_SCROLL_EASE_DEFAULT);
    assertNear(TerminalScrollGlide_getEpsilon(&badEpsilon), TERMINAL_SCROLL_EPSILON_DEFAULT);

    // Input moves only the target, clamped into both bounds.
    glide = TerminalScrollGlide(0.5, 0.1);
    TerminalScrollGlide_impulse(&glide, 5, 3, 10, 6);
    assertNear(TerminalScrollGlide_getRowTarget(&glide), 5.0);
    assertNear(TerminalScrollGlide_getColumnTarget(&glide), 3.0);
    assertNear(TerminalScrollGlide_getRowShown(&glide), 0.0);
    TerminalScrollGlide_impulse(&glide, -20, -20, 10, 6);
    assertNear(TerminalScrollGlide_getRowTarget(&glide), 0.0);
    assertNear(TerminalScrollGlide_getColumnTarget(&glide), 0.0);
    TerminalScrollGlide_impulse(&glide, 100, 100, 10, 6);
    assertNear(TerminalScrollGlide_getRowTarget(&glide), 10.0);
    assertNear(TerminalScrollGlide_getColumnTarget(&glide), 6.0);
    TerminalScrollGlide_halt(&glide);
    assertNear(TerminalScrollGlide_getRowTarget(&glide), 0.0);
    assertNear(TerminalScrollGlide_getColumnTarget(&glide), 0.0);

    // Aim without moving the shown position; out-of-bounds targets clamp.
    TerminalScrollGlide_setTarget(&glide, 4, 2, 10, 6);
    assertNear(TerminalScrollGlide_getRowTarget(&glide), 4.0);
    assertNear(TerminalScrollGlide_getColumnTarget(&glide), 2.0);
    assertNear(TerminalScrollGlide_getRowShown(&glide), 0.0);
    TerminalScrollGlide_setTarget(&glide, 99, 99, 10, 6);
    assertNear(TerminalScrollGlide_getRowTarget(&glide), 10.0);
    assertNear(TerminalScrollGlide_getColumnTarget(&glide), 6.0);

    // Snapping one or both axes lands shown and target together.
    TerminalScrollGlide_setPosition(&glide, 7, 3, 10, 6);
    TerminalScrollGlidePosition position = TerminalScrollGlide_getPosition(&glide);
    assert(position.row == 7u && position.column == 3u);
    TerminalScrollGlide_setPosition(&glide, 99, 99, 10, 6);
    position = TerminalScrollGlide_getPosition(&glide);
    assert(position.row == 10u && position.column == 6u);
    TerminalScrollGlide_setRowPosition(&glide, 2, 10);
    assertNear(TerminalScrollGlide_getRowShown(&glide), 2.0);
    assertNear(TerminalScrollGlide_getColumnShown(&glide), 6.0);
    TerminalScrollGlide_setColumnPosition(&glide, 1, 6);
    assertNear(TerminalScrollGlide_getRowShown(&glide), 2.0);
    assertNear(TerminalScrollGlide_getColumnShown(&glide), 1.0);

    // A layout change clamps both the shown position and the target.
    TerminalScrollGlide_setPosition(&glide, 10, 6, 10, 6);
    TerminalScrollGlide_clamp(&glide, 3, 2);
    position = TerminalScrollGlide_getPosition(&glide);
    assert(position.row == 3u && position.column == 2u);
    assertNear(TerminalScrollGlide_getRowTarget(&glide), 3.0);
    assertNear(TerminalScrollGlide_getColumnTarget(&glide), 2.0);

    // One frame covers half the remaining distance, snaps at the threshold, and
    // returns the whole cell to paint.
    glide = TerminalScrollGlide(0.5, 0.1);
    TerminalScrollGlide_setTarget(&glide, 10, 10, 10, 10);
    assert(TerminalScrollGlide_isActive(&glide));
    position = TerminalScrollGlide_step(&glide);
    assertNear(TerminalScrollGlide_getRowShown(&glide), 5.0);
    assertNear(TerminalScrollGlide_getColumnShown(&glide), 5.0);
    assert(position.row == 5u && position.column == 5u);
    position = TerminalScrollGlide_step(&glide);
    assertNear(TerminalScrollGlide_getRowShown(&glide), 7.5);
    assertNear(TerminalScrollGlide_getColumnShown(&glide), 7.5);
    assert(position.row == 8u && position.column == 8u);
    position = TerminalScrollGlide_step(&glide);
    assertNear(TerminalScrollGlide_getRowShown(&glide), 8.75);
    assert(position.row == 9u && position.column == 9u);
    position = TerminalScrollGlide_step(&glide);
    assertNear(TerminalScrollGlide_getRowShown(&glide), 9.375);
    assert(position.row == 9u && position.column == 9u);
    position = TerminalScrollGlide_step(&glide);
    assertNear(TerminalScrollGlide_getRowShown(&glide), 9.6875);
    assert(position.row == 10u && position.column == 10u);
    position = TerminalScrollGlide_step(&glide);
    assertNear(TerminalScrollGlide_getRowShown(&glide), 9.84375);
    position = TerminalScrollGlide_step(&glide);
    assertNear(TerminalScrollGlide_getRowShown(&glide), 9.921875);
    position = TerminalScrollGlide_step(&glide);
    assertNear(TerminalScrollGlide_getRowShown(&glide), 10.0);
    assertNear(TerminalScrollGlide_getColumnShown(&glide), 10.0);
    assert(position.row == 10u && position.column == 10u);
    assert(!TerminalScrollGlide_isActive(&glide));

    // A saturated bound is a whole-cell value, never an out-of-range d64 cast.
    // The huge epsilon snaps instead of easing, so the conversion is exercised.
    glide = TerminalScrollGlide(0.5, 1e300);
    TerminalScrollGlide_setTarget(&glide, SIZE_MAX, SIZE_MAX, SIZE_MAX, SIZE_MAX);
    position = TerminalScrollGlide_step(&glide);
    assert(position.row == SIZE_MAX && position.column == SIZE_MAX);
    position = TerminalScrollGlide_getPosition(&glide);
    assert(position.row == SIZE_MAX && position.column == SIZE_MAX);

    // Return to a settled ordinary glide for the hot-loop guard below.
    glide = TerminalScrollGlide(0.5, 0.1);
    TerminalScrollGlide_setPosition(&glide, 10, 10, 10, 10);

    // The hot step path: one null guard, no logging, stable once settled.
    position = TerminalScrollGlide_step(nullptr);
    assert(position.row == 0u && position.column == 0u);
    TerminalScrollGlide_impulse(nullptr, 1, 1, 10, 10);
    TerminalScrollGlide_setTarget(nullptr, 1, 1, 10, 10);
    TerminalScrollGlide_setPosition(nullptr, 1, 1, 10, 10);
    TerminalScrollGlide_setRowPosition(nullptr, 1, 10);
    TerminalScrollGlide_setColumnPosition(nullptr, 1, 10);
    TerminalScrollGlide_halt(nullptr);
    TerminalScrollGlide_clamp(nullptr, 10, 10);
    assert(!TerminalScrollGlide_isActive(nullptr));
    assertNear(TerminalScrollGlide_getRowShown(nullptr), 0.0);
    assertNear(TerminalScrollGlide_getRowTarget(nullptr), 0.0);
    assertNear(TerminalScrollGlide_getColumnShown(nullptr), 0.0);
    assertNear(TerminalScrollGlide_getColumnTarget(nullptr), 0.0);
    assert(TerminalScrollGlide_getPosition(nullptr).row == 0u);
    assertNear(TerminalScrollGlide_getEase(nullptr), TERMINAL_SCROLL_EASE_DEFAULT);
    assertNear(TerminalScrollGlide_getEpsilon(nullptr), TERMINAL_SCROLL_EPSILON_DEFAULT);
    for (int frame = 0; frame < 10000; ++frame) {
        position = TerminalScrollGlide_step(&glide);
        assert(position.row == 10u && position.column == 10u);
    }

    TERMINAL_PROJECT(TerminalScrollGlide, &glide);
    return 0;
}
