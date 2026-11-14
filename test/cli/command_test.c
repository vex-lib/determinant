// Owner: Command all constructor forms, ownership transfer/rejection, index,
// overflow/null/duplicate ownership, projections and free. External exclusion;
// stale raw pointers are outside contract. R2 owns allocation exhaustion.

#include "vex.h"
#include "cli/command.h"
#include "primitive/vex-string.h"
#include "nio/mem.h"
#include "determinant/test/support.h"
int main() {
    beginDiagnostics();
    Command *empty = Command();
    assert(empty && Command_argumentCount(empty) == 0);
    assert(strcmp((const char*) Command_name(empty), "") == 0);
    Command_free(empty);
    u8 *name = String("go");
    u8 *args[] = {String("a"), String("b.sh")};
    Command *cmd = Command(name, args, 2);
    assert(cmd && Command_argumentCount(cmd) == 2);
    assert(Command_name(cmd) == name && Command_argument(cmd, 1) == args[1]);
    assert(Command_argument(cmd, 2) == nullptr);
    assert(Command_name(nullptr) == nullptr && Command_argumentCount(nullptr) == 0);
    assert(Command_argument(nullptr, 0) == nullptr);
    endDiagnostics(0, nullptr);
    PROJECT(Command, cmd);
    REJECT(Command_3(nullptr, args, 2), "null input");
    REJECT(Command_3(name, nullptr, 1), "null input");
    REJECT(Command_3(name, args, SIZE_MAX), "overflow");
    u8 *duplicate[] = {args[0], args[0]};
    REJECT(Command_3(name, duplicate, 2), "duplicate");
    u8 *alias[] = {name};
    REJECT(Command_3(name, alias, 1), "owned argument");
    u8 *missing[] = {nullptr};
    REJECT(Command_3(name, missing, 1), "owned argument");
    assert(strcmp((const char*) args[0], "a") == 0); // rejection did not consume
    beginDiagnostics();
    Command_free(cmd);
    Command_free(nullptr);
    Memory_freeAll();
    endDiagnostics(0, nullptr);
    return 0;
}
