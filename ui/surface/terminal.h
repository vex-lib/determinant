#ifndef DETERMINANT_TERMINAL_H
#define DETERMINANT_TERMINAL_H
#include "cli/output.h"
#include "ui/surface/terminal_projection.h"
#include "util/constructor.h"
#include <stdint.h>
// Wire/ABI identity; changes require a new explicit admission version.
#define TERMINAL_MODULE_ABI 1u
// Host-normalized key vocabulary, independent of OS/ncurses numeric values.
typedef enum TerminalKey {
    TERMINAL_KEY_NONE, TERMINAL_KEY_ENTER, TERMINAL_KEY_SPACE,
    TERMINAL_KEY_ESCAPE, TERMINAL_KEY_UP, TERMINAL_KEY_DOWN
} TerminalKey;
typedef struct Terminal Terminal;
typedef bool (*TerminalViewFn)(any residentState, Terminal *terminal);
typedef bool (*TerminalCommandFn)(any residentState, const char *text, bytes length);
// Behaviorless module export slot. Contains no ownership or loader behavior.
typedef struct TerminalModule {
    u32 abi;
    TerminalViewFn view;
    TerminalCommandFn command;
} TerminalModule;
struct Terminal {
    char *cells;
    bytes capacity;
    bytes width;
    bytes height;
    DeterminantOutputFn output;
    any hostContext;
    any residentState;
    TerminalModule module;
    u64 generation;
    bytes refusalCount;
    bool paused;
    bool active;
};
Terminal Terminal_0();
#define Terminal(...) CONSTRUCTOR_DISPATCH(Terminal, __VA_ARGS__)
#define Terminal_zero() Terminal_0()
// All spans live/exclusive; geometry multiplication validated cold.
bool Terminal_bind(Terminal *self, char *cells, bytes capacity, bytes width, bytes height,
                   DeterminantOutputFn output, any hostContext);
bool Terminal_setOutput(Terminal *self, DeterminantOutputFn output, any hostContext);
DeterminantOutputFn Terminal_getOutput(const Terminal *self);
any Terminal_getHostContext(const Terminal *self);
bool Terminal_pause(Terminal *self);
bool Terminal_resume(Terminal *self);
// Host validates/trusts loaded code and state before this admission, excludes all
// callbacks, retains candidate/old code until replaced/detached. No dlopen here.
// Null callbacks, wrong ABI, unpaused/busy or exhausted generation reject unchanged.
bool Terminal_swap(Terminal *self, TerminalModule module, any residentState);
bool Terminal_render(Terminal *self);
bool Terminal_command(Terminal *self, const char *text, bytes length);
// Cold printable-ASCII admission; whole text fits one row or rejects unchanged.
bool Terminal_text(Terminal *self, bytes x, bytes y, const char *text, bytes length);
// Hot admitted clear/present, no allocation/logging. Host sink must be bounded.
bool Terminal_clear(Terminal *self);
bool Terminal_present(Terminal *self);
bytes Terminal_getWidth(const Terminal *self);
bytes Terminal_getHeight(const Terminal *self);
bytes Terminal_getCapacity(const Terminal *self);
u64 Terminal_getGeneration(const Terminal *self);
bytes Terminal_exhaustionCount(const Terminal *self);
bool Terminal_isPaused(const Terminal *self);
bool Terminal_isActive(const Terminal *self);
const char *Terminal_getCells(const Terminal *self);
any Terminal_getResidentState(const Terminal *self);
TerminalModule Terminal_getModule(const Terminal *self);
bool Terminal_free(Terminal *self);
TerminalProjection Terminal_toString(const Terminal *self);
TerminalProjection Terminal_toStringStruct(const Terminal *self);
#endif
