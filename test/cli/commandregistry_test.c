// Owner: explicit borrowed registry/output, duplicate replacement, cold growth,
// counted exhaustion/reset, reentrant mutation/free exclusion and projections.

#include "vex.h"
#include "cli/commandregistry.h"
#include "cli/commandparser.h"
#include "nio/mem.h"
#include "determinant/test/support.h"
static bytes calls, notices;
static CommandRegistry *current;
static fn target(Command *cmd) { assert(cmd); ++calls; }
static fn replacement(Command *cmd) { assert(cmd); calls += 10; }
static bool sink(any hostContext, const char *text, bytes length) {
    assert(hostContext == &notices && length == strlen("Unknown command"));
    assert(memcmp(text, "Unknown command", length) == 0);
    ++notices;
    return true;
}
static fn busy(Command *cmd) {
    assert(cmd);
    assert(CommandRegistry_isActive(current));
    assert(!CommandRegistry_register(current, "new", target));
    assert(!CommandRegistry_free(current));
    assert(!CommandRegistry_setOutput(current, sink, &notices));
}
int main() {
    CommandRegistryRow small[1], larger[4], largest[8];
    CommandRegistry zero = CommandRegistry();
    assert(CommandRegistry_getCount(&zero) == 0);
    CommandRegistry r = CommandRegistry(small, 1, sink, &notices);
    current = &r;
    beginDiagnostics();
    assert(CommandRegistry_add(&r, "go", target));
    Command *cmd = CommandParser_parse("go");
    assert(CommandRegistry_execute(&r, cmd) && calls == 1);
    assert(CommandRegistry_register(&r, "go", replacement));
    assert(CommandRegistry_execute(&r, cmd) && calls == 11);
    assert(CommandRegistry_getCount(&r) == 1);
    assert(CommandRegistry_getRows(&r) == small && !CommandRegistry_isActive(&r));
    assert(CommandRegistry_getOutput(&r) == sink && CommandRegistry_getHostContext(&r) == &notices);
    endDiagnostics(0, nullptr);
    beginDiagnostics();
    assert(!CommandRegistry_register(&r, "other", target));
    assert(!CommandRegistry_register(&r, "other", target));
    assert(CommandRegistry_exhaustionCount(&r) == 2);
    endDiagnostics(1, "count 1/1");
    beginDiagnostics();
    assert(CommandRegistry_reserve(&r, larger, 4));
    assert(CommandRegistry_register(&r, "other", target));
    assert(CommandRegistry_reserve(&r, largest, 8));
    assert(CommandRegistry_getCapacity(&r) == 8);
    Command *unknown = CommandParser_parse("missing");
    assert(!CommandRegistry_execute(&r, unknown) && notices == 1);
    endDiagnostics(0, nullptr);
    assert(CommandRegistry_register(&r, "go", busy));
    beginDiagnostics();
    assert(CommandRegistry_execute(&r, cmd));
    endDiagnostics(3, "busy");
    REJECT(CommandRegistry_register(&r, "bad name", target), "ASCII");
    REJECT(CommandRegistry_register(&r, nullptr, target), "invalid");
    REJECT(CommandRegistry_register(&r, "go", nullptr), "invalid");
    REJECT(CommandRegistry_execute(&r, nullptr), "null");
    REJECT(CommandRegistry_execute(nullptr, cmd), "null");
    REJECT(CommandRegistry_reserve(&r, small, 1), "backing");
    REJECT(CommandRegistry_reserve(&r, largest, SIZE_MAX), "backing");
    assert(CommandRegistry_setOutput(&r, nullptr, nullptr));
    REJECT(CommandRegistry_execute(&r, unknown), "sink");
    assert(CommandRegistry_setOutput(&r, sink, &notices));
    PROJECT(CommandRegistry, &r);
    assert(CommandRegistry_getCount(nullptr) == 0 && CommandRegistry_getCapacity(nullptr) == 0);
    assert(CommandRegistry_getOutput(nullptr) == nullptr && CommandRegistry_getHostContext(nullptr) == nullptr);
    assert(CommandRegistry_exhaustionCount(nullptr) == 0);
    assert(CommandRegistry_getRows(nullptr) == nullptr && !CommandRegistry_isActive(nullptr));
    beginDiagnostics();
    assert(CommandRegistry_free(&r) && CommandRegistry_exhaustionCount(&r) == 0);
    assert(CommandRegistry_getCount(&r) == 0);
    Command_free(cmd);
    Command_free(unknown);
    Memory_freeAll();
    endDiagnostics(0, nullptr);
    return 0;
}
