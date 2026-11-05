// Trusted externally supervised fixture, not a production loader or owner test.
// Native callbacks operate only on resident state and host-admitted cell backing.
#include "ui/surface/terminal.h"
#ifndef DETERMINANT_FIXTURE_MARK
#define DETERMINANT_FIXTURE_MARK 65
#endif
static bool view(void *residentState, Terminal *terminal) {
    size_t *count = residentState;
    ++*count;
    (*terminal).cells[0] = DETERMINANT_FIXTURE_MARK;
    return true;
}
static bool command(void *residentState, const char *text, size_t length) {
    if (length != 2 || text[0] != 'g' || text[1] != 'o')
        return false;
    ++*(size_t*) residentState;
    return true;
}
TerminalModule determinantModule(void) {
    return (TerminalModule) {TERMINAL_MODULE_ABI, view, command};
}
