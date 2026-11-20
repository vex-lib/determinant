#include "vex.h"
#include "re.h"
#include "cli/commandinfo.h"
#include "oop/oop.h"
#include <string.h>
#include <stdio.h>
#include "core/determinant_memory.h"

;;DEFINITION
/** An ordered run of classified commands. Entries are borrowed pointers to
 * caller-owned CommandInfo blocks; freeing the chain frees each entry. */
;;OVERVIEW
/** ========================================================================
 * CLASS: CommandChain (cli/commandchain.c)
 * STRUCT FIELDS (Mirroring cli/commandinfo.h)
 * CommandInfo **entries       owned pointer directory
 * bytes count                initialized directory extent
 * FUNCTION REGISTRY (exports)
 * Constructors: CommandChain *CommandChain_2(CommandInfo **entries,
 *   bytes count)
 * Getters: bytes CommandChain_getCount(const CommandChain *self)
 *   CommandInfo *CommandChain_getEntry(const CommandChain *self,
 *   bytes index)
 * Lifecycle: fn CommandChain_free(CommandChain *self)
 * Projections: bool CommandChain_toString/CommandChain_toStringStruct(
 *   const CommandChain *self, char *dest, bytes cap, bool *outTruncated)
 * CONTRACT: takes ownership of entries only on success; duplicates and null
 * entries reject before transfer. Free null is harmless.
 * ======================================================================== */

;;GETTER
static inline bytes CommandChain_count(const CommandChain *s) { return (*s).count; }
;;GETTER
static inline bytes *CommandChain_countMut(CommandChain *s) { return &(*s).count; }
;;SETTER
static inline fn CommandChain_setCount(CommandChain *s, bytes v) { (*s).count = v; }
;;GETTER
static inline CommandInfo ** CommandChain_entries(const CommandChain *s) { return (*s).entries; }
;;GETTER
static inline CommandInfo ** *CommandChain_entriesMut(CommandChain *s) { return &(*s).entries; }
;;SETTER
static inline fn CommandChain_setEntries(CommandChain *s, CommandInfo ** v) { (*s).entries = v; }

CommandChain *CommandChain_2(CommandInfo **entries, bytes count) {
    if (count != 0 && entries == nullptr) {
        THROW("CommandChain construct: null entries");
        return nullptr;
    }
    if (count > (UINT32_MAX - sizeof(CommandChain)) / sizeof(CommandInfo*)) {
        THROW("CommandChain construct: extent overflow");
        return nullptr;
    }
    for (bytes i = 0; i < count; ++i) {
        if (entries[i] == nullptr) {
            THROW("CommandChain construct: null entry");
            return nullptr;
        }
        for (bytes j = 0; j < i; ++j)
            if (entries[i] == entries[j]) {
                THROW("CommandChain construct: duplicate entry");
                return nullptr;
            }
    }
    bytes bytes = sizeof(CommandChain) + count * sizeof(CommandInfo*);
    CommandChain *self = Memory_alloc(TYPE_BYTE_ARRAY, Determinant_arena(), bytes);
    if (self == nullptr)
        return nullptr;
    CommandChain_setCount(self, count);
    CommandChain_setEntries(self, (CommandInfo**) (self + 1));
    if (count != 0)
        memcpy(CommandChain_entries(self), entries, count * sizeof(CommandInfo*));
    return self;
}
bytes CommandChain_getCount(const CommandChain *self) { return self ? CommandChain_count(self) : 0; }
CommandInfo *CommandChain_getEntry(const CommandChain *self, bytes index) {
    if (self == nullptr || index >= CommandChain_count(self))
        return nullptr;
    return CommandChain_entries(self)[index];
}
fn CommandChain_free(CommandChain *self) {
    if (self == nullptr)
        return;
    for (bytes i = 0; i < CommandChain_count(self); ++i)
        CommandInfo_free(CommandChain_entries(self)[i]);
    Memory_free(self);
}
bool CommandChain_toString(const CommandChain *self, char *dest, bytes cap, bool *outTruncated) {
    return CommandChain_toStringStruct(self, dest, cap, outTruncated);
}
bool CommandChain_toStringStruct(const CommandChain *self, char *dest, bytes cap, bool *outTruncated) {
    if (outTruncated != nullptr)
        *outTruncated = false;
    if (dest == nullptr || cap == 0) {
        if (outTruncated != nullptr)
            *outTruncated = true;
        THROW("CommandChain projection: missing destination");
        return false;
    }
    i32 n = self ? snprintf(dest, cap, "CommandChain{count=%zu,entries=<owned>}", CommandChain_count(self))
                 : snprintf(dest, cap, "nullptr");
    bool cut = n < 0 || (bytes) n >= cap;
    if (outTruncated != nullptr)
        *outTruncated = cut;
    if (cut)
        THROW("CommandChain projection: truncated at %zu bytes", cap);
    return !cut;
}
