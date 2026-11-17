#include "vex.h"
#include "ui/surface/terminal.h"
#include <string.h>
#include <stdio.h>

;;DEFINITION
/** Terminal is resident R3 presentation state, not OS terminal acquisition.
 * The host lends a flat printable-ASCII cell grid, output callback and resident
 * state. View/command exports can change while these borrowed resources stay
 * resident. Admission pauses first; the active guard excludes reentrant swaps.
 * The host owns trust, ABI/state preparation, loaded-code lifetime and external
 * serialization. No atomics claim to pin retired code, and no OS/loader or R1
 * include exists here. This foundation does not implement Primeagen /update. */
;;OVERVIEW
/** ========================================================================
 * CLASS: Terminal (ui/surface/terminal.c)
 * STRUCT FIELDS (Mirroring ui/surface/terminal.h)
 * char *cells                    borrowed contiguous cell backing
 * bytes capacity                admitted backing bytes
 * bytes width                   columns (cells)
 * bytes height                  rows (cells)
 * DeterminantOutputFn output      borrowed host sink
 * any hostContext              opaque resident sink state
 * any residentState            opaque resident consumer state
 * TerminalModule module          borrowed reloadable exports
 * u64 generation            successful export publications
 * bytes refusalCount            epoch text capacity refusals
 * bool paused                    new module admission disabled
 * bool active                    current callback exclusion
 * SLOT RECORD: TerminalModule
 * u32 abi                   explicit export ABI identity
 * TerminalViewFn view            borrowed render callback
 * TerminalCommandFn command      borrowed command callback
 * TerminalKey: normalized host key vocabulary (enum, no owned state)
 * FUNCTION REGISTRY (exports)
 * Constructor: Terminal Terminal_0() [zero/arity chooser]
 * Admission: bool Terminal_bind(Terminal *self, char *cells, bytes capacity,
 *     bytes width, bytes height, DeterminantOutputFn output, any hostContext)
 * Configuration: bool Terminal_setOutput(Terminal *self,
 *     DeterminantOutputFn output, any hostContext)
 * Lifecycle: bool Terminal_pause(Terminal *self)
 *   bool Terminal_resume(Terminal *self), Terminal_free(Terminal *self)
 * Reload: bool Terminal_swap(Terminal *self, TerminalModule module,
 *     any residentState)
 * Dispatch: bool Terminal_render(Terminal *self)
 *   bool Terminal_command(Terminal *self, const char *text, bytes length)
 * Cells: bool Terminal_text(Terminal *self, bytes x, bytes y,
 *     const char *text, bytes length)
 *   bool Terminal_clear(Terminal *self), Terminal_present(Terminal *self)
 * Queries: DeterminantOutputFn Terminal_getOutput(const Terminal *self)
 *   any Terminal_getHostContext(const Terminal *self)
 *   any Terminal_getResidentState(const Terminal *self)
 *   const char *Terminal_getCells(const Terminal *self)
 *   TerminalModule Terminal_getModule(const Terminal *self)
 *   bytes Terminal_getWidth(const Terminal *self)
 *   bytes Terminal_getHeight(const Terminal *self)
 *   bytes Terminal_getCapacity(const Terminal *self)
 *   u64 Terminal_getGeneration(const Terminal *self)
 *   bytes Terminal_exhaustionCount(const Terminal *self)
 *   bool Terminal_isPaused(const Terminal *self), Terminal_isActive(const Terminal *self)
 * Projections: TerminalProjection Terminal_toString/toStringStruct(self)
 * CONTRACT: null query defaults; cold invalid operations THROW once; hot
 * clear/present remain quiet. Text admission validates full span before copying.
 * Callback false may have changed caller cells/state: host owns candidate-state
 * preparation/rollback, not arbitrary native callback undo. Swap never invokes
 * candidate code and rejects unchanged. Render/present do not allocate. A sink
 * failure may have delivered a prefix. Free detaches borrowed state, never frees
 * it; busy rejects. All calls require host external serialization. Cold rebind
 * may grow/resize backing with active users excluded; initial capacity is not a
 * total ceiling. Zero terminal is paused; bind installs dimensions, not exports.
 * ======================================================================== */

