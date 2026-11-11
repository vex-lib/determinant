// Externally supervised real loaded-module swap. The test host owns dlopen,
// callable-code lifetime and exclusion, not Determinant. No Primeagen proof.
#include "ui/surface/terminal.h"
#include "test_support.h"
#include "determinant/test/support.h"
#include <dlfcn.h>
static bool sink(void *context, const char *text, bytes length) {
    (void) context; assert(text && length == 4); return true;
}
int main(void) {
#if !defined(DETERMINANT_MODULE_A) || !defined(DETERMINANT_MODULE_B)
    (void) sink;
    (void) beginDiagnostics;
    (void) endDiagnostics;
    fprintf(stderr, "SKIP: externally built module paths unavailable\n");
    return B_TEST_SKIP;
#else
    void *a = dlopen(DETERMINANT_MODULE_A, RTLD_NOW | RTLD_LOCAL);
    void *b = dlopen(DETERMINANT_MODULE_B, RTLD_NOW | RTLD_LOCAL);
    assert(a && b);
    TerminalModule (*getA)(void) = (TerminalModule (*)(void)) dlsym(a, "determinantModule");
    TerminalModule (*getB)(void) = (TerminalModule (*)(void)) dlsym(b, "determinantModule");
    assert(getA && getB);
    TerminalModule first = getA(), second = getB();
    char cells[4];
    bytes resident = 0;
    Terminal t = Terminal();
    beginDiagnostics();
    assert(Terminal_bind(&t, cells, 4, 4, 1, sink, nullptr));
    assert(Terminal_swap(&t, first, &resident));
    assert(Terminal_resume(&t));
    assert(Terminal_render(&t) && cells[0] == 'A' && resident == 1);
    assert(Terminal_command(&t, "go", 2) && resident == 2);
    assert(Terminal_pause(&t));
    endDiagnostics(0, nullptr);
    TerminalModule bad = second; bad.abi = 0;
    REJECT(Terminal_swap(&t, bad, &resident), "incompatible");
    assert(Terminal_getGeneration(&t) == 1 && resident == 2);
    beginDiagnostics();
    assert(Terminal_resume(&t) && Terminal_render(&t));
    assert(cells[0] == 'A' && resident == 3);
    assert(Terminal_pause(&t));
    assert(Terminal_swap(&t, second, &resident));
    assert(Terminal_getGeneration(&t) == 2);
    assert(dlclose(a) == 0); // admission paused; no active users or old callback
    assert(Terminal_resume(&t));
    assert(Terminal_render(&t) && cells[0] == 'B' && resident == 4);
    assert(Terminal_command(&t, "go", 2) && resident == 5);
    assert(Terminal_present(&t));
    assert(Terminal_pause(&t) && Terminal_free(&t));
    assert(dlclose(b) == 0); // detached before code retirement
    assert(resident == 5 && cells[0] == 'B');
    endDiagnostics(0, nullptr);
    return 0;
#endif
}
