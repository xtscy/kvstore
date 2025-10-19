#ifndef ARENA_ALLOCATOR_H
#define ARENA_ALLOCATOR_H


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdbool.h>
#include "../Sequence_lock/Sequence_lock.h"

#define STAGE_DATA_SIZE 2048 * 5

// 
typedef struct stage_s {
    
    sequence_lock_t lock;
    atomic_size_t used;
    _Atomic(uint8_t) fail_cnt;
    // _Atomic(uint8_t) deref_cnt;
    size_t capacity;
    // refernce_count 用于判断是否可reset
    atomic_size_t reference_count;
    struct stage_s *next;
    uint8_t init_pos[];
} stage_t;

typedef struct stage_allocator_s {

    stage_t *stage;
    // 在外部缓存deref,当达到阈值时,直接调用批处理deref
    _Atomic(size_t) deref_cnt;
    uint8_t monitor_sign;
    size_t last_deref_cnt;

    struct stage_allocator_s *next;
} stage_allocator_t;

// 这里对分配的内存块也进行了封装

typedef struct block_alloc_s {

    uint8_t *ptr;
    size_t size;
    // 这里由于stage有reference_count所以，还需要一个指向stage的指针
    // 当当前块不用时，可以调用该stage的reference_count--
    stage_allocator_t *allocator;
} block_alloc_t;

//create stage
extern stage_t *stage_create(size_t);
// alloc bytes from stage
extern block_alloc_t stage_alloc_optimistic(size_t, stage_allocator_t*);
extern block_alloc_t stage_alloc_pessimistic(size_t, stage_allocator_t*);
// destroy stage
extern bool stage_destroy(stage_t*);
// decrement reference of stage
extern bool stage_deref(stage_t*);
// reset stage memory
extern bool stage_reset(stage_t*);
extern bool stage_deref_batch(stage_t *, size_t);
#endif








