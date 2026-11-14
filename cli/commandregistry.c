#include "cli/commandregistry.h"
#include "exception/throw.h"
#include "annotation/annotation.h"
#include <string.h>
#include <stdio.h>

;;DEFINITION
/** A resident R3 command directory borrows flat caller rows and explicit output.
 * No global Map/type punning is necessary. Cold reserve supplies more backing;
 * admission never silently drops commands. A callback is borrowed code, not a
 * loader pin. External serialization and host residency are required. */
;;OVERVIEW
/** ========================================================================
 * CLASS: CommandRegistry (cli/commandregistry.c)
 * STRUCT FIELDS (Mirroring cli/commandregistry.h)
 * CommandRegistryRow *rows       borrowed flat directory
 * bytes capacity               admitted row extent
 * bytes count                  initialized rows
 * DeterminantOutputFn output     borrowed presentation callback
 * any hostContext             opaque host-defined presentation state
 * bytes refusalCount           counted capacity refusals, reset by free
 * bool active                   callback exclusion/reentrant admission guard
 * SLOT RECORD: CommandRegistryRow
 * const char *name               borrowed immutable ASCII command identity
 * CommandFn target              borrowed resident callback
 * FUNCTION REGISTRY (exports)
 * Constructors: CommandRegistry CommandRegistry_0()
 *   CommandRegistry CommandRegistry_4(CommandRegistryRow *rows, bytes capacity,
 *     DeterminantOutputFn output, any hostContext)
 * Admission: bool CommandRegistry_reserve(CommandRegistry *self,
 *     CommandRegistryRow *rows, bytes capacity)
 *   bool CommandRegistry_register(CommandRegistry *self, const char *name,
 *     CommandFn target) [CommandRegistry_add alias]
 * Dispatch: bool CommandRegistry_execute(CommandRegistry *self, Command *command)
 * Configuration: bool CommandRegistry_setOutput(CommandRegistry *self,
 *     DeterminantOutputFn output, any hostContext)
 * Queries: DeterminantOutputFn CommandRegistry_getOutput(const CommandRegistry *self)
 *   any CommandRegistry_getHostContext(const CommandRegistry *self)
 *   const CommandRegistryRow *CommandRegistry_getRows(const CommandRegistry *self)
 *   bytes CommandRegistry_getCount(const CommandRegistry *self)
 *   bytes CommandRegistry_getCapacity(const CommandRegistry *self)
 *   bytes CommandRegistry_exhaustionCount(const CommandRegistry *self)
 *   bool CommandRegistry_isActive(const CommandRegistry *self)
 * Lifecycle: bool CommandRegistry_free(CommandRegistry *self), detaches borrows
 * Projections: bool CommandRegistry_toString(const CommandRegistry *self,
 *     char *dest, bytes cap, bool *outTruncated)
 *   bool CommandRegistry_toStringStruct(const CommandRegistry *self,
 *     char *dest, bytes cap, bool *outTruncated)
 * CONTRACT: no internal thread safety; mutation/free rejected during callback.
 * Duplicate name replaces target in-place. Capacity refusal counts every time,
 * reports only first per epoch. reserve requires disjoint live caller spans;
 * failure preserves rows/count/callbacks. Unknown command is routed to output,
 * never stdout; absent/failing sink rejects loudly. No owned allocation.
 * ======================================================================== */
