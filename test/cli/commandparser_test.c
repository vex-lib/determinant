// Owner: both parse forms, whitespace/empty/one/ten args, malformed controls,
// embedded NUL, overflow before reads and cleanup. No shell syntax offered.
#include "cli/commandparser.h"
#include "nio/mem.h"
#include "oop/type.h"
#include "determinant/test/support.h"
#include <stdlib.h>
int main(void) {
    beginDiagnostics();
    assert(CommandParser_parse("") == nullptr);
    assert(CommandParser_parse(" \t\r\n") == nullptr);
    assert(CommandParser_parseBytes(nullptr, 0) == nullptr);
    Command *cmd = CommandParser_parse(" \tgo a b.sh c d e f g h i j \n");
    assert(cmd && Command_argumentCount(cmd) == 10);
    assert(strcmp((const char*) Command_name(cmd), "go") == 0);
    for (bytes i = 0; i < 10; ++i)
        assert(*Command_argument(cmd, i) == 'a' + i);
    Command_free(cmd);
    cmd = CommandParser_parseBytes("x", 1);
    assert(cmd && Command_argumentCount(cmd) == 0);
    Command_free(cmd);
    endDiagnostics(0, nullptr);
    REJECT(CommandParser_parse(nullptr), "null");
    REJECT(CommandParser_parseBytes(nullptr, 1), "null");
    REJECT(CommandParser_parseBytes("x", UINT32_MAX), "overflow");
    REJECT(CommandParser_parseBytes("x\0y", 3), "invalid ASCII");
    REJECT(CommandParser_parse("x\x1b[31m"), "invalid ASCII");
    REJECT(CommandParser_parse("\xff"), "invalid ASCII");
    beginDiagnostics();
    cmd = CommandParser_parse("recover");
    assert(cmd);
    Command_free(cmd);
    Memory_freeAll();
    endDiagnostics(0, nullptr);
    // Deliberately exhaust test-owned R2 backing, retain a survivor and attack
    // partial token construction. Allocation refusal belongs to R2's epoch;
    // parser does not duplicate its diagnostic or consume the retained command.
    enum { BLOCK_BYTES = 1024 * 1024, MAX_BLOCKS = 128 };
    beginDiagnostics();
    Command *survivor = CommandParser_parse("retained value");
    assert(survivor);
    void *blocks[MAX_BLOCKS];
    bytes live = 0;
    while (live < MAX_BLOCKS) {
        blocks[live] = Memory_alloc(TYPE_BYTE_ARRAY, BLOCK_BYTES);
        if (blocks[live] == nullptr)
            break;
        memset(blocks[live], (int) live, BLOCK_BYTES);
        ++live;
    }
    assert(live != 0 && live < MAX_BLOCKS && Memory_exhaustionCount() == 1);
    char *oversize = malloc(2 * BLOCK_BYTES + 1);
    assert(oversize);
    memset(oversize, 'a', 2 * BLOCK_BYTES);
    oversize[0] = 'x'; oversize[1] = ' '; oversize[2 * BLOCK_BYTES] = 0;
    assert(CommandParser_parse(oversize) == nullptr);
    assert(Memory_exhaustionCount() == 2);
    assert(strcmp((const char*) Command_name(survivor), "retained") == 0);
    assert(strcmp((const char*) Command_argument(survivor, 0), "value") == 0);
    for (bytes j = 0; j < live; ++j) {
        const unsigned char *data = blocks[j];
        assert(data[0] == (unsigned char) j && data[BLOCK_BYTES - 1] == (unsigned char) j);
        Memory_free(blocks[j]);
    }
    Command_free(survivor);
    free(oversize);
    endDiagnostics(1, "request");
    beginDiagnostics();
    Memory_freeAll();
    assert(Memory_exhaustionCount() == 0);
    cmd = CommandParser_parse("recovered after exhaustion");
    assert(cmd && Command_argumentCount(cmd) == 2);
    Command_free(cmd);
    Memory_freeAll();
    endDiagnostics(0, nullptr);
    return 0;
}
