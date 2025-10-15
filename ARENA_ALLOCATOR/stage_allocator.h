#ifndef STAGE_ALLOCATOR
#define STAGE_ALLOCATOR

#define stage_size 1024*512

#include "arena_allocator.h"
#include <unistd.h>

// typedef struct stage_allocator_s {

//     _Atomic(stage_t*) head;
//     // atomic_size_t stage_total;
//     _Atomic(stage_t*) current;

// } stage_allocator_t;

typedef struct stage_allocator_s {

    stage_t *stage;
    // 在外部缓存deref,当达到阈值时,直接调用批处理deref
    _Atomic(uint8_t) deref_cnt;
    stage_allocator_t *next;
} stage_allocator_t;



typedef struct allocator_out_s {

    _Atomic(stage_allocator_t*) head;
    // atomic_size_t stage_total;
    _Atomic(stage_t*) current;
    
} allocator_out_t;

extern bool allocator_out_init(allocator_out_t *);
// 把两个新增原子变量放到arena里面
// 因为是无锁，一般高度耦合
// typedef struct allocator_stage_s {
//     stage_t stage;
//     // alloc失败计数,用原子变量
//     // 这里计数可以放到stage结构中增加耦合度
//     // 先尝试构造一个新的结构体即当前结构体这种写法
//     // 这样可以不更改stage的底层代码
//    // _Atomic(uint8_t) deref_cnt;
////     _Atomic(uint8_t) fail_cnt;
//// } allocator_stage_t;

extern bool stage_allocator_init(stage_allocator_t *);
// extern stage_t* get_stage(stage_allocator_t *);
extern block_alloc_t allocator_alloc(stage_allocator_t *, size_t);
extern bool allocator_deref(stage_allocator_t *);

#endif