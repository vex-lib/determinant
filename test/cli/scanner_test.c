// Owner: caller-fed partial lines/words/CRLF/EOF and empty line distinction,
// exact capacity + repeated refusal, cold growth/reset, projection/null defaults.
// Input backing is caller-owned; no OS/stdin/threads or Unicode offered.
#include "cli/scanner.h"
#include "primitive/vex-string.h"
#include "nio/mem.h"
#include "determinant/test/support.h"
int main(void) {
    char small[4], medium[8], large[32];
    Scanner zero = Scanner();
    assert(Scanner_getCapacity(&zero) == 0);
    Scanner s = Scanner(small, sizeof(small));
    assert(Scanner_getBytes(&s) == small && Scanner_getBytes(nullptr) == nullptr);
    beginDiagnostics();
    assert(Scanner_feed(&s, "a", 1, false));
    assert(!Scanner_hasNextLine(&s) && Scanner_nextLine(&s) == nullptr);
    assert(Scanner_nextWord(&s) == nullptr);
    assert(Scanner_getPosition(&s) == 0);
    assert(Scanner_feed(&s, "\r\n\n", 3, false));
    assert(Scanner_getLength(&s) == 4);
    uint8_t *line = Scanner_nextLine(&s);
    assert(line && strcmp((const char*) line, "a") == 0);
    string_free(line);
    line = Scanner_nextLine(&s);
    assert(line && string_length(line) == 0);
    string_free(line);
    endDiagnostics(0, nullptr);
    beginDiagnostics();
    assert(!Scanner_feed(&s, "x", 1, false));
    assert(!Scanner_feed(&s, "x", SIZE_MAX, false));
    assert(Scanner_exhaustionCount(&s) == 2);
    assert(Scanner_getPosition(&s) == 4 && Scanner_getLength(&s) == 4);
    endDiagnostics(1, "request 1 bytes");
    beginDiagnostics();
    assert(Scanner_reserve(&s, medium, sizeof(medium)));
    assert(Scanner_getLength(&s) == 0 && Scanner_getPosition(&s) == 0);
    assert(Scanner_reserve(&s, large, sizeof(large)));
    assert(Scanner_feed(&s, " one two", 8, true));
    assert(Scanner_isEnded(&s));
    line = Scanner_nextWord(&s);
    assert(line && strcmp((const char*) line, "one") == 0);
    string_free(line);
    line = Scanner_nextWord(&s);
    assert(line && strcmp((const char*) line, "two") == 0);
    string_free(line);
    assert(Scanner_nextWord(&s) == nullptr);
    endDiagnostics(0, nullptr);
    REJECT(Scanner_feed(&s, "x", 1, false), "ended");
    Scanner_reset(&s);
    assert(Scanner_exhaustionCount(&s) == 0 && !Scanner_isEnded(&s));
    REJECT(Scanner_feed(&s, "x\0", 2, false), "ASCII");
    REJECT(Scanner_feed(&s, "\x1b", 1, false), "ASCII");
    REJECT(Scanner_feed(&s, "\xff", 1, false), "ASCII");
    assert(Scanner_getLength(&s) == 0);
    REJECT(Scanner_feed(&s, nullptr, 1, false), "span");
    REJECT(Scanner_feed(nullptr, "x", 1, false), "span");
    REJECT(Scanner_reserve(&s, small, 4), "backing");
    REJECT(Scanner_reserve(&s, large, 64), "backing");
    REJECT(Scanner_reserve(nullptr, large, 32), "backing");
    REJECT(Scanner_nextLine(nullptr), "null");
    REJECT(Scanner_nextWord(nullptr), "null");
    assert(!Scanner_hasNextLine(nullptr) && !Scanner_isEnded(nullptr));
    assert(Scanner_getLength(nullptr) == 0 && Scanner_getPosition(nullptr) == 0);
    assert(Scanner_getCapacity(nullptr) == 0 && Scanner_exhaustionCount(nullptr) == 0);
    PROJECT(Scanner, &s);
    beginDiagnostics();
    Scanner_reset(nullptr);
    Scanner_reset(&s);
    assert(Scanner_feed(&s, "last", 4, true));
    line = Scanner_nextLine(&s);
    assert(line && strcmp((const char*) line, "last") == 0);
    string_free(line);
    Memory_freeAll();
    endDiagnostics(0, nullptr);
    return 0;
}
