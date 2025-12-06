#ifndef STAGE_ALLOCATOR
#define STAGE_ALLOCATOR

#define stage_size 1024

#include "arena_allocator.h"
#include <unistd.h>

// typedef struct stage_allocator_s {

//     _Atomic(stage_t*) head;
//     // atomic_size_t stage_total;
//     _Atomic(stage_t*) current;

// } stage_allocator_t;





//* 这里可以使用循环链表
//* 目前先使用单链表
//* 如果使用循环链表
typedef struct allocator_out_s {

    _Atomic(stage_allocator_t*) head;
    // atomic_size_t stage_total;
    _Atomic(stage_allocator_t*) current;
    
} allocator_out_t;

extern bool allocator_out_init(allocator_out_t *);
// typedef struct allocator_stage_s {
//     stage_t stage;
//     // alloc失败计数,用原子变量
//     // 这里计数可以放到stage结构中增加耦合度
//     // 先尝试构造一个新的结构体即当前结构体这种写法
//     // 这样可以不更改stage的底层代码
//    // _Atomic(uint8_t) deref_cnt;
////     _Atomic(uint8_t) fail_cnt;
//// } allocator_stage_t;

// extern bool stage_allocator_init(stage_allocator_t *);
// extern stage_t* get_stage(stage_allocator_t *);
extern bool allocator_out_init(allocator_out_t*);
extern block_alloc_t allocator_alloc(allocator_out_t *, size_t);
extern bool allocator_deref(stage_allocator_t *);
extern allocator_out_t global_allocator;
#endif