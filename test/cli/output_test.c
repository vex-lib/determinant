// Header-only owner: a literal span/context sink is a typed callback contract,
// not an allocator, ANSI decoder or driver. Positive and rejecting calls.
#include "cli/output.h"
#include <assert.h>
#include <string.h>
static bool output(void *hostContext, const char *text, bytes length) {
    bytes *received = hostContext;
    if (length == 0)
        return false;
    assert(memcmp(text, "abc", length) == 0);
    *received += length;
    return true;
}
int main() {
    bytes received = 0;
    DeterminantOutputFn fn = output;
    assert(fn(&received, "abc", 3) && received == 3);
    assert(!fn(&received, nullptr, 0) && received == 3);
    return 0;
}
