#ifndef DETERMINANT_TEST_SUPPORT_H
#define DETERMINANT_TEST_SUPPORT_H
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
// Owner-local stderr capture. Each negative block checks exact line count and
// a cause fragment; quiet admitted paths fail on any unexpected diagnostics.
static FILE *diagnostics;
static int savedStderr;
static void beginDiagnostics(void) {
    fflush(stderr);
    diagnostics = tmpfile();
    assert(diagnostics != nullptr);
    savedStderr = dup(STDERR_FILENO);
    assert(savedStderr >= 0);
    assert(dup2(fileno(diagnostics), STDERR_FILENO) >= 0);
}
static void endDiagnostics(size_t expected, const char *fragment) {
    fflush(stderr);
    assert(dup2(savedStderr, STDERR_FILENO) >= 0);
    close(savedStderr);
    rewind(diagnostics);
    char text[8192];
    size_t n = fread(text, 1, sizeof(text) - 1, diagnostics);
    text[n] = 0;
    size_t lines = 0;
    for (size_t i = 0; i < n; ++i)
        if (text[i] == '\n')
            ++lines;
    assert(lines == expected);
    if (expected != 0)
        assert(strstr(text, "[vex] ") != nullptr);
    if (fragment != nullptr)
        assert(strstr(text, fragment) != nullptr);
    fclose(diagnostics);
}
#define REJECT(expression, fragment) do { beginDiagnostics(); assert(!(expression)); endDiagnostics(1, fragment); } while (0)
#define PROJECT(Class, self) do { \
    char text[512]; bool cut = true; \
    beginDiagnostics(); \
    assert(Class##_toString(self, text, sizeof(text), &cut) && !cut); \
    assert(Class##_toStringStruct(self, text, sizeof(text), nullptr)); \
    assert(Class##_toString(nullptr, text, sizeof(text), &cut)); \
    assert(strcmp(text, "nullptr") == 0); \
    endDiagnostics(0, nullptr); \
    REJECT(Class##_toString(self, text, 1, &cut), "truncated"); assert(cut); \
    REJECT(Class##_toStringStruct(self, nullptr, 0, &cut), "destination"); assert(cut); \
} while (0)
#define TERMINAL_PROJECT(Class, self) do { \
    beginDiagnostics(); \
    TerminalProjection projection = Class##_toString(self); \
    assert(projection.length > 0 && !projection.truncated); \
    TerminalProjection structured = Class##_toStringStruct(self); \
    assert(structured.length > 0 && !structured.truncated); \
    TerminalProjection nullProjection = Class##_toString(nullptr); \
    assert(strcmp(nullProjection.text, "nullptr") == 0 && !nullProjection.truncated); \
    endDiagnostics(0, nullptr); \
} while (0)
#endif
