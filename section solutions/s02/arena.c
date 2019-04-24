#include "membench.h"
#include <stdlib.h>
#include <assert.h>


union chunk_space;

// A `free_chunk` was allocated by the arena, but isn't currently in
// use. You'll likely put some bookkeeping information into such chunks.
typedef struct free_space {
    union chunk_space* next;
} free_space;

// An `allocation` object is *either* a chunk (an active allocation),
// *or* a `free_space` object.
typedef union chunk_space {
    chunk active;
    free_space free;
} chunk_space;

// An `arena_group` object contains 1024 `chunk_space` objects.
// Objects `[0, nused)` have been allocated, BUT MAY BE ON THE FREE
// LIST; objects `[nused, 1024)` have never been allocated.
#define NSPACES 1024
typedef struct arena_group {
    int nused;
    chunk_space data[NSPACES];
    struct arena_group* next;
} arena_group;

// A `membench_arena` is a singly-linked list of groups PLUS A FREE LIST.
struct membench_arena {
    arena_group* groups;
    chunk_space* free;
};


membench_arena* membench_arena_create(void) {
    membench_arena* arena = (membench_arena*) malloc(sizeof(membench_arena));
    arena->groups = NULL;
    arena->free = NULL;
    return arena;
}


// group_create(arena)
//    Allocate a new arena_group and PREPEND it to the arena->groups list.
//    Return the new group.
static arena_group* group_create(membench_arena* arena) {
    assert(!arena->groups || arena->groups->nused == NSPACES);
    arena_group* g = (arena_group*) malloc(sizeof(arena_group));
    g->nused = 0;
    g->next = arena->groups;
    arena->groups = g;
    return g;
}


chunk* membench_alloc(membench_arena* arena) {
    if (arena->free) {
        // Remove from free list
        chunk* result = &arena->free->active;
        arena->free = arena->free->free.next;
        return result;
    }

    arena_group* g = arena->groups;
    if (!g || g->nused == NSPACES) {
        g = group_create(arena);
        assert(g && g->nused < NSPACES);
    }

    chunk* result = &g->data[g->nused].active;
    ++g->nused;
    return result;
}


void membench_free(membench_arena* arena, chunk* x) {
    // Add to free list
    chunk_space* sp = (chunk_space*) x;
    sp->free.next = arena->free;
    arena->free = sp;
}


void membench_arena_destroy(membench_arena* arena) {
    while (arena->groups != NULL) {
        arena_group* g = arena->groups;
        arena->groups = g->next;
        free(g);
    }
    free(arena);
}
