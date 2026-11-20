#include "vex.h"
#include "core/determinant_memory.h"

static MemoryArena *s_determinantArena = nullptr;

MemoryArena *Determinant_arena() {
    if (s_determinantArena == nullptr)
        s_determinantArena = MemoryArena_create(DETERMINANT_ARENA_DEFAULT_BYTES);
    return s_determinantArena;
}

fn Determinant_arenaShutdown() {
    if (s_determinantArena != nullptr) {
        MemoryArena_destroy(s_determinantArena);
        s_determinantArena = nullptr;
    }
}
