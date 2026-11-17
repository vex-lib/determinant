#include "vex.h"
#include "cli/commandregistry.h"
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

;;GETTER
static inline bool CommandRegistry_active(const CommandRegistry *s) { return (*s).active; }
;;GETTER
static inline bool *CommandRegistry_activeMut(CommandRegistry *s) { return &(*s).active; }
;;SETTER
static inline fn CommandRegistry_setActive(CommandRegistry *s, bool v) { (*s).active = v; }
;;GETTER
static inline bytes CommandRegistry_capacity(const CommandRegistry *s) { return (*s).capacity; }
;;GETTER
static inline bytes *CommandRegistry_capacityMut(CommandRegistry *s) { return &(*s).capacity; }
;;SETTER
static inline fn CommandRegistry_setCapacity(CommandRegistry *s, bytes v) { (*s).capacity = v; }
;;GETTER
static inline bytes CommandRegistry_count(const CommandRegistry *s) { return (*s).count; }
;;GETTER
static inline bytes *CommandRegistry_countMut(CommandRegistry *s) { return &(*s).count; }
;;SETTER
static inline fn CommandRegistry_setCount(CommandRegistry *s, bytes v) { (*s).count = v; }
;;GETTER
static inline any CommandRegistry_hostContext(const CommandRegistry *s) { return (*s).hostContext; }
;;GETTER
static inline any *CommandRegistry_hostContextMut(CommandRegistry *s) { return &(*s).hostContext; }
;;SETTER
static inline fn CommandRegistry_setHostContext(CommandRegistry *s, any v) { (*s).hostContext = v; }
;;GETTER
static inline DeterminantOutputFn CommandRegistry_output(const CommandRegistry *s) { return (*s).output; }
;;GETTER
static inline DeterminantOutputFn *CommandRegistry_outputMut(CommandRegistry *s) { return &(*s).output; }
;;SETTER
static inline fn CommandRegistry_setOutput(CommandRegistry *s, DeterminantOutputFn v) { (*s).output = v; }
;;GETTER
static inline bytes CommandRegistry_refusalCount(const CommandRegistry *s) { return (*s).refusalCount; }
;;GETTER
static inline bytes *CommandRegistry_refusalCountMut(CommandRegistry *s) { return &(*s).refusalCount; }
;;SETTER
static inline fn CommandRegistry_setRefusalCount(CommandRegistry *s, bytes v) { (*s).refusalCount = v; }
;;GETTER
static inline CommandRegistryRow * CommandRegistry_rows(const CommandRegistry *s) { return (*s).rows; }
;;GETTER
static inline CommandRegistryRow * *CommandRegistry_rowsMut(CommandRegistry *s) { return &(*s).rows; }
;;SETTER
static inline fn CommandRegistry_setRows(CommandRegistry *s, CommandRegistryRow * v) { (*s).rows = v; }

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
    if (self == nullptr || CommandRegistry_active(self) || rows == nullptr || capacity < CommandRegistry_count(self)
        || capacity < CommandRegistry_capacity(self) || capacity > SIZE_MAX / sizeof(*rows)
        || (rows == CommandRegistry_rows(self) && capacity != CommandRegistry_capacity(self))) {
        THROW("CommandRegistry reserve: invalid backing or busy");
        return false;
    }
    if (rows != CommandRegistry_rows(self) && CommandRegistry_count(self) != 0)
        memcpy(rows, CommandRegistry_rows(self), CommandRegistry_count(self) * sizeof(*rows));
    CommandRegistry_setRows(self, rows);
    CommandRegistry_setCapacity(self, capacity);
    return true;
}
bool CommandRegistry_register(CommandRegistry *self, const char *name, CommandFn target) {
    if (self == nullptr || CommandRegistry_active(self) || name == nullptr || *name == 0 || target == nullptr) {
        THROW("CommandRegistry register: invalid input or busy");
        return false;
    }
    for (const u8 *p = (const u8*) name; *p != 0; ++p)
        if (*p <= 32 || *p >= 127) {
            THROW("CommandRegistry register: invalid ASCII name");
            return false;
        }
    CommandRegistryRow *rows = CommandRegistry_rows(self);
    for (bytes i = 0; i < CommandRegistry_count(self); ++i)
        if (strcmp(rows[i].name, name) == 0) {
            rows[i].target = target;
            return true;
        }
    if (CommandRegistry_count(self) == CommandRegistry_capacity(self)) {
        if (CommandRegistry_refusalCount(self) == 0)
            THROW("CommandRegistry exhausted: request 1 row, count %zu/%zu", CommandRegistry_count(self), CommandRegistry_capacity(self));
        if (CommandRegistry_refusalCount(self) != SIZE_MAX)
            CommandRegistry_setRefusalCount(self, CommandRegistry_refusalCount(self) + 1u);
        return false;
    }
    rows[(*CommandRegistry_countMut(self))++] = (CommandRegistryRow) {name, target};
    return true;
}
bool CommandRegistry_execute(CommandRegistry *self, Command *command) {
    if (self == nullptr || CommandRegistry_active(self) || command == nullptr) {
        THROW("CommandRegistry execute: null input or busy");
        return false;
    }
    const char *name = (const char*) Command_name(command);
    CommandRegistryRow *rows = CommandRegistry_rows(self);
    for (bytes i = 0; i < CommandRegistry_count(self); ++i)
        if (strcmp(rows[i].name, name) == 0) {
            CommandRegistry_setActive(self, true);
            rows[i].target(command);
            CommandRegistry_setActive(self, false);
            return true;
        }
    static const char unknown[] = "Unknown command";
    bool ok = false;
    CommandRegistry_setActive(self, true);
    if (CommandRegistry_output(self) != nullptr)
        ok = CommandRegistry_output(self)(CommandRegistry_hostContext(self), unknown, sizeof(unknown) - 1);
    CommandRegistry_setActive(self, false);
    if (!ok)
        THROW("CommandRegistry execute: missing or failed output sink");
    return false; // not executed, even when unknown notification succeeded
}
bool CommandRegistry_setOutput(CommandRegistry *self, DeterminantOutputFn output, any hostContext) {
    if (self == nullptr || CommandRegistry_active(self)) {
        THROW("CommandRegistry setOutput: null or busy");
        return false;
    }
    CommandRegistry_setOutput(self, output);
    CommandRegistry_setHostContext(self, hostContext);
    return true;
}
DeterminantOutputFn CommandRegistry_getOutput(const CommandRegistry *self) { return self ? CommandRegistry_output(self) : nullptr; }
any CommandRegistry_getHostContext(const CommandRegistry *self) { return self ? CommandRegistry_hostContext(self) : nullptr; }
bytes CommandRegistry_getCount(const CommandRegistry *self) { return self ? CommandRegistry_count(self) : 0; }
bytes CommandRegistry_getCapacity(const CommandRegistry *self) { return self ? CommandRegistry_capacity(self) : 0; }
bytes CommandRegistry_exhaustionCount(const CommandRegistry *self) { return self ? CommandRegistry_refusalCount(self) : 0; }
const CommandRegistryRow *CommandRegistry_getRows(const CommandRegistry *self) { return self ? CommandRegistry_rows(self) : nullptr; }
bool CommandRegistry_isActive(const CommandRegistry *self) { return self && CommandRegistry_active(self); }
bool CommandRegistry_free(CommandRegistry *self) {
    if (self == nullptr || CommandRegistry_active(self)) {
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
    i32 n = self ? snprintf(dest, cap, "CommandRegistry{rows=<borrowed>,capacity=%zu,count=%zu,output=%s,hostContext=<opaque>,refusalCount=%zu,active=%d}",
        CommandRegistry_capacity(self), CommandRegistry_count(self), CommandRegistry_output(self) ? "bound" : "none", CommandRegistry_refusalCount(self), CommandRegistry_active(self))
        : snprintf(dest, cap, "nullptr");
    bool cut = n < 0 || (bytes) n >= cap;
    if (outTruncated != nullptr)
        *outTruncated = cut;
    if (cut)
        THROW("CommandRegistry projection: truncated at %zu bytes", cap);
    return !cut;
}
