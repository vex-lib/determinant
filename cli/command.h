#ifndef DETERMINANT_COMMAND_H
#define DETERMINANT_COMMAND_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "util/constructor.h"
typedef struct Command {
    u8 *name;
    bytes argumentCount;
    u8 **arguments;
} Command;
// Takes live R2 string blocks only on success; caller retains on failure.
// Arguments must be distinct owned blocks, distinct from name. External exclusion.
Command *Command_0();
Command *Command_3(u8 *name, u8 **arguments, bytes count);
#define Command(...) CONSTRUCTOR_DISPATCH(Command, __VA_ARGS__)
#define Command_zero() Command_0()
u8 *Command_name(const Command *self);
bytes Command_argumentCount(const Command *self);
u8 *Command_argument(const Command *self, bytes index);
fn Command_free(Command *self);
bool Command_toString(const Command *self, char *dest, bytes cap, bool *outTruncated);
bool Command_toStringStruct(const Command *self, char *dest, bytes cap, bool *outTruncated);
#endif