;;GETTER
static inline u32 TerminalModule_abi(const TerminalModule *s) { return (*s).abi; }
;;GETTER
static inline u32 *TerminalModule_abiMut(TerminalModule *s) { return &(*s).abi; }
;;SETTER
static inline fn TerminalModule_setAbi(TerminalModule *s, u32 v) { (*s).abi = v; }
;;GETTER
static inline TerminalCommandFn TerminalModule_command(const TerminalModule *s) { return (*s).command; }
;;GETTER
static inline TerminalCommandFn *TerminalModule_commandMut(TerminalModule *s) { return &(*s).command; }
;;SETTER
static inline fn TerminalModule_setCommand(TerminalModule *s, TerminalCommandFn v) { (*s).command = v; }
;;GETTER
static inline TerminalViewFn TerminalModule_view(const TerminalModule *s) { return (*s).view; }
;;GETTER
static inline TerminalViewFn *TerminalModule_viewMut(TerminalModule *s) { return &(*s).view; }
;;SETTER
static inline fn TerminalModule_setView(TerminalModule *s, TerminalViewFn v) { (*s).view = v; }

;;GETTER
static inline bool Terminal_active(const Terminal *s) { return (*s).active; }
;;GETTER
static inline bool *Terminal_activeMut(Terminal *s) { return &(*s).active; }
;;SETTER
static inline fn Terminal_setActive(Terminal *s, bool v) { (*s).active = v; }
;;GETTER
static inline bytes Terminal_capacity(const Terminal *s) { return (*s).capacity; }
;;GETTER
static inline bytes *Terminal_capacityMut(Terminal *s) { return &(*s).capacity; }
;;SETTER
static inline fn Terminal_setCapacity(Terminal *s, bytes v) { (*s).capacity = v; }
;;GETTER
static inline char * Terminal_cells(const Terminal *s) { return (*s).cells; }
;;GETTER
static inline char * *Terminal_cellsMut(Terminal *s) { return &(*s).cells; }
;;SETTER
static inline fn Terminal_setCells(Terminal *s, char * v) { (*s).cells = v; }
;;GETTER
static inline u64 Terminal_generation(const Terminal *s) { return (*s).generation; }
;;GETTER
static inline u64 *Terminal_generationMut(Terminal *s) { return &(*s).generation; }
;;SETTER
static inline fn Terminal_setGeneration(Terminal *s, u64 v) { (*s).generation = v; }
;;GETTER
static inline bytes Terminal_height(const Terminal *s) { return (*s).height; }
;;GETTER
static inline bytes *Terminal_heightMut(Terminal *s) { return &(*s).height; }
;;SETTER
static inline fn Terminal_setHeight(Terminal *s, bytes v) { (*s).height = v; }
;;GETTER
static inline any Terminal_hostContext(const Terminal *s) { return (*s).hostContext; }
;;GETTER
static inline any *Terminal_hostContextMut(Terminal *s) { return &(*s).hostContext; }
;;SETTER
static inline fn Terminal_setHostContext(Terminal *s, any v) { (*s).hostContext = v; }
;;GETTER
static inline TerminalModule Terminal_module(const Terminal *s) { return (*s).module; }
;;GETTER
static inline TerminalModule *Terminal_moduleMut(Terminal *s) { return &(*s).module; }
;;SETTER
static inline fn Terminal_setModule(Terminal *s, TerminalModule v) { (*s).module = v; }
;;GETTER
static inline DeterminantOutputFn Terminal_output(const Terminal *s) { return (*s).output; }
;;GETTER
static inline DeterminantOutputFn *Terminal_outputMut(Terminal *s) { return &(*s).output; }
;;SETTER
static inline fn Terminal_setOutput(Terminal *s, DeterminantOutputFn v) { (*s).output = v; }
;;GETTER
static inline bool Terminal_paused(const Terminal *s) { return (*s).paused; }
;;GETTER
static inline bool *Terminal_pausedMut(Terminal *s) { return &(*s).paused; }
;;SETTER
static inline fn Terminal_setPaused(Terminal *s, bool v) { (*s).paused = v; }
;;GETTER
static inline bytes Terminal_refusalCount(const Terminal *s) { return (*s).refusalCount; }
;;GETTER
static inline bytes *Terminal_refusalCountMut(Terminal *s) { return &(*s).refusalCount; }
;;SETTER
static inline fn Terminal_setRefusalCount(Terminal *s, bytes v) { (*s).refusalCount = v; }
;;GETTER
static inline any Terminal_residentState(const Terminal *s) { return (*s).residentState; }
;;GETTER
static inline any *Terminal_residentStateMut(Terminal *s) { return &(*s).residentState; }
;;SETTER
static inline fn Terminal_setResidentState(Terminal *s, any v) { (*s).residentState = v; }
;;GETTER
static inline bytes Terminal_width(const Terminal *s) { return (*s).width; }
;;GETTER
static inline bytes *Terminal_widthMut(Terminal *s) { return &(*s).width; }
;;SETTER
static inline fn Terminal_setWidth(Terminal *s, bytes v) { (*s).width = v; }

