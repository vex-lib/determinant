// Trusted externally supervised fixture, not a production loader or owner test.
// Native callbacks operate only on resident state and host-admitted cell backing.

#include "vex.h"
#include "ui/surface/terminal.h"
#ifndef DETERMINANT_FIXTURE_MARK
#define DETERMINANT_FIXTURE_MARK 65

;;GETTER
static inline char * Terminal_cells(const Terminal *s) { return (*s).cells; }
;;GETTER
static inline char * *Terminal_cellsMut(Terminal *s) { return &(*s).cells; }
;;SETTER
static inline fn Terminal_setCells(Terminal *s, char * v) { (*s).cells = v; }

#endif
static bool view(any residentState, Terminal *terminal) {
    bytes *count = residentState;
    ++*count;
    Terminal_cells(terminal)[0] = DETERMINANT_FIXTURE_MARK;
    return true;
}
static bool command(any residentState, const char *text, bytes length) {
    if (length != 2 || text[0] != 'g' || text[1] != 'o')
        return false;
    ++*(bytes*) residentState;
    return true;
}
TerminalModule determinantModule() {
    return (TerminalModule) {TERMINAL_MODULE_ABI, view, command};
}
