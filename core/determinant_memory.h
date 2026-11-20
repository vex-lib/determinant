#ifndef CORE_DETERMINANT_MEMORY_H
#define CORE_DETERMINANT_MEMORY_H

#include "vex.h"
#include "nio/mem.h"

#define DETERMINANT_ARENA_DEFAULT_BYTES (64u * 1024u * 1024u)

MemoryArena *Determinant_arena();
fn Determinant_arenaShutdown();

#endif
