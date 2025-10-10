#ifndef STAGE_ALLOCATOR
#define STAGE_ALLOCATOR


#include "arena_allocator.h"
#include <unistd.h>

typedef struct stage_allocator_s {

    _Atomic(stage_t*) head;
    // atomic_size_t stage_total;
    _Atomic(stage_t*) current;


} stage_allocator_t;

extern bool stage_allocator_init(stage_allocator_t *);
// extern stage_t* get_stage(stage_allocator_t *);
extern block_alloc_t allocator_alloc(stage_allocator_t *, size_t);
#endif