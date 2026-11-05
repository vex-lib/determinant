// Owner: real disposable R2 log files, sink-routed stat/cat/limit, malformed
// extent/format/null, hostile timestamps, sink failure. Stable file required.
#include "cli/logcommands.h"
#include "nio/mem.h"
#include "determinant/test/support.h"
#include <stdlib.h>
#include <stdint.h>
static char captured[1024];
static size_t used;
static bool fail;
static bool sink(void *context, const char *text, size_t length) {
    assert(context == &used);
    if (fail)
        return false;
    assert(length < sizeof(captured) - used);
    memcpy(captured + used, text, length);
    used += length;
    captured[used++] = '\n'; captured[used] = 0;
    return true;
}
int main(void) {
    char path[4096];
    const char *tmp = getenv("TMPDIR");
    assert(tmp != nullptr);
    int n = snprintf(path, sizeof(path), "%s/determinant-log-XXXXXX", tmp);
    assert(n > 0 && (size_t) n < sizeof(path));
    int fd = mkstemp(path);
    assert(fd >= 0);
    FILE *file = fdopen(fd, "wb");
    assert(file);
    const unsigned char header[] = {'A','N','T','I','L','O','G',1,0,0,0,52};
    assert(fwrite(header, 1, sizeof(header), file) == sizeof(header));
    unsigned char record[52]; memset(record, 255, sizeof(record));
    record[0] = 0; record[1] = 0; record[2] = 0; record[3] = 1;
    assert(fwrite(record, 1, sizeof(record), file) == sizeof(record));
    record[4] = 0x80; memset(record + 5, 0, 7); // INT64_MIN timestamp
    assert(fwrite(record, 1, sizeof(record), file) == sizeof(record));
    assert(fclose(file) == 0);
    beginDiagnostics();
    assert(LogCommands_stat(path, sink, &used));
    assert(strstr(captured, "records=2 bytes=116 payload=104"));
    used = 0;
    assert(LogCommands_cat(path, 1, sink, &used));
    assert(strstr(captured, "timestamp=-1") && strstr(captured, "(1 more)"));
    used = 0;
    assert(LogCommands_cat(path, -1, sink, &used));
    assert(strstr(captured, "-9223372036854775808"));
    used = 0;
    assert(LogCommands_cat(path, 0, sink, &used));
    assert(strcmp(captured, "(2 more)\n") == 0);
    endDiagnostics(0, nullptr);
    fail = true;
    REJECT(LogCommands_stat(path, sink, &used), "sink");
    REJECT(LogCommands_cat(path, -1, sink, &used), "sink");
    fail = false;
    REJECT(LogCommands_stat(nullptr, sink, &used), "null");
    REJECT(LogCommands_cat(path, 0, nullptr, &used), "null");
    file = fopen(path, "ab"); assert(file); fputc(0, file); assert(fclose(file) == 0);
    REJECT(LogCommands_cat(path, -1, sink, &used), "incomplete");
    file = fopen(path, "wb"); assert(file); fputc(0, file); assert(fclose(file) == 0);
    REJECT(LogCommands_stat(path, sink, &used), "invalid log");
    assert(unlink(path) == 0);
    Memory_freeAll();
    return 0;
}
