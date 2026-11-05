// Owner: Terminal cold geometry/backing admission, cell preservation/refusal,
// sink routing, resident state + view/command swap, busy exclusion, projections.
// External serialized API; no arbitrary native rollback or OS acquisition.
#include "ui/surface/terminal.h"
#include "determinant/test/support.h"
static size_t deliveries;
static bool sink(void *context, const char *text, size_t length) {
    assert(context == &deliveries && length == 8 && text);
    ++deliveries;
    return true;
}
static bool failedSink(void *context, const char *text, size_t length) {
    (void) context; (void) text; (void) length; return false;
}
static bool viewA(void *state, Terminal *terminal) {
    ++*(size_t*) state;
    return Terminal_text(terminal, 0, 0, "A", 1);
}
static bool viewB(void *state, Terminal *terminal) {
    ++*(size_t*) state;
    return Terminal_text(terminal, 0, 0, "B", 1);
}
static bool command(void *state, const char *text, size_t length) {
    assert(length == 2 && memcmp(text, "go", 2) == 0);
    ++*(size_t*) state;
    return true;
}
static bool busy(void *state, Terminal *terminal) {
    assert(Terminal_isActive(terminal));
    assert(!Terminal_swap(terminal, (TerminalModule) {TERMINAL_MODULE_ABI, viewB, command}, state));
    assert(!Terminal_free(terminal));
    assert(!Terminal_bind(terminal, nullptr, 0, 1, 1, sink, &deliveries));
    assert(!Terminal_setOutput(terminal, sink, &deliveries));
    assert(!Terminal_render(terminal));
    assert(!Terminal_command(terminal, "go", 2));
    return true;
}
int main(void) {
    char cells[8], bigger[16];
    Terminal t = Terminal();
    size_t state = 0;
    assert(Terminal_isPaused(&t));
    REJECT(Terminal_bind(&t, cells, 8, SIZE_MAX, 2, sink, &deliveries), "geometry");
    REJECT(Terminal_bind(&t, cells, 8, 4, 3, sink, &deliveries), "geometry");
    REJECT(Terminal_bind(&t, cells, 8, 0, 2, sink, &deliveries), "geometry");
    REJECT(Terminal_bind(nullptr, cells, 8, 4, 2, sink, &deliveries), "geometry");
    beginDiagnostics();
    assert(Terminal_bind(&t, cells, 8, 4, 2, sink, &deliveries));
    assert(Terminal_swap(&t, (TerminalModule) {TERMINAL_MODULE_ABI, viewA, command}, &state));
    assert(Terminal_getGeneration(&t) == 1 && Terminal_resume(&t));
    assert(Terminal_render(&t) && cells[0] == 'A' && state == 1);
    assert(Terminal_command(&t, "go", 2) && state == 2);
    assert(Terminal_present(&t) && deliveries == 1);
    assert(Terminal_getWidth(&t) == 4 && Terminal_getHeight(&t) == 2 && Terminal_getCapacity(&t) == 8);
    assert(Terminal_getOutput(&t) == sink && Terminal_getHostContext(&t) == &deliveries);
    assert(Terminal_getCells(&t) == cells && Terminal_getResidentState(&t) == &state);
    TerminalModule admitted = Terminal_getModule(&t);
    assert(admitted.view == viewA && admitted.command == command && admitted.abi == TERMINAL_MODULE_ABI);
    assert(Terminal_text(&t, 0, 1, "abcd", 4));
    endDiagnostics(0, nullptr);
    char snapshot[8]; memcpy(snapshot, cells, 8);
    beginDiagnostics();
    assert(!Terminal_text(&t, 1, 1, "abcd", 4));
    assert(!Terminal_text(&t, SIZE_MAX, 0, "x", SIZE_MAX));
    assert(Terminal_exhaustionCount(&t) == 2);
    endDiagnostics(1, "bound 4x2");
    assert(memcmp(cells, snapshot, 8) == 0);
    REJECT(Terminal_text(&t, 0, 1, "\x1b", 1), "ASCII");
    REJECT(Terminal_command(&t, "\0", 1), "ASCII");
    REJECT(Terminal_swap(&t, (TerminalModule) {TERMINAL_MODULE_ABI, viewB, command}, &state), "busy");
    assert(Terminal_pause(&t));
    REJECT(Terminal_swap(&t, (TerminalModule) {0, viewB, command}, &state), "incompatible");
    REJECT(Terminal_swap(&t, (TerminalModule) {TERMINAL_MODULE_ABI, nullptr, command}, &state), "incompatible");
    assert(Terminal_getGeneration(&t) == 1 && state == 2);
    beginDiagnostics();
    assert(Terminal_resume(&t) && Terminal_render(&t) && cells[0] == 'A');
    assert(Terminal_pause(&t));
    assert(Terminal_swap(&t, (TerminalModule) {TERMINAL_MODULE_ABI, viewB, command}, &state));
    assert(Terminal_getGeneration(&t) == 2 && Terminal_resume(&t));
    assert(Terminal_render(&t) && cells[0] == 'B' && state == 4);
    assert(Terminal_pause(&t));
    assert(Terminal_swap(&t, (TerminalModule) {TERMINAL_MODULE_ABI, busy, command}, &state));
    assert(Terminal_resume(&t));
    endDiagnostics(0, nullptr);
    beginDiagnostics();
    assert(Terminal_render(&t));
    endDiagnostics(6, "busy");
    assert(Terminal_getGeneration(&t) == 3);
    assert(Terminal_pause(&t));
    assert(Terminal_bind(&t, bigger, 16, 8, 2, sink, &deliveries));
    assert(Terminal_getWidth(&t) == 8);
    assert(Terminal_setOutput(&t, failedSink, nullptr));
    beginDiagnostics();
    assert(!Terminal_present(&t));
    assert(Terminal_clear(&t));
    assert(!Terminal_clear(nullptr) && !Terminal_present(nullptr));
    endDiagnostics(0, nullptr);
    TERMINAL_PROJECT(Terminal, &t);
    assert(Terminal_getWidth(nullptr) == 0 && Terminal_getHeight(nullptr) == 0);
    assert(Terminal_getCapacity(nullptr) == 0 && Terminal_getGeneration(nullptr) == 0);
    assert(Terminal_getOutput(nullptr) == nullptr && Terminal_getHostContext(nullptr) == nullptr);
    assert(Terminal_exhaustionCount(nullptr) == 0 && !Terminal_isPaused(nullptr) && !Terminal_isActive(nullptr));
    assert(Terminal_getCells(nullptr) == nullptr && Terminal_getResidentState(nullptr) == nullptr);
    admitted = Terminal_getModule(nullptr);
    assert(admitted.view == nullptr && admitted.command == nullptr && admitted.abi == 0);
    assert(Terminal_free(&t) && Terminal_exhaustionCount(&t) == 0);
    assert(Terminal_getGeneration(&t) == 0 && Terminal_getCapacity(&t) == 0);
    bigger[0] = 'X'; // backing still belongs to host
    REJECT(Terminal_resume(&t), "unbound");
    return 0;
}