;;INTENTION("per the Conflict Triage Law: geometry, backing, callback generation and admission change through lifecycle, not setters forging ownership")
Terminal Terminal_0() { return (Terminal) {.paused = true}; }
bool Terminal_bind(Terminal *self, char *cells, bytes capacity, bytes width, bytes height,
                   DeterminantOutputFn output, any hostContext) {
    if (self == nullptr || Terminal_active(self) || !Terminal_paused(self) || cells == nullptr || output == nullptr
        || width == 0 || height == 0 || height > SIZE_MAX / width || width * height > capacity) {
        THROW("Terminal bind: invalid geometry/backing/sink or busy");
        return false;
    }
    Terminal_setCells(self, cells);
    Terminal_setCapacity(self, capacity);
    Terminal_setWidth(self, width);
    Terminal_setHeight(self, height);
    Terminal_setOutput(self, output);
    Terminal_setHostContext(self, hostContext);
    memset(cells, ' ', width * height);
    return true;
}
bool Terminal_setOutput(Terminal *self, DeterminantOutputFn output, any hostContext) {
    if (self == nullptr || Terminal_active(self) || output == nullptr) {
        THROW("Terminal setOutput: invalid sink or busy");
        return false;
    }
    Terminal_setOutput(self, output);
    Terminal_setHostContext(self, hostContext);
    return true;
}
bool Terminal_pause(Terminal *self) {
    if (self == nullptr) {
        THROW("Terminal pause: null terminal");
        return false;
    }
    Terminal_setPaused(self, true);
    return !Terminal_active(self); // stop new admission even while callback must finish
}
bool Terminal_resume(Terminal *self) {
    TerminalModule *module = self ? Terminal_moduleMut(self) : nullptr;
    if (self == nullptr || Terminal_active(self) || Terminal_cells(self) == nullptr || TerminalModule_view(module) == nullptr) {
        THROW("Terminal resume: unbound or busy");
        return false;
    }
    Terminal_setPaused(self, false);
    return true;
}
bool Terminal_swap(Terminal *self, TerminalModule module, any residentState) {
    if (self == nullptr || Terminal_active(self) || !Terminal_paused(self) || module.abi != TERMINAL_MODULE_ABI
        || module.view == nullptr || module.command == nullptr || Terminal_generation(self) == UINT64_MAX) {
        THROW("Terminal swap: incompatible exports, generation overflow or busy");
        return false;
    }
    Terminal_setModule(self, module);
    Terminal_setResidentState(self, residentState);
    Terminal_setGeneration(self, Terminal_generation(self) + 1u);
    return true;
}
bool Terminal_render(Terminal *self) {
    if (self == nullptr || Terminal_paused(self) || Terminal_active(self)) {
        THROW("Terminal render: null, paused or busy");
        return false;
    }
    TerminalModule *module = Terminal_moduleMut(self);
    Terminal_setActive(self, true);
    bool ok = TerminalModule_view(module)(Terminal_residentState(self), self);
    Terminal_setActive(self, false);
    if (!ok)
        THROW("Terminal render: view rejected");
    return ok;
}
bool Terminal_command(Terminal *self, const char *text, bytes length) {
    if (self == nullptr || Terminal_paused(self) || Terminal_active(self) || (text == nullptr && length != 0)) {
        THROW("Terminal command: invalid span, paused or busy");
        return false;
    }
    for (bytes i = 0; i < length; ++i)
        if ((u8) text[i] < 32 || (u8) text[i] >= 127) {
            THROW("Terminal command: invalid ASCII byte at %zu", i);
            return false;
        }
    TerminalModule *module = Terminal_moduleMut(self);
    Terminal_setActive(self, true);
    bool ok = TerminalModule_command(module)(Terminal_residentState(self), text, length);
    Terminal_setActive(self, false);
    if (!ok)
        THROW("Terminal command: callback rejected");
    return ok;
}
bool Terminal_text(Terminal *self, bytes x, bytes y, const char *text, bytes length) {
    if (self == nullptr || Terminal_cells(self) == nullptr || (text == nullptr && length != 0)) {
        THROW("Terminal text: invalid backing/span");
        return false;
    }
    if (y >= Terminal_height(self) || x > Terminal_width(self) || length > Terminal_width(self) - x) {
        if (Terminal_refusalCount(self) == 0)
            THROW("Terminal exhausted: request %zu cells at %zu,%zu, bound %zux%zu", length, x, y, Terminal_width(self), Terminal_height(self));
        if (Terminal_refusalCount(self) != SIZE_MAX)
            Terminal_setRefusalCount(self, Terminal_refusalCount(self) + 1u);
        return false;
    }
    for (bytes i = 0; i < length; ++i)
        if ((u8) text[i] < 32 || (u8) text[i] >= 127) {
            THROW("Terminal text: invalid ASCII byte at %zu", i);
            return false;
        }
    if (length != 0)
        memcpy(Terminal_cells(self) + y * Terminal_width(self) + x, text, length);
    return true;
}
bool Terminal_clear(Terminal *self) {
    if (self == nullptr || Terminal_cells(self) == nullptr)
        return false;
    memset(Terminal_cells(self), ' ', Terminal_width(self) * Terminal_height(self));
    return true;
}
bool Terminal_present(Terminal *self) {
    if (self == nullptr || Terminal_cells(self) == nullptr || Terminal_active(self))
        return false;
    Terminal_setActive(self, true);
    bool ok = Terminal_output(self)(Terminal_hostContext(self), Terminal_cells(self), Terminal_width(self) * Terminal_height(self));
    Terminal_setActive(self, false);
    return ok;
}
DeterminantOutputFn Terminal_getOutput(const Terminal *self) { return self ? Terminal_output(self) : nullptr; }
any Terminal_getHostContext(const Terminal *self) { return self ? Terminal_hostContext(self) : nullptr; }
bytes Terminal_getWidth(const Terminal *self) { return self ? Terminal_width(self) : 0; }
bytes Terminal_getHeight(const Terminal *self) { return self ? Terminal_height(self) : 0; }
bytes Terminal_getCapacity(const Terminal *self) { return self ? Terminal_capacity(self) : 0; }
u64 Terminal_getGeneration(const Terminal *self) { return self ? Terminal_generation(self) : 0; }
bytes Terminal_exhaustionCount(const Terminal *self) { return self ? Terminal_refusalCount(self) : 0; }
bool Terminal_isPaused(const Terminal *self) { return self && Terminal_paused(self); }
bool Terminal_isActive(const Terminal *self) { return self && Terminal_active(self); }
const char *Terminal_getCells(const Terminal *self) { return self ? Terminal_cells(self) : nullptr; }
any Terminal_getResidentState(const Terminal *self) { return self ? Terminal_residentState(self) : nullptr; }
TerminalModule Terminal_getModule(const Terminal *self) { return self ? Terminal_module(self) : (TerminalModule) {0}; }
bool Terminal_free(Terminal *self) {
    if (self == nullptr || Terminal_active(self)) {
        THROW("Terminal free: null or busy");
        return false;
    }
    *self = Terminal_0();
    return true;
}
TerminalProjection Terminal_toString(const Terminal *self) {
    return Terminal_toStringStruct(self);
}
TerminalProjection Terminal_toStringStruct(const Terminal *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    TerminalModule const *module = self ? Terminal_moduleMut(self) : nullptr;
    return TerminalProjection_format("Terminal{cells=<borrowed>,capacity=%zu,width=%zu,height=%zu,output=%s,hostContext=<opaque>,residentState=<opaque>,module={abi=%u},generation=%llu,refusalCount=%zu,paused=%d,active=%d}",
        Terminal_capacity(self), Terminal_width(self), Terminal_height(self), Terminal_output(self) ? "bound" : "none", TerminalModule_abi(module),
        (u64) Terminal_generation(self), Terminal_refusalCount(self), Terminal_paused(self), Terminal_active(self));
}