CommandRegistry CommandRegistry_0() { return (CommandRegistry) {0}; }
CommandRegistry CommandRegistry_4(CommandRegistryRow *rows, bytes capacity, DeterminantOutputFn output,
                                  any hostContext) {
    CommandRegistry self = CommandRegistry_0();
    if ((capacity != 0 && rows == nullptr) || capacity > SIZE_MAX / sizeof(*rows)) {
        THROW("CommandRegistry construct: invalid backing extent");
        return self;
    }
    self.rows = rows;
    self.capacity = capacity;
    self.output = output;
    self.hostContext = hostContext;
    return self;
}
bool CommandRegistry_reserve(CommandRegistry *self, CommandRegistryRow *rows, bytes capacity) {
    if (self == nullptr || (*self).active || rows == nullptr || capacity < (*self).count
        || capacity < (*self).capacity || capacity > SIZE_MAX / sizeof(*rows)
        || (rows == (*self).rows && capacity != (*self).capacity)) {
        THROW("CommandRegistry reserve: invalid backing or busy");
        return false;
    }
    if (rows != (*self).rows && (*self).count != 0)
        memcpy(rows, (*self).rows, (*self).count * sizeof(*rows));
    (*self).rows = rows;
    (*self).capacity = capacity;
    return true;
}
bool CommandRegistry_register(CommandRegistry *self, const char *name, CommandFn target) {
    if (self == nullptr || (*self).active || name == nullptr || *name == 0 || target == nullptr) {
        THROW("CommandRegistry register: invalid input or busy");
        return false;
    }
    for (const unsigned char *p = (const unsigned char*) name; *p != 0; ++p)
        if (*p <= 32 || *p >= 127) {
            THROW("CommandRegistry register: invalid ASCII name");
            return false;
        }
    CommandRegistryRow *rows = (*self).rows;
    for (bytes i = 0; i < (*self).count; ++i)
        if (strcmp(rows[i].name, name) == 0) {
            rows[i].target = target;
            return true;
        }
    if ((*self).count == (*self).capacity) {
        if ((*self).refusalCount == 0)
            THROW("CommandRegistry exhausted: request 1 row, count %zu/%zu", (*self).count, (*self).capacity);
        if ((*self).refusalCount != SIZE_MAX)
            ++(*self).refusalCount;
        return false;
    }
    rows[(*self).count++] = (CommandRegistryRow) {name, target};
    return true;
}
bool CommandRegistry_execute(CommandRegistry *self, Command *command) {
    if (self == nullptr || (*self).active || command == nullptr) {
        THROW("CommandRegistry execute: null input or busy");
        return false;
    }
    const char *name = (const char*) Command_name(command);
    CommandRegistryRow *rows = (*self).rows;
    for (bytes i = 0; i < (*self).count; ++i)
        if (strcmp(rows[i].name, name) == 0) {
            (*self).active = true;
            rows[i].target(command);
            (*self).active = false;
            return true;
        }
    static const char unknown[] = "Unknown command";
    bool ok = false;
    (*self).active = true;
    if ((*self).output != nullptr)
        ok = (*self).output((*self).hostContext, unknown, sizeof(unknown) - 1);
    (*self).active = false;
    if (!ok)
        THROW("CommandRegistry execute: missing or failed output sink");
    return false; // not executed, even when unknown notification succeeded
}
bool CommandRegistry_setOutput(CommandRegistry *self, DeterminantOutputFn output, any hostContext) {
    if (self == nullptr || (*self).active) {
        THROW("CommandRegistry setOutput: null or busy");
        return false;
    }
    (*self).output = output;
    (*self).hostContext = hostContext;
    return true;
}
DeterminantOutputFn CommandRegistry_getOutput(const CommandRegistry *self) { return self ? (*self).output : nullptr; }
any CommandRegistry_getHostContext(const CommandRegistry *self) { return self ? (*self).hostContext : nullptr; }
bytes CommandRegistry_getCount(const CommandRegistry *self) { return self ? (*self).count : 0; }
bytes CommandRegistry_getCapacity(const CommandRegistry *self) { return self ? (*self).capacity : 0; }
bytes CommandRegistry_exhaustionCount(const CommandRegistry *self) { return self ? (*self).refusalCount : 0; }
const CommandRegistryRow *CommandRegistry_getRows(const CommandRegistry *self) { return self ? (*self).rows : nullptr; }
bool CommandRegistry_isActive(const CommandRegistry *self) { return self && (*self).active; }
bool CommandRegistry_free(CommandRegistry *self) {
    if (self == nullptr || (*self).active) {
        THROW("CommandRegistry free: null or busy");
        return false;
    }
    *self = CommandRegistry_0();
    return true;
}
bool CommandRegistry_toString(const CommandRegistry *self, char *dest, bytes cap, bool *outTruncated) {
    return CommandRegistry_toStringStruct(self, dest, cap, outTruncated);
}
bool CommandRegistry_toStringStruct(const CommandRegistry *self, char *dest, bytes cap, bool *outTruncated) {
    if (outTruncated != nullptr)
        *outTruncated = false;
    if (dest == nullptr || cap == 0) {
        if (outTruncated != nullptr)
            *outTruncated = true;
        THROW("CommandRegistry projection: missing destination");
        return false;
    }
    int n = self ? snprintf(dest, cap, "CommandRegistry{rows=<borrowed>,capacity=%zu,count=%zu,output=%s,hostContext=<opaque>,refusalCount=%zu,active=%d}",
        (*self).capacity, (*self).count, (*self).output ? "bound" : "none", (*self).refusalCount, (*self).active)
        : snprintf(dest, cap, "nullptr");
    bool cut = n < 0 || (bytes) n >= cap;
    if (outTruncated != nullptr)
        *outTruncated = cut;
    if (cut)
        THROW("CommandRegistry projection: truncated at %zu bytes", cap);
    return !cut;
}
